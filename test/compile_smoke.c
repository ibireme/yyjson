/* Smoke test for old compilers and environments without CMake support */

#include "yyjson.h"

#define yy_assert(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "Assertion failed: %s (%s:%d)\n", \
                #expr, __FILE__, __LINE__); \
        abort(); \
    } \
} while (0)

static void test_read(void) {
    const char *json = "{\"str\":\"Harry\",\"fp\":0.5,\"arr\":[42,-42,null]}";
    yyjson_doc *doc;
    yyjson_val *root, *arr;
    char *out;

    doc = yyjson_read(json, strlen(json), 0);
    yy_assert(doc != NULL);
    
    root = yyjson_doc_get_root(doc);
    yy_assert(yyjson_is_str(yyjson_obj_get(root, "str")));
    yy_assert(yyjson_equals_str(yyjson_obj_get(root, "str"), "Harry"));
    yy_assert(yyjson_is_real(yyjson_obj_get(root, "fp")));
    yy_assert(yyjson_get_real(yyjson_obj_get(root, "fp")) == 0.5);
    
    arr = yyjson_obj_get(root, "arr");
    yy_assert(yyjson_is_arr(arr) && yyjson_arr_size(arr) == 3);
    yy_assert(yyjson_get_int(yyjson_arr_get(arr, 0)) == 42);
    yy_assert(yyjson_get_int(yyjson_arr_get(arr, 1)) == -42);
    yy_assert(yyjson_is_null(yyjson_arr_get(arr, 2)));
    
    out = yyjson_write(doc, 0, NULL);
    yy_assert(out != NULL);
    yy_assert(strcmp(json, out) == 0);
    free(out);
    
    yyjson_doc_free(doc);
}

static void test_write(void) {
    const char *json = "{\"str\":\"Harry\",\"fp\":0.5,\"arr\":[42,-42,null]}";
    yyjson_mut_doc *doc;
    yyjson_mut_val *root, *arr;
    char *out;

    doc = yyjson_mut_doc_new(NULL);
    yy_assert(doc != NULL);

    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "str", "Harry");
    yyjson_mut_obj_add_real(doc, root, "fp", 0.5);

    arr = yyjson_mut_arr(doc);
    yyjson_mut_obj_add(root, yyjson_mut_str(doc, "arr"), arr);
    yyjson_mut_arr_add_int(doc, arr, 42);
    yyjson_mut_arr_add_int(doc, arr, -42);
    yyjson_mut_arr_add_null(doc, arr);

    out = yyjson_mut_write(doc, 0, NULL);
    yy_assert(out != NULL);
    yy_assert(strcmp(out, json) == 0);
    free(out);
    
    yyjson_mut_doc_free(doc);
}

int main(void) {
    printf("yyjson %s smoke test...\n", YYJSON_VERSION_STRING);
    test_read();
    test_write();
    printf("All tests passed\n");
    return 0;
}
