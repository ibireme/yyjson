#include "yyjson.c"

int main(void) {
    return !!yyjson_read(" ", 1, 0);
}
