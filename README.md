# YYJSON

[![Build](https://img.shields.io/github/workflow/status/ibireme/yyjson/cmake?style=flat-square)](https://github.com/ibireme/yyjson/actions)
[![Codecov](https://img.shields.io/codecov/c/gh/ibireme/yyjson?style=flat-square)](https://codecov.io/gh/ibireme/yyjson)
[![License](https://img.shields.io/github/license/ibireme/yyjson?color=blue&style=flat-square)](https://github.com/ibireme/yyjson/blob/master/LICENSE)

A high performance JSON library written in ANSI C.

# Features
- **Fast**: can read or write gigabytes per second JSON data on modern CPU.
- **Portable**: compliant with ANSI C (C89).
- **Standard**: compliant with [RFC 8259](https://tools.ietf.org/html/rfc8259) and [ECMA-404](https://www.ecma-international.org/publications/standards/Ecma-404.htm) standard.
- **Safe**: complete JSON form, number format and UTF-8 validation.
- **Accuracy**: can process `int64`, `uint64` and `double` number accurately.
- **No Limit**: support large data size, unlimited JSON level, `\u0000` string.
- **Extendable**: support comments, trailing commas, nan/inf, custom memory allocator.
- **Developer Friendly**: only one `h` and one `c` file, easy to use API.

# Performance
#### Intel Core i5-8259U (3.8GHz) with Clang 10
![Reader Intel](doc/images/perf_reader_intel.svg)
![Writer Intel](doc/images/perf_writer_intel.svg)

|twitter.json|read (GB/s)|write (GB/s)|
|---|---|---|
|yyjson(insitu)|2.36|1.90|
|yyjson|2.17|1.70|
|simdjson|2.21|0.84|
|sajson|1.04|   |
|rapidjson(insitu)|0.47|   |
|rapidjson(utf8)|0.26|0.74|
|cjson|0.32|0.19|
|jansson|0.07|0.19|

#### Apple A12 (2.5GHz) with Clang 12
![Reader Apple](doc/images/perf_reader_apple.svg)
![Writer Apple](doc/images/perf_writer_apple.svg)

|twitter.json|read (GB/s)|write (GB/s)|
|---|---|---|
|yyjson(insitu)|2.57|1.94|
|yyjson|2.39|1.59|
|simdjson|1.78|0.75|
|sajson|1.41|   |
|rapidjson(insitu)|0.51|   |
|rapidjson(utf8)|0.39|0.51|
|cjson|0.28|0.27|
|jansson|0.06|0.20|


Benchmark project: [yyjson_benchmark](https://github.com/ibireme/yyjson_benchmark)

More benchmark reports with interactive charts:
* [Apple A12 clang](https://ibireme.github.io/yyjson_benchmark/reports/Apple_A12_clang.html)
* [Intel NUC 8i5 clang (Ubuntu 20.04)](https://ibireme.github.io/yyjson_benchmark/reports/Intel_NUC_8i5_clang.html)
* [Intel NUC 8i5 gcc (Ubuntu 20.04)](https://ibireme.github.io/yyjson_benchmark/reports/Intel_NUC_8i5_gcc.html) (slower, need optimize)
* [Intel NUC 8i5 msvc (Windows 10 2004)](https://ibireme.github.io/yyjson_benchmark/reports/Intel_NUC_8i5_msvc.html)

### For better performance, yyjson prefers:
* A modern processor with:
    * high instruction level parallelism
    * excellent branch predictor
    * low penalty for misaligned memory access
* A modern compiler with good optimization.

# Building

### Manually
Just copy `yyjson.h` and `yyjson.c` to your project and start using it.
Since yyjson is ANSI C compatible, no other configuration is needed typically.

### CMake
Clone repository and create build directory:
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

Build static library and run tests:
```shell
cmake .. -DYYJSON_BUILD_TESTS=ON
cmake --build .
ctest
```

Build shared library:
```shell
cmake ..-DBUILD_SHARED_LIBS=ON
cmake --build .
```

Supported CMake options:

- `-DYYJSON_BUILD_TESTS=ON` Build all tests.
- `-DYYJSON_ENABLE_COVERAGE=ON` Enable code coverage for tests.
- `-DYYJSON_ENABLE_VALGRIND=ON` Enable valgrind memory checker for tests.
- `-DYYJSON_ENABLE_SANITIZE=ON` Enable santizier for tests.
- `-DYYJSON_DISABLE_READER=ON` Disable JSON reader if you don't need it.
- `-DYYJSON_DISABLE_WRITER=ON` Disable JSON writer if you don't need it.
- `-DYYJSON_DISABLE_FP_READER=ON` Disable custom double number reader to reduce binary size.
- `-DYYJSON_DISABLE_FP_WRITER=ON` Disable custom double number writer to reduce binary size.
- `-DYYJSON_DISABLE_COMMENT_READER=ON` Disable non-standard comment support at compile time.
- `-DYYJSON_DISABLE_INF_AND_NAN_READER=ON` Disable non-standard nan/inf support at compile time.

# Usage Example

### Read JSON string
```c
const char *json = "{\"name\":\"Mash\",\"star\":4,\"hits\":[2,2,1,3]}";

yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
yyjson_val *root = yyjson_doc_get_root(doc);

yyjson_val *name = yyjson_obj_get(root, "name");
printf("name: %s\n", yyjson_get_str(name));

yyjson_val *star = yyjson_obj_get(root, "star");
printf("star: %d\n", (int)yyjson_get_int(star));

yyjson_val *hits = yyjson_obj_get(root, "hits");
size_t idx, max;
yyjson_val *hit;
yyjson_arr_foreach(hits, idx, max, hit) {
    printf("hit%d: %d\n", (int)idx, (int)yyjson_get_int(hit));
}

yyjson_doc_free(doc);
```

### Write JSON string
```c
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
yyjson_mut_val *root = yyjson_mut_obj(doc);
yyjson_mut_doc_set_root(doc, root);

yyjson_mut_obj_add_str(doc, root, "name", "Mash");
yyjson_mut_obj_add_int(doc, root, "star", 4);

int hits_arr[] = {2, 2, 1, 3};
yyjson_mut_val *hits = yyjson_mut_arr_with_sint32(doc, hits_arr, 4);
yyjson_mut_obj_add_val(doc, root, "hits", hits);

const char *json = yyjson_mut_write(doc, 0, NULL);
if (json) {
    printf("json: %s\n", json); // {"name":"Mash","star":4,"hits":[2,2,1,3]}
    free((void *)json);
}

yyjson_mut_doc_free(doc);
```

### Read JSON file with options
```c
yyjson_read_err err;
yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
yyjson_doc *doc = yyjson_read_file("/tmp/test.json", flg, NULL, &err);

if (doc) {
    yyjson_val *obj = yyjson_doc_get_root(doc);
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(obj, &iter);
    yyjson_val *key, *val;
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = key + 1;
        printf("%s: %s\n", yyjson_get_str(key), yyjson_get_type_desc(val));
    }
} else {
    printf("read error: %s at position: %ld\n", err.msg, err.pos);
}
```

### Write JSON file with options
```c
yyjson_doc *idoc = yyjson_read_file("/tmp/test.json", 0, NULL, NULL);
yyjson_mut_doc *doc = yyjson_doc_mut_copy(idoc, NULL);
yyjson_mut_val *obj = yyjson_mut_doc_get_root(doc);

yyjson_mut_obj_iter iter;
yyjson_mut_obj_iter_init(obj, &iter);
yyjson_mut_val *key, *val;
while ((key = yyjson_mut_obj_iter_next(&iter))) {
    val = key->next;
    if (yyjson_mut_is_null(val)) {
        yyjson_mut_obj_iter_remove(&iter);
    }
}

yyjson_write_err err;
yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
yyjson_mut_write_file("/tmp/test.json", doc, flg, NULL, &err);
if (err.code) {
    printf("write error: %d, message: %s\n", err.code, err.msg);
}
```

## Documentation
* [Building](https://github.com/ibireme/yyjson/blob/master/doc/Building.md)
* [API and sample code](https://github.com/ibireme/yyjson/blob/master/doc/API.md)
* [Data structure](https://github.com/ibireme/yyjson/blob/master/doc/DataStructure.md)
* [Fast way to read and write numbers](https://github.com/ibireme/c_numconv_benchmark)

## TODO
* [x] Add full document page.
* [x] Add GitHub workflow for CI and codecov.
* [ ] Add more tests: sanitizer, fuzzer.
* [ ] Support JSON Pointer to query value from document.
* [ ] Support return big number as raw string.
* [ ] Optimize performance for gcc.
* [ ] Optimize performance for 32-bit processor.
* [ ] Optimize performance of double number reader/writer with other algorithms [\[1\]](https://github.com/abolz/Drachennest) [\[2\]](https://github.com/lemire/fast_double_parser).

# License
This project is released under the MIT license.
