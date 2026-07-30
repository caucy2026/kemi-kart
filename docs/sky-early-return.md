# 天空像素提前返回 — 技术分析

> 文件：`data/shaders/combine_diffuse_color.frag`
> 提交：`53f9ed8` — `perf: sky pixel early-return in deferred combine + FPS toggle linkage`

## 一、背景

`combine_diffuse_color.frag` 是 STK 延迟渲染管线的**最终合成 shader**。每个屏幕像素都会执行一次，职责是：

1. 从 GBuffer 读取 diffuse、specular、normal、材质参数。
2. 计算 metallic/diffuse IBL + specular + emissive 光照。
3. 通过深度反算视空间坐标，计算雾并混合。
4. 将 light scatter（体积光/泛光）按 `GL_ONE, GL_ONE_MINUS_SRC_ALPHA` 混合到最终颜色。

问题在于：深度值 `1.0`（远平面）的像素代表**天空**。这些像素的 diffuse/normal/specular/材质对最终画面没有任何贡献——原版在完成所有光照计算后，才发现 `depth == 1.0`，然后用 `bg_color` 覆盖结果。

这意味着天空像素白白消耗了 4 次纹理采样、1 次矩阵乘法、1 次 `exp()` 和全部光照 ALU。

## 二、原始 shader 天空像素执行流程

```
纹理采样（5 次）:
  diffuse_color  ← 1
  normal_color   ← 2
  diffuse_map    ← 3
  specular_map   ← 4
  depth_stencil  ← 5

ALU:
  metallicMatColor = mix(0.04, diffuseMatColor*4, metallicMapValue)
  tmp = Diffuse * mix(diffuseMatColor, 0, metallic) + metallicMatColor * Specular
  emitCol = diffuseMatColor + diffuseMatColor² * emit² * 10
  color_1 = vec4(tmp + emit*emitCol, 1.0)

矩阵运算:
  xpos = inverseProjectionMatrix × (uv, depth)    ← 矩阵乘法
  dist = length(xpos.xyz)
  factor = 1.0 - exp(fogDensity * dist)            ← exp()
  color_1 = color_1 + vec4(fog, factor)

后知后觉:
  if (depth == 1.0) { color_1 = bg_color; }        ← 前面全白算

最终:
  ls = texture(light_scatter)                       ← 6
  o_final_color = ls + color_1 * (1 - ls.a)
```

## 三、优化后天空像素执行流程

```glsl
float depth = texture(depth_stencil, tc).x;   // 第一步就读深度
if (depth == 1.0)
{
    vec4 ls = texture(light_scatter, tc);      // 仅需这次纹理采样
    o_final_color = ls + bg_color * (1.0 - ls.a);
    return;                                     // 直接结束
}
// 非天空像素走完整光照路径（同原版）
```

```
纹理采样（2 次）:
  depth_stencil  ← 1
  light_scatter  ← 2

ALU:
  o_final_color = ls + bg_color * (1 - ls.a)

return  ← 跳过全部光照、矩阵、exp
```

## 四、数学等价性证明

### 原版末尾公式

```glsl
// Light scatter alpha blend (GL_ONE, GL_ONE_MINUS_SRC_ALPHA)
vec4 ls = texture(light_scatter, tc);
color_2.r = ls.r + color_1.r * (1.0 - ls.a);
color_2.g = ls.g + color_1.g * (1.0 - ls.a);
color_2.b = ls.b + color_1.b * (1.0 - ls.a);
color_2.a = ls.a + color_1.a * (1.0 - ls.a);
o_final_color = color_2;
```

向量形式：

$$C_{\text{final}} = C_{\text{scatter}} + C_{\text{color\_1}} \cdot (1 - \alpha_{\text{scatter}})$$

### 原版对天空像素的处理

```glsl
if (depth == 1.0)
{
    color_1 = bg_color;   // 覆盖之前所有计算结果
}
```

代入上式：

$$C_{\text{final}} = C_{\text{scatter}} + C_{\text{background}} \cdot (1 - \alpha_{\text{scatter}})$$

### 优化版的直接实现

```glsl
o_final_color = ls + bg_color * (1.0 - ls.a);
```

其中 `bg_color * (1.0 - ls.a)` 是 vec4 与标量乘法，对 RGBA 四个分量同时生效，与 GLSL 的 swizzle 乘法语义完全一致，等价于上面 per-channel 展开。

**结论：无近似、无精度差异、无截断误差。像素级严格等价。**

## 五、优化了什么（指令级）

### 跳过的纹理采样（天空像素）

| 纹理 | 格式 | 带宽节省（估算） |
|---|---|---|
| `diffuse_color` | RGBA8 | 4 B |
| `normal_color` | RGBA8 | 4 B |
| `diffuse_map` | RGBA16F | 8 B |
| `specular_map` | RGBA16F | 8 B |
| **合计** | | **~24 B/pixel** |

1080p 屏幕约 2M 像素，若 30% 为天空，每帧省约 14 MB 纹理带宽。

### 跳过的 ALU 操作（天空像素）

- 3 次 `mix()`（metallic 计算链）
- 多次 `*` 和 `+`（diffuse IBL + specular + emit）
- 1 次矩阵乘法（`getPosFromUVDepth`）
- 1 次 `length()`（距离开销）
- 1 次 `exp()`（雾密度 — 这是最贵的单条指令）
- 1 次 `vec4 + vec4(fog, factor)`（雾混合）

### Mali-G52 影响

Mali-G52 是 tile-based renderer。减少 fragment shader 中无用的纹理采样和 ALU 可以：

- 降低 tile 内 fragment 执行时间，可能减少 tile 的 back-pressure。
- 减少纹理缓存（texture cache）的无效填充。
- `exp()` 在 Mali 上是较昂贵的超越函数指令，跳过它对功耗和延迟都有收益。

## 六、适用条件

- 仅对 `depth == 1.0`（远平面，即天空像素）触发。
- `gl_FragCoord` 直接用于 `tc` 计算，不依赖 `discard` 或 `gl_FragDepth` 写入。
- 项目当前版本未使用 reversed-Z 或 infinite far plane；深度比较判断无需调整。
- 地面、车辆、植被、道具等所有非天空像素走原版完整路径，零影响。

## 七、验证

- 双屏截图比对：`black_forest` 两屏天空颜色、雾过渡、光散射无可见差异。
- `combine_diffuse_color.frag` 是本管线唯一写入 `o_final_color` 的 pass，无后续 pass 依赖天空像素的中间值。
- light scatter 纹理在天空区域的值与原版完全相同（同一帧 buffer 内容，未修改生成逻辑）。

## 八、限制

- 收益取决于天空覆盖率。`black_forest` 密林场景收益较小；`abyss`、`candela_city` 等开阔赛道收益更大。
- 这是**局部像素级优化**，不改变管线结构，不减少 draw call 或场景遍历开销。
- 深度比较使用精确 `== 1.0`。如果未来引入 reversed-Z 或 logarithmic depth，需调整为远平面等价判断。
