/* Author: Steffen Viken Valvaag <steffenv@cs.uit.no> */
#ifndef SET_H
#define SET_H

#include "common.h"

/*
 * The type of sets.
 */
struct set;
typedef struct set set_t;

/*
 * Search within the given set for an elem equal to the provided one.
 * If an equal elem exists in the set, returns it. 
 * Otherwise, returns NULL.
 */
void *set_get(set_t *set, void *elem);

/*
 * Try to add a given elem to the given set. 
 * 
 * Returns:
 * a) The `duplicate` elem from the set, if it exists.
 * b) The provided elem, if added to the set.
 * c) NULL, on error (e.g. out of memory).
 */
void *set_tryadd(set_t *set, void *elem);

/*
 * Creates a new set. The given cmpfunc will be used
 * to compare elements added to the set.
 */
set_t *set_create(cmpfunc_t cmpfunc);

/*
 * Destroys the given set. Subsequently accessing the set
 * will lead to undefined behavior.
 */
void set_destroy(set_t *set);

/*
 * Returns the cardinality of the given set.
 */
int set_size(set_t *set);

/*
 * Adds the given element to the given set.
 */
void set_add(set_t *set, void *elem);

/*
 * Returns 1 if the given element is contained in
 * the given set, otherwise returns 0.
 */
int set_contains(set_t *set, void *elem);

/*
 * Returns the union of the two given sets; the returned
 * set contains all elements that are contained in either
 * a or b.
 */
set_t *set_union(set_t *a, set_t *b);

/*
 * Returns the intersection of the two given sets; the
 * returned set contains all elements that are contained
 * in both a and b.
 */
set_t *set_intersection(set_t *a, set_t *b);

/*
 * Returns the set difference of the two given sets; the
 * returned set contains all elements that are contained
 * in a and not in b.
 */
set_t *set_difference(set_t *a, set_t *b);

/*
 * Returns a copy of the given set.
 */
set_t *set_copy(set_t *set);

/*
 * The type of set iterators.
 */
struct set_iter;
typedef struct set_iter set_iter_t;

/*
 * Creates a new set iterator for iterating over the given set.
 */
set_iter_t *set_createiter(set_t *set);

/*
 * Destroys the given set iterator.
 */
void set_destroyiter(set_iter_t *iter);

/*
 * Returns 0 if the given set iterator has reached the end of the
 * set, or 1 otherwise.
 */
int set_hasnext(set_iter_t *iter);

/*
 * Returns the next element in the sequence represented by the given
 * set iterator.
 */
void *set_next(set_iter_t *iter);

/* debugging */
// typedef char *(*printfunc_t)(void *);
// void print_rbtreeset(set_t *set, printfunc_t printfunc_t);


#endif
