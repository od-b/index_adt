/* 
 * simple stack implementation. 
 * allows peeking and offers plate cleanup, although only through 'free'.
*/

#include "pstack.h"
#include "printing.h"

#include <stdlib.h>

struct plate;
typedef struct plate plate_t;

struct plate {
    void *elem;
    plate_t *next;
};

struct pstack {
    int height;
    plate_t *top;
};

pstack_t *pstack_create() {
    pstack_t *stack = malloc(sizeof(pstack_t));
    if (stack == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    stack->height = 0;
    stack->top = NULL;
}

void pstack_destroy(pstack_t *stack) {
    plate_t *p;
    while (stack->top != NULL) {
        p = stack->top;
        stack->top = stack->top->next;
        free(p);
    }
    free(stack);
}

void *pstack_push(pstack_t *stack, void *elem) {
    plate_t *p = malloc(sizeof(plate_t));
    if (p == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }
    p->elem = elem;
    p->next = stack->top;
    stack->top = p;
    stack->height++;
}

void *pstack_pop(pstack_t *stack) {
    if (stack->top == NULL) {
        return NULL;
    }
    void *elem = stack->top->elem;
    plate_t *new_top = stack->top->next;
    free(stack->top);

    stack->top = new_top;
    stack->height--;
    return elem;
}

void *pstack_peek(pstack_t *stack, int depth) {
    plate_t *p = stack->top;
    if (p == NULL) {
        return NULL;
    }

    for (int i = 0; (i < depth && p->next != NULL); p = p->next, i++);

    return p->elem;
}

int pstack_height(pstack_t *stack) {
    return stack->height;
}

void pstack_cleanplates(pstack_t *stack) {
    plate_t *p;
    while (stack->top != NULL) {
        p = stack->top;
        stack->top = stack->top->next;
        free(p->elem);
        free(p);
    }
    stack->height = 0;
}
