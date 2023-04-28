#ifndef PILE_H
#define PILE_H

/* 
 * type of pile :^)
 * named pile for the obvious namespace issues
 */
typedef struct pile pile_t;

pile_t *pile_create();

/*
 * Destroys the pile. The elems are not destroyed.
 */
void pile_destroy(pile_t *pile);

void pile_push(pile_t *pile, void *elem);

/* 
 * Returns the most recently added elem, removing it from the pile.
 * Returns NULL if the pile is empty.
 */
void *pile_pop(pile_t *pile);

/* 
 * return the elem at the given depth, without removing it. 0 = top plate.
 * if the given depth is out of range, returns the last elem in the pile.
 */
void *pile_peek(pile_t *pile, int depth);

int pile_size(pile_t *pile);

/* 
 * clears all plates, freeing their elems using the given func.
 * does not destroy the pile itself.
 */ 
void pile_cleanplates(pile_t *pile, void (*freefunc)(void *));


#endif  /* PILE_H already defined */
