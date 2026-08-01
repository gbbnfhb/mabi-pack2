# CMake generated Testfile for 
# Source directory: C:/mabi-pack2
# Build directory: C:/mabi-pack2/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(mabi-pack2-tests "C:/mabi-pack2/build/Debug/mabi-pack2-tests.exe")
  set_tests_properties(mabi-pack2-tests PROPERTIES  _BACKTRACE_TRIPLES "C:/mabi-pack2/CMakeLists.txt;26;add_test;C:/mabi-pack2/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(mabi-pack2-tests "C:/mabi-pack2/build/Release/mabi-pack2-tests.exe")
  set_tests_properties(mabi-pack2-tests PROPERTIES  _BACKTRACE_TRIPLES "C:/mabi-pack2/CMakeLists.txt;26;add_test;C:/mabi-pack2/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(mabi-pack2-tests "C:/mabi-pack2/build/MinSizeRel/mabi-pack2-tests.exe")
  set_tests_properties(mabi-pack2-tests PROPERTIES  _BACKTRACE_TRIPLES "C:/mabi-pack2/CMakeLists.txt;26;add_test;C:/mabi-pack2/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(mabi-pack2-tests "C:/mabi-pack2/build/RelWithDebInfo/mabi-pack2-tests.exe")
  set_tests_properties(mabi-pack2-tests PROPERTIES  _BACKTRACE_TRIPLES "C:/mabi-pack2/CMakeLists.txt;26;add_test;C:/mabi-pack2/CMakeLists.txt;0;")
else()
  add_test(mabi-pack2-tests NOT_AVAILABLE)
endif()
