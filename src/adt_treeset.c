#include "tree.h"

/*
 * set using any given tree defined in tree.h
*/

struct set;
typedef struct set set_t;

struct set {
    tree_t *tree;
    cmpfunc_t cmpfunc;
};

set_t *set_create(cmpfunc_t cmpfunc) {
    set_t *new_set = malloc(sizeof(set_t));

    if (new_set == NULL) {
        ERROR_PRINT("out of memory\n");
    }

    // create and assign a tree to the set
    new_set->cmpfunc = cmpfunc;
    new_set->tree = tree_create(cmpfunc);

    if (new_set->tree == NULL){
        ERROR_PRINT("out of memory\n");
    }

    return new_set;
}

void set_destroy(set_t *set) {
    tree_destroy(set->tree);
    free(set);
}

int set_size(set_t *set) {
    return tree_size(set->tree);
}

void set_add(set_t *set, void *elem) {
    /* tree already takes in account not to add duplicates */
    tree_add(set->tree, elem);
}

int set_contains(set_t *set, void *elem) {
    return tree_contains(set->tree, elem);
}

set_t *set_union(set_t *a, set_t *b) {
    if (a->cmpfunc != b->cmpfunc) {
        ERROR_PRINT("sets must have the same cmpfunc\n");
    }
    set_t *c = set_create(a->cmpfunc);
    set_iter_t *iter_a = set_createiter(a);
    set_iter_t *iter_b = set_createiter(b);

    if (c == NULL || iter_a == NULL || iter_b == NULL) {
        ERROR_PRINT("out of memory\n");
    }

    void *elem;
    while ((elem = set_next(iter_a)) != NULL) {
        set_add(c, elem);
    }
    while ((elem = set_next(iter_b)) != NULL) {
        set_add(c, elem);
    }

    // clean up
    set_destroyiter(iter_a);
    set_destroyiter(iter_b);

    // return set union
    return c;
}

set_t *set_intersection(set_t *a, set_t *b) {
    if (a->cmpfunc != b->cmpfunc) {
        ERROR_PRINT("sets must have the same cmpfunc\n");
    }
    set_t *c = set_create(a->cmpfunc);
    set_iter_t *iter_a = set_createiter(a);

    if (c == NULL || iter_a == NULL) {
        ERROR_PRINT("error allocating memory @ set_difference");
    }

    void *elem_a;
    while ((elem_a = set_next(iter_a)) != NULL) {
        // if set b contains elem from set a, add it to c
        if (set_contains(b, elem_a)) set_add(c, elem_a);
    }

    // clean up
    set_destroyiter(iter_a);

    // return set set_intersection
    return c;
}

set_t *set_difference(set_t *a, set_t *b) {
    if (a->cmpfunc != b->cmpfunc) {
        ERROR_PRINT("sets must have the same cmpfunc\n");
    }
    set_t *c = set_create(a->cmpfunc);
    set_iter_t *iter_a = set_createiter(a);

    if (c == NULL || iter_a == NULL) {
        ERROR_PRINT("out of memory\n");
    }

    void *elem_a;
    while ((elem_a = set_next(iter_a)) != NULL) {
        // if set b does NOT contain elem from set a, add it to c
        if (!set_contains(b, elem_a)) set_add(c, elem_a);
    }

    // clean up
    set_destroyiter(iter_a);

    // return set difference
    return c;
}

/* 
 * TODO: set copy without cmpfunc
 */
set_t *set_copy(set_t *set) {
    set_t *copy = malloc(sizeof(set_t));

    if (copy == NULL) {
        ERROR_PRINT("out of memory\n");
    }

    // memcpy(copy, set, sizeof(set_t));
    return copy;
}


struct set_iter;
typedef struct set_iter set_iter_t;

struct set_iter {
    tree_iter_t *tree_iter;
    tree_t *tree;
};


set_iter_t *set_createiter(set_t *set) {
    set_iter_t *new_set_iter = malloc(sizeof(set_iter_t));
    tree_iter_t *new_tree_iter = tree_createiter(set->tree, 1);

    if (new_tree_iter == NULL || new_set_iter == NULL) {
        ERROR_PRINT("out of memory\n");
    }

    new_set_iter->tree_iter = new_tree_iter;
    new_set_iter->tree = set->tree;
    return new_set_iter;
}

void set_destroyiter(set_iter_t *iter) {
    free(iter->tree_iter);
    free(iter);
}

int set_hasnext(set_iter_t *iter) {
    return tree_hasnext(iter->tree_iter);
}

void *set_next(set_iter_t *iter) {
    return tree_next(iter->tree_iter);
}

