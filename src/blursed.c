#include "common.h"
#include <stdlib.h>
#include <string.h>

enum tok_t {
    L_PAREN = -2,
    R_PAREN = -1,
    OR = 1,
    AND = 2,
    ANDNOT = 3,
    TERM = 2
};

int main() {
    char *a = strdup("a");

    char *b = calloc(5, sizeof(char));
    strncpy(b, "nine", 5);
}
