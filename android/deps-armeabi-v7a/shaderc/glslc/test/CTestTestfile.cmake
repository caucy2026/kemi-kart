# CMake generated Testfile for 
# Source directory: /Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/shaderc/glslc/test
# Build directory: /Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/shaderc/glslc/test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(shaderc_expect_unittests "/usr/bin/python3" "-m" "unittest" "expect_unittest.py")
set_tests_properties(shaderc_expect_unittests PROPERTIES  WORKING_DIRECTORY "/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/shaderc/glslc/test" _BACKTRACE_TRIPLES "/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/shaderc/glslc/test/CMakeLists.txt;15;add_test;/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/shaderc/glslc/test/CMakeLists.txt;0;")
add_test(shaderc_glslc_test_framework_unittests "/usr/bin/python3" "-m" "unittest" "glslc_test_framework_unittest.py")
set_tests_properties(shaderc_glslc_test_framework_unittests PROPERTIES  WORKING_DIRECTORY "/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/shaderc/glslc/test" _BACKTRACE_TRIPLES "/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/shaderc/glslc/test/CMakeLists.txt;18;add_test;/Users/newlink/kemi/stk-code/android/deps-armeabi-v7a/shaderc/glslc/test/CMakeLists.txt;0;")
