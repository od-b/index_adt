/*
 * Implementation of a token scanner & parser for a given index.
 *
 * The parser supports OR, AND, ANDNOT operations, 
 * and allows specifying the desired order of evaluation through parantheses.
 * 
 * In the event of chained operator/term sequences without parantheses,
 * the parser defaults to a evaluation order of OR -> AND -> ANDNOT
*/

#include "parser.h"
#include "pile.h"
#include "map.h"
#include "printing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>


#define ERRMSG_MAXLEN  254
#define ASSERT_PARSE  1

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

static void debug_print_query(char *msg, list_t *tokens, qnode_t *leftmost);
static void debug_print_cattokens(qnode_t *oper);


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
    char     *token;
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
    if (!parser->leftmost) {
        ERROR_PRINT("parser has no node at leftmost\n");
        return NULL;
    }

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

parser_status_t parser_scan_tokens(parser_t *parser, list_t *tokens) {
    /* 
     * This function is fairly chaotic, with a lot of variables at play.
     * In an attempt to at least make it continuous, there's a single
     * shared exit point, regardless of status.
    */
    unsigned long long t_start = gettime();
    

    list_iter_t *tok_iter = NULL;
    pile_t *parenthesis_pile = NULL;
    map_t *searched_words = NULL;
    parser_status_t status = SKIP_PARSE;

    tok_iter = list_createiter(tokens);
    parenthesis_pile = pile_create();
    searched_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (!searched_words || !parenthesis_pile || !searched_words) {
        status = ALLOC_FAILED;
        goto end;
    }

    qnode_t *leftmost, *prev, *prev_nonpar, *node, *l_paren;
    char *errmsg, *token;

    /* initialize all declared pointers */
    leftmost = prev = prev_nonpar = node = l_paren = NULL;
    errmsg = token = NULL;

    const int n_tokens = list_size(tokens);
    int i = 0;

    /* loop until an error message is set, or there are no more tokens */
    while (!errmsg && (i++ < n_tokens)) {
        node = malloc(sizeof(qnode_t));
        if (!node) {
            status = ALLOC_FAILED;
            goto end;
        }
        token = list_next(tok_iter);

        /* initialize the node using default values */
        node->prod = NULL;
        node->right = NULL;
        node->sibling = NULL;
        node->free_prod = 1;

        // DEBUG
        node->token = token;

        /* match node type based on the token. */
        if (token[0] == '(') {
            node->type = L_PAREN;
            pile_push(parenthesis_pile, node);
        } else if (token[0] == ')') {
            node->type = R_PAREN;
            l_paren = pile_pop(parenthesis_pile);

            if (!l_paren || !prev || !prev_nonpar) {
                errmsg = "Invalid closing parenthesis";
            } else if (prev == l_paren || is_operator(prev_nonpar)) {
                errmsg = "Parentheses must enclose a valid query";
            } else if (l_paren->right == prev) {
                /* the parentheses only wrap a single <word>. remove them. */
                if (l_paren == leftmost) {
                    leftmost = prev;
                } else {
                    l_paren->left->right = prev;
                }
                prev->left = l_paren->left;
                free(node);
                free(l_paren);
                node = NULL;  // defensive measure
            } else {
                /* link the parentheses */
                l_paren->sibling = node;
                node->sibling = l_paren;
            }
        } else if (strcmp(token, "OR") == 0) {
            node->type = OP_OR;
        } else if (strcmp(token, "AND") == 0) {
            node->type = OP_AND;
        } else if (strcmp(token, "ANDNOT") == 0) {
            node->type = OP_ANDNOT;
        } else {
            /* token is a <word> */
            node->type = TERM;
            if (prev_nonpar && (prev_nonpar->type == TERM)) {
                errmsg = "Adjacent words";
            } else {
                if (map_haskey(searched_words, token)) {
                    /* Duplicate <word> within this query.
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
                if (!prev || !prev_nonpar || prev->type == L_PAREN) {
                    errmsg = "Operators require adjacent terms";
                } else if (is_operator(prev_nonpar)) {
                    errmsg = "Adjacent operators";
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
    }

    /* if errmsg is already set, avoid overwriting it. else, perform final checks. */
    if (!errmsg) {
        if (is_operator(prev_nonpar)) {
            errmsg = "Operators require adjacent terms";
        } else if (pile_size(parenthesis_pile)) {
            errmsg = "Parenthesis has no left match";
        }
    }

    /* in the event of a syntax error */
    if (errmsg) {
        /* clean up and format the error message  */
        printf("\n");
        debug_print_query("[error]: ", NULL, leftmost);

        /* print a formatted error message to the designated buffer */
        snprintf(parser->errmsg_buf, ERRMSG_MAXLEN, "<br>Error around token %d, <b>'%s'</b> ~ %s.", i, token, errmsg);
        status = SYNTAX_ERROR;
    } else {
        /* valid syntax */
        parser->leftmost = leftmost;
        printf("\n");
        printf("[parser_scan_tokens]: scan of %d tokens completed in %.5fms\n", i, ((float)(gettime()-t_start)) / 1000);
        debug_print_query("[query_validated]\n", tokens, leftmost);
        printf("\n");
    }

end:
    if (tok_iter) 
        list_destroyiter(tok_iter);
    if (parenthesis_pile)
        pile_destroy(parenthesis_pile);
    if (searched_words) 
        map_destroy(searched_words, NULL, NULL);
    if (status != PARSE_READY)
        destroy_querynodes(leftmost);

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
            printf("<<< goto l_paren\n");
            /* End of query/subquery. Splice parantheses and shift to lparen->right */
            return parse_node(splice_nodes(node->sibling, node));
        case OP_OR:
            return term_or(node);
        case OP_AND:
            return term_and(node);
        case OP_ANDNOT:
            return term_andnot(node);
        default: {
            if (node->right) {
                printf(">>\n");
                return parse_node(node->right);
            } else if (node->left) {
                printf("<<\n");
                return parse_node(node->left);
            } else {
                /* Single node remaining, containing the final product. Return it. */
                printf("^^ returning node: %s\n", node->token);
                return node;
            }
        }
    }
}

static qnode_t *term_andnot(qnode_t *oper) {
    assert(oper->type == OP_ANDNOT);
    qnode_t *a = oper->left;
    qnode_t *c = oper->right;

    if (oper->right->type != TERM) {
        /* Cannot consume right yet, parse subquery first. */
        return parse_node(c);
    }

    printf("[term_andnot] ");
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

        assert(set_size(a->prod));
        assert(set_size(c->prod));

        assert(set_size(a->prod) && set_size(c->prod));
        assert(set_size(oper->prod) <= set_size(a->prod));

        /* if the new set is empty, destroy it :^( */
        printf("new set: ");
        if (!set_size(oper->prod)) {
            destroy_product(oper);
            printf(" (empty set, destroyed)");
        }
        /* Free sets that are no longer needed */
        destroy_product(a);
        destroy_product(c);
    }
    debug_print_cattokens(oper);

    /* Consume the terms and parse self. */
    oper->type = TERM;
    return parse_node(splice_nodes(a, c));
}

static qnode_t *term_and(qnode_t *oper) {
    assert(oper->type == OP_AND);
    qnode_t *a = oper->left;
    qnode_t *c = oper->right;

    if (oper->right->type != TERM) {
        /* Cannot consume right yet, parse subquery first. */
        return parse_node(c);
    }

    printf("[term_and] ");
    /* Check if a set operation is nescessary. */
    if (!a->prod || !c->prod) {
        /* One set is empty, and nullifies the need for any operation. */
        oper->prod = NULL;
        destroy_product(a);
        destroy_product(c);
        printf("case 0: min. one empty set: ");
    }
    else if (a->prod == c->prod) {
        /* Same <word>'s. `x AND x` == x. Inherit set of a. */
        oper->prod = a->prod;
        oper->free_prod = a->free_prod;
        destroy_product(c);
        printf("case 1: inherited left set: ");
        assert(set_size(a->prod) == set_size(c->prod));
    }
    else {
        /* Both products are non-empty sets. Produce the intersection. */
        oper->prod = set_intersection(a->prod, c->prod);
        oper->free_prod = 1;

        assert(set_size(a->prod));
        assert(set_size(c->prod));

        assert(set_size(oper->prod) <= set_size(a->prod));
        assert(set_size(oper->prod) <= set_size(c->prod));

        /* if the new set is empty, destroy it :^( */
        printf("new set: ");
        if (!set_size(oper->prod)) {
            destroy_product(oper);
            printf(" (empty set, destroyed)");
        }
        /* Free sets that are no longer needed */
        destroy_product(a);
        destroy_product(c);
    }
    debug_print_cattokens(oper);

    /* Consume the terms and parse self. */
    oper->type = TERM;
    return parse_node(splice_nodes(a, c));
}

static qnode_t *term_or(qnode_t *oper) {
    assert(oper->type == OP_OR);
    qnode_t *a = oper->left;
    qnode_t *c = oper->right;

    if (oper->right->type != TERM) {
        /* Cannot consume right yet, parse subquery first. */
        return parse_node(c);
    }

    printf("[term_or] ");
    /* Check if a set operation is nescessary. */
    if (!a->prod && !c->prod) {
        oper->prod = NULL;
        printf("case 0 (%s && %s): null-sets. ", a->token, c->token);
    }
    else if (!c->prod || (a->prod == c->prod)) {
        /* Not C --> inherit A. 
         * If duplicate <word>'s, union is pointless --> inherit A */
        oper->prod = a->prod;
        oper->free_prod = a->free_prod;
        printf("case 1 '%s': inherited left set: ", c->token);
    }
    else if (!a->prod) {
        /* Not A --> inherit C */
        oper->prod = c->prod;
        oper->free_prod = c->free_prod;
        printf("case 2 '%s': inherited right set: ", a->token);
    } else {
        /* Both products are non-empty sets, and an union operation is nescessary. */
        oper->prod = set_union(a->prod, c->prod);
        oper->free_prod = 1;
        assert(set_size(a->prod));
        assert(set_size(c->prod));

        assert(set_size(oper->prod) >= set_size(a->prod));
        assert(set_size(oper->prod) >= set_size(c->prod));
        assert(set_size(oper->prod) <= (set_size(a->prod) + set_size(c->prod)));

        /* Free sets that are no longer needed */
        destroy_product(a);
        destroy_product(c);
        printf("new set: ");
    }
    debug_print_cattokens(oper);

    /* Consume the terms and parse self. */
    oper->type = TERM;
    return parse_node(splice_nodes(a, c));
}

/*
 * Destroys two nodes, denoted a and z, without dismembering any adjacent nodes.
 * There must exist at least one node node inbetween a and z. Returns a->right.
 */
static qnode_t *splice_nodes(qnode_t *a, qnode_t *z) {
    assert(a->right != z);
    assert(a->right && z->left);
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


/*
 * Testing, Printing, Debugging
*/

/* causes memory leaks. for testing purposes only. */
static void debug_print_cattokens(qnode_t *oper) {
    if (oper) {
        qnode_t *a = oper->left;
        qnode_t *b = oper;
        qnode_t *c = oper->right;
        b->token = concatenate_strings(5, "[", a->token, b->token, c->token, "]");
        switch (a->right->type) {
            case OP_OR:
                printf("%s U-->  %s  <--U %s\n", a->token, b->token, c->token);
                break;
            case OP_AND:
                printf("%s &-->  %s  <--& %s\n", a->token, b->token, c->token);
                break;
            case OP_ANDNOT:
                printf("%s /-->  %s  <--/ %s\n", a->token, b->token, c->token);
                break;
            default:
                break;
        }
    }
}

static void debug_print_query(char *msg, list_t *tokens, qnode_t *leftmost) {
    if (msg) DEBUG_PRINT("%s", msg);
    printf("[q_");

    if (tokens) {
        list_iter_t *tok_iter = list_createiter(tokens);
        printf("tokens]\t`");
        while (list_hasnext(tok_iter)) {
            char *tok = (char *)list_next(tok_iter);
            if (isupper(tok[0]))
                printf(" %s ", tok);
            else
                printf("%s", tok);
        }
        printf("`\n");
        list_destroyiter(tok_iter);
    }

    if (leftmost) {
        if (tokens) printf("[q_");
        qnode_t *n = leftmost;
        printf("nodes]\t`");
        // int c = 97;
        while (n) {
            switch (n->type) {
                case TERM:
                    // printf("%c", (char)c);
                    // c = (c == 122) ? (97) : (c+1);
                    printf("%s", n->token);
                    break;
                case OP_OR:
                    printf(" OR ");
                    break;
                case OP_AND:
                    printf(" AND ");
                    break;
                case OP_ANDNOT:
                    printf(" ANDNOT ");
                    break;
                case L_PAREN:
                    printf("(");
                    break;
                case R_PAREN:
                    printf(")");
                    break;
            }
            n = n->right;
        }
        printf("`\n");
    }
}
