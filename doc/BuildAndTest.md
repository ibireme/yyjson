Building and testing
==============

There are several ways to integrate yyjson into your project: source code, package manager, and CMake.


# Source code
yyjson aims to provide a cross-platform JSON library, so it is written in ANSI C (actually C99, but compatible with strict C89). You can copy `yyjson.h` and `yyjson.c` to your project and start using it without any configuration.

yyjson has been tested with the following compilers: `gcc`, `clang`, `msvc`, `icc`, `tcc`. If you get a compile error, please [report a bug](https://github.com/ibireme/yyjson/issues/new?template=bug_report.md).

yyjson has all features enabled by default, but you can trim out some of them by adding compile-time options. For example, disable JSON writer to reduce the binary size when you don't need serialization, or disable comments support to improve parsing performance. See `Compile-time Options` for details.


# Package manager

You can use some popular package managers to download and install yyjson, such as `vcpkg`, `conan`, and `xmake`. The yyjson package in these package managers is kept up to date by community contributors. If the version is out of date, please create an issue or pull request on their repository.

## Use vcpkg

You can build and install yyjson using [vcpkg](https://github.com/Microsoft/vcpkg/) dependency manager:

```shell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # ./bootstrap-vcpkg.bat for Powershell
./vcpkg integrate install
./vcpkg install yyjson
```

If the version is out of date, please [create an issue or pull request](https://github.com/Microsoft/vcpkg) on the vcpkg repository.

# CMake

## Use CMake to build a library

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
- `-DYYJSON_BUILD_DOC=ON` Build documentation with doxygen.
- `-DYYJSON_ENABLE_COVERAGE=ON` Enable code coverage for tests.
- `-DYYJSON_ENABLE_VALGRIND=ON` Enable valgrind memory checker for tests.
- `-DYYJSON_ENABLE_SANITIZE=ON` Enable sanitizer for tests.
- `-DYYJSON_ENABLE_FASTMATH=ON` Enable fast-math for tests.
- `-DYYJSON_FORCE_32_BIT=ON` Force 32-bit for tests (gcc/clang/icc).

- `-DYYJSON_DISABLE_READER=ON` Disable JSON reader if you don't need it.
- `-DYYJSON_DISABLE_WRITER=ON` Disable JSON writer if you don't need it.
- `-DYYJSON_DISABLE_FAST_FP_CONV=ON` Disable builtin fast floating-pointer conversion.
- `-DYYJSON_DISABLE_NON_STANDARD=ON` Disable non-standard JSON support at compile-time.


## Use CMake as a dependency

You can download and unzip yyjson to your project folder and link it in your `CMakeLists.txt` file:
```cmake
# Add some options (optional)
set(YYJSON_DISABLE_NON_STANDARD ON CACHE INTERNAL "")

# Add the `yyjson` subdirectory
add_subdirectory(vendor/yyjson)

# Link yyjson to your target
target_link_libraries(your_target PRIVATE yyjson)
```

If your CMake version is higher than 3.14, you can use the following method to let CMake automatically download it:
```cmake
include(FetchContent)

# Let CMake download yyjson
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG master # master, or version number, e.g. 0.6.0
)
FetchContent_MakeAvailable(yyjson)

# Link yyjson to your target
target_link_libraries(your_target PRIVATE yyjson)
```


## Use CMake to generate project
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

## Use CMake to generate documentation

yyjson uses [doxygen](https://www.doxygen.nl/) to generate the documentation (you must have `doxygen` installed):
```shell
cmake .. -DYYJSON_BUILD_DOC=ON
cmake --build .
```
After executing this script, doxygen will output the generated html files to `build/doxygen/html`. You can also read the pre-generated document online: https://ibireme.github.io/yyjson/doc/doxygen/html/


## Testing With CMake and CTest

Build and run all tests:
```shell
cmake .. -DYYJSON_BUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

Build and run tests with [valgrind](https://valgrind.org/) memory checker (you must have `valgrind` installed):
```shell
cmake .. -DYYJSON_BUILD_TESTS=ON -DYYJSON_ENABLE_VALGRIND=ON
cmake --build .
ctest --output-on-failure
```

Build and run tests with sanitizer (compiler should be `gcc` or `clang`):
```shell
cmake .. -DYYJSON_BUILD_TESTS=ON -DYYJSON_ENABLE_SANITIZE=ON
cmake --build .
ctest --output-on-failure
```

Build and run code coverage (compiler should be `gcc`):
```shell
cmake .. -DCMAKE_BUILD_TYPE=Debug -DYYJSON_BUILD_TESTS=ON -DYYJSON_ENABLE_COVERAGE=ON
cmake --build . --config Debug
ctest --output-on-failure

lcov -c -d ./CMakeFiles/yyjson.dir/src -o cov.info
genhtml cov.info -o ./cov_report
```

Build and run fuzz test with [LibFuzzer](https://llvm.org/docs/LibFuzzer.html) (compiler should be `LLVM Clang`, while `Apple Clang` or `gcc` are not supported):
```shell
cmake .. -DYYJSON_BUILD_FUZZER=ON -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build .
./fuzzer -dict=fuzzer.dict ./corpus
```


# Compile-time Options
yyjson supports some compile-time options, you can define these macros as `1` to disable some features at compile-time.

● **YYJSON_DISABLE_READER**<br/>
Define it as 1 to disable the JSON reader.<br/>
This will disable these functions at compile-time:
```c
yyjson_read_opts()
yyjson_read_file()
yyjson_read()
 ```
This will reduce the binary size by about 60%.<br/>
It is recommended when you don't need to parse JSON.

● **YYJSON_DISABLE_WRITER**<br/>
Define as 1 to disable JSON writer.<br/>
This will disable these functions at compile-time:
```c
yyjson_write()
yyjson_write_file()
yyjson_write_opts()
yyjson_val_write()
yyjson_val_write_file()
yyjson_val_write_opts()
yyjson_mut_write()
yyjson_mut_write_file()
yyjson_mut_write_opts()
yyjson_mut_val_write()
yyjson_mut_val_write_file()
yyjson_mut_val_write_opts()
```
This will reduce the binary size by about 30%.<br/>
It is recommended when you don't need to serialize JSON.

● **YYJSON_DISABLE_FAST_FP_CONV**<br/>
Define as 1 to disable the fast floating-point number conversion in yyjson,
 and use libc's `strtod/snprintf` instead.<br/>
This will reduce binary size by about 30%, but significantly slow down the floating-point read/write speed.<br/>
It is recommended when you don't need to deal with JSON that contains a lot of floating point numbers.

● **YYJSON_DISABLE_NON_STANDARD**<br/>
Define as 1 to disable non-standard JSON support at compile-time:

- Reading and writing inf/nan literal, such as `NaN`, `-Infinity`.
- Single line and multiple line comments.
- Single trailing comma at the end of an object or array.
- Invalid unicode in string value.

This will also invalidate these run-time options:
```c
YYJSON_READ_ALLOW_INF_AND_NAN
YYJSON_READ_ALLOW_COMMENTS
YYJSON_READ_ALLOW_TRAILING_COMMAS
YYJSON_READ_ALLOW_INVALID_UNICODE
YYJSON_WRITE_ALLOW_INF_AND_NAN
YYJSON_WRITE_ALLOW_INVALID_UNICODE
```

This will reduce binary size by about 10%, and increase performance slightly.<br/>
It is recommended when you don't need to deal with non-standard JSON.

● **YYJSON_EXPORTS**<br/>
Define it as 1 to export symbols when building the library as Windows DLL.

● **YYJSON_IMPORTS**<br/>
Define it as 1 to import symbols when using the library as Windows DLL.
