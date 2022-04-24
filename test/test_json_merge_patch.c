#include "yyjson.h"
#include "yy_test_utils.h"

static void test_one(const char *original_json,
                     const char *patch_json,
                     const char *want_result_json) {
#if !YYJSON_DISABLE_READER
    yyjson_doc *original_doc = yyjson_read(original_json, strlen(original_json), 0);
    yyjson_doc *patch_doc = yyjson_read(patch_json, strlen(patch_json), 0);
    yyjson_doc *want_result_doc = yyjson_read(want_result_json, strlen(want_result_json), 0);
    yyjson_mut_doc *mut_want_result_doc = yyjson_doc_mut_copy(want_result_doc, NULL);
    
    yyjson_mut_doc *result_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *result = yyjson_merge_patch(result_doc, original_doc->root, patch_doc->root);
    
    yy_assert(yyjson_mut_equals(mut_want_result_doc->root, result));
    
    yy_assert(yyjson_merge_patch(NULL, NULL, NULL) == NULL);
    yy_assert(yyjson_merge_patch(NULL, original_doc->root, NULL) == NULL);
    yy_assert(yyjson_merge_patch(NULL, NULL, patch_doc->root) == NULL);
    yy_assert(yyjson_merge_patch(NULL, original_doc->root, patch_doc->root) == NULL);
    yy_assert(yyjson_merge_patch(result_doc, original_doc->root, NULL) == NULL);
    yy_assert(yyjson_merge_patch(result_doc, NULL, patch_doc->root) != NULL);
    
    yyjson_mut_doc_free(result_doc);
    yyjson_mut_doc_free(mut_want_result_doc);
    yyjson_doc_free(want_result_doc);
    yyjson_doc_free(patch_doc);
    yyjson_doc_free(original_doc);
#endif
}

yy_test_case(test_json_merge_patch) {
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
}
