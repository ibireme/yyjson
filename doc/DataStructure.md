Data Structures
===============


yyjson has 2 types of data structures: immutable and mutable.

When reading a JSON, yyjson returns immutable document and values;<br/>
When building a JSON, yyjson creates mutable document and values.<br/>
yyjson also provides some methods to convert immutable document into mutable document.<br/>
<br/>
Note that the data structures described in this document are private, and you should use public API to access them.


---------------
## Immutable Value
Each JSON value is stored in an immutable `yyjson_val` struct:
```c
struct yyjson_val {
    uint64_t tag;
    union {
        uint64_t    u64;
        int64_t     i64;
        double      f64;
        const char *str;
        void       *ptr;
        size_t      ofs;
    } uni;
}
```
![yyjson_val](images/struct_ival.svg)

The lower 8 bits of `tag` stores the type of value.<br/>
The higher 56 bits of `tag` stores the size of value (string length, object size or array size).

Modern 64-bit processors are typically limited to supporting fewer than 64 bits for RAM addresses ([Wikipedia](https://en.wikipedia.org/wiki/RAM_limit)). For example, Intel64, AMD64 and ARMv8 has a 52-bit (4PB) physical address limit. So we can safely store type and size into a 64 bits `tag`.

## Immutable Document
A JSON document stores all strings in a **contiguous** memory area.<br/> 
Each string is unescaped in-place and ended with a null-terminator.<br/>
For example:

![yyjson_val](images/struct_idoc1.svg)


A JSON document stores all values in another **contiguous** memory area.<br/>
The `object` and `array` stores their own memory usage, so we can easily walk through a container's child values.<br/>
For example:

![yyjson_val](images/struct_idoc2.svg)

---------------
## Mutable Value
Each mutable JSON value is stored in an `yyjson_mut_val` struct:
```c
struct yyjson_mut_val {
    uint64_t tag;
    union {
        uint64_t    u64;
        int64_t     i64;
        double      f64;
        const char *str;
        void       *ptr;
        size_t      ofs;
    } uni;
    yyjson_mut_val *next;
}
```
![yyjson_val](images/struct_mval.svg)

The `tag` and `uni` field is same as immutable value, the `next` field is used to build linked list.


## Mutable Document
A mutable JSON document is composed of multiple `yyjson_mut_val`.

The child values of an `object` or `array` are linked as a cycle,<br/>
the parent hold the **tail** of the linked list, so yyjson can do `append`, `prepend` and `remove_first` in O(1) time.

For example:

![yyjson_val](images/struct_mdoc.svg)