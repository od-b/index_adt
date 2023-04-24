#ifndef nstack_H
#define nstack_H

#include "common.h"

struct nstack;

/* type of nstack :^)
 * named nstack in order to not cause namespace issues */
typedef struct nstack nstack_t;

nstack_t *nstack_create();

void nstack_destroy(nstack_t *stack);

void nstack_push(nstack_t *stack, void *elem);

/* remove the top plate and return its elem */
void *nstack_pop(nstack_t *stack);

/* return the elem at the given depth. 0 = top plate.
 * if the depth is out of range, returns the last elem in the stack.
*/
void *nstack_peek(nstack_t *stack, int depth);

int nstack_height(nstack_t *stack);

/* clears all plates and their elems (by using free(elem)) 
 * does not destroy the stack struct itself.
*/ 
void nstack_cleanplates(nstack_t *stack);

#endif
