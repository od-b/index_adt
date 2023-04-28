/* 
 * Simple stack implementation.
 * Named pile for the obvious namespace issues.
 * allows peeking and may clean the plates. (free elems)
*/

#include "pile.h"

#include <stdlib.h>


struct plate;
typedef struct plate plate_t;

struct plate {
    void *elem;
    plate_t *next;
};

struct pile {
    int height;
    plate_t *top;
};


pile_t *pile_create() {
    pile_t *pile = malloc(sizeof(pile_t));
    if (pile == NULL) {
        return NULL;
    }

    pile->height = 0;
    pile->top = NULL;
    return pile;
}

void pile_destroy(pile_t *pile) {
    plate_t *p;
    while (pile->top != NULL) {
        p = pile->top;
        pile->top = pile->top->next;
        free(p);
    }
    free(pile);
}

void pile_push(pile_t *pile, void *elem) {
    plate_t *p = malloc(sizeof(plate_t));
    if (p != NULL) {
        p->elem = elem;
        p->next = pile->top;
        pile->top = p;
        pile->height++;
    }
}

void *pile_pop(pile_t *pile) {
    if (!pile->height) {
        return NULL;
    }

    plate_t *p = pile->top;
    void *elem = p->elem;
    pile->top = p->next;

    free(p);
    pile->height--;

    return elem;
}

void *pile_peek(pile_t *pile, int depth) {
    if (!pile->height) {
        return NULL;
    }

    plate_t *p = pile->top;

    for (int i = 0; (i < depth && p->next != NULL); p = p->next, i++);

    return p;
}

int pile_size(pile_t *pile) {
    return pile->height;
}

void pile_cleanplates(pile_t *pile, void (*freefunc)(void *)) {
    if (freefunc != NULL) {
        plate_t *p;
        while (pile->top != NULL) {
            p = pile->top;
            pile->top = pile->top->next;
            freefunc(p->elem);
            free(p);
        }
        pile->height = 0;
    }
}
