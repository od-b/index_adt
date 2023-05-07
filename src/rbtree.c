#include "tree.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


/*
 * ----- README: REFERENCES & INFO -----
 * 
 * A vast majority of the code within this file is taken from an earlier assignment.
 * 
 * All code and implementations contained within this file is my own work.
 * 
 * -------------------------------------
 *
 * Implementation of a Red-Black Binary Search Tree.
 * The iterator is in-order, and uses no temporary list, stack or queue.
*/


typedef struct treenode {
    struct treenode *parent;
    struct treenode *left;
    struct treenode *right;
    void *elem;
    int8_t black;
} treenode_t;

typedef struct tree {
    treenode_t *root;
    /* The sentinel-node (NIL) functions as a 'colored NULL-pointer' for leaf nodes 
     * eliminates a lot of edge-case conditions for the various rotations, etc. */
    treenode_t *NIL;
    cmpfunc_t cmpfunc;
    unsigned int size;
} tree_t;


tree_t *tree_create(cmpfunc_t cmpfunc) {
    tree_t *new_tree = malloc(sizeof(tree_t));
    treenode_t *sentinel = malloc(sizeof(treenode_t));
    if (new_tree == NULL || sentinel == NULL) {
        ERROR_PRINT("out of memory\n");
        return NULL;
    }
    
    /* create the sentinel */
    sentinel->left = NULL;
    sentinel->right = NULL;
    sentinel->parent = NULL;
    sentinel->elem = NULL;
    sentinel->black = 1;

    new_tree->NIL = sentinel;
    new_tree->root = sentinel;
    new_tree->cmpfunc = cmpfunc;
    new_tree->size = 0;

    return new_tree;
}

int tree_size(tree_t *tree) {
    return tree->size;
}

/* recursive part of tree_destroy */
static void node_destroy(treenode_t *node) {
    // postorder recursive call stack
    if (node->elem == NULL) return;  // ... if node == tree->NIL
    node_destroy(node->left);
    node_destroy(node->right);
    free(node);
}

void tree_destroy(tree_t *tree) {
    // call the recursive part first
    node_destroy(tree->root);
    // free sentinel (NIL-node), then tree itself
    free(tree->NIL);
    free(tree);
}


/* ---------------Insertion, searching, rotation-----------------
 * TODO: Deletion, tree copying
*/

/* rotate node counter-clockwise */
static void rotate_left(tree_t *tree, treenode_t *a) {
    treenode_t *b = a->right;
    treenode_t *c = a->right->left;

    /* fix root _or_ node a parents' left/right pointers */
    if (a == tree->root) {
        tree->root = b;
    } else {
        (a->parent->left == a) ? (a->parent->left = b) : (a->parent->right = b);
    }

    /* rotate parent-pointers */
    b->parent = a->parent;
    a->parent = b;
    if (c != tree->NIL) c->parent = a;     // c can be a NIL-node

    /* rotate left/right pointers */
    a->right = c;
    b->left = a;
}

/* rotate node clockwise */
static void rotate_right(tree_t *tree, treenode_t *a) {
    treenode_t *b = a->left;
    treenode_t *c = a->left->right;

    /* change root _or_ node a parents' left/right pointers */
    if (a == tree->root) {
        tree->root = b;
    } else {
        (a->parent->left == a) ? (a->parent->left = b) : (a->parent->right = b);
    }

    /* rotate parent-pointers */
    b->parent = a->parent;
    a->parent = b;
    if (c != tree->NIL) c->parent = a;      // c can be a NIL-node

    /* rotate left/right pointers */
    a->left = c;
    b->right = a;
}

void *tree_search(tree_t *tree, void *elem) {
    treenode_t *curr = tree->root;
    int direction;

    /* traverse until a NIL-node, or return is an equal element is found */
    while (curr != tree->NIL) {
        direction = tree->cmpfunc(elem, curr->elem);
        if (direction > 0) {
            curr = curr->right;
        } else if (direction < 0) {
            curr = curr->left;
        } else {
            return curr->elem;
        }
    }

    /* not contained in tree */
    return NULL;
}

/* balance the tree after adding a node */
static void post_add_balance(tree_t *T, treenode_t *added_node) {
    int_fast8_t curr_is_leftchild, par_is_leftchild;
    treenode_t *curr, *par, *unc, *gp;

    curr = added_node;
    while (!curr->parent->black) {
        par = curr->parent;
        gp = par->parent;

        // determine uncle by parent to grandparent relation
        (gp->left == par) ? (unc = gp->right) : (unc = gp->left);

        // if uncle is red
        if (!unc->black) {
            par->black = 1;
            unc->black = 1;
            if (gp != T->root) gp->black = 0;
            // grandparent may have a red parent at this point, set curr to gp and re-loop
            curr = gp;
        } else {
            // determine currents' parent relation (what side)
            (gp->left == par) ? (par_is_leftchild = 1) : (par_is_leftchild = 0);

            // determine parents' relation to their parent (what side)
            (par->left == curr) ? (curr_is_leftchild = 1) : (curr_is_leftchild = 0);

            // Is currents' and uncles' parent relation equal?
            if (par_is_leftchild != curr_is_leftchild) {
                // rotate parent 'away'
                (curr_is_leftchild) ? (rotate_right(T, par)) : (rotate_left(T, par));
            } else {
                // rotate grandparent 'away'
                (curr_is_leftchild) ? (rotate_right(T, gp)) : (rotate_left(T, gp));
                // fix colors
                par->black = 1;
                if (gp != T->root) gp->black = 0;
            }
        }
    }
}

void *tree_tryadd(tree_t *tree, void *elem) {
    /* case: tree does not have a root yet */
    if (tree->size == 0) {
        treenode_t *root = malloc(sizeof(treenode_t));
        if (root == NULL) {
            return NULL;
        }
        root->elem = elem;
        root->left = tree->NIL;
        root->right = tree->NIL;
        root->parent = tree->NIL;
        root->black = 1;
        tree->root = root;
        tree->size = 1;
        return elem;
    }

    treenode_t *curr = tree->root;
    int direction;
    treenode_t *new_node;

    /* iteratively add the node.
     * traverses the tree using cmpfunc until a NIL-node, or node with equal element is found.
     * if an equal element if found, returns it. Otherwise, creates a new node and breaks the loop.
     */
    while (1) {
        direction = tree->cmpfunc(elem, curr->elem);
        if (direction > 0) {
            if (curr->right == tree->NIL) {
                new_node = malloc(sizeof(treenode_t));
                if (new_node == NULL) {
                    return NULL;
                }
                curr->right = new_node;
                break;
            }
            // else ... move right in tree
            curr = curr->right;
        } else if (direction < 0) {
            if (curr->left == tree->NIL) {
                // same as above, but curr->left
                new_node = malloc(sizeof(treenode_t));
                if (new_node == NULL) {
                    return NULL;
                }
                curr->left = new_node;
                break;
            }
            // else ... move left in tree
            curr = curr->left;
        } else {
            /* duplicate elem found. return a pointer to the duplicate elem. */
            return curr->elem;
        }
    }

    /* new node was added, increment tree size and initialize attributes of the new node. */
    tree->size++;
    new_node->parent = curr;
    new_node->elem = elem;
    new_node->left = tree->NIL;
    new_node->right = tree->NIL;
    new_node->black = 0;

    /* if new node parent is red, call for balancing */
    if (!new_node->parent->black) {
        post_add_balance(tree, new_node);
    }

    return elem;
}

typedef struct tree_iter {
    tree_t *tree;
    treenode_t *node;
} tree_iter_t;

tree_iter_t *tree_createiter(tree_t *tree) {
    tree_iter_t *new_iter = malloc(sizeof(tree_iter_t));
    if (new_iter == NULL) {
        return NULL;
    }

    new_iter->tree = tree;
    new_iter->node = tree->root;
    return new_iter;
}

/* Morris traversal implementation
 * Works by creating 'paths' between a subtrees 'far right leaf' and (sub)root,
 * through temporary alteration of the tree leaves' ->right pointer.
 */
void *tree_next(tree_iter_t *iter) {
    if (iter->node == iter->tree->NIL) {
        return NULL;
    }

    /* Morris traversal implementation
     * Works by creating 'paths' between a subtrees 'far right leaf' and (sub)root,
     * through temporary alteration of the tree leaves' ->right pointer.
    */

    treenode_t *N = iter->node;    // alias for iter->node.
    void *next = NULL;             // elem to be returned

    while (next == NULL) {
        if (N->left == iter->tree->NIL) {
            /* can't traverse further left in current subtree, so move right */
            next = N->elem;
            N = N->right;
        } else {
            /* N has a left child. create a predecessor pointer, P, starting at the left child.*/
            treenode_t *P = N->left;

            /* Traverse right with P go right untill we hit a leaf or *N */
            while ((P->right != iter->tree->NIL) && (P->right != N)) {
                P = P->right;
            }
            /* Determine if we hit *N, or if we and need to create a new path */
            if (P->right == iter->tree->NIL) {
                /* set the pointer at leaf->right to point to N, then move N left */
                P->right = N;
                N = N->left;
            } else {
                /* we have previously traversed the path using N. Destroy the path and move N right */
                P->right = iter->tree->NIL;
                next = N->elem;
                N = N->right;
            }
        }
    }

    /* increment iter to the next node, then return */
    iter->node = N;

    return next;
}

int tree_hasnext(tree_iter_t *iter) {
    if (iter->node == iter->tree->NIL)
        return 0;
    return 1;
}

void tree_destroyiter(tree_iter_t *iter) {
    if (tree_hasnext(iter)) {
        /* Finish the morris iterator process to avoid leaving any mutated leaves
         * this loop does nothing other than correctly finish the iteration process 
         * TODO: iterate backwards instead, to erase modified leaf pointers. For now,
         * this is a temp fix, and won't matter much as an iter typically is used to
         * access all the nodes instead of searching.
         */
        for (void *elem = tree_next(iter); elem; elem = tree_next(iter));
    }
    free(iter);
}
