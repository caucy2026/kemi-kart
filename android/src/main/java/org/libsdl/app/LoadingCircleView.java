package org.libsdl.app;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.View;

/**
 * LoadingCircleView - draws a white circle outline with blue "KEMI" text centered.
 * Used on Display 2 while native rendering initializes.
 */
public class LoadingCircleView extends View {

    private final Paint mCirclePaint;
    private final Paint mTextPaint;
    private final Rect mTextBounds = new Rect();
    private float mRadius = 0f;
    private float mAnimPhase = 0f;

    public LoadingCircleView(Context context) {
        this(context, null);
    }

    public LoadingCircleView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setBackgroundColor(Color.parseColor("#0A0F28"));

        mCirclePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mCirclePaint.setStyle(Paint.Style.STROKE);
        mCirclePaint.setStrokeWidth(8f);
        mCirclePaint.setColor(Color.WHITE);

        mTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mTextPaint.setColor(Color.parseColor("#3366FF"));
        mTextPaint.setTextSize(120f);
        mTextPaint.setTextAlign(Paint.Align.CENTER);
        mTextPaint.setFakeBoldText(true);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float minDim = Math.min(w, h);
        mRadius = minDim * 0.30f; // 30% of screen min dimension
        mTextPaint.setTextSize(minDim * 0.18f);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float cx = getWidth() / 2f;
        float cy = getHeight() / 2f;

        // Pulsing radius: 30% → 60% of min dimension
        float minDim = Math.min(getWidth(), getHeight());
        float baseRadius = minDim * 0.30f;
        float pulseAmp = minDim * 0.15f;
        mAnimPhase += 0.05f;
        float radius = baseRadius + pulseAmp * (1f + (float) Math.sin(mAnimPhase)) / 2f;

        // Draw white circle
        canvas.drawCircle(cx, cy, radius, mCirclePaint);

        // Draw blue "KEMI" centered
        String text = "KEMI";
        mTextPaint.getTextBounds(text, 0, text.length(), mTextBounds);
        float textY = cy - mTextBounds.exactCenterY();
        canvas.drawText(text, cx, textY, mTextPaint);

        // Request next frame for animation
        postInvalidateOnAnimation();
    }
}
