cmake_minimum_required(VERSION 3.15)
project(mabi-pack2 VERSION 1.3.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(mabi_pack2_lib STATIC
    src/snow2_fast.c
    src/miniz.c
    src/encryption.cpp
    src/common.cpp
    src/extract.cpp
    src/list.cpp
    src/pack.cpp
)

add_executable(mabi-pack2 src/main.cpp)
target_link_libraries(mabi-pack2 PRIVATE mabi_pack2_lib)

add_executable(mabi-pack2-tests src/tests.cpp)
target_link_libraries(mabi-pack2-tests PRIVATE mabi_pack2_lib)

enable_testing()
add_test(NAME mabi-pack2-tests COMMAND mabi-pack2-tests)
