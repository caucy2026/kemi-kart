# This code is from the CMake FAQ

if (NOT EXISTS "/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/libjpeg/install_manifest.txt")
  message(FATAL_ERROR "Cannot find install manifest: \"/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/libjpeg/install_manifest.txt\"")
endif(NOT EXISTS "/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/libjpeg/install_manifest.txt")

file(READ "/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/libjpeg/install_manifest.txt" files)
string(REGEX REPLACE "\n" ";" files "${files}")
list(REVERSE files)
foreach (file ${files})
  message(STATUS "Uninstalling \"$ENV{DESTDIR}${file}\"")
    if (EXISTS "$ENV{DESTDIR}${file}")
      execute_process(
        COMMAND "/private/tmp/cmake-3.30.0-macos-universal/CMake.app/Contents/bin/cmake" -E remove "$ENV{DESTDIR}${file}"
        OUTPUT_VARIABLE rm_out
        RESULT_VARIABLE rm_retval
      )
    if(NOT ${rm_retval} EQUAL 0)
      message(FATAL_ERROR "Problem when removing \"$ENV{DESTDIR}${file}\"")
    endif (NOT ${rm_retval} EQUAL 0)
  else (EXISTS "$ENV{DESTDIR}${file}")
    message(STATUS "File \"$ENV{DESTDIR}${file}\" does not exist.")
  endif (EXISTS "$ENV{DESTDIR}${file}")
endforeach(file)
