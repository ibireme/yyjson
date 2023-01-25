#include "yyjson.h"
#include "yy_test_utils.h"



static void test_read_err_code(void) {
#if !YYJSON_DISABLE_READER
    yyjson_read_err err;
    const char *str;
    yyjson_alc alc;
    
    
    
    // Success, no error.
    memset(&err, -1, sizeof(err));
    str = "[]";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_SUCCESS);
    yy_assert(err.pos == 0);
    
    
    
    // Invalid parameter, such as NULL input string or 0 input length.
    memset(&err, -1, sizeof(err));
    str = "";
    yyjson_doc_free(yyjson_read_opts((char *)str, 0, 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_PARAMETER);
    yy_assert(err.pos == 0);
    
    memset(&err, -1, sizeof(err));
    str = NULL;
    yyjson_doc_free(yyjson_read_opts((char *)str, 0, 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_PARAMETER);
    yy_assert(err.pos == 0);
    
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_file(NULL, 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_PARAMETER);
    yy_assert(err.pos == 0);
    
    
    
    // Memory allocation failure occurs.
    yyjson_alc_pool_init(&alc, NULL, 0);
    memset(&err, -1, sizeof(err));
    str = "[]";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, &alc, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION);
    yy_assert(err.pos == 0);
    
    
    
    // Input JSON string is empty.
    memset(&err, -1, sizeof(err));
    str = " ";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_EMPTY_CONTENT);
    yy_assert(err.pos == 0);
    
    memset(&err, -1, sizeof(err));
    str = "\n\n\r\n";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_EMPTY_CONTENT);
    yy_assert(err.pos == 0);
    
    
    
    // Unexpected content after document, such as `[1]abc`.
    memset(&err, -1, sizeof(err));
    str = "[1]abc";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
    yy_assert(err.pos == strlen(str) - 3);
    
    memset(&err, -1, sizeof(err));
    str = "[1],";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
    yy_assert(err.pos == strlen(str) - 1);
    
#if !YYJSON_DISABLE_NON_STANDARD
    memset(&err, -1, sizeof(err));
    str = "[1],";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str),
                                     YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_CONTENT);
    yy_assert(err.pos == strlen(str) - 1);
#endif
    
    
    
    // Unexpected ending, such as `[123`.
    memset(&err, -1, sizeof(err));
    str = "[123";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
    yy_assert(err.pos == strlen(str));
    
    memset(&err, -1, sizeof(err));
    str = "[123.";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
    yy_assert(err.pos == strlen(str));
    
    memset(&err, -1, sizeof(err));
    str = "[123e";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
    yy_assert(err.pos == strlen(str));
    
    memset(&err, -1, sizeof(err));
    str = "[123e-";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
    yy_assert(err.pos == strlen(str));
    
    memset(&err, -1, sizeof(err));
    str = "[-";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
    yy_assert(err.pos == strlen(str));
    
    memset(&err, -1, sizeof(err));
    str = "[\"abc";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_UNEXPECTED_END);
    yy_assert(err.pos == strlen(str));
    
    
    
    // Invalid JSON structure, such as `[1,]`.
    memset(&err, -1, sizeof(err));
    str = "[1,]";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_JSON_STRUCTURE);
    yy_assert(err.pos == strlen(str) - 1);
    
    
    
    // Invalid comment, such as unclosed multi-line comment.
#if !YYJSON_DISABLE_NON_STANDARD
    memset(&err, -1, sizeof(err));
    str = "[123]/*";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str),
                                     YYJSON_READ_ALLOW_COMMENTS, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_COMMENT);
    yy_assert(err.pos == strlen(str) - 2);
#endif
    
    
    
    // Invalid number, such as `123.e12`, `000`.
    memset(&err, -1, sizeof(err));
    str = "123.e12";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
    yy_assert(err.pos == 4);
    
    memset(&err, -1, sizeof(err));
    str = "000";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
    yy_assert(err.pos == 0);
    
    memset(&err, -1, sizeof(err));
    str = "[01";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
    yy_assert(err.pos == 1);
    
    memset(&err, -1, sizeof(err));
    str = "[123.]";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_NUMBER);
    yy_assert(err.pos == 5);
    
    
    
    // Invalid string, such as invalid escaped character inside a string.
    memset(&err, -1, sizeof(err));
    str = "\"\\uD800\"";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_INVALID_STRING);
    yy_assert(err.pos == 7);
    
    
    
    // Invalid JSON literal, such as `truu`.
    memset(&err, -1, sizeof(err));
    str = "[truu]";
    yyjson_doc_free(yyjson_read_opts((char *)str, strlen(str), 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_LITERAL);
    yy_assert(err.pos == 1);
    
    
    
    // Failed to open a file.
    memset(&err, -1, sizeof(err));
    yyjson_doc_free(yyjson_read_file("/yyjson/no_such_file.test", 0, NULL, &err));
    yy_assert(err.code == YYJSON_READ_ERROR_FILE_OPEN);
    yy_assert(err.pos == 0);
    
#endif
}



static void test_write_err_code(void) {
#if !YYJSON_DISABLE_WRITER
    yyjson_mut_doc *doc;
    yyjson_mut_val *val;
    yyjson_write_err err;
    char *json;
    yyjson_alc alc;
    
    
    
    // Success, no error.
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_int(doc, 123);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);
    yy_assert(strcmp(json, "123") == 0);
    yyjson_mut_doc_free(doc);
    free(json);
    yy_assert(err.code == YYJSON_WRITE_SUCCESS);
    
    
    
    // Invalid parameter, such as NULL document.
    memset(&err, -1, sizeof(err));
    json = yyjson_mut_write_opts(NULL, 0, NULL, NULL, &err);
    yy_assert(json == NULL);
    yy_assert(err.code == YYJSON_WRITE_ERROR_INVALID_PARAMETER);
    
    
    
    // Memory allocation failure occurs.
    yyjson_alc_pool_init(&alc, NULL, 0);
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_int(doc, 123);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, &alc, NULL, &err);
    yy_assert(json == NULL);
    yyjson_mut_doc_free(doc);
    yy_assert(err.code == YYJSON_WRITE_ERROR_MEMORY_ALLOCATION);
    
    
    
    // Invalid value type in JSON document.
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_int(doc, 123);
    unsafe_yyjson_set_type(val, YYJSON_TYPE_NONE, YYJSON_SUBTYPE_NONE);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);
    yy_assert(json == NULL);
    yyjson_mut_doc_free(doc);
    yy_assert(err.code == YYJSON_WRITE_ERROR_INVALID_VALUE_TYPE);
    
    
    
    // NaN or Infinity number occurs.
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_real(doc, INFINITY);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);
    yy_assert(json == NULL);
    yyjson_mut_doc_free(doc);
    yy_assert(err.code == YYJSON_WRITE_ERROR_NAN_OR_INF);
    
    
    
    // Invalid unicode in string.
    memset(&err, -1, sizeof(err));
    doc = yyjson_mut_doc_new(NULL);
    val = yyjson_mut_strn(doc, "abc\x80", 4);
    yyjson_mut_doc_set_root(doc, val);
    json = yyjson_mut_write_opts(doc, 0, NULL, NULL, &err);
    yy_assert(json == NULL);
    yyjson_mut_doc_free(doc);
    yy_assert(err.code == YYJSON_WRITE_ERROR_INVALID_STRING);
    
#endif
}



yy_test_case(test_err_code) {
    test_read_err_code();
    test_write_err_code();
}
