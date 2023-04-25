/* Author: Steffen Viken Valvaag <steffenv@cs.uit.no> */
#ifndef LIST_H
#define LIST_H

#include "common.h"

/*
 * The type of lists.
 */
typedef struct list list_t;


/*
 * Creates a new, empty list that uses the given comparison function
 * to compare elements. [The comparison function accepts two elements,
 * and returns -1 if the first is smaller than the second, 1 if the
 * first is greater than the second, and 0 if the elements are equal.]1
 * Returns the new list.
 *
 * NOTE 1: Inaccurate description. Reasoning and correction:
 * The cmpfuncs used within this precode o not return "==-1" or "==1", 
 * but rather an arbitrary int x where (x<0 || x==0 || x>0).
 * Subsequently, greater than or less than is also the checks performed by the 
 * list src provided in this precode.
 * 
 * This follows the standard used by libraries such as string.h, 
 * and this description should be updated to accurately reflect cmpfunc_t.
 */
list_t *list_create(cmpfunc_t cmpfunc);

/*
 * Destroys the given list.  Subsequently accessing the list
 * will lead to undefined behavior.
 */
void list_destroy(list_t *list);

/*
 * Returns the current size of the given list.
 */
int list_size(list_t *list);

/*
 * Adds the given element to the start of the given list.
 * Returns 1 on success, and 0 if the operation failed.
 */
int list_addfirst(list_t *list, void *elem);

/*
 * Adds the given element to the end of the given list.
 * Returns 1 on success, and 0 if the operation failed.
 */
int list_addlast(list_t *list, void *elem);

/*
 * Removes and returns the first element of the given list,
 * or NULL on empty list.
 */
void *list_popfirst(list_t *list);

/*
 * Removes and returns the last element of the given list,
 * or NULL on empty list.
 */
void *list_poplast(list_t *list);

/*
 * Returns 1 if the given list contains the given element, 0 otherwise.
 *
 * The comparison function of the list is used to check elements for equality.
 */
int list_contains(list_t *list, void *elem);

/*
 * Sorts the elements of the given list, using the comparison function
 * of the list to determine the ordering of the elements.
 */
void list_sort(list_t *list);

/*
 * The type of list iterators.
 */
typedef struct list_iter list_iter_t;

/*
 * Creates a new list iterator for iterating over the given list.
 */
list_iter_t *list_createiter(list_t *list);

/*
 * Destroys the given list iterator.
 */
void list_destroyiter(list_iter_t *iter);

/*
 * Returns 0 if the given list iterator has reached the end of the
 * list, or 1 otherwise.
 */
int list_hasnext(list_iter_t *iter);

/*
 * Returns the [next element]1 in the sequence represented by the given list iterator. []2
 * 
 * NOTE 1: Inaccurate description. Correction:
 * Returns the current element of the given iterator.
 * 
 * NOTE 2: Missing information:
 * Increments the iterator, returns NULL when end of list is reached.
 */
void *list_next(list_iter_t *iter);

#endif
