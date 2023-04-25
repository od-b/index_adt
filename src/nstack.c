/* 
 * simple stack implementation. 
 * allows peeking and offers plate cleanup, although only through 'free'.
*/

#include "nstack.h"
#include "printing.h"

#include <stdlib.h>

struct plate;
typedef struct plate plate_t;

struct plate {
    void *elem;
    plate_t *next;
};

struct nstack {
    int height;
    plate_t *top;
};

nstack_t *nstack_create() {
    nstack_t *stack = malloc(sizeof(nstack_t));
    if (stack == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    stack->height = 0;
    stack->top = NULL;
    return stack;
}

void nstack_destroy(nstack_t *stack) {
    plate_t *p;
    while (stack->top != NULL) {
        p = stack->top;
        stack->top = stack->top->next;
        free(p);
    }
    free(stack);
}

void nstack_push(nstack_t *stack, void *elem) {
    plate_t *p = malloc(sizeof(plate_t));
    if (p == NULL) {
        ERROR_PRINT("out of memory");
    }
    p->elem = elem;
    p->next = stack->top;
    stack->top = p;
    stack->height++;
}

void *nstack_pop(nstack_t *stack) {
    if (!stack->height) {
        return NULL;
    }

    plate_t *p = stack->top;
    void *elem = p->elem;
    stack->top = p->next;

    free(p);
    stack->height--;

    return elem;
}

void *nstack_peek(nstack_t *stack, int depth) {
    if (!stack->height) {
        return NULL;
    }

    plate_t *p = stack->top;

    for (int i = 0; (i < depth && p->next != NULL); p = p->next, i++);

    return p->elem;
}

int nstack_height(nstack_t *stack) {
    return stack->height;
}

void nstack_cleanplates(nstack_t *stack) {
    plate_t *p;
    while (stack->top != NULL) {
        p = stack->top;
        stack->top = stack->top->next;
        free(p->elem);
        free(p);
    }
    stack->height = 0;
}
