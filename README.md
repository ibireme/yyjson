# yyjson

[![Build](https://img.shields.io/github/workflow/status/ibireme/yyjson/cmake?style=flat-square)](https://github.com/ibireme/yyjson/actions)
[![Codecov](https://img.shields.io/codecov/c/gh/ibireme/yyjson?style=flat-square)](https://codecov.io/gh/ibireme/yyjson)
[![License](https://img.shields.io/github/license/ibireme/yyjson?color=blue&style=flat-square)](https://github.com/ibireme/yyjson/blob/master/LICENSE)
[![Version](https://img.shields.io/github/v/release/ibireme/yyjson?color=orange&style=flat-square)](https://github.com/ibireme/yyjson/releases)

A high performance JSON library written in ANSI C.

# Features
- **Fast**: can read or write gigabytes per second JSON data on modern CPU.
- **Portable**: compliance with ANSI C (C89).
- **Standard**: strict compliance with [RFC 8259](https://tools.ietf.org/html/rfc8259) standard.
- **Safe**: complete JSON form, number format and UTF-8 validation.
- **Accuracy**: can read and write `int64`, `uint64` and `double` numbers accurately.
- **Less Restriction**: support unlimited JSON level, `\u0000` and non null-terminated string.
- **Extendable**: options to allow comments, trailing commas, nan/inf, custom memory allocator.
- **Developer Friendly**: only one `h` and one `c` file, easy to integrate.

# Limitations
- An array or object is stored as some [data structure](https://github.com/ibireme/yyjson/blob/master/doc/DataStructure.md) like linked list, access elements with index or key is slower than iterator.
- Duplicate keys are allowed in an object, and the order of the keys is preserved.
- JSON parsing result is immutable, a `mutable copy` is required for modification.

# Performance
Benchmark project and dataset: [yyjson_benchmark](https://github.com/ibireme/yyjson_benchmark)

The simdjson's new `On Demand` API is faster if most JSON fields are known at compile time.
This benchmark project only checks the DOM API, a new benchmark will be added later.

#### AWS EC2 (AMD EPYC 7R32, gcc 9.3)
![ec2_chart](doc/images/perf_reader_ec2.svg)

|twitter.json|parse (GB/s)|stringify (GB/s)|
|---|---|---|
|yyjson(insitu)|1.80|1.51|
|yyjson|1.72|1.42|
|simdjson|1.52|0.61|
|sajson|1.16|   |
|rapidjson(insitu)|0.77|   |
|rapidjson(utf8)|0.26|0.39|
|cjson|0.32|0.17|
|jansson|0.05|0.11|


#### iPhone (Apple A14, clang 12)
![a14_chart](doc/images/perf_reader_a14.svg)

|twitter.json|parse (GB/s)|stringify (GB/s)|
|---|---|---|
|yyjson(insitu)|3.51|2.41|
|yyjson|2.39|2.01|
|simdjson|2.19|0.80|
|sajson|1.74||
|rapidjson(insitu)|0.75| |
|rapidjson(utf8)|0.30|0.58|
|cjson|0.48|0.33|
|jansson|0.09|0.24|

More benchmark reports with interactive charts (update 2020-12-12)

|Platform|CPU|Compiler|OS|Report|
|---|---|---|---|---|
|Intel NUC 8i5|Core i5-8259U|msvc 2019|Windows 10 2004|[Charts](https://ibireme.github.io/yyjson_benchmark/reports/Intel_NUC_8i5_msvc_2019.html)|
|Intel NUC 8i5|Core i5-8259U|clang 10.0|Ubuntu 20.04|[Charts](https://ibireme.github.io/yyjson_benchmark/reports/Intel_NUC_8i5_clang_10.html)|
|Intel NUC 8i5|Core i5-8259U|gcc 9.3|Ubuntu 20.04|[Charts](https://ibireme.github.io/yyjson_benchmark/reports/Intel_NUC_8i5_gcc_9.html)|
|AWS EC2 c5a.large|AMD EPYC 7R32|gcc 9.3|Ubuntu 20.04|[Charts](https://ibireme.github.io/yyjson_benchmark/reports/EC2_c5a.large_gcc_9.html)|
|AWS EC2 t4g.medium|Graviton2 (ARM64)|gcc 9.3|Ubuntu 20.04|[Charts](https://ibireme.github.io/yyjson_benchmark/reports/EC2_t4g.medium_gcc_9.html)|
|Apple iPhone 12 Pro|A14 (ARM64)|clang 12.0|iOS 14|[Charts](https://ibireme.github.io/yyjson_benchmark/reports/Apple_A14_clang_12.html)|

### For better performance, yyjson prefers:
* A modern processor with:
    * high instruction level parallelism
    * excellent branch predictor
    * low penalty for misaligned memory access
* A modern compiler with good optimizer.

# Build and test

### Manually
Just copy `yyjson.h` and `yyjson.c` to your project and start using it.
Since `yyjson` is ANSI C compatible, no other configuration is needed typically.

`yyjson` has been tested with the following compilers: `gcc`, `clang`, `msvc`, `icc`, `tcc`. If you find a compile error, please [report a bug](https://github.com/ibireme/yyjson/issues/new?template=bug_report.md).

`yyjson` has all features enabled by default, but you can trim out some of them by adding compile flags. For example, disable JSON writer to reduce the binary size when you don't need serialization, or disable comments support to improve parsing performance. See [compile flags](https://github.com/ibireme/yyjson/blob/master/doc/BuildAndTest.md#compile-flags) for details.

### CMake
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

Build static library and run tests:
```shell
cmake .. -DYYJSON_BUILD_TESTS=ON
cmake --build .
ctest
```

Supported CMake options:

- `-DBUILD_SHARED_LIBS=ON` Build shared library instead of static library.
- `-DYYJSON_BUILD_TESTS=ON` Build all tests.
- `-DYYJSON_DISABLE_READER=ON` Disable JSON reader if you don't need it.
- `-DYYJSON_DISABLE_WRITER=ON` Disable JSON writer if you don't need it.
- `-DYYJSON_DISABLE_FAST_FP_CONV=ON` Disable fast floating-point number conversion.
- `-DYYJSON_DISABLE_NON_STANDARD=ON` Disable non-standard JSON support at compile time.

[More details](https://github.com/ibireme/yyjson/blob/master/doc/BuildAndTest.md)

# Sample Code

### Read JSON string
```c
const char *json = "{\"name\":\"Mash\",\"star\":4,\"hits\":[2,2,1,3]}";

// Read JSON and get root
yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
yyjson_val *root = yyjson_doc_get_root(doc);

// Get root["name"]
yyjson_val *name = yyjson_obj_get(root, "name");
printf("name: %s\n", yyjson_get_str(name));
printf("name length:%d\n", (int)yyjson_get_len(name));

// Get root["star"]
yyjson_val *star = yyjson_obj_get(root, "star");
printf("star: %d\n", (int)yyjson_get_int(star));

// Get root["hits"], iterate over the array
yyjson_val *hits = yyjson_obj_get(root, "hits");
size_t idx, max;
yyjson_val *hit;
yyjson_arr_foreach(hits, idx, max, hit) {
    printf("hit%d: %d\n", (int)idx, (int)yyjson_get_int(hit));
}

// Free the doc
yyjson_doc_free(doc);

// All functions accept NULL input, and return NULL on error.
```

### Write JSON string
```c
// Create a mutable doc
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
yyjson_mut_val *root = yyjson_mut_obj(doc);
yyjson_mut_doc_set_root(doc, root);

// Set root["name"] and root["star"]
yyjson_mut_obj_add_str(doc, root, "name", "Mash");
yyjson_mut_obj_add_int(doc, root, "star", 4);

// Set root["hits"] with an array
int hits_arr[] = {2, 2, 1, 3};
yyjson_mut_val *hits = yyjson_mut_arr_with_sint32(doc, hits_arr, 4);
yyjson_mut_obj_add_val(doc, root, "hits", hits);

// To string, minified
const char *json = yyjson_mut_write(doc, 0, NULL);
if (json) {
    printf("json: %s\n", json); // {"name":"Mash","star":4,"hits":[2,2,1,3]}
    free((void *)json);
}

// Free the doc
yyjson_mut_doc_free(doc);
```

### Read JSON file with options
```c
// Read JSON file, allowing comments and trailing commas
yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
yyjson_read_err err;
yyjson_doc *doc = yyjson_read_file("/tmp/config.json", flg, NULL, &err);

// Iterate over the root object
if (doc) {
    yyjson_val *obj = yyjson_doc_get_root(doc);
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(obj, &iter);
    yyjson_val *key, *val;
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        printf("%s: %s\n", yyjson_get_str(key), yyjson_get_type_desc(val));
    }
} else {
    printf("read error (%u): %s at position: %ld\n", err.code, err.msg, err.pos);
}

// Free the doc
yyjson_doc_free(doc);
```

### Write JSON file with options
```c
// Read the JSON file as a mutable doc
yyjson_doc *idoc = yyjson_read_file("/tmp/config.json", 0, NULL, NULL);
yyjson_mut_doc *doc = yyjson_doc_mut_copy(idoc, NULL);
yyjson_mut_val *obj = yyjson_mut_doc_get_root(doc);

// Remove null values in root object
yyjson_mut_obj_iter iter;
yyjson_mut_obj_iter_init(obj, &iter);
yyjson_mut_val *key, *val;
while ((key = yyjson_mut_obj_iter_next(&iter))) {
    val = yyjson_mut_obj_iter_get_val(key);
    if (yyjson_mut_is_null(val)) {
        yyjson_mut_obj_iter_remove(&iter);
    }
}

// Write the json pretty, escape unicode
yyjson_write_flag flg = YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE;
yyjson_write_err err;
yyjson_mut_write_file("/tmp/config.json", doc, flg, NULL, &err);
if (err.code) {
    printf("write error (%u): %s\n", err.code, err.msg);
}

// Free the doc
yyjson_doc_free(idoc);
yyjson_mut_doc_free(doc);
```

# Documentation
* [Build and test](https://github.com/ibireme/yyjson/blob/master/doc/BuildAndTest.md)
* [API and sample code](https://github.com/ibireme/yyjson/blob/master/doc/API.md)
* [Data structure](https://github.com/ibireme/yyjson/blob/master/doc/DataStructure.md)

# TODO
* [x] Add documentation page.
* [x] Add GitHub workflow for CI and codecov.
* [x] Add more tests: valgrind, sanitizer.
* [x] Support JSON Pointer to query value from document.
* [x] Add fuzzer.
* [ ] Add `RAW` type for JSON reader and writer.
* [ ] Add streaming API for JSON reader and writer.
* [ ] Add documentation about performance.
* [ ] Optimize performance for 32-bit processor.

# License
This project is released under the MIT license.
