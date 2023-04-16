#include "printing.h"
#include "tree.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


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
    int size;
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

    /* set tree attributes to their default values */
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

void *tree_contains(tree_t *tree, void *elem) {
    treenode_t *curr = tree->root;
    int8_t direction;

    /* traverse until a NIL-node, or return is an equal element is found */
    while (curr != tree->NIL) {
        direction = tree->cmpfunc(elem, curr->elem);
        if (direction > 0) {
            curr = curr->right;
        } else if (direction < 0) {
            curr = curr->left;
        } else {
            // ... direction == 0, tree contains an equal element
            return curr->elem;
        }
    }

    // tree does not contain an equal element
    return NULL;
}

/* 
 * Iterative node adding
 * This approach is a bit ugly/spacious, but does not allocate any
 * memory until we're certain elem is to be added
 * returns:
 * a) the added node, if it was succesfully added
 * b) NULL if malloc failed
 * c) tree->NIL if element already exists (not adding duplicate)
 */
static treenode_t *node_add(tree_t *tree, void *elem) {
    treenode_t *NIL = tree->NIL;
    treenode_t *curr = tree->root;
    int8_t direction;

    /* traverse until a NIL-node, or node with equal element is found */
    while ((direction = tree->cmpfunc(elem, curr->elem)) != 0) {
        if (direction > 0) {
            if (curr->right == NIL) {
                treenode_t *new_node = malloc(sizeof(treenode_t));
                if (new_node != NULL) {
                    new_node->parent = curr;
                    curr->right = new_node;
                }
                return new_node;
            }
            // else ... move right in tree
            curr = curr->right;
        } else {
            // ... direction < 0
            if (curr->left == NIL) {
                // same as above, but curr->left
                treenode_t *new_node = malloc(sizeof(treenode_t));
                if (new_node != NULL) {
                    new_node->parent = curr;
                    curr->left = new_node;
                }
                return new_node;
            }
            // else ... move left in tree
            curr = curr->left;
        }
    }
    // cmpfunc == 0, don't add the elem
    return tree->NIL;
}

/* balance the tree after adding a node */
static void post_add_balance(tree_t *T, treenode_t *added_node) {
    int8_t curr_is_leftchild, par_is_leftchild;
    treenode_t *curr, *par, *unc, *gp;
    curr = added_node;

    while (!curr->parent->black) {
        par = curr->parent;
        gp = par->parent;

        // determine uncle by parent/grandparent relation
        (gp->left == par) ? (unc = gp->right) : (unc = gp->left);

        // if uncle is red
        if (!unc->black) {
            par->black = 1;
            unc->black = 1;
            if (gp != T->root) gp->black = 0;
            // grandparent may have a red parent at this point, set curr to gp and re-loop
            curr = gp;
        }
        else {
            // determine currents' parent relation (what side)
            (gp->left == par) ? (par_is_leftchild = 1) : (par_is_leftchild = 0);

            // determine parents' relation to their parent (what side)
            (par->left == curr) ? (curr_is_leftchild = 1) : (curr_is_leftchild = 0);

            // Is currents' and uncles' parent relation equal?
            if (par_is_leftchild != curr_is_leftchild) {
                // rotate parent 'away'
                (curr_is_leftchild) ? (rotate_right(T, par)) : (rotate_left(T, par));
            }
            else {
                // rotate grandparent 'away'
                (curr_is_leftchild) ? (rotate_right(T, gp)) : (rotate_left(T, gp));
                // fix colors
                par->black = 1;
                if (gp != T->root) gp->black = 0;
            }
        }
    }
}

int8_t tree_add(tree_t *tree, void *elem) {
    /* case: tree does not have a root yet */
    if (tree->size == 0) {
        treenode_t *root = malloc(sizeof(treenode_t));
        if (root == NULL) {
            printf("out of memory\n");
            return;
        }
        root->elem = elem;
        root->left = tree->NIL;
        root->right = tree->NIL;
        root->parent = tree->NIL;
        root->black = 1;
        tree->root = root;
        tree->size = 1;
        return 1;
    }

    /* add elem to the tree */
    treenode_t *new_node = node_add(tree, elem);
    if (new_node == tree->NIL) {
        /* avoid adding duplicate */
        return 0;
    }
    if (new_node == NULL) {
        /* only occurs if malloc failed */
        ERROR_PRINT("out of memory\n");
        return -1;
    }

    /* ... else, the elem was added, increment tree size */
    tree->size++;

    /* assign the default values for new nodes */
    new_node->elem = elem;
    new_node->left = tree->NIL;
    new_node->right = tree->NIL;
    new_node->black = 0;

    /* if needed, call for balancing */
    if (!new_node->parent->black) {
        post_add_balance(tree, new_node);
    }

    return 1;
}


/* -------------------------Iteration----------------------------
 * TODO: pre-order iterator for nxtfunc
*/

/* the type of 'next' function used by the iterator */
typedef struct treenode *(*nxtfunc_t)(tree_iter_t *);

typedef struct tree_iter {
    tree_t *tree;
    treenode_t *node;
    nxtfunc_t nxtfunc;
} tree_iter_t;

/* 
 * Morris traversal implementation
 * Works by creating 'links' between a subtrees 'far right leaf' and (sub)root,
 * through temporary alteration of the tree leafs
 */
static treenode_t *next_node_inorder(tree_iter_t *iter) {

    treenode_t *NIL = iter->tree->NIL;
    if (iter->node == NIL) return NIL;

    treenode_t *curr = iter->node;
    treenode_t *ret = NIL;          // node to be returned

    while (ret == NIL) {
        if (curr->left == NIL) {
            // can't move further left in current subtree, move right
            ret = curr;
            curr = curr->right;
        } else {
            // ... else, curr has a left child
            // create a predecessor pointer (pre), starting at currents left child
            // then traverse right with pre until we encounter either NIL or curr
            treenode_t *pre = curr->left;
            while (pre->right != NIL && pre->right != curr) {
                pre = pre->right;
            }
            // Determine if we hit a sentinel and need to form a new link,
            // Or whether we have already used the link using curr
            if (pre->right == NIL) {
                // We have reached a leaf, 
                // link it with curr before moving curr pointer left
                pre->right = curr;
                curr = curr->left;
            } else {
                /* ... (pre->right == curr) */
                // Delete the link and move curr pointer right
                pre->right = NIL;
                ret = curr;
                curr = curr->right;
            }
        }
    }
    /* increment iter to the next node */
    iter->node = curr;
    return ret;
}

tree_iter_t *tree_createiter(tree_t *tree, int inOrder) {
    tree_iter_t *new_iter = malloc(sizeof(tree_iter_t));
    if (new_iter == NULL) {
        ERROR_PRINT("new_iter == NULL");
        return NULL;
    }
    new_iter->tree = tree;
    new_iter->node = tree->root;

    /* decide what nxtfunc to point to */
    if (inOrder) {
        new_iter->nxtfunc = next_node_inorder;
        return new_iter;
    } else {
        // direct nxtfunc to a preorder iterator
        /* wasnt sure if i needed a different type of iterator as well, 
         * so future proofed a bit */
        ERROR_PRINT("preorder iteration is not yet implemented\n");
        return NULL;
    }
}

void tree_resetiter(tree_iter_t *iter) {
    if ((iter->node != iter->tree->NIL) && (iter->nxtfunc == next_node_inorder)) {
        treenode_t *curr = NULL;
        while (curr != iter->tree->NIL) {
            // finish the morris iterator process to avoid leaving any mutated leaves
            // this loop does nothing other than correctly finish the iteration process
            curr = next_node_inorder(iter);
        }
    }
    iter->node = iter->tree->root;
}

void tree_destroyiter(tree_iter_t *iter) {
    tree_resetiter(iter);
    free(iter);
}

void *tree_next(tree_iter_t *iter) {
    treenode_t *curr = iter->nxtfunc(iter);
    /* if end of tree is reached, curr->elem will be NULL */
    return curr->elem;
}

int tree_hasnext(tree_iter_t *iter) {
    if (iter->node == iter->tree->NIL)
        return 0;
    return 1;
}

