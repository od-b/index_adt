/*
 * Basic documentation at ../docs/README_queryparser.md
 */

#include "queryparser.h"
#include "pile.h"
#include "map.h"

#include <stdlib.h>
#include <string.h>

#define ERRMSG_MAXLEN  254

typedef enum qnode_types qnode_types_t;
typedef struct qnode qnode_t;
typedef struct parser parser_t;


/* declarations of static functions to allow reference prior to initialization. */

static qnode_t *parse_node(qnode_t *node);
static qnode_t *term_andnot(qnode_t *oper);
static qnode_t *term_and(qnode_t *oper);
static qnode_t *term_or(qnode_t *oper);
static qnode_t *splice_nodes(qnode_t *a, qnode_t *z);
static int is_operator(qnode_t *node);
static void destroy_product(qnode_t *term);
static void destroy_querynodes(qnode_t *leftmost);


struct parser {
    void *parent;
    search_func_t search_func;
    qnode_t *leftmost;
    char *errmsg_buf;
};

/* Types of query node (qnode_t->type) */
enum qnode_types {
    TERM      =  0,  // terminated node. All perator nodes end up as a TERM.
    OP_OR     =  1,
    OP_AND    =  2,
    OP_ANDNOT =  3,
    L_PAREN   = -2,
    R_PAREN   = -1
};

/* Type of query node */
struct qnode {
    qnode_types_t type;
    qnode_t  *left;
    qnode_t  *right;
    qnode_t  *sibling;    // matching left/right parentheses
    set_t    *prod;       // product of a TERM. For <word>'s, points directly to an iword->paths (or NULL)
    int       free_prod;  // whether product is result of an operation or points to some iword->paths
};


parser_t *parser_create(void *parent, search_func_t search_func) {
    parser_t *parser = malloc(sizeof(parser_t));
    if (!parser) {
        return NULL;
    }

    /* The following buffer could instead be allocated on demand,
     * However, this simplifies both errmsg assignment & cleanup. */
    parser->errmsg_buf = calloc((ERRMSG_MAXLEN + 1), sizeof(char));
    if (!parser->errmsg_buf) {
        free(parser);
        return NULL;
    }

    parser->parent = parent;
    parser->search_func = search_func;
    parser->leftmost = NULL;

    return parser;
}

void parser_destroy(parser_t *parser) {
    free(parser->errmsg_buf);
    free(parser);
}

char *parser_get_errmsg(parser_t *parser) {
    return parser->errmsg_buf;
}

set_t *parser_get_result(parser_t *parser) {
    /* initialize the process of node parsing */
    parser->leftmost = parse_node(parser->leftmost);

    if (!parser->leftmost->prod) {
        /* query completed with no results. return an empty set. */
        free(parser->leftmost);
        parser->leftmost = NULL;
        return set_create((cmpfunc_t)strcmp);
    }

    /* query yielded results. copy and return the result set */
    set_t *result = set_copy(parser->leftmost->prod);
    destroy_product(parser->leftmost);
    free(parser->leftmost);
    parser->leftmost = NULL;

    return result;
}

parser_status_t parser_scan(parser_t *parser, list_t *tokens) {
    /* declare and initialize all pointers */
    qnode_t *leftmost, *prev, *prev_nonpar, *node;
    leftmost = prev = prev_nonpar = node = NULL;

    char *errmsg = NULL, *token = NULL;
    pile_t *paren_pile = NULL, *tok_pile = NULL;
    list_iter_t *tok_iter = NULL;
    map_t *searched_words = NULL;
    parser_status_t status = SKIP_PARSE;

    /* create temporary constructs */
    tok_iter = list_createiter(tokens);
    paren_pile = pile_create();
    tok_pile = pile_create();
    searched_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (!searched_words || !paren_pile || !searched_words) {
        status = ALLOC_FAILED;
        goto end;
    }

    /* loop until an error message is set, or there are no more tokens */
    while (!errmsg && list_hasnext(tok_iter)) {
        node = malloc(sizeof(qnode_t));
        if (!node) {
            status = ALLOC_FAILED;
            goto end;
        }
        token = list_next(tok_iter);

        /* initialize the node */
        node->prod = NULL;
        node->right = NULL;
        node->sibling = NULL;
        node->free_prod = 1;

        /* match node type based on the token. */
        if (token[0] == '(') {
            node->type = L_PAREN;
            pile_push(paren_pile, node);
        } else if (token[0] == ')') {
            node->type = R_PAREN;

            /* set the most recent parenthesis as sibling */
            node->sibling = pile_pop(paren_pile);

            /* validate parentheses */
            if (!node->sibling || !prev || !prev_nonpar) {
                errmsg = "Unexpected closing parenthesis";
            } else if (prev == node->sibling || is_operator(prev_nonpar)) {
                errmsg = "Expected a query within parentheses";
            } else if (node->sibling->right == prev) {
                /* the parentheses only wrap a single <word>. remove them. */
                if (node->sibling == leftmost) {
                    leftmost = prev;
                } else {
                    node->sibling->left->right = prev;
                }
                prev->left = node->sibling->left;
                free(node);
                free(node->sibling);
                node = NULL;  // defensive measure
            } else {
                /* link the other parentheses */
                node->sibling->sibling = node;
            }
        } else if (strcmp(token, "OR") == 0) {
            node->type = OP_OR;
        } else if (strcmp(token, "AND") == 0) {
            node->type = OP_AND;
        } else if (strcmp(token, "ANDNOT") == 0) {
            node->type = OP_ANDNOT;
        } else {
            /* terminable token */
            node->type = TERM;
            if (prev_nonpar && (prev_nonpar->type == TERM)) {
                errmsg = "Adjacent terms";
            } else {
                if (map_haskey(searched_words, token)) {
                    /* Duplicate terminable within this query.
                    * Get result from map instead of searching set again. */
                    node->prod = map_get(searched_words, token);
                } else {
                    /* search index for word, add result to node & map of searched words */
                    node->prod = parser->search_func(parser->parent, token);
                    map_put(searched_words, token, node->prod);
                }
                /* mark this node to not have its set destroyed, as that would break the index :^) */
                node->free_prod = 0;
                prev_nonpar = node;

                if (node->prod) {
                    status = PARSE_READY;
                }
            }
        }

        if (node) {
            /* operator specific checks */
            if (is_operator(node)) {
                if (!prev || !prev_nonpar) {
                    errmsg = "Expected operator to have adjacent term(s)";
                } else if (is_operator(prev_nonpar) || prev->type == L_PAREN) {
                    errmsg = "Unexpected operator";
                }
                prev_nonpar = node;
            }

            /* Link node and the previous node, or set as leftmost for first node */
            if (leftmost) {
                prev->right = node;
                node->left = prev;
            } else {
                leftmost = node;
                node->left = NULL;
            }
            prev = node;
        }
        pile_push(tok_pile, token);
    }

    /* if errmsg is already set, avoid overwriting it. else, perform final checks. */
    if (!errmsg) {
        if (is_operator(prev_nonpar)) {
            errmsg = "Expected a term or query following operator";
        } else if (pile_size(paren_pile)) {
            errmsg = "Expected a closing parenthesis";
        }
    }

    if (errmsg) {
        /* print a formatted error message to the parsers errmsg buffer */
        if ((pile_size(tok_pile) > 2) && list_hasnext(tok_iter)) {
            /* (╯°□°）╯︵ ┻━┻ */
            snprintf(parser->errmsg_buf, ERRMSG_MAXLEN,
                "<br>Error around %s%s %s %s%s ~ %s.",
                ((pile_size(tok_pile) > 3) ? ("[ ... ") : ("[")),
                (char *)pile_peek(tok_pile, 1), token,
                (char *)list_next(tok_iter), 
                (list_hasnext(tok_iter) ? (" ... ]") : ("]")), errmsg);
        } else {
            snprintf(parser->errmsg_buf, ERRMSG_MAXLEN,
                "<br>Error around token %d: \"%s\" ~ %s.",
                pile_size(tok_pile), token, errmsg); 
        }
        status = SYNTAX_ERROR;
    } else {
        /* valid syntax */
        parser->leftmost = leftmost;
    }

end:
    /* cleanup and return */
    if (tok_iter) list_destroyiter(tok_iter);
    if (paren_pile) pile_destroy(paren_pile);
    if (tok_pile) pile_destroy(tok_pile);
    if (searched_words) map_destroy(searched_words, NULL, NULL);
    if (status != PARSE_READY) destroy_querynodes(leftmost);

    return status;
}

/*
 * Given a query node, decides the next action until the query is reduced
 * to a single terminated node, which will be the point of return.
 */
static qnode_t *parse_node(qnode_t *node) {
    /* 
     * The following loop may be ommited, but greatly reduces recursion depth
     * for nested queries by skipping to the next operator || rparen
     */
    while (node->right && (node->type == L_PAREN || node->type == TERM)) {
        node = node->right;
    }

    switch (node->type) {
        case R_PAREN:
            /* End of query/subquery. Splice parentheses and shift to lparen->right */
            return parse_node(splice_nodes(node->sibling, node));
        case OP_OR:
            return term_or(node);
        case OP_AND:
            return term_and(node);
        case OP_ANDNOT:
            return term_andnot(node);
        default: {
            if (node->right) {
                return parse_node(node->right);
            } else if (node->left) {
                return parse_node(node->left);
            } else {
                /* Single node remaining containing the final product. Return it. */
                return node;
            }
        }
    }
}

static qnode_t *term_andnot(qnode_t *oper) {
    qnode_t *a = oper->left;
    qnode_t *c = oper->right;

    if (oper->right->type != TERM) {
        /* Cannot consume right yet, parse subquery first. */
        return parse_node(c);
    }

    /* Check if a set operation is nescessary. */
    if (a->prod == c->prod) {
        /* both sets are either empty, or the same <word>. */
        oper->prod = NULL;
    }
    else if (!a->prod || !c->prod) {
        /* Combined case. Let Ø = empty set, A = a->prod, C = c->prod.
         * 1) if (A == Ø), then (A \ C === A);
         * 2) if (C == Ø), then (A \ C === A), regardless of whether (A == Ø) or (A == !Ø); */
        oper->prod = a->prod;
        oper->free_prod = a->free_prod;
    }
    else {
        /* Both products are non-empty sets. Produce the difference. */
        oper->prod = set_difference(a->prod, c->prod);
        oper->free_prod = 1;


        /* if the new set is empty, destroy it :^( */
        if (!set_size(oper->prod)) {
            destroy_product(oper);
        }
        /* Free sets that are no longer needed */
        destroy_product(a);
        destroy_product(c);
    }

    /* Consume the terms and parse self. */
    oper->type = TERM;
    return parse_node(splice_nodes(a, c));
}

static qnode_t *term_and(qnode_t *oper) {
    qnode_t *a = oper->left;
    qnode_t *c = oper->right;

    if (oper->right->type != TERM) {
        /* Cannot consume right yet, parse subquery first. */
        return parse_node(c);
    } 

    /* Check if a set operation is nescessary. */
    if (!a->prod || !c->prod) {
        /* One set is empty, and nullifies the need for any operation. */
        oper->prod = NULL;
        destroy_product(a);
        destroy_product(c);
    }
    else if (a->prod == c->prod) {
        /* Same <word>'s. `x AND x` == x. Inherit set of a. */
        oper->prod = a->prod;
        oper->free_prod = a->free_prod;
        destroy_product(c);
    }
    else {
        /* Both products are non-empty sets. Produce the intersection. */
        oper->prod = set_intersection(a->prod, c->prod);
        oper->free_prod = 1;

        /* if the new set is empty, destroy it :^( */
        if (!set_size(oper->prod)) {
            destroy_product(oper);
        }
        /* Free sets that are no longer needed */
        destroy_product(a);
        destroy_product(c);
    }

    /* Consume the terms and parse self. */
    oper->type = TERM;
    return parse_node(splice_nodes(a, c));
}

static qnode_t *term_or(qnode_t *oper) {
    qnode_t *a = oper->left;
    qnode_t *c = oper->right;

    if (oper->right->type != TERM) {
        /* Cannot consume right yet, parse subquery first. */
        return parse_node(c);
    }

    /* Check if a set operation is nescessary. */
    if (!a->prod && !c->prod) {
        oper->prod = NULL;
    }
    else if (!c->prod || (a->prod == c->prod)) {
        /* Not C --> inherit A. 
         * If duplicate <word>'s, union is pointless --> inherit A */
        oper->prod = a->prod;
        oper->free_prod = a->free_prod;
    }
    else if (!a->prod) {
        /* Not A --> inherit C */
        oper->prod = c->prod;
        oper->free_prod = c->free_prod;
    } else {
        /* Both products are non-empty sets, and an union operation is nescessary. */
        oper->prod = set_union(a->prod, c->prod);
        oper->free_prod = 1;

        /* Free sets that are no longer needed */
        destroy_product(a);
        destroy_product(c);
    }

    /* Consume the terms and parse self. */
    oper->type = TERM;
    return parse_node(splice_nodes(a, c));
}

/*
 * Destroys two nodes, denoted a and z, without dismembering any adjacent nodes.
 * There must exist at least one node node inbetween a and z. Returns a->right.
 */
static qnode_t *splice_nodes(qnode_t *a, qnode_t *z) {
    qnode_t *b = a->right;

    /* splice a, reattach a->left if any. */
    b->left = a->left;
    if (a->left) {
        a->left->right = b;
    }

    /* splice z, reattach z->right if any. */
    z->left->right = z->right;
    if (z->right) {
        z->right->left = z->left;
    }

    free(a);
    free(z);

    return b;
}

/* 
 * Destroys the given node and all nodes to the right of it.
 * Used to clean up in the event of syntax errors, etc.
 * If the parsing completes without error, this does not have to be called,
 * as the parsing process cleans up nodes as it reduces terms.
 */
static void destroy_querynodes(qnode_t *leftmost) {
    while (leftmost) {
        qnode_t *tmp = leftmost;
        leftmost = leftmost->right;
        destroy_product(tmp);
        free(tmp);
    }
}

/*
 * Destroys the product of a node unless it is inherited from an indexed word.
 * NULL-safe for both term and its prod. Does not destroy the node itself.
 */
static void destroy_product(qnode_t *term) {
    if (term && (term->prod && term->free_prod)) {
        set_destroy(term->prod);
        term->prod = NULL;
    }
}

/*
 * return 0 on NULL or non-operator, otherwise returns operator type
 */
static int is_operator(qnode_t *node) {
    if (node && (node->type > 0))
        return node->type;
    return 0;
}
