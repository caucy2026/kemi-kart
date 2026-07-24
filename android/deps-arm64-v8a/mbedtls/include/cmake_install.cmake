# Install script for directory: /Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/Users/newlink/android-sdk/ndk/26.1.10909125/toolchains/llvm/prebuilt/darwin-x86_64/bin")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/Users/newlink/android-sdk/ndk/26.1.10909125/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/aes.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/aria.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/asn1.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/asn1write.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/base64.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/bignum.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/build_info.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/camellia.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ccm.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/chacha20.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/chachapoly.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/check_config.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/cipher.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/cmac.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/compat-2.x.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/config_adjust_x509.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/config_psa.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/constant_time.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ctr_drbg.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/debug.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/des.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/dhm.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ecdh.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ecdsa.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ecjpake.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ecp.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/entropy.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/error.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/gcm.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/hkdf.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/hmac_drbg.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/lms.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/mbedtls_config.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/md.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/md5.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/net_sockets.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/nist_kw.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/oid.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/pem.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/pk.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/pkcs12.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/pkcs5.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/pkcs7.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/platform.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/platform_time.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/platform_util.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/poly1305.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/private_access.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/psa_util.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ripemd160.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/rsa.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/sha1.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/sha256.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/sha3.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/sha512.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ssl.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ssl_cache.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ssl_cookie.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/ssl_ticket.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/threading.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/timing.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/version.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/x509.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/x509_crl.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/x509_crt.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/build_info.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_builtin_composites.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_builtin_primitives.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_compat.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_config.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_driver_common.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_extra.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_legacy.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_platform.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_se_driver.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_sizes.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_struct.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_types.h"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/mbedtls/include/psa/crypto_values.h"
    )
endif()

