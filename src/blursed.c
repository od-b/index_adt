#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <string.h>
#include <ctype.h>

int main(int argc, char **argv) {
    if (argc != 2) return 1;
    printf("original: %s\n", argv[1]);

    size_t len = strlen(argv[1]);
    char buf[len+1];
    buf[len] = 0;
    size_t i;

    for (i = 0; i < len; i++) {
        buf[i] = tolower(argv[1][i]);
    }
    printf("after tolower: %s\n", buf);

    for (i = 0; i < len; i++) {
        buf[i] = toupper(buf[i]);
    }
    printf("to upper again: %s\n", buf);

    for (i = 0; i < len; i++) {
        assert(isupper(buf[i]));
    }

    return 0;
}
