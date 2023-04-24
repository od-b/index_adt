#include "set.h"
#include "printing.h"


/*
 * ----- REFERENCES -----
 *
 * The code within this file is an altered version of code used in an earlier assignment.
 * The code within this file is my own work.
 * 
 * ----------------------
*/


/*
 * Implementation of a Red-Black BST. Inorder iterator with no list, stack or queue.
 * Traversal is done iteratively as this proved to be slightly faster in initial testing.
 * (exception being tree_destroy, which is only performed once per tree, so who cares about that)
*/


struct treenode;
typedef struct treenode treenode_t;

struct treenode {
    treenode_t *parent;
    treenode_t *left;
    treenode_t *right;
    void *elem;
    int_fast8_t black;
};

typedef struct set {
    treenode_t *root;
    /* The sentinel-node (NIL) functions as a 'NULL-pointer' for leaf nodes,
     * eliminates a lot of edge-case conditions for the various rotations, etc. */
    treenode_t *NIL;
    cmpfunc_t cmpfunc;
    int size;
} set_t;


set_t *set_create(cmpfunc_t cmpfunc) {
    set_t *new_set = malloc(sizeof(set_t));
    treenode_t *sentinel = malloc(sizeof(treenode_t));
    if (new_set == NULL || sentinel == NULL) {
        return NULL;
    }

    /* create the sentinel aka. NIL */
    sentinel->left = NULL;
    sentinel->right = NULL;
    sentinel->parent = NULL;
    sentinel->elem = NULL;
    sentinel->black = 1;

    /* set tree attributes to their default values */
    new_set->NIL = sentinel;
    new_set->root = sentinel;
    new_set->cmpfunc = cmpfunc;
    new_set->size = 0;

    return new_set;
}

int set_size(set_t *set) {
    return set->size;
}

/* postorder recursive call stack to destroy the tree bottom-up */
static void node_destroy(treenode_t *node, treenode_t *sentinel) {
    if (node == sentinel) 
        return;

    node_destroy(node->left, sentinel);
    node_destroy(node->right, sentinel);
    free(node);
}

void set_destroy(set_t *tree) {
    // call the recursive part first
    node_destroy(tree->root, tree->NIL);
    // free sentinel (NIL-node), then tree itself
    free(tree->NIL);
    free(tree);
}


/* ---------------Insertion, searching, rotation----------------- */

/* rotate nodes counter-clockwise */
inline static void rotate_left(set_t *tree, treenode_t *a) {
    treenode_t *b = a->right;
    treenode_t *c = a->right->left;

    /* fix root _or_ node 'a' parents' left/right pointers */
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

/* rotate nodes clockwise */
inline static void rotate_right(set_t *tree, treenode_t *a) {
    treenode_t *b = a->left;
    treenode_t *c = a->left->right;

    /* change root _or_ node 'a' parents' left/right pointers */
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

int set_contains(set_t *set, void *elem) {
    treenode_t *curr = set->root;
    int_fast8_t direction;

    /* traverse until a NIL-node, or return if an equal element is found */
    while (curr != set->NIL) {
        direction = set->cmpfunc(elem, curr->elem);
        if (direction > 0) {
            curr = curr->right;
        } else if (direction < 0) {
            curr = curr->left;
        } else {
            // ... direction == 0, tree contains an equal element
            return 1;
        }
    }

    // tree does not contain an equal element
    return 0;
}

void *set_get(set_t *tree, void *elem) {
    treenode_t *curr = tree->root;
    int_fast8_t direction;

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

    // elem does not exist in tree, and so cannot be returned.
    return NULL;
}

/* balance the tree after adding a node */
inline static void post_add_balance(set_t *T, treenode_t *added_node) {
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

void *set_tryadd(set_t *tree, void *elem) {
    /* case: tree does not have a root yet */
    if (tree->size == 0) {
        treenode_t *root = malloc(sizeof(treenode_t));
        if (root == NULL) {
            ERROR_PRINT("out of memory\n");
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
    int_fast8_t direction;
    treenode_t *new_node;

    /* iteratively add the node.
     * traverses the tree using cmpfunc until a NIL-node, or node with equal element is found.
     * if an equal element if found, returns it. Otherwise, creates a new node and breaks the loop.
     * TODO: compare iterative and recursion add speed on different architectures
     */
    while (1) {
        direction = tree->cmpfunc(elem, curr->elem);
        if (direction > 0) {
            if (curr->right == tree->NIL) {
                new_node = malloc(sizeof(treenode_t));
                if (new_node == NULL) {
                    ERROR_PRINT("out of memory");
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
                    ERROR_PRINT("out of memory");
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

void set_add(set_t *set, void *elem) {
    /* to comply with the header */
    set_tryadd(set, elem);
}

/* -------------------------Iteration----------------------------*/

typedef struct set_iter {
    set_t *set;
    treenode_t *node;
} set_iter_t;

set_iter_t *set_createiter(set_t *set) {
    set_iter_t *new_iter = malloc(sizeof(set_iter_t));
    if (new_iter == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }
    new_iter->set = set;
    new_iter->node = set->root;
    return new_iter;
}

void tree_resetiter(set_iter_t *iter) {
    if (iter->node != iter->set->NIL) {
        // finish the morris iterator process to avoid leaving any mutated leaves
        // this loop does nothing other than correctly finish the iteration process
        treenode_t *curr;
        for (curr = next_node_inorder(iter); curr != iter->set->NIL; curr = next_node_inorder(iter));
    }
    iter->node = iter->set->root;
}

void set_destroyiter(set_iter_t *iter) {
    if (iter->node != NULL) {
        tree_resetiter(iter);
    }
    free(iter);
}

void *set_next(set_iter_t *iter) {
    if (iter->node == iter->set->NIL) {
        return NULL;
    }

    /* Morris traversal implementation
     * Works by creating 'paths' between a subtrees 'far right leaf' and (sub)root,
     * through temporary alteration of the tree leaves' ->right pointer.
    */

    treenode_t *N = iter->node;          // alias for iter->node.
    treenode_t *ret_N = iter->set->NIL;  // set to a non->NIL node when iter->node has moved to the next inorder

    while (ret_N == iter->set->NIL) {
        if (N->left == iter->set->NIL) {
            /* can't traverse further left in current subtree, so move right */
            ret_N = N;
            N = N->right;
        } else {
            /* N has a left child. create a predecessor pointer, P, starting at the left child.*/
            treenode_t *P = N->left;

            /* Traverse right with P go right untill we hit a leaf or *N */
            while ((P->right != iter->set->NIL) && (P->right != N)) {
                P = P->right;
            }
            /* Determine if we hit *N, or if we and need to create a new path */
            if (P->right == iter->set->NIL) {
                /* set the pointer at leaf->right to point to N, then move N left */
                P->right = N;
                N = N->left;
            } else {
                /* we have previously traversed the path using N. Destroy the path and move N right */
                P->right = iter->set->NIL;
                ret_N = N;
                N = N->right;
            }
        }
    }

    /* increment iter to the next node, then return */
    iter->node = N;

    return ret_N;
}

int set_hasnext(set_iter_t *iter) {
    if (iter->node == iter->set->NIL)
        return 0;
    return 1;
}


/* -------------------------Set Specific----------------------------*/

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
        set_add(c, elem_a);
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
        set_add(c, elem_a);
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

    return copy;
}


/* debugging stuff */

/* static void print_tree(printfunc_t printfunc, set_t *tree, treenode_t* root, int height) {
    if (root == NULL) {
        return;
    }
    height += 10;

    print_tree(printfunc, tree, root->right, height);

    printf("\n");
    for (int i = 10; i < height; i++) {
        printf(" ");
    }

    if (root == NULL) {
        printf("NULL");
    } else {
        if (root->elem == NULL) {
            printf(BLKB "NIL" reset "");
        } else {
            char *elem_str = printfunc(root->elem);
            if (root->black) {
                printf(BLKHB " %s " reset "", elem_str);
            } else {
                printf(REDB " %s " reset "", elem_str);
            }
        }
    }

    print_tree(printfunc, tree, root->left, height);
}

void print_rbtreeset(set_t *set, printfunc_t printfunc) {
    print_tree(printfunc, set, set->root, 0);
} */
