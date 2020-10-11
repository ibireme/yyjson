cmake_minimum_required (VERSION 3.5)
project (yyjson VERSION 0.1.0)


# Compiler Options
# See yyjson.h for more explain.
option(YYJSON_DISABLE_READER "Disable JSON reader" OFF)
option(YYJSON_DISABLE_WRITER "disable JSON writer" OFF)
option(YYJSON_DISABLE_FP_READER "Disable custom floating-point number reader" OFF)
option(YYJSON_DISABLE_FP_WRITER "Disable custom floating-point number writer" OFF)
option(YYJSON_DISABLE_COMMENT_READER "Disable non-standard comment reader" OFF)
option(YYJSON_DISABLE_INF_AND_NAN_READER "Disable non-standard inf and nan reader" OFF)
option(YYJSON_BUILD_TEST "Build test" OFF)
option(YYJSON_ENABLE_TEST_COVERAGE "Enable code coverage for text" OFF)
option(YYJSON_BUILD_MISC "Build misc" OFF)


# Build Type
if (XCODE OR MSVC)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to: Release")
    set(CMAKE_BUILD_TYPE Release)
endif()


# Library
add_library(yyjson src/yyjson.h src/yyjson.c)
target_include_directories(yyjson PUBLIC src)


# Compiler Flags
if(YYJSON_DISABLE_READER)
    add_definitions(-DYYJSON_DISABLE_READER=1)
endif()
if(YYJSON_DISABLE_WRITER)
    add_definitions(-DYYJSON_DISABLE_WRITER=1)
endif()
if(YYJSON_DISABLE_FP_READER)
    add_definitions(-DYYJSON_DISABLE_FP_READER=1)
endif()
if(YYJSON_DISABLE_FP_WRITER)
    add_definitions(-DYYJSON_DISABLE_FP_WRITER=1)
endif()
if(YYJSON_DISABLE_COMMENT_READER)
    add_definitions(-DYYJSON_DISABLE_COMMENT_READER=1)
endif()
if(YYJSON_DISABLE_INF_AND_NAN_READER)
    add_definitions(-DYYJSON_DISABLE_INF_AND_NAN_READER=1)
endif()


# Project Config
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(XcodeProperty)

if(XCODE)
    set_default_xcode_property(yyjson)
    set_xcode_deployment_version(yyjson "10.11" "9.0" "9.0" "2.0")

    set_xcode_property(yyjson GCC_C_LANGUAGE_STANDARD "c89")
    set_xcode_property(yyjson CLANG_CXX_LANGUAGE_STANDARD "c++98")
    
    set_xcode_property(yyjson OTHER_CFLAGS[variant=Debug] "-Wall -Wextra -Werror -pedantic -pedantic-errors")
    set_xcode_property(yyjson OTHER_CFLAGS[variant=Release] "-Wall -Wextra -Werror -pedantic -pedantic-errors")
elseif(MSVC)
    target_compile_options(yyjson PRIVATE $<$<C_COMPILER_ID:MSVC>:/permissive- /utf-8>)
    target_compile_options(yyjson PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/permissive- /utf-8>)
endif()

if(BUILD_SHARED_LIBS)
    if(WIN32)
        target_compile_definitions(yyjson PRIVATE YYJSON_EXPORTS)
    endif()
endif()

if(YYJSON_ENABLE_TEST_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(yyjson INTERFACE -O0 -g -ftest-coverage -fprofile-arcs)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        target_link_options(yyjson INTERFACE -lgcov --coverage)
    else()
        target_link_libraries(yyjson INTERFACE -lgcov --coverage)
    endif()
endif()


# Testing
if(YYJSON_BUILD_TEST)
    enable_testing()

    if(XCODE)
        # Config XCTest
        find_package(XCTest REQUIRED)
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
        set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
        set(YYJSON_TEST_DATA ${CMAKE_CURRENT_SOURCE_DIR}/test/data)

        # Add test cases to XCTest
        file(GLOB YYJSON_TEST_SOURCE
            "test/test_*.c"
            "test/test_*.cpp"
        )
        set(YYJSON_TEST_LINES "")
        foreach(SRC_FILE ${YYJSON_TEST_SOURCE})
            string(REGEX REPLACE "(^.*/|\\.[^.]*$)" "" SRC_NAME ${SRC_FILE})
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}")
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}- (void)${SRC_NAME} {\n")
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}    extern void ${SRC_NAME}(void);\n")
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}    ${SRC_NAME}();\n")
            set(YYJSON_TEST_LINES "${YYJSON_TEST_LINES}}\n\n")
        endforeach()
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/test/xctest/yy_xctest.m.in"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/xctest/yy_xctest.m"
            @ONLY
        )
        unset(YYJSON_TEST_LINES)

        # Add source files and search path to XCTest
        file(GLOB YYJSON_TEST_SOURCE 
            "test/test_*.c"
            "test/test_*.cpp"
            "test/util/*.h"
            "test/util/*.c"
            "test/xctest/*"
        )
        xctest_add_bundle(yyjson_tests yyjson
            ${YYJSON_TEST_SOURCE}
            ${YYJSON_TEST_DATA}
        )
        set_target_properties(yyjson_tests PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/test/xctest/Info.plist
            RESOURCE ${YYJSON_TEST_DATA}
        )
        target_include_directories(yyjson_tests PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/test/util
            ${CMAKE_CURRENT_SOURCE_DIR}/test/xctest
        )
        xctest_add_test(XCTest.yyjson yyjson_tests)

        set_default_xcode_property(yyjson_tests)
        set_xcode_deployment_version(yyjson_tests "10.11" "9.0" "9.0" "2.0")

    else()
        # Copy test data
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/test/data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
        add_definitions(-DYYJSON_TEST_DATA_PATH="${CMAKE_CURRENT_BINARY_DIR}")

        # Add dependency
        add_library(yyjson_test_utils
            test/util/yy_test_utils.c
            test/util/david_gay_dtoa.c
        )
        target_include_directories(yyjson_test_utils PUBLIC test/util)
        target_link_libraries(yyjson_test_utils PRIVATE yyjson)   
        
        # Add test cases
        file(GLOB YYJSON_TEST_SOURCE
            "test/test_*.c"
            "test/test_*.cpp"
        )
        foreach(SRC_FILE ${YYJSON_TEST_SOURCE})
            string(REGEX REPLACE "(^.*/|\\.[^.]*$)" "" SRC_NAME ${SRC_FILE})
            add_executable(${SRC_NAME} ${SRC_FILE})
            target_link_libraries(${SRC_NAME} PRIVATE yyjson yyjson_test_utils)
            add_test(${SRC_NAME} ${SRC_NAME})
            set_tests_properties(${SRC_NAME} PROPERTIES TIMEOUT 60)
            message("add test ${SRC_NAME}")
        endforeach()
    endif()

    # Test strict C89/C++98 compatibility
    add_executable(test_compile_c89 test/compile_ansi.c)
    target_include_directories(test_compile_c89 PRIVATE src)
    add_executable(test_compile_cxx98 test/compile_ansi.cpp)
    target_include_directories(test_compile_cxx98 PRIVATE src)

    if(MSVC)
        # set warning level 4, treat warnings as errors
        set(YYJSON_STRICT_FLAGS /W4 /WX)
        target_compile_options(test_compile_c89 PRIVATE $<$<COMPILE_LANGUAGE:C>:${YYJSON_STRICT_FLAGS}>)
        target_compile_options(test_compile_cxx98 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${YYJSON_STRICT_FLAGS}>)

    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

        include(CheckCCompilerFlag)
        check_c_compiler_flag("-std=c89" COMPILER_SUPPORTS_C89)
        if(COMPILER_SUPPORTS_C89)
            # check ANSI standard strictly, treat warnings as errors
            set(YYJSON_STRICT_C_FLAGS -std=c89 -Wall -Wextra -Werror -pedantic -pedantic-errors -Wmissing-prototypes -Wstrict-prototypes)
            target_compile_options(test_compile_c89 PRIVATE $<$<COMPILE_LANGUAGE:C>:${YYJSON_STRICT_C_FLAGS}>)
        endif()

        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-std=c++98" COMPILER_SUPPORTS_CXX98)
        if(COMPILER_SUPPORTS_CXX98)
            # check ANSI standard strictly, treat warnings as errors
            set(YYJSON_STRICT_CXX_FLAGS -std=c++98 -Wall -Wextra -Werror -pedantic -pedantic-errors)
            target_compile_options(test_compile_cxx98 PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${YYJSON_STRICT_CXX_FLAGS}>)
        endif()
    endif()

endif()


# Miscellaneous
if(YYJSON_BUILD_MISC)
    # jsoninfo
    add_executable(jsoninfo "misc/jsoninfo.c")
    target_link_libraries(jsoninfo PRIVATE yyjson)
    if(XCODE)
        set_default_xcode_property(jsoninfo)
    endif()

    # make tables
    find_package(GMP REQUIRED)
    find_package(MPFR REQUIRED)
    add_executable(make_tables "misc/make_tables.c")
    target_include_directories(make_tables PRIVATE 
        ${GMP_INCLUDE_DIR}
        ${MPFR_INCLUDES}
    )
    target_link_libraries(make_tables PRIVATE
        ${GMP_LIBRARIES}
        ${MPFR_LIBRARIES}
    )
    if(XCODE)
        set_default_xcode_property(make_tables)
    endif()
endif()
