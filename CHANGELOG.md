# Changelog
All notable changes to this project will be documented in this file.


## 0.6.0 (2022-12-12)
#### Added
- Add functions to modify the content of a JSON value, for example `yyjson_set_int(yyjson_val *val, int num)`.
- Add functions to copy from mutable doc to immutable doc.
- Add functions to support renaming an object's key.
- Add the `yyjson_read_number()` function to parse numeric strings.
- Add a placeholder allocator if `yyjson_alc_pool_init()` fails.

#### Fixed
- Fix quite NaN on MIPS and HPPA arch.
- Fixed compile error before `GCC 4.5` which doesn't support empty optional extended asm label.
- When built-in floating point conversion is disabled, sprintf() output for floating point numbers is missing a decimal point, for example 123 should be 123.0.


## 0.5.1 (2022-06-17)
#### Fixed
- Fix run-time error when compiling as cpp and 32-bit (g++-5 -m32 -fPIC) #85
- Fix incurrect output number format, remove unnecessary digits (e.g. 2.0e34 -> 2e34).


## 0.5.0 (2022-05-25)
#### Added
- Add LibFuzzer support.
- Add Doxygen support.
- Add functions to support serializing a single JSON value.
- Add `yyjson_mut_doc_mut_copy()`, `yyjson_mut_val_mut_copy()`, `yyjson_mut_merge_patch()` function for mutable input.
- Add `yyjson_equals()` and `yyjson_mut_equals()` function to compare two values.
- Add `yyjson_mut_obj_remove_key()` and `yyjson_mut_obj_remove_keyn()` to make it easier to remove a key.
- Add `YYJSON_READ_NUMBER_AS_RAW` option and `RAW` type support.
- Add `YYJSON_READ_ALLOW_INVALID_UNICODE` and `YYJSON_WRITE_ALLOW_INVALID_UNICODE` options to allow invalid unicode.

#### Changed
- Change `yyjson_mut_obj_remove()` return type from `bool` to `yyjson_mut_val *`.
- Rewrite string serialization function, validate unicode encoding by default.
- Rewrite the JSON Pointer implementation, remove internal malloc() calls.

#### Fixed
- Make the code work correctly with setlocale() function and fast-math flag: #54
- Fix negative infinity literals read error: #64
- Fix non null-terminated string write error.
- Fix incorrect behavior of `YYJSON_DISABLE_NON_STANDARD` flag: #80


## 0.4.0 (2021-12-12)
#### Added
- Add `YYJSON_WRITE_INF_AND_NAN_AS_NULL` flag for JSON writer.
- Add `yyjson_merge_patch()` function for JSON Merge-Path API (RFC 7386).
- Add `yyjson_mut_obj_replace()` and `yyjson_mut_obj_insert()` function for object modification.
- Add `yyjson_obj_iter_get()` and `yyjson_mut_obj_iter_get()` function for faster object search.
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
