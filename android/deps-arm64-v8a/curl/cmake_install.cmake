# Install script for directory: /Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/docs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/scripts/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/lib/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/docs/examples/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE FILES "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/curl-config")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/libcurl.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE DIRECTORY FILES "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/include/curl" FILES_MATCHING REGEX "/[^/]*\\.h$")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/CURL/CURLTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/CURL/CURLTargets.cmake"
         "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMakeFiles/Export/8e83d16133499b505bf3986f4f209a65/CURLTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/CURL/CURLTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/CURL/CURLTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/CURL" TYPE FILE FILES "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMakeFiles/Export/8e83d16133499b505bf3986f4f209a65/CURLTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/CURL" TYPE FILE FILES "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMakeFiles/Export/8e83d16133499b505bf3986f4f209a65/CURLTargets-noconfig.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/CURL" TYPE FILE FILES
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/generated/CURLConfigVersion.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/generated/CURLConfig.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindBrotli.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindCares.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindGSS.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindGnuTLS.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindLDAP.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindLibbacktrace.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindLibgsasl.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindLibidn2.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindLibpsl.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindLibssh.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindLibssh2.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindLibuv.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindMbedTLS.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindNGHTTP2.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindNGHTTP3.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindNGTCP2.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindNettle.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindQuiche.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindRustls.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindWolfSSL.cmake"
    "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/CMake/FindZstd.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
  file(WRITE "/Users/newlink/kemi/stk-code/android/deps-arm64-v8a/curl/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
