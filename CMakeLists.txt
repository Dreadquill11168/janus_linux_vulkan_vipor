cmake_minimum_required(VERSION 3.16)
project(janusv_vipor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_executable(janusv
    src/main.cpp
    src/vk_compute.cpp
    src/sha_utils.cpp
    src/sha256.cpp
    src/stratum.cpp
    src/verus_wrapper.cpp
)

target_include_directories(janusv PRIVATE ${Vulkan_INCLUDE_DIRS} src)
target_link_libraries(janusv PRIVATE ${Vulkan_LIBRARIES})

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(
  verushash_node
  GIT_REPOSITORY https://github.com/VerusCoin/verushash-node.git
  GIT_TAG master
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(verushash_node)

if (EXISTS ${verushash_node_SOURCE_DIR}/verushash.cc)
    add_library(veruscore STATIC
        ${verushash_node_SOURCE_DIR}/verushash.cc
        ${verushash_node_SOURCE_DIR}/crypto/haraka_portable.c
        ${verushash_node_SOURCE_DIR}/crypto/sse2neon.h
    )
    target_include_directories(veruscore PUBLIC ${verushash_node_SOURCE_DIR})
    target_link_libraries(janusv PRIVATE veruscore)
    target_compile_definitions(janusv PRIVATE HAVE_VERUS=1)
else()
    message(WARNING "VerusHash sources not found; building with stub. Shares will NOT be valid.")
endif()

file(COPY shaders/sha256t.comp DESTINATION ${CMAKE_BINARY_DIR}/shaders)
