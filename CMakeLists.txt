project(luma_usd)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(USD REQUIRED)

set (CMAKE_CXX_STANDARD 11)

add_compile_options(-Wall -Wl,--no-undefined -DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")
option(ENABLE_RESOLVER_BUILD "Enabling building the s3 resolver." On)

cmake_minimum_required(VERSION 3.2)

if (ENABLE_RESOLVER_BUILD)
    add_subdirectory(S3Resolver)
endif ()

install(FILES plugInfo.json
        DESTINATION .)
