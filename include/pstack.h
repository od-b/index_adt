#ifndef pstack_H
#define pstack_H

#include "common.h"

struct pstack;

/* type of pstack :^)
 * named pstack in order to not cause namespace issues */
typedef struct pstack pstack_t;

pstack_t *pstack_create();

void pstack_destroy(pstack_t *stack);

void *pstack_push(pstack_t *stack, void *elem);

/* remove the top plate and return its elem */
void *pstack_pop(pstack_t *stack);

/* return the elem at the given depth. 0 = top plate.
 * if the depth is out of range, returns the last elem in the stack.
*/
void *pstack_peek(pstack_t *stack, int depth);

int pstack_size(pstack_t *stack);

/* clears all plates and their elems (by using free(elem)) 
 * does not destroy the stack struct itself.
*/ 
void pstack_cleanplates(pstack_t *stack);

#endif
