# Changelog
All notable changes to this project will be documented in this file.


## 0.4.0 (2021-12-12)
#### Added
- Add `YYJSON_WRITE_INF_AND_NAN_AS_NULL` flag for JSON writer.
- Add `merge_path()` function for JSON Merge-Path API (RFC 7386).
- Add `obj_replace()` and `obj_insert()` function for object modification.
- Add `obj_iter_get()` function for faster object query.
- Add `yyjson_version()` function.

#### Changed
- Replace `YYJSON_DISABLE_COMMENT_READER` and `YYJSON_DISABLE_INF_AND_NAN_READER` with `YYJSON_DISABLE_NON_STANDARD` compiler flag.
- Replace `YYJSON_DISABLE_FP_READER` and `YYJSON_DISABLE_FP_WRITER` with `YYJSON_DISABLE_FAST_FP_CONV` compiler flag.

#### Fixed
- Fix compiler warning with `-Wconversion`
- Fix compiler error for GCC 4.4 (#53) and MSVC 6.0 (#55)


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
- Remove `YYJSON_READ_FASTFP` compiler flag.

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
