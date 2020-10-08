#include "yyjson.h"
#include "yy_test_utils.h"

#if !YYJSON_DISABLE_READER

/// Validate value type
static bool validate_val_type(yyjson_val *val,
                              yyjson_type type,
                              yyjson_subtype subtype) {
    
    if (yyjson_is_null(val) != (type == YYJSON_TYPE_NULL &&
                                subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_is_true(val) != (type == YYJSON_TYPE_BOOL &&
                                subtype == YYJSON_SUBTYPE_TRUE)) return false;
    if (yyjson_is_false(val) != (type == YYJSON_TYPE_BOOL &&
                                 subtype == YYJSON_SUBTYPE_FALSE)) return false;
    if (yyjson_is_bool(val) != (type == YYJSON_TYPE_BOOL &&
                                (subtype == YYJSON_SUBTYPE_TRUE ||
                                 subtype == YYJSON_SUBTYPE_FALSE))) return false;
    if (yyjson_is_uint(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_UINT)) return false;
    if (yyjson_is_sint(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_SINT)) return false;
    if (yyjson_is_int(val) != (type == YYJSON_TYPE_NUM &&
                               (subtype == YYJSON_SUBTYPE_UINT ||
                                subtype == YYJSON_SUBTYPE_SINT))) return false;
    if (yyjson_is_real(val) != (type == YYJSON_TYPE_NUM &&
                                subtype == YYJSON_SUBTYPE_REAL)) return false;
    if (yyjson_is_num(val) != (type == YYJSON_TYPE_NUM &&
                               (subtype == YYJSON_SUBTYPE_UINT ||
                                subtype == YYJSON_SUBTYPE_SINT ||
                                subtype == YYJSON_SUBTYPE_REAL))) return false;
    if (yyjson_is_str(val) != (type == YYJSON_TYPE_STR &&
                               subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_is_arr(val) != (type == YYJSON_TYPE_ARR &&
                               subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_is_obj(val) != (type == YYJSON_TYPE_OBJ &&
                               subtype == YYJSON_SUBTYPE_NONE)) return false;
    if (yyjson_is_ctn(val) != ((type == YYJSON_TYPE_ARR ||
                                type == YYJSON_TYPE_OBJ) &&
                                subtype == YYJSON_SUBTYPE_NONE)) return false;
    
    if (yyjson_get_type(val) != type) return false;
    if (yyjson_get_subtype(val) != subtype) return false;
    if (yyjson_get_tag(val) != (type | subtype)) return false;
    
    return true;
}

/// Test simple json value api
static void test_json_val_api(void) {
    yyjson_doc *doc;
    yyjson_val *val;
    const char *json;
    
    val = NULL;
    yy_assert(strcmp(yyjson_get_type_desc(val), "unknown") == 0);
    yy_assert(yyjson_get_len(val) == 0);
    yy_assert(yyjson_equals_str(val, "") == false);
    yy_assert(yyjson_equals_strn(val, "", 0) == false);
    yy_assert(yyjson_arr_size(val) == 0);
    yy_assert(yyjson_obj_size(val) == 0);
    
    
    json = "null";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_NULL, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "null") == 0);
    yyjson_doc_free(doc);
    
    json = "true";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_BOOL, YYJSON_SUBTYPE_TRUE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "true") == 0);
    yy_assert(yyjson_get_bool(val) == true);
    yyjson_doc_free(doc);
    
    json = "false";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_BOOL, YYJSON_SUBTYPE_FALSE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "false") == 0);
    yy_assert(yyjson_get_bool(val) == false);
    yyjson_doc_free(doc);
    
    json = "123";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_UINT));
    yy_assert(strcmp(yyjson_get_type_desc(val), "uint") == 0);
    yy_assert(yyjson_get_uint(val) == (u64)123);
    yy_assert(yyjson_get_sint(val) == (i64)123);
    yy_assert(yyjson_get_int(val) == (i64)123);
    yy_assert(yyjson_get_real(val) == (f64)0);
    yy_assert(yyjson_get_bool(val) == false);
    yyjson_doc_free(doc);
    
    json = "-123";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_SINT));
    yy_assert(strcmp(yyjson_get_type_desc(val), "sint") == 0);
    yy_assert(yyjson_get_uint(val) == (u64)-123);
    yy_assert(yyjson_get_sint(val) == (i64)-123);
    yy_assert(yyjson_get_int(val) == (i64)-123);
    yy_assert(yyjson_get_real(val) == (f64)0);
    yyjson_doc_free(doc);
    
    json = "123.0";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_NUM, YYJSON_SUBTYPE_REAL));
    yy_assert(strcmp(yyjson_get_type_desc(val), "real") == 0);
    yy_assert(yyjson_get_uint(val) == (u64)0);
    yy_assert(yyjson_get_sint(val) == (i64)0);
    yy_assert(yyjson_get_int(val) == (i64)0);
    yy_assert(yyjson_get_real(val) == (f64)123.0);
    yyjson_doc_free(doc);
    
    json = "\"abc\"";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_STR, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "string") == 0);
    yy_assert(strcmp(yyjson_get_str(val), "abc") == 0);
    yy_assert(yyjson_get_len(val) == 3);
    yy_assert(yyjson_equals_str(val, "abc"));
    yyjson_doc_free(doc);
    
    json = "\"abc\\u0000def\"";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_STR, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "string") == 0);
    yy_assert(strcmp(yyjson_get_str(val), "abc") == 0);
    yy_assert(memcmp(yyjson_get_str(val), "abc\0def", 7) == 0);
    yy_assert(yyjson_get_len(val) == 7);
    yy_assert(yyjson_equals_str(val, "abc") == false);
    yy_assert(yyjson_equals_strn(val, "abc", 3) == false);
    yy_assert(yyjson_equals_str(val, "abc\0def") == false);
    yy_assert(yyjson_equals_strn(val, "abc\0def", 7) == true);
    yyjson_doc_free(doc);
    
    json = "[]";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_ARR, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "array") == 0);
    yyjson_doc_free(doc);
    
    json = "{}";
    doc = yyjson_read(json, strlen(json), 0);
    val = yyjson_doc_get_root(doc);
    yy_assert(validate_val_type(val, YYJSON_TYPE_OBJ, YYJSON_SUBTYPE_NONE));
    yy_assert(strcmp(yyjson_get_type_desc(val), "object") == 0);
    yyjson_doc_free(doc);
    
    val = NULL;
    yy_assert(validate_val_type(val, YYJSON_TYPE_NONE, YYJSON_SUBTYPE_NONE));
}

/// Test json array api
static void test_json_arr_api(void) {
    yyjson_doc *doc;
    yyjson_val *arr, *val;
    const char *json;
    yyjson_arr_iter iter;
    size_t idx, max, tmp[16];
    
    //---------------------------------------------
    // array (size 0)
    
    json = "[]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_arr(arr));
    yy_assert(yyjson_arr_size(arr) == 0);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(val == NULL);
    val = yyjson_arr_get_last(arr);
    yy_assert(val == NULL);
    
    // iter
    yyjson_arr_iter_init(arr, &iter);
    yy_assert(yyjson_arr_iter_has_next(&iter) == false);
    while ((val = yyjson_arr_iter_next(&iter))) {
        yy_assert(false);
    }
    
    // foreach
    yyjson_arr_foreach(arr, idx, max, val) {
        yy_assert(false);
    }
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // array (size 1)
    
    json = "[1]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_arr_size(arr) == 1);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get(arr, 1);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get_last(arr);
    yy_assert(yyjson_get_int(val) == 1);
    
    // iter
    idx = 0;
    yyjson_arr_iter_init(arr, &iter);
    yy_assert(yyjson_arr_iter_has_next(&iter) == true);
    while ((val = yyjson_arr_iter_next(&iter))) {
        idx++;
        yy_assert(yyjson_get_int(val) == (i64)idx);
        yy_assert(yyjson_arr_iter_has_next(&iter) == idx < 1);
    }
    yy_assert(yyjson_arr_iter_has_next(&iter) == false);
    yy_assert(idx == 1);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_arr_foreach(arr, idx, max, val) {
        yy_assert(yyjson_get_int(val) == (i64)idx + 1);
        yy_assert(max == 1);
        yy_assert(tmp[idx] == 0);
        tmp[idx] = idx + 1;
    }
    yy_assert(tmp[0] == 1);
    yy_assert(tmp[1] == 0);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // array (size 2)
    
    json = "[1,2]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_arr_size(arr) == 2);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get(arr, 1);
    yy_assert(yyjson_get_int(val) == 2);
    val = yyjson_arr_get(arr, 2);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get_last(arr);
    yy_assert(yyjson_get_int(val) == 2);
    
    // iter
    idx = 0;
    yyjson_arr_iter_init(arr, &iter);
    yy_assert(yyjson_arr_iter_has_next(&iter) == true);
    while ((val = yyjson_arr_iter_next(&iter))) {
        idx++;
        yy_assert(yyjson_get_int(val) == (i64)idx);
        yy_assert(yyjson_arr_iter_has_next(&iter) == idx < 2);
    }
    yy_assert(yyjson_arr_iter_has_next(&iter) == false);
    yy_assert(idx == 2);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_arr_foreach(arr, idx, max, val) {
        yy_assert(yyjson_get_int(val) == (i64)idx + 1);
        yy_assert(max == 2);
        yy_assert(tmp[idx] == 0);
        tmp[idx] = idx + 1;
    }
    yy_assert(tmp[0] == 1);
    yy_assert(tmp[1] == 2);
    yy_assert(tmp[2] == 0);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // array (size 3)
    
    json = "[1,2,3]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_arr_size(arr) == 3);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get(arr, 1);
    yy_assert(yyjson_get_int(val) == 2);
    val = yyjson_arr_get(arr, 2);
    yy_assert(yyjson_get_int(val) == 3);
    val = yyjson_arr_get(arr, 3);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get_last(arr);
    yy_assert(yyjson_get_int(val) == 3);
    
    // iter
    idx = 0;
    yyjson_arr_iter_init(arr, &iter);
    yy_assert(yyjson_arr_iter_has_next(&iter) == true);
    while ((val = yyjson_arr_iter_next(&iter))) {
        idx++;
        yy_assert(yyjson_get_int(val) == (i64)idx);
        yy_assert(yyjson_arr_iter_has_next(&iter) == idx < 3);
    }
    yy_assert(yyjson_arr_iter_has_next(&iter) == false);
    yy_assert(idx == 3);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_arr_foreach(arr, idx, max, val) {
        yy_assert(yyjson_get_int(val) == (i64)idx + 1);
        yy_assert(max == 3);
        yy_assert(tmp[idx] == 0);
        tmp[idx] = idx + 1;
    }
    yy_assert(tmp[0] == 1);
    yy_assert(tmp[1] == 2);
    yy_assert(tmp[2] == 3);
    yy_assert(tmp[3] == 0);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // array (size 3, non-flat)
    
    json = "[1,[null],3]";
    doc = yyjson_read(json, strlen(json), 0);
    arr = yyjson_doc_get_root(doc);
    yy_assert(yyjson_arr_size(arr) == 3);
    
    val = yyjson_arr_get(arr, 0);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get(arr, 2);
    yy_assert(yyjson_get_int(val) == 3);
    val = yyjson_arr_get(arr, 3);
    yy_assert(val == NULL);
    
    val = yyjson_arr_get_first(arr);
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_arr_get_last(arr);
    yy_assert(yyjson_get_int(val) == 3);
    
    //---------------------------------------------
    // iter
    yy_assert(yyjson_arr_iter_init(arr, NULL) == false);
    yy_assert(yyjson_arr_iter_init(NULL, &iter) == false);
    yy_assert(yyjson_arr_iter_init(NULL, NULL) == false);
    
    
    yyjson_doc_free(doc);
}

/// Test json object api
static void test_json_obj_api(void) {
    yyjson_doc *doc;
    yyjson_val *obj, *key, *val;
    const char *json;
    yyjson_obj_iter iter;
    size_t idx, max, tmp[16];
    
    
    //---------------------------------------------
    // object (size 0)
    
    json = "{}";
    doc = yyjson_read(json, strlen(json), 0);
    obj = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_obj(obj));
    yy_assert(yyjson_obj_size(obj) == 0);
    
    val = yyjson_obj_get(obj, "x");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, "");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, NULL);
    yy_assert(val == NULL);
    
    // iter
    yyjson_obj_iter_init(obj, &iter);
    yy_assert(yyjson_obj_iter_has_next(&iter) == false);
    while ((key = yyjson_obj_iter_next(&iter))) {
        yy_assert(false);
    }
    
    // foreach
    yyjson_obj_foreach(obj, idx, max, key, val) {
        yy_assert(false);
    }

    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // object (size 1)
    
    json = "{\"a\":1}";
    doc = yyjson_read(json, strlen(json), 0);
    obj = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_obj(obj));
    yy_assert(yyjson_obj_size(obj) == 1);
    
    val = yyjson_obj_get(obj, "a");
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_obj_get(obj, "x");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, "");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, NULL);
    yy_assert(val == NULL);
    
    // iter
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_iter_init(obj, &iter);
    yy_assert(yyjson_obj_iter_has_next(&iter) == true);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = key + 1;
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
            yy_assert(yyjson_obj_iter_has_next(&iter) == false);
        } else {
            yy_assert(false);
        }
    }
    yy_assert(yyjson_obj_iter_has_next(&iter) == false);
    yy_assert(tmp[0]);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_foreach(obj, idx, max, key, val) {
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
        } else {
            yy_assert(false);
        }
    }
    yy_assert(tmp[0]);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // object (size 2)
    
    json = "{\"a\":1,\"b\":2}";
    doc = yyjson_read(json, strlen(json), 0);
    obj = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_obj(obj));
    yy_assert(yyjson_obj_size(obj) == 2);
    
    val = yyjson_obj_get(obj, "a");
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_obj_get(obj, "b");
    yy_assert(yyjson_get_int(val) == 2);
    val = yyjson_obj_get(obj, "x");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, "");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, NULL);
    yy_assert(val == NULL);
    
    // iter
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_iter_init(obj, &iter);
    yy_assert(yyjson_obj_iter_has_next(&iter) == true);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = key + 1;
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
            yy_assert(yyjson_obj_iter_has_next(&iter) == true);
        } else if (yyjson_equals_str(key, "b")) {
            yy_assert(yyjson_get_int(val) == 2);
            yy_assert(tmp[1] == 0);
            tmp[1] = 2;
            yy_assert(yyjson_obj_iter_has_next(&iter) == false);
        } else {
            yy_assert(false);
        }
    }
    yy_assert(yyjson_obj_iter_has_next(&iter) == false);
    yy_assert(tmp[0]);
    yy_assert(tmp[1]);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_foreach(obj, idx, max, key, val) {
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
        } else if (yyjson_equals_str(key, "b")) {
            yy_assert(yyjson_get_int(val) == 2);
            yy_assert(tmp[1] == 0);
            tmp[1] = 2;
        } else {
            yy_assert(false);
        }
    }
    yy_assert(tmp[0]);
    yy_assert(tmp[1]);
    
    yyjson_doc_free(doc);
    
    
    //---------------------------------------------
    // object (size 3)
    
    json = "{\"a\":1,\"b\":2,\"c\":3}";
    doc = yyjson_read(json, strlen(json), 0);
    obj = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_obj(obj));
    yy_assert(yyjson_obj_size(obj) == 3);
    
    val = yyjson_obj_get(obj, "a");
    yy_assert(yyjson_get_int(val) == 1);
    val = yyjson_obj_get(obj, "b");
    yy_assert(yyjson_get_int(val) == 2);
    val = yyjson_obj_get(obj, "c");
    yy_assert(yyjson_get_int(val) == 3);
    val = yyjson_obj_get(obj, "x");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, "");
    yy_assert(val == NULL);
    val = yyjson_obj_get(obj, NULL);
    yy_assert(val == NULL);
    
    // iter
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_iter_init(obj, &iter);
    yy_assert(yyjson_obj_iter_has_next(&iter) == true);
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = key + 1;
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
            yy_assert(yyjson_obj_iter_has_next(&iter) == true);
        } else if (yyjson_equals_str(key, "b")) {
            yy_assert(yyjson_get_int(val) == 2);
            yy_assert(tmp[1] == 0);
            tmp[1] = 2;
            yy_assert(yyjson_obj_iter_has_next(&iter) == true);
        } else if (yyjson_equals_str(key, "c")) {
            yy_assert(yyjson_get_int(val) == 3);
            yy_assert(tmp[2] == 0);
            tmp[2] = 3;
            yy_assert(yyjson_obj_iter_has_next(&iter) == false);
        } else {
            yy_assert(false);
        }
    }
    yy_assert(yyjson_obj_iter_has_next(&iter) == false);
    yy_assert(tmp[0]);
    yy_assert(tmp[1]);
    yy_assert(tmp[2]);
    
    // foreach
    memset(tmp, 0, sizeof(tmp));
    yyjson_obj_foreach(obj, idx, max, key, val) {
        if (yyjson_equals_str(key, "a")) {
            yy_assert(yyjson_get_int(val) == 1);
            yy_assert(tmp[0] == 0);
            tmp[0] = 1;
        } else if (yyjson_equals_str(key, "b")) {
            yy_assert(yyjson_get_int(val) == 2);
            yy_assert(tmp[1] == 0);
            tmp[1] = 2;
        } else if (yyjson_equals_str(key, "c")) {
            yy_assert(yyjson_get_int(val) == 3);
            yy_assert(tmp[2] == 0);
            tmp[2] = 3;
        } else {
            yy_assert(false);
        }
    }
    yy_assert(tmp[0]);
    yy_assert(tmp[1]);
    yy_assert(tmp[2]);
    
    //---------------------------------------------
    // iter
    yy_assert(yyjson_obj_iter_init(obj, NULL) == false);
    yy_assert(yyjson_obj_iter_init(NULL, &iter) == false);
    yy_assert(yyjson_obj_iter_init(NULL, NULL) == false);
    
    yyjson_doc_free(doc);
}

yy_test_case(test_json_val) {
    test_json_val_api();
    test_json_arr_api();
    test_json_obj_api();
}

#else
yy_test_case(test_json_val) {}
#endif
