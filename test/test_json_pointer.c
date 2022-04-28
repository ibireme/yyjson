#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_READER

// `num` 0: should not match, otherwise match a number value
static void validate_val_with_len(const char *json, const char *ptr, usize ptr_len, int num) {
    usize json_len = strlen(json);
    
    yyjson_doc *doc = yyjson_read(json, json_len, 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *val;
    val = yyjson_doc_get_pointern(doc, ptr, ptr_len);
    if (num) yy_assert(yyjson_get_int(val) == num);
    else yy_assert(!val);
    val = yyjson_get_pointern(root, ptr, ptr_len);
    if (num) yy_assert(yyjson_get_int(val) == num);
    else yy_assert(!val);
    
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
    yyjson_mut_val *mroot = yyjson_mut_doc_get_root(mdoc);
    yyjson_mut_val *mval;
    mval = yyjson_mut_doc_get_pointern(mdoc, ptr, ptr_len);
    if (num) yy_assert(yyjson_get_int(val) == num);
    else yy_assert(!mval);
    mval = yyjson_mut_get_pointern(mroot, ptr, ptr_len);
    if (num) yy_assert(yyjson_mut_get_int(mval) == num);
    else yy_assert(!mval);
    
    yyjson_doc_free(doc);
    yyjson_mut_doc_free(mdoc);
}

// `num` 0: should not match, otherwise match a number value
static void validate_val(const char *json, const char *ptr, int num) {
    usize json_len = strlen(json);
    validate_val_with_len(json, ptr, strlen(ptr), num);
    
    yyjson_doc *doc = yyjson_read(json, json_len, 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *val;
    val = yyjson_doc_get_pointer(doc, ptr);
    if (num) yy_assert(yyjson_get_int(val) == num);
    else yy_assert(!val);
    val = yyjson_get_pointer(root, ptr);
    if (num) yy_assert(yyjson_get_int(val) == num);
    else yy_assert(!val);
    
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
    yyjson_mut_val *mroot = yyjson_mut_doc_get_root(mdoc);
    yyjson_mut_val *mval;
    mval = yyjson_mut_doc_get_pointer(mdoc, ptr);
    if (num) yy_assert(yyjson_get_int(val) == num);
    else yy_assert(!mval);
    mval = yyjson_mut_get_pointer(mroot, ptr);
    if (num) yy_assert(yyjson_mut_get_int(mval) == num);
    else yy_assert(!mval);
    
    yyjson_doc_free(doc);
    yyjson_mut_doc_free(mdoc);
}

// test number key
static void validate_num(void) {
    // test number key
    const char *json = "{\"a\":{\"1\":{\"b\":2}},\" \":{\"\":{\"\":3}},\" \\u0000\":4,\"\\u0000 \":{\"\":5}}";
    validate_val(json, "/a/1/b", 2);
    validate_val(json, "/a/1/b/", 0);
    validate_val(json, "/a/1/b~", 0);
    validate_val(json, "/a/~0/b", 0);
    validate_val(json, "/a/~1/b", 0);
    validate_val(json, "/a/0/b", 0);
    validate_val(json, "/a/0/", 0);
    validate_val(json, "/", 0);
    validate_val(json, "a", 0);
    validate_val(json, "~", 0);
    validate_val(json, "~1", 0);
    validate_val(json, "~0", 0);
    validate_val(json, "/ //", 3);
    validate_val(json, "///", 0);
    validate_val(json, "/// ", 0);
    validate_val_with_len(json, "/ ", 3, 4);
    validate_val_with_len(json, "/\x00 /", 4, 5);
}

// test long path (larger than 512)
static void validate_long_str(void) {
    // test long path (larger than 512)
    const char *json1 = "{\"a\":{\"123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\":{\"b\":1}}}";
    validate_val(json1, "/a/123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b", 1);
    
    // test long path with escaped character (larger than 512)
    const char *json2 = "{\"a\":{\"12345/67890~12345/678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\":{\"b\":1}}}";
    validate_val(json2, "/a/12345~167890~012345~1678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b", 1);
    validate_val(json2, "/a/12345~167890~012345~2678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b", 0);
}

// test large index
static void validate_long_idx(void) {
    const char *json = "{\"a\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30]}";
    validate_val(json, "/a/0", 1);
    validate_val(json, "/a/1", 2);
    validate_val(json, "/a/29", 30);
    validate_val(json, "/a/30", 0);
    validate_val(json, "/a/00", 0);
    validate_val(json, "/a/01", 0);
    validate_val(json, "/a/1 ", 0);
    validate_val(json, "/a/ 1", 0);
    validate_val(json, "/a/100", 0);
    validate_val(json, "/a/100000000000000000000", 0);
}

// test invalid input
static void validate_err(void) {
    const char *json = "[0,1,2]";
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
    yy_assert(doc);
    yy_assert(mdoc);
    
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/1")) == 1);
    yy_assert(yyjson_doc_get_pointer(doc, NULL) == NULL);
    yy_assert(yyjson_doc_get_pointer(NULL, "/1") == NULL);
    yy_assert(yyjson_doc_get_pointer(NULL, NULL) == NULL);
    
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/1")) == 1);
    yy_assert(yyjson_mut_doc_get_pointer(mdoc, NULL) == NULL);
    yy_assert(yyjson_mut_doc_get_pointer(NULL, "/1") == NULL);
    yy_assert(yyjson_mut_doc_get_pointer(NULL, NULL) == NULL);
    
    yyjson_doc_free(doc);
    yyjson_mut_doc_free(mdoc);
}

// test case from spec: https://tools.ietf.org/html/rfc6901
static void validate_spec(void) {
    const char *json = "{ \
        \"foo\": [\"bar\", \"baz\"], \
        \"\": -1, \
        \"a/b\": 1, \
        \"c%d\": 2, \
        \"e^f\": 3, \
        \"g|h\": 4, \
        \"i\\\\j\": 5, \
        \"k\\\"l\": 6, \
        \" \": 7, \
        \"m~n\": 8 \
    }";
    
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yy_assert(yyjson_doc_get_pointer(doc, "") == yyjson_doc_get_root(doc));
    yy_assert(yyjson_is_arr(yyjson_doc_get_pointer(doc, "/foo")));
    yy_assert(yyjson_equals_str(yyjson_doc_get_pointer(doc, "/foo/0"), "bar"));
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/")) == -1);
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/a~1b")) == 1);
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/c%d")) == 2);
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/e^f")) == 3);
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/g|h")) == 4);
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/i\\j")) == 5);
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/k\"l")) == 6);
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/ ")) == 7);
    yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/m~0n")) == 8);
    
    yy_assert(yyjson_doc_get_pointer(doc, "/a~0b") == NULL);
    yy_assert(yyjson_doc_get_pointer(doc, "/a~2b") == NULL);
    yy_assert(yyjson_doc_get_pointer(doc, "/m~1n") == NULL);
    yy_assert(yyjson_doc_get_pointer(doc, "/m~2n") == NULL);
    
    
    yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
    yy_assert(yyjson_mut_doc_get_pointer(mdoc, "") == yyjson_mut_doc_get_root(mdoc));
    yy_assert(yyjson_mut_is_arr(yyjson_mut_doc_get_pointer(mdoc, "/foo")));
    yy_assert(yyjson_mut_equals_str(yyjson_mut_doc_get_pointer(mdoc, "/foo/0"), "bar"));
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/")) == -1);
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/a~1b")) == 1);
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/c%d")) == 2);
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/e^f")) == 3);
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/g|h")) == 4);
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/i\\j")) == 5);
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/k\"l")) == 6);
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/ ")) == 7);
    yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/m~0n")) == 8);
    
    yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a~0b") == NULL);
    yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a~2b") == NULL);
    yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/m~1n") == NULL);
    yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/m~2n") == NULL);
    
    yyjson_doc_free(doc);
    yyjson_mut_doc_free(mdoc);
}

yy_test_case(test_json_pointer) {
    validate_num();
    validate_long_str();
    validate_long_idx();
    validate_err();
    validate_spec();
}

#else
yy_test_case(test_json_pointer) {}
#endif
