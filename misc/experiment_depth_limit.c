#include "yyjson.h"

#if YYJSON_READER_DEPTH_LIMIT
static void make_nested_json_arrays(char *json, int depth)
{
    char *jsonp = json;
    for (int i = 0; i < depth; i++) {
        *jsonp++ = '[';
    }
    *jsonp++ = '8';
    for (int i = 0; i < depth; i++) {
        *jsonp++ = ']';
    }
    *jsonp = 0;
}
static void make_nested_json_objects(char *json, int depth)
{
    char *jsonp = json;
    for (int i = 0; i < depth; i++) {
        *jsonp++ = '{';
        *jsonp++ = '"';
        *jsonp++ = 'a' + i % 26;
        *jsonp++ = '"';
        *jsonp++ = ':';
    }
    *jsonp++ = '8';
    for (int i = 0; i < depth; i++) {
        *jsonp++ = '}';
    }
    *jsonp = 0;
}

static int check_parse(char *json)
{
    yyjson_read_err err;
    yyjson_doc *doc;
    yyjson_val *val;
    printf("Parsing: %s, depth limit = %d\n", json, YYJSON_READER_DEPTH_LIMIT);
    doc = yyjson_read_opts(json, strlen(json), 0, NULL, &err);
    yyjson_doc_free(doc);
    printf("=> Error code: %d\n", err.code);
    if (err.code != YYJSON_READ_ERROR_DEPTH)
    {
        printf("Expected depth error, but didn't get one!");
        return 1;
    }
    return 0;
}

int main()
{
    char json[512];
    make_nested_json_arrays(json, YYJSON_READER_DEPTH_LIMIT + 1);
    check_parse(json);
    make_nested_json_objects(json, YYJSON_READER_DEPTH_LIMIT + 1);
    check_parse(json);
    return 0;
}
#else
int main()
{
    printf("Library not compiled with depth limit support.\n");
    return 0;
}
#endif
