#ifndef TREE_H
#define TREE_H

#include "common.h"

typedef struct tree tree_t;
typedef struct tree_iter tree_iter_t;

/* Creates a tree using the given comparison function.
 * Returns a pointer to the newly created tree, or
 * NULL if memory allocation failed */
tree_t *tree_create(cmpfunc_t cmpfunc);

/* Destroys the given tree */
void tree_destroy(tree_t *tree);

/* Returns the current number of nodes in the tree */
int tree_size(tree_t *tree);

/* Uses cmpfunc to check elements for equality.
 * * Returns 1 if tree has an equal value element
 * * otherwise, returns 0 */
int tree_contains(tree_t *tree, void *elem);

/* Adds the given element to the tree
 * (ignores duplicate elements) */
void tree_add(tree_t *tree, void *elem);

/*
 * Creates a new tree iterator for iterating over the given tree.
 * takes in a tree and a order of iteration:
 * * inOrder == 0 => pre-order (NOT YET IMPLEMENTED)
 * * inOrder == 1 => in-order (a < b < c ... < z)
 * 
 * Note: never alted the structure of a tree while an iterator is active for that tree.
 * (i.e., do NOT perform insertion / deletion.)
 * 
 * Note1: It's imperative the iterator be destroyed with tree_destroyiter after use.
 */
tree_iter_t *tree_createiter(tree_t *tree, int inOrder);

/* Destroys the given tree iterator */
void tree_destroyiter(tree_iter_t *iter);

/* Returns a pointer to the current node, then increments iter
 * returns NULL when all tree items have been iterated over */
void *tree_next(tree_iter_t *iter);

#endif /* TREE_H */
