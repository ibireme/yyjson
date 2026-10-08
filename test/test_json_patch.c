// This file is used to test the `JSON Patch` functions.

#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_READER && !YYJSON_DISABLE_WRITER && !YYJSON_DISABLE_UTILS

typedef struct {
    const char *src;
    const char *patch;
    const char *dst;
    yyjson_patch_err err;
} patch_data;

// -----------------------------------------------------------------------------
// assert (str == expected)
static void assert_str_eq(const char *str, const char *exp) {
    if (!str && !exp) return;
    if (str && !exp) yy_assertf(false, "expected NULL, but <%s>", str);
    if (!str && exp) yy_assertf(false, "expected <%s>, but NULL", exp);
    yy_assertf(strcmp(str, exp) == 0, "expected <%s>, but <%s>", exp, str);
}

// assert (str(val) == json)
static void assert_mut_val_eq(yyjson_mut_val *val, const char *json) {
    char *str = yyjson_mut_val_write(val, 0, NULL);
    assert_str_eq(str, json);
    if (str) free(str);
}

// assert (str(val) == json)
static void assert_err_eq(yyjson_patch_err *err, patch_data *data) {
    yy_assert(err->code == data->err.code);
    yy_assert(err->idx == data->err.idx);
    yy_assert(err->ptr.code == data->err.ptr.code);
    if (err->code) {
        yy_assert(err->msg != NULL);
    } else {
        yy_assert(err->msg == NULL);
    }
    if (err->code == YYJSON_PATCH_ERROR_POINTER) {
        yy_assert(err->ptr.code != 0);
        yy_assert(err->ptr.msg != NULL);
    } else {
        yy_assert(err->ptr.code == 0);
        yy_assert(err->ptr.msg == NULL);
    }
}

// -----------------------------------------------------------------------------
// test JSON patch
static void test_patch(patch_data data) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_doc *src_doc = yyjson_read(data.src, data.src ? strlen(data.src) : 0, 0);
    yyjson_doc *pat_doc = yyjson_read(data.patch, data.patch ? strlen(data.patch) : 0, 0);
    yyjson_val *src = yyjson_doc_get_root(src_doc);
    yyjson_val *pat = yyjson_doc_get_root(pat_doc);
    yyjson_mut_val *msrc = yyjson_val_mut_copy(doc, src);
    yyjson_mut_val *mpat = yyjson_val_mut_copy(doc, pat);
    yyjson_mut_val *ret;
    yyjson_patch_err err;
    
    ret = yyjson_patch(doc, src, pat, NULL);
    assert_mut_val_eq(ret, data.dst);
    
    memset(&err, -1, sizeof(err));
    ret = yyjson_patch(doc, src, pat, &err);
    assert_mut_val_eq(ret, data.dst);
    assert_err_eq(&err, &data);
    
    ret = yyjson_mut_patch(doc, msrc, mpat, NULL);
    assert_mut_val_eq(ret, data.dst);
    
    memset(&err, -1, sizeof(err));
    ret = yyjson_mut_patch(doc, msrc, mpat, &err);
    assert_mut_val_eq(ret, data.dst);
    assert_err_eq(&err, &data);
    
    yyjson_mut_doc_free(doc);
    yyjson_doc_free(src_doc);
    yyjson_doc_free(pat_doc);
}

// -----------------------------------------------------------------------------
// test cases from https://www.rfc-editor.org/rfc/rfc6902
static void test_spec(void) {
    // A.1.  Adding an Object Member
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/baz\",\"value\":\"qux\"}"
        "]",
        .dst = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
    });
    
    // A.2.  Adding an Array Element
    test_patch((patch_data){
        .src = "{\"foo\":[\"bar\",\"baz\"]}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/foo/1\",\"value\":\"qux\"}"
        "]",
        .dst = "{\"foo\":[\"bar\",\"qux\",\"baz\"]}",
    });
    
    // A.3.  Removing an Object Member
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
        .patch = "["
            "{\"op\":\"remove\",\"path\":\"/baz\"}"
        "]",
        .dst = "{\"foo\":\"bar\"}",
    });
    
    // A.4.  Removing an Array Element
    test_patch((patch_data){
        .src = "{\"foo\":[\"bar\",\"qux\",\"baz\"]}",
        .patch = "["
            "{\"op\":\"remove\",\"path\":\"/foo/1\"}"
        "]",
        .dst = "{\"foo\":[\"bar\",\"baz\"]}",
    });
    
    // A.5.  Replacing a Value
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
        .patch = "["
            "{\"op\":\"replace\",\"path\":\"/baz\",\"value\":\"boo\"}"
        "]",
        .dst = "{\"foo\":\"bar\",\"baz\":\"boo\"}",
    });
    
    // A.6.  Moving a Value
    test_patch((patch_data){
        .src = "{\"foo\":{\"bar\":\"baz\",\"waldo\":\"fred\"},\"qux\":{\"corge\":\"grault\"}}",
        .patch = "["
            "{\"op\":\"move\",\"from\":\"/foo/waldo\",\"path\":\"/qux/thud\"}"
        "]",
        .dst = "{\"foo\":{\"bar\":\"baz\"},\"qux\":{\"corge\":\"grault\",\"thud\":\"fred\"}}",
    });
    
    // A.7.  Moving an Array Element
    test_patch((patch_data){
        .src = "{\"foo\":[\"all\",\"grass\",\"cows\",\"eat\"]}",
        .patch = "["
            "{\"op\":\"move\",\"from\":\"/foo/1\",\"path\":\"/foo/3\"}"
        "]",
        .dst = "{\"foo\":[\"all\",\"cows\",\"eat\",\"grass\"]}",
    });
    
    // A.8.  Testing a Value: Success
    test_patch((patch_data){
        .src = "{\"baz\":\"qux\",\"foo\":[\"a\",2,\"c\"]}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/baz\",\"value\":\"qux\"},"
            "{\"op\":\"test\",\"path\":\"/foo/1\",\"value\":2}"
        "]",
        .dst = "{\"baz\":\"qux\",\"foo\":[\"a\",2,\"c\"]}",
    });
    
    // A.9.  Testing a Value: Error
    test_patch((patch_data){
        .src = "{\"baz\":\"qux\"}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/baz\",\"value\":\"bar\"}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_EQUAL,
            .idx = 0,
        },
    });
    
    // A.10.  Adding a Nested Member Object
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/child\",\"value\":{\"grandchild\":{}}}"
        "]",
        .dst = "{\"foo\":\"bar\",\"child\":{\"grandchild\":{}}}",
    });
    
    // A.11.  Ignoring Unrecognized Elements
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/baz\",\"value\":\"qux\",\"xyz\":123}"
        "]",
        .dst = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
    });
    
    // A.12.  Adding to a Nonexistent Target
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/baz/bat\",\"value\":\"qux\"}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_POINTER,
            .idx = 0,
            .ptr = YYJSON_PTR_ERR_RESOLVE,
        },
    });
    
    // A.13.  Invalid JSON Patch Document
    // Note:  yyjson allows duplicate keys, here only the first "op" is taken
    test_patch((patch_data){
        .src = "{\"foo\":\"bar\"}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/baz\",\"value\":\"qux\",\"op\":\"remove\"}"
        "]",
        .dst = "{\"foo\":\"bar\",\"baz\":\"qux\"}",
    });
    
    // A.14.  ~ Escape Ordering
    test_patch((patch_data){
        .src = "{\"/\":9,\"~1\":10}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/~01\",\"value\":10}"
        "]",
        .dst = "{\"/\":9,\"~1\":10}",
    });
    
    // A.15.  Comparing Strings and Numbers
    test_patch((patch_data){
        .src = "{\"/\":9,\"~1\":10}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/~01\",\"value\":\"10\"}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_EQUAL,
            .idx = 0,
        },
    });
    
    // A.16.  Adding an Array Value
    test_patch((patch_data){
        .src = "{\"foo\":[\"bar\"]}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/foo/-\",\"value\":[\"abc\",\"def\"]}"
        "]",
        .dst = "{\"foo\":[\"bar\",[\"abc\",\"def\"]]}",
    });
}

static void test_more(void) {
    // ---------------------------------
    // invalid parameter
    test_patch((patch_data){
        .src = "",
        .patch = "[]",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_PARAMETER,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_PARAMETER,
        }
    });
    test_patch((patch_data){
        .src = "",
        .patch = "",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_PARAMETER,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "{}",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_PARAMETER,
        }
    });
    
    // ---------------------------------
    // error with index
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "123"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_OPERATION,
            .idx = 2,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "{\"op\":\"err\",\"path\":\"/-\",\"value\":1}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_MEMBER,
            .idx = 2,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "{\"path\":\"/-\",\"value\":1}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_MISSING_KEY,
            .idx = 2,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "{\"op\":\"add\",\"value\":2}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_MISSING_KEY,
            .idx = 2,
        }
    });
    test_patch((patch_data){
        .src = "[]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":0},"
            "{\"op\":\"add\",\"path\":\"/-\",\"value\":1},"
            "{\"op\":\"add\",\"path\":null,\"value\":2}"
        "]",
        .err = {
            .code = YYJSON_PATCH_ERROR_INVALID_MEMBER,
            .idx = 2,
        }
    });
    
    // ---------------------------------
    // error op
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":0,\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"at\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"set\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"puts\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"delete\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"unknown\",\"path\":\"/0\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    
    // ---------------------------------
    // add
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"add\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"/1\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"/1\",\"value\":1}]",
        .dst = "[0,1]",
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"/2\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"/~2\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"add\",\"path\":\"\",\"value\":1}]",
        .dst = "1",
    });
    
    // ---------------------------------
    // remove
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\",\"path\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\",\"path\":\"\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SET_ROOT }
        }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\",\"path\":\"/-\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE, }
        }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"remove\",\"path\":\"/0\"}]",
        .dst = "[]",
    });
    
    // ---------------------------------
    // replace
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"replace\",\"value\":0}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"/1\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"/0\",\"value\":1}]",
        .dst = "[1]",
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"/1\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"/~2\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[0]",
        .patch = "[{\"op\":\"replace\",\"path\":\"\",\"value\":1}]",
        .dst = "1",
    });
    
    // ---------------------------------
    // move
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/0\",\"path\":\"/1/0\"}]",
        .dst = "[[2],[1,3,4]]",
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":0,\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/a\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/0\",\"path\":\"/1/a\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/~\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"\",\"path\":\"\"}]",
        .dst = "[[1,2],[3,4]]",
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"move\",\"from\":\"/0/0\",\"path\":\"\"}]",
        .dst = "1",
    });
    
    // ---------------------------------
    // copy
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/0\",\"path\":\"/1/0\"}]",
        .dst = "[[1,2],[1,3,4]]",
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":0,\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_INVALID_MEMBER }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/a\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/0\",\"path\":\"/1/a\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/~\",\"path\":\"/1/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"\",\"path\":\"\"}]",
        .dst = "[[1,2],[3,4]]",
    });
    test_patch((patch_data){
        .src = "[[1,2],[3,4]]",
        .patch = "[{\"op\":\"copy\",\"from\":\"/0/0\",\"path\":\"\"}]",
        .dst = "1",
    });
    
    // ---------------------------------
    // test
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"/0\"}]",
        .err = { .code = YYJSON_PATCH_ERROR_MISSING_KEY, }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"/0\",\"value\":1}]",
        .dst = "[1]",
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"/1\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_RESOLVE } }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"/~2\",\"value\":1}]",
        .err = { .code = YYJSON_PATCH_ERROR_POINTER,
                 .ptr = { .code = YYJSON_PTR_ERR_SYNTAX } }
    });
    test_patch((patch_data){
        .src = "[1]",
        .patch = "[{\"op\":\"test\",\"path\":\"\",\"value\":2}]",
        .err = { .code = YYJSON_PATCH_ERROR_EQUAL }
    });
    
    // ---------------------------------
    // multiple ops
    test_patch((patch_data){
        .src = "[1,2,3]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/3\",\"value\":4}," // [1,2,3,4]
            "{\"op\":\"remove\",\"path\":\"/1\"}," // [1,3,4]
            "{\"op\":\"replace\",\"path\":\"/0\",\"value\":{\"a\":0}}," // [{"a":0},3,4]
            "{\"op\":\"move\",\"from\":\"/0/a\",\"path\":\"/1\"}," // [{},0,3,4]
            "{\"op\":\"copy\",\"from\":\"/3\",\"path\":\"/0/b\"}," // [{"b":4},0,3,4]
            "{\"op\":\"test\",\"path\":\"/0\",\"value\":{\"b\":4}}" // [{"b":4},0,3,4]
        "]",
        .dst = "[{\"b\":4},0,3,4]"
    });
    
    // ---------------------------------
    // add, move and copy replace an existing object member (RFC 6902, 4.1)
    test_patch((patch_data){
        .src = "{\"foo\":null}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/foo\",\"value\":1}"
        "]",
        .dst = "{\"foo\":1}"
    });
    test_patch((patch_data){
        .src = "{\"a\":{\"foo\":\"bar\",\"baz\":2}}",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/a/foo\",\"value\":[1]}"
        "]",
        .dst = "{\"a\":{\"foo\":[1],\"baz\":2}}"
    });
    test_patch((patch_data){
        .src = "{\"foo\":1,\"bar\":2}",
        .patch = "["
            "{\"op\":\"move\",\"from\":\"/bar\",\"path\":\"/foo\"}"
        "]",
        .dst = "{\"foo\":2}"
    });
    test_patch((patch_data){
        .src = "{\"foo\":1,\"bar\":2}",
        .patch = "["
            "{\"op\":\"copy\",\"from\":\"/bar\",\"path\":\"/foo\"}"
        "]",
        .dst = "{\"foo\":2,\"bar\":2}"
    });
    // add still inserts into an array
    test_patch((patch_data){
        .src = "[1,2]",
        .patch = "["
            "{\"op\":\"add\",\"path\":\"/0\",\"value\":0}"
        "]",
        .dst = "[0,1,2]"
    });
}

// Test parsed values through both patch APIs, with and without error output.
static void test_patch_equals(const char *lhs, const char *rhs, bool equal) {
    char patch[512];
    int len = snprintf(patch, sizeof(patch),
                       "[{\"op\":\"test\",\"path\":\"\",\"value\":%s}]", rhs);
    yyjson_doc *src = yyjson_read(lhs, strlen(lhs), 0);
    char *dst = equal ? yyjson_write(src, 0, NULL) : NULL;
    yy_assert(len > 0 && (size_t)len < sizeof(patch));
    yy_assert(src != NULL);
    yy_assert(!equal || dst != NULL);
    test_patch((patch_data){
        .src = lhs,
        .patch = patch,
        .dst = dst,
        .err = { .code = equal ? YYJSON_PATCH_SUCCESS : YYJSON_PATCH_ERROR_EQUAL },
    });
    free(dst);
    yyjson_doc_free(src);
}

// Preserve builder-selected subtypes, including positive signed integers and RAW.
static void test_patch_values(yyjson_mut_val *lhs, yyjson_mut_val *rhs,
                              bool equal) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *patch = yyjson_mut_arr(doc);
    yyjson_mut_val *op = yyjson_mut_obj(doc);
    yyjson_doc *src_doc, *pat_doc;
    yyjson_patch_err err;
    patch_data data = { .err = {
        .code = equal ? YYJSON_PATCH_SUCCESS : YYJSON_PATCH_ERROR_EQUAL
    } };
    size_t idx;
    yy_assert(doc && lhs && rhs && patch && op);
    yy_assert(yyjson_mut_obj_add_str(doc, op, "op", "test"));
    yy_assert(yyjson_mut_obj_add_str(doc, op, "path", ""));
    yy_assert(yyjson_mut_obj_add_val(doc, op, "value", rhs));
    yy_assert(yyjson_mut_arr_add_val(patch, op));
    src_doc = yyjson_mut_val_imut_copy(lhs, NULL);
    pat_doc = yyjson_mut_val_imut_copy(patch, NULL);
    yy_assert(src_doc && pat_doc);
    for (idx = 0; idx < 2; idx++) {
        yyjson_mut_val *ret;
        yyjson_patch_err *err_ptr = idx ? &err : NULL;
        memset(&err, -1, sizeof(err));
        ret = yyjson_patch(doc, yyjson_doc_get_root(src_doc),
                           yyjson_doc_get_root(pat_doc), err_ptr);
        yy_assertf((ret != NULL) == equal,
                   "expected equal=%d for payloads %llx and %llx",
                   (int)equal, (unsigned long long)lhs->uni.u64,
                   (unsigned long long)rhs->uni.u64);
        if (ret) yy_assert(yyjson_mut_equals(ret, lhs));
        if (err_ptr) assert_err_eq(err_ptr, &data);
        memset(&err, -1, sizeof(err));
        ret = yyjson_mut_patch(doc, lhs, patch, err_ptr);
        yy_assert((ret != NULL) == equal);
        if (ret) yy_assert(yyjson_mut_equals(ret, lhs));
        if (err_ptr) assert_err_eq(err_ptr, &data);
    }
    yyjson_doc_free(src_doc);
    yyjson_doc_free(pat_doc);
    yyjson_mut_doc_free(doc);
}

static void test_numeric_equals(void) {
    const struct {
        const char *lhs;
        const char *rhs;
        bool equal;
    } cases[] = {
        { "1", "1.0", true },
        { "-5", "-5.0", true },
        { "1", "1.5", false },
        { "1", "-1.0", false },
        { "-5", "-5.5", false },
        { "9007199254740992", "9007199254740992.0", true },
        { "9007199254740993", "9007199254740992.0", false },
        { "9007199254740994", "9007199254740994.0", true },
        { "-9223372036854775808", "-9223372036854775808.0", true },
        { "-9223372036854775808", "-9223372036854777856.0", false },
        { "9223372036854775807", "9223372036854775808.0", false },
        { "9223372036854775808", "9223372036854775808.0", true },
        { "18446744073709549568", "18446744073709549568.0", true },
        { "18446744073709551615", "18446744073709551616.0", false },
        { "{\"a\":[1,{\"b\":-5}],\"c\":0.0}",
          "{\"c\":-0.0,\"a\":[1.0,{\"b\":-5.0}]}", true },
        { "[1,{\"a\":2}]", "[1.0,{\"a\":2.5}]", false },
        { "{\"1\":1}", "{\"1.0\":1.0}", false },
        { "1", "\"1\"", false },
        { "\"1\"", "\"1.0\"", false },
        { "1", "true", false },
    };
    const char *zeros[] = { "0", "-0", "0.0", "-0.0" };
    size_t idx, jdx;
    for (idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        test_patch_equals(cases[idx].lhs, cases[idx].rhs, cases[idx].equal);
        test_patch_equals(cases[idx].rhs, cases[idx].lhs, cases[idx].equal);
    }
    for (idx = 0; idx < sizeof(zeros) / sizeof(zeros[0]); idx++) {
        for (jdx = 0; jdx < sizeof(zeros) / sizeof(zeros[0]); jdx++) {
            test_patch_equals(zeros[idx], zeros[jdx], true);
        }
    }

    test_patch((patch_data){
        .src = "{\"n\":1}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/n\",\"value\":1.0},"
            "{\"op\":\"replace\",\"path\":\"/n\",\"value\":2}"
        "]",
        .dst = "{\"n\":2}",
    });
    test_patch((patch_data){
        .src = "{\"n\":1}",
        .patch = "["
            "{\"op\":\"test\",\"path\":\"/n\",\"value\":1.0},"
            "{\"op\":\"test\",\"path\":\"/n\",\"value\":1.5},"
            "{\"op\":\"remove\",\"path\":\"/missing\"}"
        "]",
        .err = { .code = YYJSON_PATCH_ERROR_EQUAL, .idx = 1 },
    });
}

static void test_numeric_builder_equals(void) {
    const struct {
        int64_t sint;
        double real;
        bool equal;
    } cases[] = {
        { 1, 1.0, true },
        { 0, -0.0, true },
        { 0, 0.5, false },
        { INT64_C(9007199254740993), 9007199254740992.0, false },
        { INT64_C(9007199254740994), 9007199254740994.0, true },
        { INT64_MIN, -9223372036854775808.0, true },
        { INT64_MAX, 9223372036854775808.0, false },
    };
    const double non_finite[] = { NAN, INFINITY, -INFINITY };
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    size_t idx;
    yy_assert(doc);
    for (idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        yyjson_mut_val *lhs = yyjson_mut_sint(doc, cases[idx].sint);
        yyjson_mut_val *rhs = yyjson_mut_real(doc, cases[idx].real);
        test_patch_values(lhs, rhs, cases[idx].equal);
        test_patch_values(rhs, lhs, cases[idx].equal);
    }
#if FLT_RADIX == 2 && DBL_MANT_DIG == 53 && \
    DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024
    if (sizeof(double) == sizeof(uint64_t)) {
        const struct {
            uint64_t bits;
            bool equal;
        } zeros[] = {
            { UINT64_C(0x0000000000000001), false },
            { UINT64_C(0x8000000000000001), false },
            { UINT64_C(0x000fffffffffffff), false },
            { UINT64_C(0x0000000000000000), true },
            { UINT64_C(0x8000000000000000), true },
        };
        const uint64_t nan_bits[] = {
            UINT64_C(0x7ff8000000000000),
            UINT64_C(0x7ff8000000000001),
        };
        yyjson_mut_val *nan_vals[2];
        for (idx = 0; idx < sizeof(zeros) / sizeof(zeros[0]); idx++) {
            double value;
            uint64_t bits;
            yyjson_mut_val *real, *sint, *uint;
            // Preserve subnormal inputs without floating-point arithmetic.
            memcpy(&value, &zeros[idx].bits, sizeof(value));
            memcpy(&bits, &value, sizeof(bits));
            yy_assert(bits == zeros[idx].bits);
            real = yyjson_mut_real(doc, value);
            sint = yyjson_mut_sint(doc, 0);
            uint = yyjson_mut_uint(doc, 0);
            yy_assert(real && real->uni.u64 == zeros[idx].bits);
            test_patch_values(real, sint, zeros[idx].equal);
            test_patch_values(sint, real, zeros[idx].equal);
            test_patch_values(real, uint, zeros[idx].equal);
            test_patch_values(uint, real, zeros[idx].equal);
        }
        // Use distinct NaN payloads without relying on negation.
        for (idx = 0; idx < sizeof(nan_bits) / sizeof(nan_bits[0]); idx++) {
            double value;
            memcpy(&value, &nan_bits[idx], sizeof(value));
            nan_vals[idx] = yyjson_mut_real(doc, value);
            yy_assert(nan_vals[idx] && nan_vals[idx]->uni.u64 == nan_bits[idx]);
            test_patch_values(nan_vals[idx], nan_vals[idx], true);
        }
        test_patch_values(nan_vals[0], nan_vals[1], false);
        test_patch_values(nan_vals[1], nan_vals[0], false);
    }
#endif
    test_patch_values(yyjson_mut_sint(doc, INT64_MAX),
                      yyjson_mut_uint(doc, (uint64_t)INT64_MAX), true);
    test_patch_values(yyjson_mut_sint(doc, INT64_MIN),
                      yyjson_mut_uint(doc, UINT64_C(9223372036854775808)), false);
    test_patch_values(yyjson_mut_raw(doc, "1"), yyjson_mut_raw(doc, "1"), true);
    test_patch_values(yyjson_mut_raw(doc, "1"), yyjson_mut_raw(doc, "1.0"), false);
    test_patch_values(yyjson_mut_raw(doc, "1"), yyjson_mut_uint(doc, 1), false);
    // Non-finite extension values retain their existing equality behavior.
    test_patch_values(yyjson_mut_real(doc, NAN), yyjson_mut_real(doc, NAN), true);
    test_patch_values(yyjson_mut_real(doc, INFINITY),
                      yyjson_mut_real(doc, INFINITY), true);
    test_patch_values(yyjson_mut_real(doc, INFINITY),
                      yyjson_mut_real(doc, -INFINITY), false);
    for (idx = 0; idx < sizeof(non_finite) / sizeof(non_finite[0]); idx++) {
        yyjson_mut_val *real = yyjson_mut_real(doc, non_finite[idx]);
        yyjson_mut_val *sint = yyjson_mut_sint(doc, 0);
        yyjson_mut_val *uint = yyjson_mut_uint(doc, UINT64_MAX);
        test_patch_values(real, sint, false);
        test_patch_values(sint, real, false);
        test_patch_values(real, uint, false);
        test_patch_values(uint, real, false);
    }
    yyjson_mut_doc_free(doc);
}

yy_test_case(test_json_patch) {
    test_spec();
    test_more();
    test_numeric_equals();
    test_numeric_builder_equals();
}

#else
yy_test_case(test_json_patch) {}
#endif
