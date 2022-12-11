API
===




# API Design

## API prefix

All public functions and structs are prefixed with `yyjson_`, and all constants are prefixed with `YYJSON_`.

## API for immutable/mutable data

The library have 2 types of data structures: immutable and mutable:

|type|immutable|mutable|
|---|---|---|
|document|yyjson_doc|yyjson_mut_doc|
|value|yyjson_val|yyjson_mut_val|


When reading a JSON, yyjson returns immutable documents and values;<br/>
When building a JSON, yyjson creates mutable documents and values;<br/>
The document holds the memory for all its JSON values and strings.<br/>

For most immutable APIs, you can just add a `mut` after `yyjson_` to get the mutable version, for example:
```c
char *yyjson_write(yyjson_doc *doc, ...);
char *yyjson_mut_write(yyjson_mut_doc *doc, ...);

bool yyjson_is_str(yyjson_val *val);
bool yyjson_mut_is_str(yyjson_mut_val *val);
```

The library also provides some functions to convert values between immutable and mutable:<br/>

```c
// doc -> mut_doc
yyjson_mut_doc *yyjson_doc_mut_copy(yyjson_doc *doc, ...);
// val -> mut_val
yyjson_mut_val *yyjson_val_mut_copy(yyjson_val *val, ...);

// mut_doc -> doc
yyjson_doc *yyjson_mut_doc_imut_copy(yyjson_mut_doc *doc, ...);
// mut_val -> val
yyjson_doc *yyjson_mut_val_imut_copy(yyjson_mut_val *val, ...);
```

## API for string
The library supports strings with or without null-terminator ('\0').<br/>
When you need to use a string without null terminator, or you know the length of the string explicitly, you can use the function that ends with `n`, for example:
```c
// null-terminator is required
bool yyjson_equals_str(yyjson_val *val, const char *str);
// null-terminator is optional
bool yyjson_equals_strn(yyjson_val *val, const char *str, size_t len);
```

When creating JSON, yyjson treats strings as constants for better performance. When your string will be modified, you should use a function with a `cpy` to copy the string to the document, for example:
```c
// reference only, null-terminated is required
yyjson_mut_val *yyjson_mut_str(yyjson_mut_doc *doc, const char *str);
// reference only, null-terminator is optional
yyjson_mut_val *yyjson_mut_strn(yyjson_mut_doc *doc, const char *str, size_t len);

// copied, null-terminated is required
yyjson_mut_val *yyjson_mut_strcpy(yyjson_mut_doc *doc, const char *str);
// copied, null-terminator is optional
yyjson_mut_val *yyjson_mut_strncpy(yyjson_mut_doc *doc, const char *str, size_t len);
```



---------------
# Read JSON
The library provides 3 functions for reading JSON,<br/>
each function accepts an input of UTF-8 data or a file,<br/>
returns a document if it succeeds or returns NULL if it fails.

## Read JSON from string
The `dat` should be a UTF-8 string, null-terminator is not required.<br/>
The `len` is the byte length of `dat`.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see `reader flag` for details.<br/>
If input is invalid, `NULL` is returned.

```c
yyjson_doc *yyjson_read(const char *dat, 
                        size_t len, 
                        yyjson_read_flag flg);
```
Sample code:

```c
const char *str = "[1,2,3,4]";
yyjson_doc *doc = yyjson_read(str, strlen(str), 0);
if (doc) {...}
yyjson_doc_free(doc);
```

## Read JSON from file

The `path` is JSON file path.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see `reader flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>
If input is invalid, `NULL` is returned.

```c
yyjson_doc *yyjson_read_file(const char *path,
                             yyjson_read_flag flg,
                             const yyjson_alc *alc,
                             yyjson_read_err *err);
```

Sample code:

```c
yyjson_doc *doc = yyjson_read_file("/tmp/test.json", 0, NULL, NULL);
if (doc) {...}
yyjson_doc_free(doc);
```

## Read JSON with options
The `dat` should be a UTF-8 string, you can pass a const string if you don't use `YYJSON_READ_INSITU` flag.<br/>
The `len` is the `dat`'s length in bytes.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see `reader flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>

```c
yyjson_doc *yyjson_read_opts(char *dat, 
                             size_t len, 
                             yyjson_read_flag flg,
                             const yyjson_alc *alc, 
                             yyjson_read_err *err);
```

Sample code:

```c
const char *dat = your_file.bytes;
size_t len = your_file.size;

yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_INF_AND_NAN;
yyjson_err err;
yyjson_doc *doc = yyjson_read_opts((char *)dat, len, flg, NULL, &err);

if (doc) {...}
else printf("read error: %s code: %u at position: %ld\n", err.msg, err.code, err.pos);

yyjson_doc_free(doc);
```

## Reader flag
The library provides a set of flags for JSON reader.<br/>
You can use a single flag, or combine multiple flags with bitwise `|` operator.

● **YYJSON_READ_NOFLAG = 0**<br/>

This is the default flag for JSON reader (RFC-8259 or ECMA-404 compliant):

- Read positive integer as `uint64_t`.
- Read negative integer as `int64_t`.
- Read floating-point number as `double` with correct rounding.
- Read integer which cannot fit in `uint64_t` or `int64_t` as `double`.
- Report error if real number is infinity.
- Report error if string contains invalid UTF-8 character or BOM.
- Report error on trailing commas, comments, `inf` and `nan` literals.

● **YYJSON_READ_INSITU**<br/>
Read the input data in-situ.<br/>
This option allows the reader to modify and use input data to store string values, which can increase reading speed slightly. The caller should hold the input data before free the document. The input data must be padded by at least `YYJSON_PADDING_SIZE` byte. For example: `[1,2]` should be `[1,2]\0\0\0\0`, input length should be 5.

Sample code:

```c
size_t dat_len = ...;
char *buf = malloc(dat_len + YYJSON_PADDING_SIZE); // create a buffer larger than (len + 4)
read_from_socket(buf, ...);
memset(buf + file_size, 0, YYJSON_PADDING_SIZE); // set 4-byte padding after data

yyjson_doc *doc = yyjson_read_opts(buf, dat_len, YYJSON_READ_INSITU, NULL, NULL);
if (doc) {...}
yyjson_doc_free(doc);
free(buf); // the input dat should free after document.
```

● **YYJSON_READ_STOP_WHEN_DONE**<br/>
Stop when done instead of issues an error if there's additional content after a JSON document.<br/> 
This option may used to parse small pieces of JSON in larger data, such as [NDJSON](https://en.wikipedia.org/wiki/JSON_streaming).<br/>

Sample code:

```c
// Single file with multiple JSON, such as:
// [1,2,3] [4,5,6] {"a":"b"}

size_t file_size = ...;
char *dat = malloc(file_size + 4);
your_read_file(dat, file);
memset(dat + file_size, 0, 4); // add padding
    
char *hdr = dat;
char *end = dat + file_size;
yyjson_read_flag flg = YYJSON_READ_INSITU | YYJSON_READ_STOP_WHEN_DONE;

while (true) {
    yyjson_doc *doc = yyjson_read_opts(hdr, end - hdr, flg, NULL, NULL);
    if (!doc) break;
    your_doc_process(doc);
    hdr += yyjson_doc_get_read_size(doc); // move to next position
    yyjson_doc_free(doc);
}
free(dat);
```

● **YYJSON_READ_ALLOW_TRAILING_COMMAS**<br/>
Allow single trailing comma at the end of an object or array, for example:

```
{
    "a": 1,
    "b": 2,
}

[
    "a",
    "b",
]
```

● **YYJSON_READ_ALLOW_COMMENTS**<br/>
Allow C-style single line and multiple line comments, for example:

```
{
    "name": "Harry", // single line comment
    "id": /* multiple line comment */ 123
}
```

● **YYJSON_READ_ALLOW_INF_AND_NAN**<br/>
Allow nan/inf number or literal (case-insensitive), such as 1e999, NaN, Inf, -Infinity, for example:

```
{
    "large": 123e999,
    "nan1": NaN,
    "nan2": nan,
    "inf1:" Inf,
    "inf2": -Infinity
}
```

● **YYJSON_READ_NUMBER_AS_RAW**<br/>
Read numbers as raw strings without parsing, allowing you to keep arbitrarily large numbers. 

You can use these functions to extract raw strings:
```c
bool yyjson_is_raw(yyjson_val *val);
const char *yyjson_get_raw(yyjson_val *val);
size_t yyjson_get_len(yyjson_val *val)
```

● **YYJSON_READ_ALLOW_INVALID_UNICODE**<br/>
Allow reading invalid unicode when parsing string values (non-standard),
for example:
```
"\x80xyz"
"\xF0\x81\x81\x81"
```
Invalid characters will be allowed to appear in the string values, but invalid escape sequences will still be reported as errors. This flag does not affect the performance of correctly encoded strings.

***Warning***: strings in JSON values may contain incorrect encoding when this option is used, you need to handle these strings carefully to avoid security risks.


---------------
# Write JSON
The library provides 3 sets of functions for writing JSON,<br/>
each function accepts an input of JSON document or root value, and returns a UTF-8 string or file.

## Write JSON to string
The `doc/val` is JSON document or root value, if you pass NULL, you will get NULL result.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see `writer flag` for details.<br/>
The `len` is a pointer to receive output length, pass NULL if you don't need it.<br/>
This function returns a new JSON string, or NULL if error occurs.<br/>
The string is encoded as UTF-8 with a null-terminator. <br/>
You should use free() or alc->free() to release it when it's no longer needed.

```c
// doc -> str
char *yyjson_write(const yyjson_doc *doc, yyjson_write_flag flg, size_t *len);
// mut_doc -> str
char *yyjson_mut_write(const yyjson_mut_doc *doc, yyjson_write_flag flg, size_t *len);
// val -> str
char *yyjson_val_write(const yyjson_val *val, yyjson_write_flag flg, size_t *len);
// mut_val -> str
char *yyjson_mut_val_write(const yyjson_mut_val *val, yyjson_write_flag flg, size_t *len);
```

Sample code 1:

```c
yyjson_doc *doc = yyjson_read("[1,2,3]", 7, 0);
char *json = yyjson_write(doc, YYJSON_WRITE_PRETTY, NULL);
printf("%s\n", json);
free(json);
```

Sample code 2:
```c
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
yyjson_mut_val *arr = yyjson_mut_arr(doc);
yyjson_mut_doc_set_root(doc, arr);
yyjson_mut_arr_add_int(doc, arr, 1);
yyjson_mut_arr_add_int(doc, arr, 2);
yyjson_mut_arr_add_int(doc, arr, 3);
    
char *json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, NULL);
printf("%s\n", json);
free(json);
```

## Write JSON to file
The `path` is output JSON file path, If the path is invalid, you will get an error. If the file is not empty, the content will be discarded.<br/>
The `doc/val` is JSON document or root value, if you pass NULL, you will get an error.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see `writer flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>
This function returns true on success, or false if error occurs.<br/>

```c
// doc -> file
bool yyjson_write_file(const char *path, const yyjson_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// mut_doc -> file
bool yyjson_mut_write_file(const char *path, const yyjson_mut_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// val -> file
bool yyjson_val_write_file(const char *path, const yyjson_val *val, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
// mut_val -> file
bool yyjson_mut_val_write_file(const char *path, const yyjson_mut_val *val, yyjson_write_flag flg, const yyjson_alc *alc, yyjson_write_err *err);
```

Sample code:

```c
yyjson_doc *doc = yyjson_read_file("/tmp/test.json", 0, NULL, NULL);
bool suc = yyjson_write_file("tmp/test.json", doc, YYJSON_WRITE_PRETTY, NULL, NULL);
if (suc) printf("OK");
```

## Write JSON with options
The `doc/val` is JSON document or root value, if you pass NULL, you will get NULL result.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see `writer flag` for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see `memory allocator` for details.<br/>
The `len` is a pointer to receive output length, pass NULL if you don't need it.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>

This function returns a new JSON string, or NULL if error occurs.<br/>
The string is encoded as UTF-8 with a null-terminator. <br/>
You should use free() or alc->free() to release it when it's no longer needed.

```c
char *yyjson_write_opts(const yyjson_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, size_t *len, yyjson_write_err *err);

char *yyjson_mut_write_opts(const yyjson_mut_doc *doc, yyjson_write_flag flg, const yyjson_alc *alc, size_t *len, yyjson_write_err *err);

char *yyjson_val_write_opts(const yyjson_val *val, yyjson_write_flag flg, const yyjson_alc *alc, size_t *len, yyjson_write_err *err);

char *yyjson_mut_val_write_opts(const yyjson_mut_val *val, yyjson_write_flag flg, const yyjson_alc *alc, size_t *len, yyjson_write_err *err);
```

Sample code:

```c
yyjson_doc *doc = ...;

// init an allocator with stack memory
char buf[64 * 1024];
yyjson_alc alc;
yyjson_alc_pool_init(&alc, buf, sizeof(buf));

// write
size_t len;
yyjson_write_err err;
char *json = yyjson_write_opts(doc, YYJSON_WRITE_PRETTY | YYJSON_WRITE_ESCAPE_UNICODE, &alc, &len, &err);

// get result
if (json) {
    printf("suc: %lu\n%s\n", len, json);
} else {
    printf("err: %u msg:%s\n", err.code, err.msg);
}
alc.free(alc.ctx, json);
```


## Writer flag
The library provides a set of flags for JSON writer.<br/>
You can use a single flag, or combine multiple flags with bitwise `|` operator.

● **YYJSON_WRITE_NOFLAG = 0**<br/>
This is the default flag for JSON writer:

- Write JSON minify.
- Report error on inf or nan number.
- Report error on invalid UTF-8 string.
- Do not escape unicode or slash. 

● **YYJSON_WRITE_PRETTY**<br/>
Write JSON pretty with 4 space indent.

● **YYJSON_WRITE_ESCAPE_UNICODE**<br/>
Escape unicode as `\uXXXX`, make the output ASCII only, for example:

```json
["Alizée, 😊"]
["Aliz\\u00E9e, \\uD83D\\uDE0A"]
```

● **YYJSON_WRITE_ESCAPE_SLASHES**<br/>
Escape `/` as `\/`, for example:

```json
["https://github.com"]
["https:\/\/github.com"]
```

● **YYJSON_WRITE_ALLOW_INF_AND_NAN**<br/>
Write inf/nan number as `Infinity` and `NaN` literals instead of reporting errors.<br/>

Note that this output is **NOT** standard JSON and may be rejected by other JSON libraries, for example:

```js
{"not_a_number":NaN,"large_number":Infinity}
```

● **YYJSON_WRITE_INF_AND_NAN_AS_NULL**<br/>
Write inf/nan number as `null` literal instead of reporting errors.<br/>
This flag will override `YYJSON_WRITE_ALLOW_INF_AND_NAN` flag, for example:

```js
{"not_a_number":null,"large_number":null}
```

● **YYJSON_WRITE_ALLOW_INVALID_UNICODE**<br/>
Allow invalid unicode when encoding string values.

Invalid characters in string value will be copied byte by byte. If `YYJSON_WRITE_ESCAPE_UNICODE` flag is also set, invalid character will be escaped as `\uFFFD` (replacement character).

This flag does not affect the performance of correctly encoded string.


---------------
# Access JSON Document

## JSON Document

You can access the content of a document with the following functions:
```c
// Get the root value of this JSON document.
yyjson_val *yyjson_doc_get_root(yyjson_doc *doc);

// Get how many bytes are read when parsing JSON.
// e.g. "[1,2,3]" returns 7.
size_t yyjson_doc_get_read_size(yyjson_doc *doc);

// Get total value count in this JSON document.
// e.g. "[1,2,3]" returns 4 (1 array and 3 numbers).
size_t yyjson_doc_get_val_count(yyjson_doc *doc);
```

A document holds all the memory for its internal values and strings. When you no longer need it, you should release the document and free up all the memory:
```c
// Free the document; if NULL is passed in, do nothing.
void yyjson_doc_free(yyjson_doc *doc);
```

## JSON Value

The following functions can be used to determine the type of a JSON value.

```c
// Returns the type and subtype of a JSON value.
// Returns 0 if the input is NULL.
yyjson_type yyjson_get_type(yyjson_val *val);
yyjson_subtype yyjson_get_subtype(yyjson_val *val);

// Returns value's tag, see `Data Structures` doc for details.
uint8_t yyjson_get_tag(yyjson_val *val);

// returns type description, such as:  
// "null", "string", "array", "object", "true", "false",
// "uint", "sint", "real", "unknown"
const char *yyjson_get_type_desc(yyjson_val *val);

// Returns true if the JSON value is specified type.
// Returns false if the input is NULL or not the specified type.
bool yyjson_is_null(yyjson_val *val);  // null
bool yyjson_is_true(yyjson_val *val);  // true
bool yyjson_is_false(yyjson_val *val); // false
bool yyjson_is_bool(yyjson_val *val);  // true/false
bool yyjson_is_uint(yyjson_val *val);  // uint64_t
bool yyjson_is_sint(yyjson_val *val);  // int64_t
bool yyjson_is_int(yyjson_val *val);   // uint64_t/int64_t
bool yyjson_is_real(yyjson_val *val);  // double
bool yyjson_is_num(yyjson_val *val);   // uint64_t/int64_t/double
bool yyjson_is_str(yyjson_val *val);   // string
bool yyjson_is_arr(yyjson_val *val);   // array
bool yyjson_is_obj(yyjson_val *val);   // object
bool yyjson_is_ctn(yyjson_val *val);   // array/object
```

The following functions can be used to get the contents of the JSON value.

```c
// Returns bool value, or false if `val` is not bool type.
bool yyjson_get_bool(yyjson_val *val);

// Returns uint64_t value, or 0 if `val` is not uint type.
uint64_t yyjson_get_uint(yyjson_val *val);

// Returns int64_t value, or 0 if `val` is not sint type.
int64_t yyjson_get_sint(yyjson_val *val);

// Returns int value (may overflow), or 0 if `val` is not uint/sint type.
int yyjson_get_int(yyjson_val *val);

// Returns double value, or 0 if `val` is not real type.
double yyjson_get_real(yyjson_val *val);

// Returns the string value, or NULL if `val` is not string type.
const char *yyjson_get_str(yyjson_val *val);

// Returns the content length (string length in bytes, array size, 
// object size), or 0 if the value does not contains length data.
size_t yyjson_get_len(yyjson_val *val);

// Returns whether the value is equals to a string.
// Returns false if input is NULL or `val` is not string.
bool yyjson_equals_str(yyjson_val *val, const char *str);
bool yyjson_equals_strn(yyjson_val *val, const char *str, size_t len);
```


The following functions can be used to modify the content of a JSON value.<br/>

Warning: For immutable documents, these functions will break the `immutable` convention, you should use this set of APIs with caution (e.g. make sure the document is only accessed in a single thread).

```c
// Set the value to new type and content.
// Returns false if input is NULL or `val` is object or array.
bool yyjson_set_raw(yyjson_val *val, const char *raw, size_t len);
bool yyjson_set_null(yyjson_val *val);
bool yyjson_set_bool(yyjson_val *val, bool num);
bool yyjson_set_uint(yyjson_val *val, uint64_t num);
bool yyjson_set_sint(yyjson_val *val, int64_t num);
bool yyjson_set_int(yyjson_val *val, int num);
bool yyjson_set_real(yyjson_val *val, double num);

// The string is not copied, should be held by caller.
bool yyjson_set_str(yyjson_val *val, const char *str);
bool yyjson_set_strn(yyjson_val *val, const char *str, size_t len);
```


## JSON Array

The following functions can be used to access a JSON array.<br/>

Note that accessing elements by an index may take a linear search time. Therefore, if you need to iterate through an array, it is recommended to use the iterator API.

```c
// Returns the number of elements in this array.
// Returns 0 if the input is not an array.
size_t yyjson_arr_size(yyjson_val *arr);

// Returns the element at the specified position (linear search time).
// Returns NULL if the index is out of bounds, or input is not an array.
yyjson_val *yyjson_arr_get(yyjson_val *arr, size_t idx);

// Returns the first element of this array (constant time).
// Returns NULL if array is empty or intput is not an array.
yyjson_val *yyjson_arr_get_first(yyjson_val *arr);

// Returns the last element of this array (linear search time).
// Returns NULL if array is empty or intput is not an array.
yyjson_val *yyjson_arr_get_last(yyjson_val *arr);
```

## JSON Array Iterator
There are two ways to traverse an array:<br/>

Sample code 1 (iterator API):
```c
yyjson_val *arr; // the array to be traversed

yyjson_val *val;
yyjson_arr_iter iter;
yyjson_arr_iter_init(arr, &iter);
while ((val = yyjson_arr_iter_next(&iter))) {
    your_func(val);
}
```

Sample code 2 (foreach macro):
```c
yyjson_val *arr; // the array to be traversed

size_t idx, max;
yyjson_val *val;
yyjson_arr_foreach(arr, idx, max, val) {
    your_func(idx, val);
}
```
<br/>

There's also mutable version API to traverse an mutable array:<br/>

Sample code 1 (mutable iterator API):
```c
yyjson_mut_val *arr; // the array to be traversed

yyjson_mut_val *val;
yyjson_mut_arr_iter iter;
yyjson_mut_arr_iter_init(arr, &iter);
while ((val = yyjson_mut_arr_iter_next(&iter))) {
    if (your_val_is_unused(val)) {
        // you can remove current value inside iteration
        yyjson_mut_arr_iter_remove(&iter); 
    }
}
```

Sample code 2 (mutable foreach macro):
```c
yyjson_mut_val *arr; // the array to be traversed

size_t idx, max;
yyjson_mut_val *val;
yyjson_mut_arr_foreach(arr, idx, max, val) {
    your_func(idx, val);
}
```


## JSON Object
The following functions can be used to access a JSON object.<br/>

Note that accessing elements by a key may take a linear search time. Therefore, if you need to iterate through an object, it is recommended to use the iterator API.


```c
// Returns the number of key-value pairs in this object.
// Returns 0 if input is not an object.
size_t yyjson_obj_size(yyjson_val *obj);

// Returns the value to which the specified key is mapped.
// Returns NULL if this object contains no mapping for the key.
yyjson_val *yyjson_obj_get(yyjson_val *obj, const char *key);
yyjson_val *yyjson_obj_getn(yyjson_val *obj, const char *key, size_t key_len);

// If the order of object's key is known at compile-time,
// you can use this method to avoid searching the entire object.
// e.g. { "x":1, "y":2, "z":3 }
yyjson_val *obj = ...;
yyjson_obj_iter iter;
yyjson_obj_iter_init(obj, &iter);

yyjson_val *x = yyjson_obj_iter_get(&iter, "x");
yyjson_val *y = yyjson_obj_iter_get(&iter, "y");
yyjson_val *z = yyjson_obj_iter_get(&iter, "z");
```

## JSON Object Iterator
There are two ways to traverse an object:<br/>

Sample code 1 (iterator API):
```c
yyjson_val *obj; // the object to be traversed

yyjson_val *key, *val;
yyjson_obj_iter iter;
yyjson_obj_iter_init(obj, &iter);
while ((key = yyjson_obj_iter_next(&iter))) {
    val = yyjson_obj_iter_get_val(key);
    your_func(key, val);
}
```

Sample code 2 (foreach macro):
```c
yyjson_val *obj; // this is your object

size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    your_func(key, val);
}
```
<br/>

There's also mutable version API to traverse an mutable object:<br/>

Sample code 1 (mutable iterator API):
```c
yyjson_mut_val *obj; // the object to be traversed

yyjson_mut_val *key, *val;
yyjson_mut_obj_iter iter;
yyjson_mut_obj_iter_init(obj, &iter);
while ((key = yyjson_mut_obj_iter_next(&iter))) {
    val = yyjson_mut_obj_iter_get_val(key);
    if (your_key_is_unused(key)) {
        // you can remove current kv pair inside iteration
        yyjson_mut_obj_iter_remove(&iter);
    }
}
```

Sample code 2 (mutable foreach macro):
```c
yyjson_mut_val *obj; // the object to be traversed

size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    your_func(key, val);
}
```

## JSON Pointer
The library supports querying JSON values via `JSON Pointer` ([RFC 6901](https://tools.ietf.org/html/rfc6901)).

```c
// `JSON pointer` is a null-terminated string.
yyjson_val *yyjson_get_pointer(yyjson_val *val, const char *ptr);
yyjson_val *yyjson_doc_get_pointer(yyjson_doc *doc, const char *ptr);
yyjson_mut_val *yyjson_mut_get_pointer(yyjson_mut_val *val, const char *ptr);
yyjson_mut_val *yyjson_mut_doc_get_pointer(yyjson_mut_doc *doc, const char *ptr);

// `JSON pointer` with string length, allow NUL (Unicode U+0000) characters inside.
yyjson_val *yyjson_get_pointern(yyjson_val *val, const char *ptr, size_t len);
yyjson_val *yyjson_doc_get_pointern(yyjson_doc *doc, const char *ptr, size_t len);
yyjson_mut_val *yyjson_mut_get_pointern(yyjson_mut_val *val, const char *ptr, size_t len);
yyjson_mut_val *yyjson_mut_doc_get_pointern(yyjson_mut_doc *doc, const char *ptr, size_t len);
```

For example, given the JSON document:
```json
{
    "size" : 3,
    "users" : [
        {"id": 1, "name": "Harry"},
        {"id": 2, "name": "Ron"},
        {"id": 3, "name": "Hermione"}
    ]
}
```
The following JSON strings evaluate to the accompanying values:

|Pointer|Matched Value|
|:--|:--|
| `""` | `the whole document` |
| `"/size"`| `3` |
| `"/users/0"` | `{"id": 1, "name": "Harry"}` |
| `"/users/1/name"` | `"Ron"` |
| `"/no_match"` | NULL |
| `"no_slash"` | NULL |
| `"/"` | NULL (match to empty key: root[""]) |


---------------
# Create JSON Document
`yyjson_mut_doc` and related APIs are used to build JSON documents. <br/>

Notice that `yyjson_mut_doc` uses a **memory pool** to hold all strings and values; the pool can only be created, grown, or freed in its entirety. Thus `yyjson_mut_doc` is more suitable for write-once than mutation of an existing document.<br/>

JSON objects and arrays are made up of linked lists, so each `yyjson_mut_val` can only be added to one object or array.

Sample code:

```c
// Build this JSON:
//     {
//        "page": 123,
//        "names": [ "Harry", "Ron", "Hermione" ]
//     }

// Create a mutable document.
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);

// Create an object, the value's memory is held by doc.
yyjson_mut_val *root = yyjson_mut_obj(doc);

// Create key and value, add to the root object.
yyjson_mut_val *key = yyjson_mut_str(doc, "page");
yyjson_mut_val *num = yyjson_mut_int(doc, 123);
yyjson_mut_obj_add(root, key, num);

// Create 3 string value, add to the array object.
yyjson_mut_val *names = yyjson_mut_arr(doc);
yyjson_mut_val *name1 = yyjson_mut_str(doc, "Harry");
yyjson_mut_val *name2 = yyjson_mut_str(doc, "Ron");
yyjson_mut_val *name3 = yyjson_mut_str(doc, "Hermione");
yyjson_mut_arr_append(names, name1);
yyjson_mut_arr_append(names, name2);
yyjson_mut_arr_append(names, name3);

// ❌ Wrong! the value is already added to another container.
yyjson_mut_obj_add(root, key, name1);

// Set the document's root value.
yyjson_mut_doc_set_root(doc, root);

// Write to JSON string
const char *json = yyjson_mut_write(doc, 0, NULL);

// Free the memory of doc and all values which is created from this doc.
yyjson_mut_doc_free(doc);
```


## Mutable Document

The following functions are used to create, modify, copy, and destroy a JSON document.<br/>

```c
// Creates and returns a new mutable JSON document.
// Returns NULL on error (e.g. memory allocation failure).
// If `alc` is NULL, the default allocator will be used.
yyjson_mut_doc *yyjson_mut_doc_new(yyjson_alc *alc);

// Delete the JSON document, free the memory of this doc
// and all values created from this doc
void yyjson_mut_doc_free(yyjson_mut_doc *doc);

// Get or set the root value of this JSON document.
yyjson_mut_val *yyjson_mut_doc_get_root(yyjson_mut_doc *doc);
void yyjson_mut_doc_set_root(yyjson_mut_doc *doc, yyjson_mut_val *root);

// Copies and returns a new mutable document/value from input.
// Returns NULL on error (e.g. memory allocation failure).

// doc -> mut_doc
yyjson_mut_doc *yyjson_doc_mut_copy(yyjson_doc *doc, const yyjson_alc *alc);
// val -> mut_val
yyjson_mut_val *yyjson_val_mut_copy(yyjson_mut_doc *doc,  yyjson_val *val);
// mut_doc -> mut_doc
yyjson_mut_doc *yyjson_mut_doc_mut_copy(yyjson_mut_doc *doc, const yyjson_alc *alc);
// mut_val -> mut_val
yyjson_mut_val *yyjson_mut_val_mut_copy(yyjson_mut_doc *doc, yyjson_mut_val *val);
// mut_doc -> doc
yyjson_doc *yyjson_mut_doc_imut_copy(yyjson_mut_doc *doc, yyjson_alc *alc);
// mut_val -> doc
yyjson_doc *yyjson_mut_val_imut_copy(yyjson_mut_val *val, yyjson_alc *alc);
```

## JSON Value Creation
The following functions are used to create mutable JSON value, 
the value's memory is held by the document.<br/>

```c
// Creates and returns a new value, returns NULL on error.
yyjson_mut_val *yyjson_mut_null(yyjson_mut_doc *doc);
yyjson_mut_val *yyjson_mut_true(yyjson_mut_doc *doc);
yyjson_mut_val *yyjson_mut_false(yyjson_mut_doc *doc);
yyjson_mut_val *yyjson_mut_bool(yyjson_mut_doc *doc, bool val);
yyjson_mut_val *yyjson_mut_uint(yyjson_mut_doc *doc, uint64_t num);
yyjson_mut_val *yyjson_mut_sint(yyjson_mut_doc *doc, int64_t num);
yyjson_mut_val *yyjson_mut_int(yyjson_mut_doc *doc, int64_t num);
yyjson_mut_val *yyjson_mut_real(yyjson_mut_doc *doc, double num);

// Creates a string value, the input string is NOT copied.
yyjson_mut_val *yyjson_mut_str(yyjson_mut_doc *doc, const char *str);
yyjson_mut_val *yyjson_mut_strn(yyjson_mut_doc *doc, const char *str, size_t len);

// Creates a string value, the input string is copied and held by the document.
yyjson_mut_val *yyjson_mut_strcpy(yyjson_mut_doc *doc, const char *str);
yyjson_mut_val *yyjson_mut_strncpy(yyjson_mut_doc *doc, const char *str, size_t len);
```


## JSON Array Creation
The following functions are used to create mutable JSON array.<br/>

```c
// Creates and returns an empty mutable array, returns NULL on error.
yyjson_mut_val *yyjson_mut_arr(yyjson_mut_doc *doc);

// Creates and returns a mutable array with c array.
yyjson_mut_val *yyjson_mut_arr_with_bool(yyjson_mut_doc *doc, bool *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint(yyjson_mut_doc *doc, int64_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint(yyjson_mut_doc *doc, uint64_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_real(yyjson_mut_doc *doc, double *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint8(yyjson_mut_doc *doc, int8_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint16(yyjson_mut_doc *doc, int16_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint32(yyjson_mut_doc *doc, int32_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_sint64(yyjson_mut_doc *doc, int64_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint8(yyjson_mut_doc *doc, uint8_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint16(yyjson_mut_doc *doc, uint16_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint32(yyjson_mut_doc *doc, uint32_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_uint64(yyjson_mut_doc *doc, uint64_t *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_float(yyjson_mut_doc *doc, float *vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_double(yyjson_mut_doc *doc, double *vals, size_t count);
// sample code:
int vals[3] = {-1, 0, 1};
yyjson_mut_val *arr = yyjson_mut_arr_with_sint32(doc, vals, 3);

// Creates and returns a mutable array with strings,
// the strings should be encoded as UTF-8.
yyjson_mut_val *yyjson_mut_arr_with_str(yyjson_mut_doc *doc, const char **vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strn(yyjson_mut_doc *doc, const char **vals, const size_t *lens, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strcpy(yyjson_mut_doc *doc, const char **vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strncpy(yyjson_mut_doc *doc, const char **vals, const size_t *lens, size_t count);
// sample code:
const char strs[3] = {"Jan", "Feb", "Mar"};
yyjson_mut_val *arr = yyjson_mut_arr_with_str(doc, strs, 3);
```

## JSON Array Modification

The following functions are used to modify the contents of a JSON array.<br/>

```c
// Inserts a value into an array at a given index.
// Returns false on error (e.g. out of bounds).
// Note that this function takes a linear search time.
bool yyjson_mut_arr_insert(yyjson_mut_val *arr, yyjson_mut_val *val, size_t idx);

// Inserts a val at the end of the array, returns false on error.
bool yyjson_mut_arr_append(yyjson_mut_val *arr, yyjson_mut_val *val);

// Inserts a val at the head of the array, returns false on error.
bool yyjson_mut_arr_prepend(yyjson_mut_val *arr, yyjson_mut_val *val);

// Replaces a value at index and returns old value, returns NULL on error.
// Note that this function takes a linear search time.
yyjson_mut_val *yyjson_mut_arr_replace(yyjson_mut_val *arr, size_t idx, yyjson_mut_val *val);

// Removes and returns a value at index, returns NULL on error.
// Note that this function takes a linear search time.
yyjson_mut_val *yyjson_mut_arr_remove(yyjson_mut_val *arr, size_t idx);

// Removes and returns the first value in this array, returns NULL on error.
yyjson_mut_val *yyjson_mut_arr_remove_first(yyjson_mut_val *arr);

// Removes and returns the last value in this array, returns NULL on error.
yyjson_mut_val *yyjson_mut_arr_remove_last(yyjson_mut_val *arr);

// Removes all values within a specified range in the array.
// Note that this function takes a linear search time.
bool yyjson_mut_arr_remove_range(yyjson_mut_val *arr, size_t idx, size_t len);

// Removes all values in this array.
bool yyjson_mut_arr_clear(yyjson_mut_val *arr);

// Convenience API:
// Adds a value at the end of this array, returns false on error.
bool yyjson_mut_arr_add_val(yyjson_mut_val *arr, yyjson_mut_val *val);
bool yyjson_mut_arr_add_null(yyjson_mut_doc *doc, yyjson_mut_val *arr);
bool yyjson_mut_arr_add_true(yyjson_mut_doc *doc, yyjson_mut_val *arr);
bool yyjson_mut_arr_add_false(yyjson_mut_doc *doc, yyjson_mut_val *arr);
bool yyjson_mut_arr_add_bool(yyjson_mut_doc *doc, yyjson_mut_val *arr, bool val);
bool yyjson_mut_arr_add_uint(yyjson_mut_doc *doc, yyjson_mut_val *arr, uint64_t num);
bool yyjson_mut_arr_add_sint(yyjson_mut_doc *doc, yyjson_mut_val *arr, int64_t num);
bool yyjson_mut_arr_add_int(yyjson_mut_doc *doc, yyjson_mut_val *arr, int64_t num);
bool yyjson_mut_arr_add_real(yyjson_mut_doc *doc, yyjson_mut_val *arr, double num);
bool yyjson_mut_arr_add_str(yyjson_mut_doc *doc, yyjson_mut_val *arr, const char *str);
bool yyjson_mut_arr_add_strn(yyjson_mut_doc *doc, yyjson_mut_val *arr, const char *str, size_t len);
bool yyjson_mut_arr_add_strcpy(yyjson_mut_doc *doc, yyjson_mut_val *arr, const char *str);
bool yyjson_mut_arr_add_strncpy(yyjson_mut_doc *doc, yyjson_mut_val *arr, const char *str, size_t len);

// Convenience API:
// Creates and adds a new array at the end of the array.
// Returns the new array, or NULL on error.
yyjson_mut_val *yyjson_mut_arr_add_arr(yyjson_mut_doc *doc, yyjson_mut_val *arr);

// Convenience API:
// Creates and adds a new object at the end of the array.
// Returns the new object, or NULL on error.
yyjson_mut_val *yyjson_mut_arr_add_obj(yyjson_mut_doc *doc, yyjson_mut_val *arr);
```

## JSON Object Creation
The following functions are used to create mutable JSON object.<br/>

```c
// Creates and returns a mutable object, returns NULL on error.
yyjson_mut_val *yyjson_mut_obj(yyjson_mut_doc *doc);

// Creates and returns a mutable object with keys and values,
// returns NULL on error. The keys and values are NOT copied.
// The strings should be encoded as UTF-8 with null-terminator.
yyjson_mut_val *yyjson_mut_obj_with_str(yyjson_mut_doc *doc,
                                        const char **keys,
                                        const char **vals,
                                        size_t count);
// sample code:
const char keys[] = {"name", "type", "id"};
const char *vals[] = {"Harry", "student", "123456"};
yyjson_mut_obj_with_str(doc, keys, vals, 3);

// Creates and returns a mutable object with key-value pairs,
// returns NULL on error. The keys and values are NOT copied.
// The strings should be encoded as UTF-8 with null-terminator.
yyjson_mut_val *yyjson_mut_obj_with_kv(yyjson_mut_doc *doc,
                                       const char **kv_pairs,
                                       size_t pair_count);
// sample code:
const char *pairs[] = {"name", "Harry", "type", "student", "id", "123456"};
yyjson_mut_obj_with_kv(doc, pairs, 3);
```

## JSON Object Modification
The following functions are used to modify the contents of a JSON object.<br/>

```c
// Adds a key-value pair at the end of the object. 
// The key must be a string value.
// This function allows duplicated key in one object.
bool yyjson_mut_obj_add(yyjson_mut_val *obj, yyjson_mut_val *key,yyjson_mut_val *val);

// Adds a key-value pair to the object.
// The key must be a string value.
// This function may remove all key-value pairs for the given key before add.
// Note that this function takes a linear search time.
bool yyjson_mut_obj_put(yyjson_mut_val *obj, yyjson_mut_val *key, yyjson_mut_val *val);

// Removes key-value pair from the object with a given key.
// Note that this function takes a linear search time.
bool yyjson_mut_obj_remove(yyjson_mut_val *obj, yyjson_mut_val *key);

// Removes all key-value pairs in this object.
bool yyjson_mut_obj_clear(yyjson_mut_val *obj);

// Convenience API:
// Adds a key-value pair at the end of the object. The key is not copied.
// Note that these functions allow duplicated key in one object.
bool yyjson_mut_obj_add_null(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_add_true(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_add_false(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_add_bool(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, bool val);
bool yyjson_mut_obj_add_uint(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, uint64_t val);
bool yyjson_mut_obj_add_sint(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int64_t val);
bool yyjson_mut_obj_add_int(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, int64_t val);
bool yyjson_mut_obj_add_real(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, double val);
bool yyjson_mut_obj_add_str(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val);
bool yyjson_mut_obj_add_strn(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val, size_t len);
bool yyjson_mut_obj_add_strcpy(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val);
bool yyjson_mut_obj_add_strncpy(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *val, size_t len);

// Convenience API:
// Removes all key-value pairs for the given key.
// Note that this function takes a linear search time.
bool yyjson_mut_obj_remove_str(yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_remove_strn(yyjson_mut_val *obj, const char *key, size_t len);

// Convenience API:
// Replaces all matching keys with the new key.
// Returns true if at least one key was renamed.
// This function takes a linear search time.
yyjson_api_inline bool yyjson_mut_obj_rename_key(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *new_key);
yyjson_api_inline bool yyjson_mut_obj_rename_keyn(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, size_t len, const char *new_key, size_t new_len);
```

## JSON Merge Patch
The library supports JSON Merge Patch (RFC 7386).
Specification and example: <https://tools.ietf.org/html/rfc7386>
```c
// Creates and returns a merge-patched JSON value.
// Returns NULL if the patch could not be applied.
yyjson_mut_val *yyjson_merge_patch(yyjson_mut_doc *doc,
                                   yyjson_val *orig,
                                   yyjson_val *patch);

yyjson_mut_val *yyjson_mut_merge_patch(yyjson_mut_doc *doc,
                                       yyjson_mut_val *orig,
                                       yyjson_mut_val *patch);
```


---------------
# Number Processing

## Number reader
The library has a built-in high-performance number reader,<br/>
it will parse numbers according to these rules by default:<br/>

* Read positive integer as `uint64_t`, if overflow, convert to `double`.
* Read negative integer as `int64_t`, if overflow, convert to `double`.
* Read floating-point number as `double` with correct rounding (no ulp error).
* If a `real` number overflow (infinity), it will report an error.
* If a number does not match the [JSON](https://www.json.org) standard, it will report an error.

You can use `YYJSON_READ_ALLOW_INF_AND_NAN` flag to allow `nan` and `inf` number/literal. You can also use `YYJSON_READ_NUMBER_AS_RAW` to read numbers as raw strings without parsing them. See `Reader flag` for details.

## Number writer
The library has a built-in high-performance number writer,<br/>
it will write numbers according to these rules by default:<br/>

* Write positive integer without sign.
* Write negative integer with a negative sign.
* Write floating-point number with [ECMAScript format](https://www.ecma-international.org/ecma-262/11.0/index.html#sec-numeric-types-number-tostring), but with the following changes:
    * If number is `Infinity` or `NaN`, report an error.
    * Keep the negative sign of 0.0 to preserve input information.
    * Remove positive sign of exponent part.
* Floating-point number writer should generate shortest correctly rounded decimal representation.

You can use `YYJSON_WRITE_ALLOW_INF_AND_NAN` flag to write inf/nan number as `Infinity` and `NaN` literals without error,
but this is not standard JSON. You can also use `YYJSON_WRITE_INF_AND_NAN_AS_NULL` to write inf/nan number as `null` literal. See `Writer flag` for details.



# Text Processing

## Character Encoding
The library only supports UTF-8 encoding without BOM, as specified in [RFC 8259](https://datatracker.ietf.org/doc/html/rfc8259#section-8.1):

> JSON text exchanged between systems that are not part of a closed ecosystem MUST be encoded using UTF-8.
> Implementations MUST NOT add a byte order mark (U+FEFF) to the beginning of a networked-transmitted JSON text.

By default, yyjson performs a strict UTF-8 encoding validation on input strings. An error will be reported when an invalid character is encountered.

You could use `YYJSON_READ_ALLOW_INVALID_UNICODE` and `YYJSON_WRITE_ALLOW_INVALID_UNICODE` flags to allow invalid unicode encoding. However, you should be aware that the result value from yyjson may contain invalid characters, which can be used by other code and may pose security risks.

## NUL Character
The library supports the `NUL` character (also known as `null terminator`, or Unicode `U+0000`, ASCII `\0`) inside strings.

When reading JSON, `\u0000` will be unescaped to `NUL`. If a string contains `NUL`, the length obtained with strlen() will be inaccurate, and you should use yyjson_get_len() to get the actual length.

When building JSON, the input string is treated as null-terminated. If you need to pass in a string with `NUL` inside, you should use the API with the `n` suffix and pass in the actual length.

For example:
```c
// null-terminated string
yyjson_mut_str(doc, str);
yyjson_obj_get(obj, str);

// any string, with or without null terminator
yyjson_mut_strn(doc, str, len);
yyjson_obj_getn(obj, str, len);

// C++ string
std::string sstr = ...;
yyjson_obj_getn(obj, sstr.data(), sstr.length());
```



# Memory Allocator
The library does not call libc's memory allocation functions (malloc/realloc/free) **directly**. Instead, when memory allocation is required, yyjson's API takes a parameter named `alc` that allows the caller to pass in an allocator. If the `alc` is NULL, yyjson will use the default memory allocator, which is a simple wrapper of libc's functions.

Custom memory allocator allows you to take more control over memory allocation, here are a few examples:

## Single allocator for multiple JSON
If you need to parse multiple small JSON, you can use a single allocator with pre-allocated buffer to avoid frequent memory allocation.

Sample code:
```c
// max data size for single JSON
size_t max_json_size = 64 * 1024;
// calculate the max memory usage for a single JSON
size_t buf_size = yyjson_read_max_memory_usage(max_json_size, 0);
// create a buffer for allocator
void *buf = malloc(buf_size);
// setup the allocator with buffer
yyjson_alc alc;
yyjson_alc_pool_init(&alc, buf, buf_size);

// read multiple JSON with single allocator
for(int i = 0, i < your_json_file_count; i++) {
    const char *your_json_file_path = ...;
    yyjson_doc *doc = yyjson_read_file(your_json_file_path, 0, &alc, NULL);
    ...
    yyjson_doc_free(doc);
}

// free the buffer
free(buf);
```

## Stack memory allocator
If the JSON is small enough, you can use stack memory to read or write it.

Sample code:
```c
char buf[128 * 1024]; // stack buffer
yyjson_alc alc;
yyjson_alc_pool_init(&alc, buf, sizeof(buf));

yyjson_doc *doc = yyjson_read_opts(dat, len, 0, &alc, NULL);
...
yyjson_doc_free(doc); // this is optional, as the memory is on stack
```

## Use third-party allocator library
You can use a third-party high-performance memory allocator for yyjson,<br/>
such as [jemalloc](https://github.com/jemalloc/jemalloc), [tcmalloc](https://github.com/google/tcmalloc), [mimalloc](https://github.com/microsoft/mimalloc).

Sample code:
```c
// Use https://github.com/microsoft/mimalloc

#include <mimalloc.h>

static void *priv_malloc(void *ctx, size_t size) {
    return mi_malloc(size);
}

static void *priv_realloc(void *ctx, void *ptr, size_t size) {
    return mi_realloc(ptr, size);
}

static void priv_free(void *ctx, void *ptr) {
    mi_free(ptr);
}

static const yyjson_alc PRIV_ALC = {
    priv_malloc,
    priv_realloc,
    priv_free,
    NULL
};

// Read with custom allocator
yyjson_doc *doc = yyjson_doc_read_opts(dat, len, 0, &PRIV_ALC, NULL);
...
yyjson_doc_free(doc);

// Write with custom allocator
yyjson_alc *alc = &PRIV_ALC;
char *json = yyjson_doc_write(doc, 0, alc, NULL, NULL);
...
alc->free(alc->ctx, json);

```




# Null Check
The library's public API will do `null check` for every input parameter to avoid crashes.

For example, when reading a JSON, you don't need to do null check or type check on each value:
```c
yyjson_doc *doc = yyjson_read(NULL, 0, 0); // doc is NULL
yyjson_val *val = yyjson_doc_get_root(doc); // val is NULL
const char *str = yyjson_get_str(val); // str is NULL
if (!str) printf("err!");
yyjson_doc_free(doc); // do nothing
```

But if you are sure that a value is non-null and the type is matched, you can use the `unsafe` prefix API to avoid the null check.

For example, when iterating over an array or object, the value and key must be non-null:
```c
size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    // this is a valid JSON, so the key must be a valid string
    if (unsafe_yyjson_equals_str(key, "id") &&
        unsafe_yyjson_is_uint(val) &&
        unsafe_yyjson_get_uint(val) == 1234) {
        ...
    }
}
```



# Thread Safe
The library does not use global variables, so if you can ensure that the input parameters of a function are thread-safe, then the function calls are also thread-safe.<br/>

Typically, `yyjson_doc` and `yyjson_val` are immutable and thread-safe, while `yyjson_mut_doc` and `yyjson_mut_val` are mutable and not thread-safe.



# Locale Dependent
The library is locale-independent.

However, there are some special conditions that you need to be aware of:

1. You use libc's `setlocale()` function to change locale.
2. Your environment does not use IEEE 754 floating-point standard (e.g. some IBM mainframes), or you explicitly set `YYJSON_DISABLE_FAST_FP_CONV` during build, in which case yyjson will use `strtod()` to parse floating-point numbers.

When you meet **both** of these conditions, you should avoid calling `setlocale()` while other thread is parsing JSON, otherwise an error may be returned for JSON floating point number parsing.
