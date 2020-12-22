#include "yyjson.h"
#include "yy_test_utils.h"

yy_test_case(test_json_pointer) {
#if !YYJSON_DISABLE_READER
    
    
    {
        // test case from spec: https://tools.ietf.org/html/rfc6901
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
    
    {
        // test number key
        const char *json = "{\"a\":{\"1\":{\"b\":-1}}}";
        
        yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
        yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/a/1/b")) == -1);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/1/b/") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/1/b~") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/0/b") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/0/") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "a") == NULL);
        
        yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
        yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/a/1/b")) == -1);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/1/b/") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/1/b~") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/0/") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "a") == NULL);
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
    }
    
    {
        // test long path (larger than 512)
        const char *json = "{\"a\":{\"123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\":{\"b\":-1}}}";
        
        yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
        yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/a/123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b")) == -1);
        
        yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
        yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/a/123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b")) == -1);
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
    }
    
    {
        // test long path with escaped character (larger than 512)
        const char *json = "{\"a\":{\"12345/67890~12345/678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\":{\"b\":-1}}}";
        
        yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
        yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/a/12345~167890~012345~1678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b")) == -1);
        
        yy_assert(yyjson_doc_get_pointer(doc, "/a/12345~167890~012345~2678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b") == NULL);
        
        yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
        yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/a/12345~167890~012345~1678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b")) == -1);
        
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/12345~167890~012345~2678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890/b") == NULL);
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
    }
    
    
    {
        // test large index
        const char *json = "{\"a\":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30]}";
        
        yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
        yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/a/0")) == 1);
        yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/a/1")) == 2);
        yy_assert(yyjson_get_int(yyjson_doc_get_pointer(doc, "/a/29")) == 30);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/30") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/00") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/01") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/1 ") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/ 1") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/100") == NULL);
        yy_assert(yyjson_doc_get_pointer(doc, "/a/100000000000000000000") == NULL);
        
        yyjson_mut_doc *mdoc = yyjson_doc_mut_copy(doc, NULL);
        yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/a/0")) == 1);
        yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/a/1")) == 2);
        yy_assert(yyjson_mut_get_int(yyjson_mut_doc_get_pointer(mdoc, "/a/29")) == 30);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/30") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/00") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/01") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/1 ") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/ 1") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/100") == NULL);
        yy_assert(yyjson_mut_doc_get_pointer(mdoc, "/a/100000000000000000000") == NULL);
        
        yyjson_doc_free(doc);
        yyjson_mut_doc_free(mdoc);
    }
    
    {
        // test escaped character
    }
    
#endif
}
