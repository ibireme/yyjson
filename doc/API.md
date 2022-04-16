# Table of Contents

* [Read JSON](#read-json)
    * [Read JSON from string](#read-json-from-string)
    * [Read JSON from file](#read-json-from-file)
    * [Read JSON with options](#read-json-with-options)
    * [Reader flag](#reader-flag)
* [Write JSON](#write-json)
    * [Write JSON to string](#write-json-to-string)
    * [Write JSON to file](#write-json-to-file)
    * [Write JSON with options](#write-json-with-options)
    * [Writer flag](#writer-flag)
* [Access JSON Document](#access-json-document)
    * [JSON Document](#json-document-api)
    * [JSON Value](#json-value-api)
    * [JSON Value Content](#json-value-content-api)
    * [JSON Array](#json-array-api)
    * [JSON Array Iterator](#json-array-iterator-api)
    * [JSON Object](#json-object-api)
    * [JSON Object Iterator](#json-object-iterator-api)
    * [JSON Pointer](#json-pointer)
* [Create JSON Document](#create-json-document)
    * [Mutable Document API](#mutable-document-api)
    * [Mutable JSON Value Creation](#mutable-json-value-creation-api)
    * [Mutable JSON Array Creation](#mutable-json-array-creation-api)
    * [Mutable JSON Array Modification](#mutable-json-array-modification-api)
    * [Mutable JSON Array Modification (Convenience)](#mutable-json-array-modification-convenience-api)
    * [Mutable JSON Object Creation](#mutable-json-object-creation-api)
    * [Mutable JSON Object Modification](#mutable-json-object-modification-api)
    * [Mutable JSON Object Modification (Convenience)](#mutable-json-object-modification-convenience-api)
    * [JSON Merge Path](#json-merge-path)
* [Number Processing](#number-processing)
    * [Number Reader](#number-reader)
    * [Number Writer](#number-writer)
* [Memory Allocator](#memory-allocator)
    * [Single allocator for multiple JSON](#single-allocator-for-multiple-json)
    * [Stack memory allocator](#stack-memory-allocator)
    * [Use third-party allocator library](#use-third-party-allocator-library)
* [Mutable and Immutable](#mutable-and-immutable)
* [Null Check](#null-check)
* [Thread Safe](#thread-safe)
* [Locale Dependent](#locale-dependent)


---------------
# Read JSON
yyjson provides 3 methods for reading JSON,<br/>
each method accepts an input of UTF-8 data or file,<br/>
returns a document if succeeds, or returns NULL if fails.

### Read JSON from string
The `dat` should be a UTF-8 string, null-terminator is not required.<br/>
The `len` is the byte length of `dat`.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see [reader flag](#reader-flag) for details.<br/>
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

### Read JSON from file

The `path` is JSON file path.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see [reader flag](#reader-flag) for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see [memory allocator](#memory-allocator) for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>
If input is invalid, `NULL` is returned.

```c
yyjson_doc *yyjson_read_file(const char *path,
                             yyjson_read_flag flg,
                             yyjson_alc *alc,
                             yyjson_read_err *err);
```

Sample code:

```c
yyjson_doc *doc = yyjson_read_file("/tmp/test.json", 0, NULL, NULL);
if (doc) {...}
yyjson_doc_free(doc);
```

### Read JSON with options
The `dat` should be a UTF-8 string, you can pass a const string if you don't use `YYJSON_READ_INSITU` flag.<br/>
The `len` is the `dat`'s length in bytes.<br/>
The `flg` is reader flag, pass 0 if you don't need it, see [reader flag](#reader-flag) for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see [memory allocator](#memory-allocator) for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>

```c
yyjson_doc *yyjson_read_opts(char *dat, 
                             size_t len, 
                             yyjson_read_flag flg,
                             yyjson_alc *alc, 
                             yyjson_read_err *err);
```

Sample code:

```c
const char *dat = your_file.bytes;
size_t len = your_file.size;

yyjson_read_flag flg = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_INF_AND_NAN;
yyjson_err err;
yyjson_doc *doc = yyjson_read_opts(dat, len, flg, NULL, &err);

if (doc) {...}
else printf("read error: %s code: %u at position: %ld\n", err.msg, err.code, err.pos);

yyjson_doc_free(doc);
```

### Reader flag
yyjson provides a set of flags for JSON reader.<br/>
You can use a single flag, or combine multiple flags with bitwise `|` operator.

●**YYJSON_READ_NOFLAG = 0**<br/>

This is the default flag for JSON reader (RFC-8259 or ECMA-404 compliant):

- Read positive integer as `uint64_t`.
- Read negative integer as `int64_t`.
- Read floating-point number as `double` with correct rounding.
- Read integer which cannot fit in `uint64_t` or `int64_t` as `double`.
- Report error if real number is infinity.
- Report error if string contains invalid UTF-8 character or BOM.
- Report error on trailing commas, comments, `inf` and `nan` literals.

●**YYJSON_READ_INSITU**<br/>
Read the input data in-situ.<br/>
This option allows the reader to modify and use input data to store string values, which can increase reading speed slightly. The caller should hold the input data before free the document. The input data must be padded by at least `YYJSON_PADDING_SIZE` byte. For example: "[1,2]" should be "[1,2]\0\0\0\0", length should be 5.

Sample code:

```c
size_t dat_len = ...;
char *buf = malloc(dat_len + 4); // create a buffer larger than (len + 4)
read_from_socket(buf, ...);
memset(buf + file_size, 0, 4); // set 4-byte padding after data

yyjson_doc *doc = yyjson_read_opts(buf, dat_len, YYJSON_READ_INSITU, NULL, NULL);
if (doc) {...}
yyjson_doc_free(doc);
free(buf); // the input dat should free after document.
```

●**YYJSON_READ_STOP_WHEN_DONE**<br/>
Stop when done instead of issues an error if there's additional content after a JSON document.<br/> 
This option may used to parse small pieces of JSON in larger data, such as [NDJSON](https://en.wikipedia.org/wiki/JSON_streaming).<br/>

Sample code:

```c
// single file with multiple json, such as:
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

●**YYJSON_READ_ALLOW_TRAILING_COMMAS**<br/>
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

●**YYJSON_READ_ALLOW_COMMENTS**<br/>
Allow C-style single line and multiple line comments, for example:

```
{
    "name": "Harry", // single line comment
    "id": /* multiple line comment */ 123
}
```

●**YYJSON_READ_ALLOW_INF_AND_NAN**<br/>
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


●**YYJSON_READ_NUMBER_AS_RAW**<br/>
Read numbers as raw strings without parsing, allowing you to keep arbitrarily large numbers. 

You can use these functions to extract raw strings:
```
bool yyjson_is_raw(yyjson_val *val);
const char *yyjson_get_raw(yyjson_val *val);
size_t yyjson_get_len(yyjson_val *val)
```


---------------
# Write JSON
yyjson provides 3 methods for writing JSON,<br/>
each method accepts an input of JSON document or root value, returns a UTF-8 string or file.

### Write JSON to string
The `doc/val` is JSON document or root value, if you pass NULL, you will get NULL result.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see [writer flag](#writer-flag) for details.<br/>
The `len` is a pointer to receive output length, pass NULL if you don't need it.<br/>
This function returns a new JSON string, or NULL if error occurs.<br/>
The string is encoded as UTF-8 with a null-terminator. <br/>
You should use free() or alc->free() to release it when it's no longer needed.

```c
char *yyjson_write(const yyjson_doc *doc, yyjson_write_flag flg, size_t *len);

char *yyjson_mut_write(const yyjson_mut_doc *doc, yyjson_write_flag flg, size_t *len);

char *yyjson_val_write(const yyjson_val *val, yyjson_write_flag flg, size_t *len);

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

### Write JSON to file
The `path` is output JSON file path, If the path is invalid, you will get an error. If the file is not empty, the content will be discarded.<br/>
The `doc/val` is JSON document or root value, if you pass NULL, you will get an error.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see [writer flag](#writer-flag) for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see [memory allocator](#memory-allocator) for details.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>
This function returns true on success, or false if error occurs.<br/>

```c
bool yyjson_write_file(const char *path,
                       const yyjson_doc *doc,
                       yyjson_write_flag flg,
                       const yyjson_alc *alc,
                       yyjson_write_err *err);

bool yyjson_mut_write_file(const char *path,
                           const yyjson_mut_doc *doc,
                           yyjson_write_flag flg,
                           const yyjson_alc *alc,
                           yyjson_write_err *err);

bool yyjson_val_write_file(const char *path,
                           const yyjson_val *val,
                           yyjson_write_flag flg,
                           const yyjson_alc *alc,
                           yyjson_write_err *err);

bool yyjson_mut_val_write_file(const char *path,
                               const yyjson_mut_val *val,
                               yyjson_write_flag flg,
                               const yyjson_alc *alc,
                               yyjson_write_err *err);
```

Sample code:

```c
yyjson_doc *doc = yyjson_read_file("/tmp/test.json", 0, NULL, NULL);
bool suc = yyjson_write_file("tmp/test.json", doc, YYJSON_WRITE_PRETTY, NULL, NULL);
if (suc) printf("OK");
```

### Write JSON with options
The `doc/val` is JSON document or root value, if you pass NULL, you will get NULL result.<br/>
The `flg` is writer flag, pass 0 if you don't need it, see [writer flag](#writer-flag) for details.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see [memory allocator](#memory-allocator) for details.<br/>
The `len` is a pointer to receive output length, pass NULL if you don't need it.<br/>
The `err` is a pointer to receive error message, pass NULL if you don't need it.<br/>

This function returns a new JSON string, or NULL if error occurs.<br/>
The string is encoded as UTF-8 with a null-terminator. <br/>
You should use free() or alc->free() to release it when it's no longer needed.

```c
char *yyjson_write_opts(const yyjson_doc *doc,
                        yyjson_write_flag flg,
                        const yyjson_alc *alc,
                        size_t *len,
                        yyjson_write_err *err);

char *yyjson_mut_write_opts(const yyjson_mut_doc *doc,
                            yyjson_write_flag flg,
                            const yyjson_alc *alc,
                            size_t *len,
                            yyjson_write_err *err);

char *yyjson_val_write_opts(const yyjson_val *val,
                            yyjson_write_flag flg,
                            const yyjson_alc *alc,
                            size_t *len,
                            yyjson_write_err *err);

char *yyjson_mut_val_write_opts(const yyjson_mut_val *val,
                                yyjson_write_flag flg,
                                const yyjson_alc *alc,
                                size_t *len,
                                yyjson_write_err *err);
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


### Writer flag
yyjson provides a set of flags for JSON writer.<br/>
You can use a single flag, or combine multiple flags with bitwise `|` operator.

●**YYJSON_WRITE_NOFLAG = 0**<br/>
This is the default flag for JSON writer:

- Write JSON minify.
- Report error on inf or nan number.
- Do not validate string encoding.
- Do not escape unicode or slash. 

●**YYJSON_WRITE_PRETTY**<br/>
Write JSON pretty with 4 space indent.

●**YYJSON_WRITE_ESCAPE_UNICODE**<br/>
Escape unicode as `\uXXXX`, make the output ASCII only, for example:

```json
["Alizée, 😊"]
["Aliz\\u00E9e, \\uD83D\\uDE0A"]
```

●**YYJSON_WRITE_ESCAPE_SLASHES**<br/>
Escape `/` as `\/`, for example:

```json
["https://github.com"]
["https:\/\/github.com"]
```

●**YYJSON_WRITE_ALLOW_INF_AND_NAN**<br/>
Write inf/nan number as `Infinity` and `NaN` literals.<br/>
Note that this output is **NOT** standard JSON and may be rejected by other JSON libraries, for example:

```js
{"not a number":NaN,"large number":Infinity}
```

●**YYJSON_WRITE_INF_AND_NAN_AS_NULL**<br/>
Write inf/nan number as `null` literal.<br/>
This flag will override `YYJSON_WRITE_ALLOW_INF_AND_NAN` flag, for example:

```js
{"not a number":null,"large number":null}
```


---------------
# Access JSON Document

### JSON Document API

Returns the root value of this JSON document.
```c
yyjson_val *yyjson_doc_get_root(yyjson_doc *doc);
```

Returns read size of input JSON data.
```c
size_t yyjson_doc_get_read_size(yyjson_doc *doc);
```

Returns total value count in this JSON document.
```c
size_t yyjson_doc_get_val_count(yyjson_doc *doc);
```

Release the JSON document and free the memory.
```c
void yyjson_doc_free(yyjson_doc *doc);
```

### JSON Value API
Returns true if the JSON value is specified type.<br/>
Returns false if the input is `NULL` or not the specified type.<br/>

This set of APIs also have version for mutable values, see [mutable and immutable](#mutable-and-immutable) for details.

```c
bool yyjson_is_null(yyjson_val *val); // null
bool yyjson_is_true(yyjson_val *val); // true
bool yyjson_is_false(yyjson_val *val); // false
bool yyjson_is_bool(yyjson_val *val); // true or false
bool yyjson_is_uint(yyjson_val *val); // unsigned integer
bool yyjson_is_sint(yyjson_val *val); // signed integer
bool yyjson_is_int(yyjson_val *val); // unsigned or signed integer
bool yyjson_is_real(yyjson_val *val); // double number
bool yyjson_is_num(yyjson_val *val); // integer or double number
bool yyjson_is_str(yyjson_val *val); // string
bool yyjson_is_arr(yyjson_val *val); // array
bool yyjson_is_obj(yyjson_val *val); // object
bool yyjson_is_ctn(yyjson_val *val); // array or object
```



### JSON Value Content API
Returns the content or type of a JSON value.<br/>
This set of APIs also have version for mutable values, see [mutable and immutable](#mutable-and-immutable) for details.

<br/>

Returns value's type.
```c
yyjson_type yyjson_get_type(yyjson_val *val);
```

Returns value's subtype.
```c
yyjson_subtype yyjson_get_subtype(yyjson_val *val);
```

Returns value's tag.
```c
uint8_t yyjson_get_tag(yyjson_val *val);
```

Returns type description, such as:  "null", "string",
"array", "object", "true", "false", "uint", "sint", "real", "unknown"
```c
const char *yyjson_get_type_desc(yyjson_val *val);
```
Returns bool value, or false if the value is not bool type.
```c
bool yyjson_get_bool(yyjson_val *val);
```
Returns uint value, or 0 if the value is not uint type.
```c
uint64_t yyjson_get_uint(yyjson_val *val);
```
Returns sint value, or 0 if the value is not sint type.
```c
int64_t yyjson_get_sint(yyjson_val *val);
```
Returns int value (uint may overflow), or 0 if the value is not uint or sint type.
```c
int yyjson_get_int(yyjson_val *val);
```
Returns double value, or 0 if the value is not real type.
```c
double yyjson_get_real(yyjson_val *val);
```

Returns the string value, or NULL if the value is not string type
```c
const char *yyjson_get_str(yyjson_val *val);
```
Returns the string's length, or 0 if the value is not string type.
```c
size_t yyjson_get_len(yyjson_val *val);
```

Returns whether the value is equals to a string.
```c
bool yyjson_equals_str(yyjson_val *val, const char *str);
```

Same as `yyjson_equals_str(), but you can pass an explicit string length.
```c
bool yyjson_equals_strn(yyjson_val *val, const char *str, size_t len);
```

### JSON Array API
Returns the property or child value of a JSON array.<br/>
This set of APIs also have version for mutable values, see [mutable and immutable](#mutable-and-immutable) for details.

<br/>

Returns the number of elements in this array, or 0 if the input is not an array.
```c
size_t yyjson_arr_size(yyjson_val *arr);
```

Returns the element at the specified position in this array, or NULL if array is empty or the index is out of bounds.<br/>
Note that this function takes a **linear search time** if array is not flat.
```c
yyjson_val *yyjson_arr_get(yyjson_val *arr, size_t idx);
```

Returns the first element of this array, or NULL if array is empty.
```c
yyjson_val *yyjson_arr_get_first(yyjson_val *arr);
```

Returns the last element of this array, or NULL if array is empty.<br/>
Note tha this function takes a **linear search time** if array is not flat.
```c
yyjson_val *yyjson_arr_get_last(yyjson_val *arr);
```

### JSON Array Iterator API
You can use two methods to traverse an array:<br/>

Sample code (iterator):
```c
yyjson_val *arr; // this is your array

yyjson_val *val;
yyjson_arr_iter iter;
yyjson_arr_iter_init(arr, &iter);
while ((val = yyjson_arr_iter_next(&iter))) {
    print(val);
}
```

Sample code (foreach):
```c
yyjson_val *arr; // this is your array

size_t idx, max;
yyjson_val *val;
yyjson_arr_foreach(arr, idx, max, val) {
    print(idx, val);
}
```
<br/>
There's also mutable version API to traverse an mutable array:

Sample code (mutable iterator):
```c
yyjson_mut_val *arr; // this is your mutable array

yyjson_mut_val *val;
yyjson_mut_arr_iter iter;
yyjson_mut_arr_iter_init(arr, &iter);
while ((val = yyjson_mut_arr_iter_next(&iter))) {
    if (val_is_unused(val)) {
        // remove current value inside iteration
        yyjson_mut_arr_iter_remove(&iter); 
    }
}
```

Sample code (mutable foreach):
```c
yyjson_mut_val *arr; // this is your mutable array

size_t idx, max;
yyjson_mut_val *val;
yyjson_mut_arr_foreach(arr, idx, max, val) {
    print(idx, val);
}
```


### JSON Object API
Returns the property or child value of a JSON object.<br/>
This set of APIs also have version for mutable values, see [mutable and immutable](#mutable-and-immutable) for details.

<br/>

Returns the number of key-value pairs in this object, or 0 if input is not an object.
```c
size_t yyjson_obj_size(yyjson_val *obj);
```

Returns the value to which the specified key is mapped, <br/>
or NULL if this object contains no mapping for the key.<br/>
Note that this function takes a **linear search time**.
```c
yyjson_val *yyjson_obj_get(yyjson_val *obj, const char *key);
```

Same as `yyjson_obj_get(), but you can pass an explicit string length.
```c
yyjson_val *yyjson_obj_getn(yyjson_val *obj, const char *key, size_t key_len);
```

If the order of object's key is known at compile time, you can use this method to avoid searching the entire object:
```c
// { "x":1, "y":2, "z":3 }
yyjson_val *obj = ...;
yyjson_obj_iter iter;
if (yyjson_obj_iter_init(obj, &iter)) {
    yyjson_val *x = yyjson_obj_iter_get(&iter, "x");
    yyjson_val *y = yyjson_obj_iter_get(&iter, "y");
    yyjson_val *z = yyjson_obj_iter_get(&iter, "z");
}
```

### JSON Object Iterator API
You can use two methods to traverse an object:<br/>

Sample code (iterator):
```c
yyjson_val *obj; // this is your object

yyjson_val *key, *val;
yyjson_obj_iter iter;
yyjson_obj_iter_init(obj, &iter);
while ((key = yyjson_obj_iter_next(&iter))) {
    val = yyjson_obj_iter_get_val(key);
    print(key, val);
}
```

Sample code (foreach):
```c
yyjson_val *obj; // this is your object

size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    print(key, val);
}
```
<br/>

There's also mutable version API to traverse an mutable object:

Sample code (mutable iterator):
```c
yyjson_mut_val *obj; // this is your mutable object

yyjson_mut_val *key, *val;
yyjson_mut_obj_iter iter;
yyjson_mut_obj_iter_init(obj, &iter);
while ((key = yyjson_mut_obj_iter_next(&iter))) {
    val = yyjson_mut_obj_iter_get_val(key);
    if (key_is_unused(key)) {
        // remove current key-value pair inside iteration
        yyjson_mut_obj_iter_remove(&iter);
    }
}
```

Sample code (mutable foreach):
```c
yyjson_mut_val *obj; // this is your mutable object

size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    print(key, val);
}
```

### JSON Pointer
`yyjson` allows you to query JSON value with `JSON Pointer` ([RFC 6901](https://tools.ietf.org/html/rfc6901)).

```c
yyjson_val *yyjson_get_pointer(yyjson_val *val, const char *pointer);
yyjson_val *yyjson_doc_get_pointer(yyjson_doc *doc, const char *pointer);
yyjson_mut_val *yyjson_mut_get_pointer(yyjson_mut_val *val, const char *pointer);
yyjson_mut_val *yyjson_mut_doc_get_pointer(yyjson_mut_doc *doc, const char *pointer);
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
| `"/none"` | NULL | 


---------------
# Create JSON Document
You can use a `yyjson_mut_doc` to build your JSON document.<br/>

Notice that `yyjson_mut_doc` use a **memory pool** to hold all strings and values; the pool can only be created, grown or freed in its entirety. Thus `yyjson_mut_doc` is more suitable for write-once, than mutation of an existing document.

Sample code:

```c
// Create a mutable document.
yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);

// Create a `uint` value, the value's memory is held by doc.
yyjson_mut_val *val = yyjson_mut_uint(doc, 100);

// Create an array, the array's memory is held by doc.
yyjson_mut_val *arr1 = yyjson_mut_arr(doc);
yyjson_mut_val *arr2 = yyjson_mut_arr(doc);

// Add the value to arr1
yyjson_mut_arr_append(arr1, val);

// ❌ Wrong, the value is already added to another container.
yyjson_mut_arr_append(arr2, val);

// Free the memory of doc and all values which is created from this doc.
yyjson_mut_doc_free(doc);
```


### Mutable Document API

Creates and returns a new mutable JSON document, returns NULL on error.<br/>
If allocator is NULL, the default allocator will be used.
The `alc` is memory allocator, pass NULL if you don't need it, see [memory allocator](#memory-allocator) for details.<br/>
```c
yyjson_mut_doc *yyjson_mut_doc_new(yyjson_alc *alc);
```

Delete the JSON document, free the memory of this doc (and all values created from this doc).
```c
void yyjson_mut_doc_free(yyjson_mut_doc *doc);
```

Get or set the root value of this JSON document.
```c
yyjson_mut_val *yyjson_mut_doc_get_root(yyjson_mut_doc *doc);
void yyjson_mut_doc_set_root(yyjson_mut_doc *doc, yyjson_mut_val *root);
```

Copies and returns a new mutable document from input, returns NULL on error.<br/>
The `alc` is memory allocator, pass NULL if you don't need it, see [memory allocator](#memory-allocator) for details.<br/>
```c
yyjson_mut_doc *yyjson_doc_mut_copy(yyjson_doc *doc, yyjson_alc *alc);
```

Copies and returns a new mutable value from input, returns NULL on error.<br/>
The memory was managed by document. */
```c
yyjson_mut_val *yyjson_val_mut_copy(yyjson_mut_doc *doc, yyjson_val *val);
```

### Mutable JSON Value Creation API
You can use these functions to create mutable JSON value,<br/>
The value's memory is held by the document.<br/>
<br/>

Creates and returns a null value, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_null(yyjson_mut_doc *doc);
```

Creates and returns a true value, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_true(yyjson_mut_doc *doc);
```

Creates and returns a false value, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_false(yyjson_mut_doc *doc);
```

Creates and returns a bool value, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_bool(yyjson_mut_doc *doc, bool val);
```

Creates and returns an unsigned integer value, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_uint(yyjson_mut_doc *doc, uint64_t num);
```

Creates and returns a signed integer value, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_sint(yyjson_mut_doc *doc, int64_t num);
```

Creates and returns a signed integer value, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_int(yyjson_mut_doc *doc, int64_t num);
```

Creates and returns an real number value, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_real(yyjson_mut_doc *doc, double num);
```

Creates and returns a string value, returns NULL on error.<br/>
The input value should be a valid UTF-8 encoded string with null-terminator.<br/>
Note that the input string is **NOT** copied.
```c
yyjson_mut_val *yyjson_mut_str(yyjson_mut_doc *doc, const char *str);
```

Creates and returns a string value, returns NULL on error.<br/>
The input value should be a valid UTF-8 encoded string.<br/>
Note that the input string is **NOT** copied.
```c
yyjson_mut_val *yyjson_mut_strn(yyjson_mut_doc *doc, const char *str, size_t len);
```

Creates and returns a string value, returns NULL on error.<br/>
The input value should be a valid UTF-8 encoded string with null-terminator.<br/>
The input string is copied and held by the document.
```c
yyjson_mut_val *yyjson_mut_strcpy(yyjson_mut_doc *doc, const char *str);
```

Creates and returns a string value, returns NULL on error.<br/>
The input value should be a valid UTF-8 encoded string.<br/>
The input string is copied and held by the document.
```c
yyjson_mut_val *yyjson_mut_strncpy(yyjson_mut_doc *doc, const char *str, size_t len);
```


### Mutable JSON Array Creation API

Creates and returns an empty mutable array, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_arr(yyjson_mut_doc *doc);
```
Creates and returns a mutable array with c array.
```c
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

// sample:
int vals[3] = {-1, 0, 1};
yyjson_mut_val *arr = yyjson_mut_arr_with_sint32(doc, vals, 3);
```

Creates and returns a mutable array with strings.
The strings should be encoded as UTF-8.
```c
yyjson_mut_val *yyjson_mut_arr_with_str(yyjson_mut_doc *doc, const char **vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strn(yyjson_mut_doc *doc, const char **vals, const size_t *lens, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strcpy(yyjson_mut_doc *doc, const char **vals, size_t count);
yyjson_mut_val *yyjson_mut_arr_with_strncpy(yyjson_mut_doc *doc, const char **vals, const size_t *lens, size_t count);

// sample:
const char strs[3] = {"Jan", "Feb", "Mar"};
yyjson_mut_val *arr = yyjson_mut_arr_with_str(doc, strs, 3);
```

### Mutable JSON Array Modification API

Inserts a value into an array at a given index, returns false on error.<br/>
Note that Tthis function takes a **linear search time**.
```c
bool yyjson_mut_arr_insert(yyjson_mut_val *arr, yyjson_mut_val *val, size_t idx);
```

Inserts a val at the end of the array, returns false on error.
```c
bool yyjson_mut_arr_append(yyjson_mut_val *arr, yyjson_mut_val *val);
```

Inserts a val at the head of the array, returns false on error.
```c
bool yyjson_mut_arr_prepend(yyjson_mut_val *arr, yyjson_mut_val *val);
```

Replaces a value at index and returns old value, returns NULL on error.<br/>
Note that this function takes a **linear search time**.
```c
yyjson_mut_val *yyjson_mut_arr_replace(yyjson_mut_val *arr, size_t idx, yyjson_mut_val *val);
```

Removes and returns a value at index, returns NULL on error.<br/>
Note that this function takes a **linear search time**.
```c
yyjson_mut_val *yyjson_mut_arr_remove(yyjson_mut_val *arr, size_t idx);
```

Removes and returns the first value in this array, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_arr_remove_first(yyjson_mut_val *arr);
```

Removes and returns the last value in this array, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_arr_remove_last(yyjson_mut_val *arr);
```

Removes all values within a specified range in the array.<br/>
Note that this function takes a **linear search time**.
```c
bool yyjson_mut_arr_remove_range(yyjson_mut_val *arr, size_t idx, size_t len);
```

Removes all values in this array.
```c
bool yyjson_mut_arr_clear(yyjson_mut_val *arr);
```

### Mutable JSON Array Modification Convenience API

Adds a value at the end of this array, returns false on error.
```c
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
```

Creates and adds a new array at the end of the array.<br/>
Returns the new array, or NULL on error.
```c
yyjson_mut_val *yyjson_mut_arr_add_arr(yyjson_mut_doc *doc, yyjson_mut_val *arr);
```

Creates and adds a new object at the end of the array.<br/>
Returns the new object, or NULL on error.
```c
yyjson_mut_val *yyjson_mut_arr_add_obj(yyjson_mut_doc *doc, yyjson_mut_val *arr);
```

### Mutable JSON Object Creation API

Creates and returns a mutable object, returns NULL on error.
```c
yyjson_mut_val *yyjson_mut_obj(yyjson_mut_doc *doc);
```

Creates and returns a mutable object with keys and values,<br/>
returns NULL on error. The keys and values are **NOT** copied.<br/>
The strings should be encoded as UTF-8 with null-terminator.
```c
yyjson_mut_val *yyjson_mut_obj_with_str(yyjson_mut_doc *doc,
                                        const char **keys,
                                        const char **vals,
                                        size_t count);
// sample:
const char vkeys[] = {"name", "type", "id"};
const char *vals[] = {"Harry", "student", "888999"};
yyjson_mut_obj_with_str(doc, keys, vals, 3);
```

Creates and returns a mutable object with key-value pairs and pair count,<br/>
returns NULL on error. The keys and values are **NOT** copied.<br/>
The strings should be encoded as UTF-8 with null-terminator.
```c
yyjson_mut_val *yyjson_mut_obj_with_kv(yyjson_mut_doc *doc,
                                       const char **kv_pairs,
                                       size_t pair_count);
// sample:
const char *pairs[] = {"name", "Harry", "type", "student", "id", "888999"};
yyjson_mut_obj_with_kv(doc, pairs, 3);
```

### Mutable JSON Object Modification API
Adds a key-value pair at the end of the object. The key must be a string.<br/>
This function allows duplicated key in one object.
```
bool yyjson_mut_obj_add(yyjson_mut_val *obj, yyjson_mut_val *key,yyjson_mut_val *val);
```

Adds a key-value pair to the object, The key must be a string.<br/>
This function may remove all key-value pairs for the given key before add.<br/>
Note that this function takes a **linear search time**.
```c
bool yyjson_mut_obj_put(yyjson_mut_val *obj, yyjson_mut_val *key, yyjson_mut_val *val);
```

Removes key-value pair from the object with given key.<br/>
Note that this function takes a **linear search time**.
```c
bool yyjson_mut_obj_remove(yyjson_mut_val *obj, yyjson_mut_val *key);
```

Removes all key-value pairs in this object.
```c
bool yyjson_mut_obj_clear(yyjson_mut_val *obj);
```

### Mutable JSON Object Modification Convenience API
Adds a key-value pair at the end of the object. The key is not copied.<br/>
Note that these functions allow duplicated key in one object.

```c
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
```

Removes all key-value pairs for the given key.<br/>
Note that this function takes a **linear search time**.
```c
bool yyjson_mut_obj_remove_str(yyjson_mut_val *obj, const char *key);
bool yyjson_mut_obj_remove_strn(yyjson_mut_val *obj, const char *key, size_t len);
```

### JSON Merge Path
Creates and returns a merge-patched JSON value (RFC 7386).
Returns NULL if the patch could not be applied.
Specification and example: <https://tools.ietf.org/html/rfc7386>
```c
yyjson_mut_val *yyjson_merge_patch(yyjson_mut_doc *doc,
                                   yyjson_val *orig,
                                   yyjson_val *patch);
```


---------------
# Number Processing

### Number reader
yyjson has a built-in high-performance number reader,<br/>
it will parse numbers according to these rules by default:<br/>

* Read positive integer as `uint64_t`, if overflow, convert to `double`.
* Read negative integer as `int64_t`, if overflow, convert to `double`.
* Read floating-point number as `double` with correct rounding (no ulp error).
* If a `real` number overflow (infinity), it will report an error.
* If a number does not match the [JSON](https://www.json.org) standard, it will report an error.

You can use `YYJSON_READ_ALLOW_INF_AND_NAN` flag to allow `nan` and `inf` number/literal, see [reader flag](#reader-flag) for details.

See [reader flag](#reader-flag) for details.

### Number writer
yyjson has a built-in high-performance number writer,<br/>
it will write numbers according to these rules by default:<br/>

* Write positive integer without sign.
* Write negative integer with a negative sign.
* Write floating-point number with [ECMAScript format](https://www.ecma-international.org/ecma-262/11.0/index.html#sec-numeric-types-number-tostring), but with the following changes:
    * If number is `Infinity` or `NaN`, report an error.
    * Keep the negative sign of 0.0 to preserve input information.
    * Remove positive sign of exponent part.
* Floating-point number writer should generate shortest correctly rounded decimal representation.

You can use `YYJSON_WRITE_ALLOW_INF_AND_NAN` flag to write inf/nan number as `Infinity` and `NaN` literals without error,
but this is not standard JSON, see [writer flag](#writer-flag) for details.

You can also use `YYJSON_WRITE_INF_AND_NAN_AS_NULL` to write inf/nan number as `null` without error.



# Memory Allocator
yyjson use libc's `malloc()`, `realloc()` and `free()` as default allocator, but yyjson allows you to customize the memory allocator to achieve better performance or lower memory usage.

### Single allocator for multiple JSON
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

### Stack memory allocator
If the JSON is small enough, you can use stack memory only to read or write it.

Sample code:
```c
char buf[128 * 1024]; // stack buffer
yyjson_alc alc;
yyjson_alc_pool_init(&alc, buf, sizeof(buf));

yyjson_doc *doc = yyjson_read_opts(dat, len, 0, &alc, NULL);
...
yyjson_doc_free(doc); // this is optional, as the memory is on stack
```

### Use third-party allocator library
You can use a third-party high-performance memory allocator for yyjson,<br/>
such as [jemalloc](https://github.com/jemalloc/jemalloc), [tcmalloc](https://github.com/google/tcmalloc), [mimalloc](https://github.com/microsoft/mimalloc).

Sample code:
```c
// Use https://github.com/microsoft/mimalloc

#include <mimalloc.h>

static void *my_malloc_func(void *ctx, size_t size) {
    return mi_malloc(size);
}

static void *my_realloc_func(void *ctx, void *ptr, size_t size) {
    return mi_realloc(ptr, size);
}

static void my_free_func(void *ctx, void *ptr) {
    mi_free(ptr);
}

static const yyjson_alc MY_ALC = {
    my_malloc_func,
    my_realloc_func,
    my_free_func,
    NULL
};

// Read with custom allocator
yyjson_doc *doc = yyjson_doc_read_opts(dat, len, 0, &MY_ALC, NULL);
...
yyjson_doc_free(doc);

// Write with custom allocator
yyjson_alc *alc = &MY_ALC;
char *json = yyjson_doc_write(doc, 0, alc, NULL, NULL);
...
alc->free(alc->ctx, json);

```



# Mutable and Immutable
yyjson have 2 types of data structures: immutable and mutable:

|type|immutable|mutable|
|---|---|---|
|JSON value|yyjson_val|yyjson_mut_val|
|JSON document|yyjson_doc|yyjson_mut_doc|

When reading a JSON, yyjson returns immutable document and values;<br/>
When building a JSON, yyjson creates mutable document and values.<br/>
yyjson also provides some methods to convert immutable document into mutable document:<br/>

```c
yyjson_mut_doc *yyjson_doc_mut_copy(yyjson_doc *doc, yyjson_alc *alc);
yyjson_mut_val *yyjson_val_mut_copy(yyjson_mut_doc *doc, yyjson_val *val);
```

For most immutable APIs, you can just add a `mut` prefix to get the mutable version,
for example:

```c
char *yyjson_write(yyjson_doc *doc, yyjson_write_flag flg, size_t *len);
char *yyjson_mut_write(yyjson_mut_doc *doc, yyjson_write_flag flg, size_t *len);

bool yyjson_is_str(yyjson_val *val);
bool yyjson_mut_is_str(yyjson_mut_val *val);

yyjson_type yyjson_get_type(yyjson_val *val);
yyjson_type yyjson_mut_get_type(yyjson_mut_val *val);
```

See [data structure](https://github.com/ibireme/yyjson/blob/master/doc/DataStructure.md) for more details.



# Null Check
`yyjson`'s public API will do `null check` for every input parameters to avoid crashes.

For example, when reading a JSON, you don't need to do null check or type check on each value:
```c
yyjson_doc *doc = yyjson_read(NULL, 0, 0); // doc is NULL
yyjson_val *val = yyjson_doc_get_root(doc); // val is NULL
const char *str = yyjson_get_str(val); // str is NULL
if (!str) printf("err!");
yyjson_doc_free(doc); // do nothing
```

But if you are sure that a value is non-null, and the type is matched, you can use the `unsafe` prefix API to avoid the null check.

For example, when iterating over an array or object, the value and key must be non-null:
```c
size_t idx, max;
yyjson_val *key, *val;
yyjson_obj_foreach(obj, idx, max, key, val) {
    // this is a valid JSON, so the key must be a non-null string
    if (unsafe_yyjson_equals_str(key, "id") &&
        unsafe_yyjson_is_uint(val) &&
        unsafe_yyjson_get_uint(val) == 1234) {
        // found
    }
}
```



# Thread Safe
yyjson does not use global variables, so if you can ensure that the input parameters of a function are immutable, then the function call is thread-safe.<br/>

`yyjson_doc` and `yyjson_val` is immutable and thread-safe,<br/>
`yyjson_mut_doc` and `yyjson_mut_val` is mutable and not thread-safe.



# Locale Dependent
yyjson is locale-independent.

However, there are some special conditions that you need to be aware of:

1. You use libc's `setlocale()` function to change locale.
2. Your environment does not use IEEE 754 floating-point (e.g. some IBM mainframes) or you explicitly specified the `YYJSON_DISABLE_FAST_FP_CONV` flag at build time.

When you meet both of these conditions, you should avoid call `setlocale()` while other thread is parsing JSON, otherwise an error may be returned for JSON floating point number parsing.
