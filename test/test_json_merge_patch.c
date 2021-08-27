#include "yyjson.h"
#include "yy_test_utils.h"

#include <fcntl.h>
#include <stdio.h>

/* TODO: Move something like this into yyjson.h */
bool mut_val_eq(yyjson_mut_val* lhs, yyjson_mut_val* rhs) {
    if (yyjson_mut_get_type(lhs) != yyjson_mut_get_type(rhs)) {
        return false;
    }
    if (yyjson_mut_is_null(lhs)) {
        return true;
    }
    if (yyjson_mut_is_bool(lhs)) {
        return yyjson_mut_get_bool(lhs) == yyjson_mut_get_bool(rhs);
    }
    if (yyjson_mut_is_uint(lhs)) {
        return yyjson_mut_get_uint(lhs) == yyjson_mut_get_uint(rhs);
    }
    if (yyjson_mut_is_sint(lhs)) {
        return yyjson_mut_get_sint(lhs) == yyjson_mut_get_sint(rhs);
    }
    if (yyjson_mut_is_int(lhs)) {
        return yyjson_mut_get_int(lhs) == yyjson_mut_get_int(rhs);
    }
    if (yyjson_mut_is_real(lhs)) {
        return yyjson_mut_get_real(lhs) == yyjson_mut_get_real(rhs);
    }
    if (yyjson_mut_is_str(lhs)) {
        return !strcmp(yyjson_mut_get_str(lhs), yyjson_mut_get_str(rhs));
    }
    if (yyjson_mut_is_arr(lhs)) {
        size_t len = yyjson_mut_arr_size(lhs);
        if (yyjson_mut_arr_size(rhs) != len) {
            return false;
        }
        if (!len) {
            return true;
        }

        yyjson_mut_arr_iter lhs_iter;
        yyjson_mut_arr_iter_init(lhs, &lhs_iter);

        yyjson_mut_arr_iter rhs_iter;
        yyjson_mut_arr_iter_init(rhs, &rhs_iter);

        yyjson_mut_val *lhs_val, *rhs_val;
        while ((lhs_val = yyjson_mut_arr_iter_next(&lhs_iter)) &&
               (rhs_val = yyjson_mut_arr_iter_next(&rhs_iter))) {
            if (!mut_val_eq(lhs_val, rhs_val)) {
                return false;
            }
        }
        return true;
    }
    if (yyjson_mut_is_obj(lhs)) {
        size_t len = yyjson_mut_obj_size(lhs);
        if (yyjson_mut_obj_size(rhs) != len) {
            return false;
        }
        if (!len) {
            return true;
        }

        yyjson_mut_obj_iter lhs_iter;
        yyjson_mut_obj_iter_init(lhs, &lhs_iter);
        
        yyjson_mut_val *key;
        while ((key = yyjson_mut_obj_iter_next(&lhs_iter))) {
            yyjson_mut_val *lhs_val = yyjson_mut_obj_iter_get_val(key);
            yyjson_mut_val *rhs_val = yyjson_mut_obj_get(rhs, yyjson_mut_get_str(key));
            if (!rhs_val) {
                return false;
            }
            if (!mut_val_eq(lhs_val, rhs_val)) {
                return false;
            }
        }
        return true;
    }

    return false;
}

static
void test_one(const char *original_json,
              const char *patch_json,
              const char *want_result_json) {
    yyjson_doc *original_doc = yyjson_read(original_json, strlen(original_json), 0);
    yyjson_doc *patch_doc = yyjson_read(patch_json, strlen(patch_json), 0);
    yyjson_doc *want_result_doc = yyjson_read(want_result_json, strlen(want_result_json), 0);
    yyjson_mut_doc *mut_want_result_doc = yyjson_doc_mut_copy(want_result_doc, NULL);

    yyjson_mut_doc *result_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *result = yyjson_merge_patch(result_doc, original_doc->root, patch_doc->root);

    yy_assert(mut_val_eq(mut_want_result_doc->root, result));

    yyjson_mut_doc_free(result_doc);
    yyjson_mut_doc_free(mut_want_result_doc);
    yyjson_doc_free(want_result_doc);
    yyjson_doc_free(patch_doc);
    yyjson_doc_free(original_doc);
}

yy_test_case(test_json_merge_patch) {
#if !YYJSON_DISABLE_READER
    // test cases from spec: https://tools.ietf.org/html/rfc7386
    test_one("{\"a\":\"b\"}", "{\"a\":\"c\"}", "{\"a\":\"c\"}");
    test_one("{\"a\":\"b\"}", "{\"b\":\"c\"}", "{\"a\":\"b\",\"b\":\"c\"}");
    test_one("{\"a\":\"b\"}", "{\"a\":null }", "{}");
    test_one("{\"a\":\"b\"}", "{\"a\":null }", "{}");
    test_one("{\"a\":\"b\", \"b\":\"c\"}", "{\"a\":null }", "{\"b\":\"c\"}");
    test_one("{\"a\":[\"b\"] }", "{\"a\":\"c\"}", "{\"a\":\"c\"}");
    test_one("{\"a\":\"c\"}", "{\"a\":[\"b\"]}", "{\"a\":[\"b\"]}");
    test_one("{\"a\":{\"b\":\"c\"}}", "{\"a\":{\"b\":\"d\",\"c\":null}}", "{\"a\":{\"b\":\"d\"}}");
    test_one("{\"a\":{\"b\":\"c\"}}", "{\"a\":[1]}", "{\"a\":[1]}");
    test_one("[\"a\",\"b\"]", "[\"c\",\"d\"]", "[\"c\",\"d\"]");
    test_one("{\"a\":\"b\"}", "[\"c\"]", "[\"c\"]");
    test_one("{\"a\":\"foo\"}", "null", "null");
    test_one("{\"a\":\"foo\"}", "\"bar\"", "\"bar\"");
    test_one("{\"e\":null}", "{\"a\":1}", "{\"e\":null,\"a\":1}");
    test_one("[1,2]", "{\"a\":\"b\",\"c\":null}", "{\"a\":\"b\"}");
    test_one("{}", "{\"a\":{\"bb\":{\"ccc\":null}}}", "{\"a\":{\"bb\":{}}}");

#endif
}
