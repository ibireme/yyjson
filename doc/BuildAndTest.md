# Table of Contents

* [Import Manually](#import-manually)
* [Use CMake to build library](#use-cmake-to-build-library)
* [Use CMake as a dependency](#use-cmake-as-a-dependency)
* [Use CMake to generate project](#use-cmake-to-generate-project)
* [Testing With CMake and CTest](#testing-with-cmake-and-ctest)
* [Compile Flags](#compile-flags)


------
# Import Manually
`yyjson` aims to provide a cross-platform JSON library, so it was written in ANSI C (actually C99, but compatible with strict C89). You can copy `yyjson.h` and `yyjson.c` to your project and start using it without any configuration.

If you get a compile error, please [report a bug](https://github.com/ibireme/yyjson/issues/new?template=bug_report.md).

# Use CMake to build library
Clone the repository and create build directory:
```shell
git clone https://github.com/ibireme/yyjson.git
mkdir build
cd build
```
Build static library:
```shell
cmake .. 
cmake --build .
```

Build shared library:
```shell
cmake .. -DBUILD_SHARED_LIBS=ON
cmake --build .
```

Supported CMake options:

- `-DYYJSON_BUILD_TESTS=ON` Build all tests.
- `-DYYJSON_BUILD_FUZZER=ON` Build fuzzer with LibFuzzing.
- `-DYYJSON_BUILD_MISC=ON` Build misc.
- `-DYYJSON_ENABLE_COVERAGE=ON` Enable code coverage for tests.
- `-DYYJSON_ENABLE_VALGRIND=ON` Enable valgrind memory checker for tests.
- `-DYYJSON_ENABLE_SANITIZE=ON` Enable sanitizer for tests.
- `-DYYJSON_ENABLE_FASTMATH=ON` Enable fast-math for tests.
- `-DYYJSON_DISABLE_READER=ON` Disable JSON reader if you don't need it.
- `-DYYJSON_DISABLE_WRITER=ON` Disable JSON writer if you don't need it.
- `-DYYJSON_DISABLE_FAST_FP_CONV=ON` Disable fast floating-pointer conversion.
- `-DYYJSON_DISABLE_NON_STANDARD=ON` Disable non-standard JSON support at compile time.

See [compile flags](#compile-flags) for details.

# Use CMake as a dependency

You may add the `yyjson` subdirectory to your CMakeFile.txt, and link it to your target:
```cmake
add_subdirectory(vendor/yyjson)
target_link_libraries(your_target yyjson)
```

You may also add some compile flag for yyjson library:
```cmake
set(YYJSON_DISABLE_NON_STANDARD ON CACHE INTERNAL "")
add_subdirectory(vendor/yyjson)
target_link_libraries(your_target yyjson)
```

# Use CMake to generate project
If you want to build or debug `yyjson` with another compiler or IDE, try these commands:
```shell
# Clang for Linux/Unix:
cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++

# Intel ICC for Linux/Unix:
cmake .. -DCMAKE_C_COMPILER=icc -DCMAKE_CXX_COMPILER=icpc

# Other version of GCC:
cmake .. -DCMAKE_C_COMPILER=/usr/local/gcc-8.2/bin/gcc -DCMAKE_CXX_COMPILER=/usr/local/gcc-8.2/bin/g++

# Microsoft Visual Studio for Windows:
cmake .. -G "Visual Studio 16 2019" -A x64
cmake .. -G "Visual Studio 16 2019" -A Win32
cmake .. -G "Visual Studio 15 2017 Win64"

# Xcode for macOS:
cmake .. -G Xcode

# Xcode for iOS:
cmake .. -G Xcode -DCMAKE_SYSTEM_NAME=iOS

# Xcode with XCTest
cmake .. -G Xcode -DYYJSON_BUILD_TESTS=ON
```


# Testing With CMake and CTest

Build and run all tests.
```shell
cmake .. -DYYJSON_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

Build and run tests with valgrind memory checker, you must have `valgrind` installed.
```shell
cmake .. -DYYJSON_BUILD_TESTS=ON -DYYJSON_ENABLE_VALGRIND=ON
cmake --build .
ctest --output-on-failure
```

Build and run tests with sanitizer, compiler should be `gcc` or `clang`.
```shell
cmake .. -DYYJSON_BUILD_TESTS=ON -DYYJSON_ENABLE_SANITIZE=ON
cmake --build .
ctest --output-on-failure
```

Build and run code coverage, compiler should be `gcc`.
```shell
cmake .. -DCMAKE_BUILD_TYPE=Debug -DYYJSON_BUILD_TESTS=ON -DYYJSON_ENABLE_COVERAGE=ON
cmake --build . --config Debug
ctest --output-on-failure

lcov -c -d ./CMakeFiles/yyjson.dir/src -o cov.info
genhtml cov.info -o ./cov_report
```

Build and run fuzz test with LibFuzzer, compiler should be `LLVM Clang`, `Apple Clang` or `gcc` is not supported.
```shell
cmake .. -DYYJSON_BUILD_FUZZER=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build .
./fuzzer -dict=fuzzer.dict ./corpus
```


# Compile Flags
`yyjson` supports some compile flags, you can define these macros as `1` to disable some features at compile time.

●**YYJSON_DISABLE_READER**<br/>
Define it as 1 to disable the JSON reader.<br/>
This flag can reduce the binary size if you don't need to read JSON.<br/>
These functions will be disabled by this flag:

```
yyjson_read_opts()
yyjson_read_file()
yyjson_read()
```

●**YYJSON_DISABLE_WRITER**<br/>
Define it as 1 to disable the JSON writer.<br/>
This flag can reduce the binary size if you don't need to write JSON.<br/>
These functions will be disabled by this flag:

```
yyjson_write_opts()
yyjson_write_file()
yyjson_write()
yyjson_mut_write_opts()
yyjson_mut_write_file()
yyjson_mut_write()
```

●**YYJSON_DISABLE_FAST_FP_CONV**<br/>
Define as 1 to disable the fast floating-point number conversion in yyjson,<br/>
and use libc's `strtod/snprintf` instead. This may reduce binary size,<br/>
but slow down floating-point reading and writing speed.

●**YYJSON_DISABLE_NON_STANDARD**<br/>
Define as 1 to disable non-standard JSON support at compile time:<br/>
    - Reading and writing inf/nan literal, such as 'NaN', '-Infinity'.<br/>
    - Single line and multiple line comments.<br/>
    - Single trailing comma at the end of an object or array.<br/>
This may also invalidate these options:<br/>
    - YYJSON_READ_ALLOW_INF_AND_NAN<br/>
    - YYJSON_READ_ALLOW_COMMENTS<br/>
    - YYJSON_READ_ALLOW_TRAILING_COMMAS<br/>
    - YYJSON_WRITE_ALLOW_INF_AND_NAN<br/>
This may reduce binary size, and increase performance slightly.

●**YYJSON_EXPORTS**<br/>
Define it as 1 to export symbols when building the library as Windows DLL.

●**YYJSON_IMPORTS**<br/>
Define it as 1 to import symbols when using the library as Windows DLL.
