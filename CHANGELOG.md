# Changelog
All notable changes to this project will be documented in this file.


## 0.3.0 (2021-05-25)
#### Added
- Add `JSON Pointer` support.
- Add CMake install target.

#### Changed
- Improve performance for some arch which doesn't support unaligned memory access.

#### Fixed
- Fix some compiler warning for GCC and Clang.
- Fix MSVC build error on UWP (uninitialized local variable).
- Fix stream file reading error on some platform.


## 0.2.0 (2020-12-12)
#### Added
- Add swift package manager support.

#### Changed
- Improve JSON reader performance for gcc.
- Improve double number reader performance with a fast path.
- Rewrite double number writer with Schubfach algorithm: #4.
- Strict UTF-8 validation for JSON reader.

#### Removed
- Remove `YYJSON_READ_FASTFP` flag.

#### Fixed
- Fix a compile error for old version gcc on linux: #7.


## 0.1.0 (2020-10-26)
#### Added
- Initial release.
- Add the basic JSON reader and writer (RFC 8259).
- Add CMake support.
- Add GitHub CI workflow.
- Add test code and test data.
- Add `sanitizer` and `valgrind` memory checker.
- Add `API` and `DataStructure` documentation.
