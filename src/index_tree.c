/*
 * Index ADT implementation relying mainly on the use of tree-based set(s).
 * 
 * In an attempt to improve readability, the code within this file is split into sections.
 * Sections 1 & 2 consist solely of functions.
 * 
 * 0. Forward declarations, typedefs, struct initialization. 
 * 1. Creating and building the index
 * 2. Query handling, parsing
 * 
 */

#include "index.h"
#include "common.h"
#include "printing.h"
#include "map.h"
#include "set.h"
#include "pile.h"   // simple stack
#include "assert.h"

#include <ctype.h>
#include <string.h>


/*
 *  -------- Section 0: Definitions & Declarations --------
*/

#define ERRMSG_MAXLEN  254
#define ASSERT_PARSE  1

typedef enum qnode_types qnode_types_t;
typedef struct iword iword_t;
typedef struct qnode qnode_t;
typedef struct qproduct qproduct_t;
typedef struct qhandler qhandler_t;

static qnode_t *identify_tokens(index_t *index, list_t *tokens, int *n_matched_words);
static qnode_t *parse_query(qnode_t *node);
static qnode_t *term_andnot(qnode_t *oper);
static qnode_t *term_and(qnode_t *oper);
static qnode_t *term_or(qnode_t *oper);
static qnode_t *splice_nodes(qnode_t *a, qnode_t *z);
static list_t *get_query_results(qnode_t *node);
static void destroy_product(qnode_t *term);
static int is_operator(qnode_t *node);

static qnode_t *assert_term_or(qnode_t *a);
static void print_query(char *msg, list_t *tokens, qnode_t *leftmost);
static void print_term(qnode_t *oper, char *op);


/* Type of index */
struct index {
    set_t    *iwords;         // set of all indexed words
    iword_t  *iword_buf;      // the index keeps a buffer of one iword for searching and adding words
    char     *errmsg_buf;
    // debugging
    int        print_once;
    int        run_number;
};

/* Type of indexed word */
struct iword {
    char   *word;
    set_t  *paths;  // set of paths where word can be found
};


/* The possible types of query node (qnode_t->type) */
enum qnode_types {
    TERM      =  0,       // processed(terminated) nodes will have TERM as type
    OP_OR     =  1,
    OP_AND    =  2,
    OP_ANDNOT =  5,
    L_PAREN   = -2,
    R_PAREN   = -1
};

/* Type of query node */
struct qnode {
    qnode_types_t type;
    char     *token;
    set_t    *prod;        // product of a completed term. will be a set or NULL.
    int       free_prod;   // track if the product points to an indexed set or a 'temp' set, e.g. union.
    qnode_t  *par;         // parentheses only. Links respective left/right parentheses 
    qnode_t  *left;
    qnode_t  *right;
    int pos;
};

/* type of query product */
struct qproduct {
    int     temp;           // track if the product points to an indexed set or a 'temp' set, e.g. union.
    int    *operations;  /* 'set' of operations performed on this product () */
    set_t  *paths;
};


/* strcmp wrapper */
int strcmp_iwords(iword_t *a, iword_t *b) {
    return strcmp(a->word, b->word);
}

/* Compares two iword_t based on the size of their path sets */
int compare_iwords_by_occurance(iword_t *a, iword_t *b) {
    return (set_size(a->paths) - set_size(b->paths));
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}


/*
 *  -------- Section 1: Index Creation, Destruction and Building --------
*/

index_t *index_create() {
    index_t *index = malloc(sizeof(index_t));
    if (!index) {
        ERROR_PRINT("malloc failed");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    index->iwords = set_create((cmpfunc_t)strcmp_iwords);
    index->iword_buf = malloc(sizeof(iword_t));
    index->errmsg_buf = calloc((ERRMSG_MAXLEN + 1), sizeof(char));

    if (!index->iwords || !index->iword_buf || !index->errmsg_buf) {
        ERROR_PRINT("malloc failed");
        return NULL;
    }

    index->iword_buf->word = NULL;
    index->iword_buf->paths = NULL;

    /* for tests etc */
    index->print_once = 0;
    index->run_number = 0;

    return index;
}

void index_destroy(index_t *index) {
    int n_freed_words = 0;
    int n_freed_paths = 0;

    /* Set to group all paths before freeing them. Avoids any hocus pocus 
     * such as double free or potentially dismembering trees */
    set_t *all_paths = set_create((cmpfunc_t)strcmp);
    set_iter_t *iword_iter = set_createiter(index->iwords);

    while (set_hasnext(iword_iter)) {
        iword_t *curr = set_next(iword_iter);
        set_iter_t *path_iter = set_createiter(curr->paths);

        /* add to set of all paths */
        while (set_hasnext(path_iter)) {
            set_add(all_paths, set_next(path_iter));
        }
        set_destroyiter(path_iter);

        /* free the iword & its members */
        set_destroy(curr->paths);
        free(curr->word);
        free(curr);
        n_freed_words++;
    }
    set_destroyiter(iword_iter);
    set_destroy(index->iwords);

    /* free all paths now that they're grouped in one set */
    set_iter_t *all_paths_iter = set_createiter(all_paths);
    while (set_hasnext(all_paths_iter)) {
        free(set_next(all_paths_iter));
        n_freed_paths++;
    }
    set_destroyiter(all_paths_iter);
    set_destroy(all_paths);

    /* free index & co */
    free(index->iword_buf);
    free(index->errmsg_buf);
    free(index);

    DEBUG_PRINT("index_destroy: Freed %d paths, %d words\n", n_freed_paths, n_freed_words);
}

void index_addpath(index_t *index, char *path, list_t *words) {
    /*
     * TODO: test index build with and without list_sort, as well as with different sorts
     * will no doubt be correlated to n_words, but so will sorting the list.
     * Compared scaling with varied, large sets needed.
     * If the list sort is slower for small sets, but faster for varied/large sets,
     * using the sort method will be the best option, seeing as small sets are rather quick to add regardless.
    */

    list_iter_t *words_iter = list_createiter(words);
    if (words_iter == NULL) {
        ERROR_PRINT("malloc failed");
        return;
    }
    // list_sort(words);

    while (list_hasnext(words_iter)) {
        char *w = list_next(words_iter);

        /* try to add the word to the index, using word_buf to allow comparison */
        index->iword_buf->word = w;
        iword_t *iword = set_tryadd(index->iwords, index->iword_buf);

        if (iword == index->iword_buf) {
            /* first index entry for this word. initialize it as an indexed word. */
            iword->word = w;
            iword->paths = set_create((cmpfunc_t)strcmp);
            if (!iword->paths) {
                ERROR_PRINT("malloc failed");
            }

            /* Since the search word was added, create a new iword for searching. */
            index->iword_buf = malloc(sizeof(iword_t));
            if (!index->iword_buf) {
                ERROR_PRINT("malloc failed");
            }
            index->iword_buf->paths = NULL;
        } else {
            /* index is already storing this word. free the duplicate string. */
            free(w);
        }

        /* add path to the indexed words set. */
        set_add(iword->paths, path);
    }

    list_destroyiter(words_iter);
}


/*
 *  -------- Section 2: Query Parsing & Response --------
*/

list_t *index_query(index_t *index, list_t *tokens, char **errmsg) {
    if (!list_size(tokens)) {
        *errmsg = "empty query";
        return NULL;
    }
    if (ASSERT_PARSE) print_query("\n", tokens, NULL);

    int n_matched_words = 0;
    qnode_t *leftmost = identify_tokens(index, tokens, &n_matched_words);

    if (!leftmost) {
        if (index->errmsg_buf)
            *errmsg = index->errmsg_buf;
        else
            *errmsg = "index failed to allocate memeory";
        return NULL;
    }


    // if (ASSERT_PARSE) print_query(tokens, leftmost, 1, 0, 0);

    qnode_t *parsed = NULL;
    if (n_matched_words) {
        if (ASSERT_PARSE) print_query("pre parse", NULL, leftmost);

        parsed = parse_query(leftmost);
        if (!parsed) {
            *errmsg = "parsing error";
            return NULL;
        }
    } else if (ASSERT_PARSE) {
        printf("no word matches. Returning empty list.\n");
    }

    list_t *query_results = get_query_results(parsed);
    if (!query_results) {
        *errmsg = "malloc failed";
    }/* else {
        score_query_results(query_result);
    }*/

    if (parsed) {
        destroy_product(parsed);
        free(parsed);
    }

    return query_results;
}

/*
 * Identifies tokens and checks for common grammar errors.
 * Creates query nodes from the tokens, terminating any <word> nodes.
 * Removes parantheses if they serve no logical purpose. e.g. (<word>).
 */
static qnode_t *identify_tokens(index_t *index, list_t *tokens, int *n_matched_words) {
    list_iter_t *tok_iter = list_createiter(tokens);
    pile_t *paranthesis_pile = pile_create();
    map_t *searched_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (!paranthesis_pile || !searched_words || !tok_iter) {
        return NULL;
    }

    qnode_t *leftmost, *prev, *prev_nonpar, *node, *l_paren;
    char *errmsg, *token;
    iword_t *iword;

    /* initialize all declared pointers to NULL */
    leftmost = prev = prev_nonpar = node = l_paren = NULL;
    errmsg = token = NULL;
    iword = NULL;
    int i = 0;  // iteration count, to improve msg in case of error

    while (!errmsg && list_hasnext(tok_iter)) {
        node = malloc(sizeof(qnode_t));
        if (!node) {
            return NULL;
        }
        i++;
        token = list_next(tok_iter);

        /* initialize the node using default values */
        node->prod = NULL;
        node->right = NULL;
        node->par = NULL;
        node->free_prod = 1;

        // DEBUG
        node->token = token;
        node->pos = i-1;

        /* match node type based on the token. */
        if (token[0] == '(') {
            node->type = L_PAREN;
            pile_push(paranthesis_pile, node);
        } else if (token[0] == ')') {
            node->type = R_PAREN;
            l_paren = pile_pop(paranthesis_pile);

            if (!prev || !l_paren) {
                errmsg = "Unmatched closing paranthesis";
            } else if (prev == l_paren) {
                errmsg = "Parantheses must contain a query";
            } else if (l_paren->right == prev) {
                if (prev->left->type == TERM) {
                    // for cases like `a AND b (c) OR d`
                    errmsg = "Adjacent words";
                } else {
                    /* the parantheses only wrap a single <word>. remove them. */
                    if (l_paren == leftmost) {
                        // in case left paranthesis is 'root'
                        leftmost = prev;
                    } else {
                        l_paren->left->right = prev;
                    }
                    prev->left = l_paren->left;
                    free(node);
                    free(l_paren);
                    node = NULL; // not strictly needed, but ensures if(node) will not be true.
                }
            } else {
                /* link the parantheses */
                l_paren->par = node;
                node->par = l_paren;
            }
        } else if (isupper(token[0])) {
            /* match the type of operator */
            if (strcmp(token, "OR") == 0)
                node->type = OP_OR;
            else if (strcmp(token, "AND") == 0)
                node->type = OP_AND;
            else if (strcmp(token, "ANDNOT") == 0)
                node->type = OP_ANDNOT;
            else {
                /* default: verifies that all given uppercase tokens are operators */
                errmsg = "indexer // Index error. Expexted all uppercase tokens to be OR | AND | ANDNOT";
                break;
            }

            /* operator specific checks */
            if (!prev || !prev_nonpar) {
                errmsg = "Operators require adjacent terms";
            } else if (prev_nonpar->type != TERM) {
                errmsg = "Adjacent operators.";
            } else if (prev->type == L_PAREN) {
                errmsg = "Operators may not occur directly after an opening paranthesis";
            }
            prev_nonpar = node;
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
                    index->iword_buf->word = token;
                    iword = set_get(index->iwords, index->iword_buf);

                    /* set product to the iword paths, or NULL if <word> is not indexed. */
                    node->prod = (iword) ? (iword->paths) : (NULL);
                    map_put(searched_words, token, node->prod);
                }
                /* mark this node to not have its set destroyed, as that would break the index :^) */
                node->free_prod = 0;
                prev_nonpar = node;

                if (node->prod) *n_matched_words += 1;
            }
        }

        /* Link node and the previous node, or set as leftmost for first node */
        if (node) {
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
            errmsg = "Query may not begin or end with an operator";
        } else if (pile_size(paranthesis_pile)) {
            errmsg = "Unmatched opening paranthesis";
        }
    }

    if (errmsg) {
        /* clean up and format the error message  */
        print_query("error", NULL, leftmost);

        /* clean up nodes */
        qnode_t *tmp;
        while (leftmost) {
            tmp = leftmost;
            leftmost = leftmost->right;
            free(tmp);
        }
        leftmost = NULL;  // not strictly needed, but ensures returning NULL

        /* print a formatted error message to the designated buffer */
        snprintf(index->errmsg_buf, ERRMSG_MAXLEN, "<br>Error around token %d, <b>'%s'</b> ~ %s.", i, token, errmsg);
    } /* else {
        // pop all/any parantheses pairs expanding the entire width of the query
        while (leftmost->par == node) {
            node = node->left;
            leftmost = splice_nodes(leftmost, node->right);
        }
    } */

    /* cleanup temp structs. leftmost will be NULL in case of an error. */
    list_destroyiter(tok_iter);
    pile_destroy(paranthesis_pile);
    map_destroy(searched_words, NULL, NULL);

    return leftmost;
}

static qnode_t *parse_query(qnode_t *node) {
    if (!node->left && !node->right) {
        printf("^^^ returning node %s\n", node->token);
        return node;
    }

    switch (node->type) {
        case R_PAREN:
            printf("<<< goto l_paren\n");
            return parse_query(splice_nodes(node->par, node));
        case OP_OR:
            return term_or(node);
        case OP_AND:
            return term_and(node);
        case OP_ANDNOT:
            return term_andnot(node);
        default:
            break;
    }

    if (node->right) {
        printf(">>\n");
        return parse_query(node->right);
    } else {
        printf("<<\n");
        return parse_query(node->left);
    }
}

static qnode_t *term_or(qnode_t *oper) {
    qnode_t *a = oper->left;
    qnode_t *c = oper->right;

    /* cannot terminate right yet, go deeper. */
    if (oper->right->type != TERM) {
        return parse_query(c);
    }

    /* perform checks to see if an union operation is nescessary. */
    if (!a->prod && !c->prod) {
        oper->prod = NULL;
        printf("case 0 (%s && %s): null-sets.\n", a->token, c->token);
    } 
    else if (!c->prod || (a->prod == c->prod)) {
        /* Combined trigger. either left set is NULL, or sets are duplicate
         * pointers from from equal <word> leaves, e.g. `a OR a` */
        oper->prod = a->prod;
        oper->free_prod = a->free_prod;
        printf("case 1 '%s': inherited left set:\n", c->token);
    } 
    else if (!a->prod) {
        /* inherit the product of the right term */
        oper->prod = c->prod;
        oper->free_prod = c->free_prod;
        printf("case 2 '%s': inherited right set:\n", a->token);
    } 
    else {
        /* Both products are non-empty sets, and an union operation is nescessary. */
        oper->prod = set_union(a->prod, c->prod);
        oper->free_prod = 1;

        /* free sets that are no longer needed */
        destroy_product(a);
        destroy_product(c);
        printf("created union set\n");
    }

    /* consume the terms and return self as a term. */
    oper->type = TERM;
    return parse_query(splice_nodes(a, c));
}

static qnode_t *term_andnot(qnode_t *oper) {
    return NULL;
}

static qnode_t *term_and(qnode_t *oper) {
    return NULL;
}

/* Returns a list of query results containing each path in a term product. */
static list_t *get_query_results(qnode_t *term) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);
    if (!query_results) {
        return NULL;
    }

    if (term && (term->prod && set_size(term->prod))) {
        set_iter_t *path_iter = set_createiter(term->prod);
        if (!path_iter) {
            return NULL;
        }
        double score = 0.0;

        while (set_hasnext(path_iter)) {
            query_result_t *q_result = malloc(sizeof(query_result_t));
            if (!q_result) {
                return NULL;
            }
            q_result->path = set_next(path_iter);
            q_result->score = score;
            score += 0.1;
            if (!list_addlast(query_results, q_result)) {
                return NULL;
            }
        }
        set_destroyiter(path_iter);
    }

    return query_results;
}


/* -- section 2 helper functions -- */

/*
 * Destroys two nodes, denoted a and z, without dismembering any adjacent nodes.
 * There must exist at least one node node inbetween a and z. Returns a->right.
 */
static qnode_t *splice_nodes(qnode_t *a, qnode_t *z) {
    if (a->right == z) {
        ERROR_PRINT("splice_nodes: missing node b\n");
    }
    qnode_t *b = a->right;

    b->left = a->left;
    if (a->left) {
        a->left->right = b;
    }

    z->left->right = z->right;
    if (z->right) {
        z->right->left = z->left;
    }

    if (ASSERT_PARSE && (a->type >= 0) && (b->type == TERM)) {
        if (b->prod && b->prod != a->prod && b->prod != z->prod) {
            b->token = concatenate_strings(5, "[", a->token, b->token, z->token, "]");
            printf("%s ->  %s  <- %s\n", a->token, b->token, z->token);
        } else {
            b->token = (b->prod == a->prod) ? (a->token) : (z->token);
            printf(" < %s > \n", b->token);
        }
    }

    free(a);
    free(z);

    return b;
}

/*
 * Destroys the product of a node unless it is inherited from an indexed word.
 * Does not destroy the node itself.
 */
static void destroy_product(qnode_t *term) {
    if (term && (term->prod && term->free_prod)) {
        set_destroy(term->prod);
        term->prod = NULL;
    }
}

/* return 0 on NULL or non-operator, otherwise returns operator type */
static int is_operator(qnode_t *node) {
    if (node && (node->type > 0))
        return node->type;
    return 0;
}

// /* return 0 on NULL or nonpar, otherwise returns paranthesis type */
// static int is_parenthesis(qnode_t *node) {
//     if (node && (node->type < 0))
//         return node->type;
//     return 0;
// }

/*
 * ------- Section 3: Testing, Printing, Debugging -------
*/

static qnode_t *assert_term_or(qnode_t *a) {
    qnode_t *self = a->right;
    qnode_t *c = self->right;

    if (c != TERM) {
        printf("[assert_or] > case 0");
        print_term(c->left, "OR");

        c = parse_query(c);
    }


    printf("[assert_or] > ");
    /* perform various checks to see if an union operation can be circumvented. */
    if (!a->prod && !c->prod) {
        self->prod = NULL;

        printf("case 1: l = r = 0");
        self->token = a->token;
    }
    else if (!c->prod) {
        /* inherit the product of the left term */
        self->prod = a->prod;
        self->free_prod = a->free_prod;

        printf("case 2: r = 0, inherit L set_size(%d)", set_size(a->prod));
        self->token = a->token;
    }
    else if (!a->prod) {
        /* inherit the product of the right term */
        self->prod = c->prod;
        self->free_prod = c->free_prod;

        printf("case 3: l = 0, inherit R, set_size(%d)", set_size(c->prod));
        self->token = a->token;
    }
    else if (a->prod == c->prod) {
        /* Duplicate sets from equal <word> leaves, e.g. `a OR a` */
        self->prod = a->prod;
        self->free_prod = 0;

        printf("case 4: L == R, inherit left          ");
        self->token = concatenate_strings(4, "[", a->token, c->token, "]");
        if (a->prod)
            assert(set_size(a->prod) == set_size(c->prod));
    }
    else {
        /* Both products are non-empty sets, and an union operation is nescessary. */
        self->prod = set_union(a->prod, c->prod);
        self->free_prod = 1;

        printf("case 5: union                         ");
        self->token = concatenate_strings(5, "[", a->token, self->token, c->token, "]");

        assert(set_size(self->prod) >= set_size(a->prod));
        assert(set_size(self->prod) >= set_size(c->prod));

        /* free sets that are no longer needed */
        destroy_product(a);
        destroy_product(c);
    }
    print_term(self, "OR");
    printf("   >>   ");
    /* consume the terms and return self as a term. */
    self->type = TERM;
    return parse_query(splice_nodes(a, c));
}

static void print_term(qnode_t *oper, char *op) {
    /* this will not be pretty */
    size_t len = strlen(oper->left->token) + strlen(op) + strlen(oper->right->token) + 3;
    size_t diff = 40-len;
    if (diff <= 1) diff = 3;

    char space_buf[diff+1];
    memset(space_buf, ' ', (sizeof(char) * diff));
    space_buf[diff] = '\0';

    printf("%s", space_buf);
    printf(" %s '%s' %s ", oper->left->token, op, oper->right->token);
}

static void print_query(char *msg, list_t *tokens, qnode_t *leftmost) {
    if (msg) printf("%s", msg);
    printf("[q_");

    if (tokens) {
        list_iter_t *tok_iter = list_createiter(tokens);
        printf("tokens] = `");
        while (list_hasnext(tok_iter)) {
            printf("%s ", (char *)list_next(tok_iter));
            // (list_hasnext(tok_iter)) ? (printf(" ")) : (printf("`\n"));
        }
        printf("`\n");
        list_destroyiter(tok_iter);
    }

    if (leftmost) {
        qnode_t *n = leftmost;
        printf("nodes]: `");
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

// static void print_int_arr_as_list(int **arr, int n) {
//     for (int i = 0; i < n; i++) {
//         printf("%d", *arr[i]);
//         (i == n-1) ? (printf("\n")) : (printf(", "));
//     }
// }

/*
Original:  `(a OR b) OR (c OR (x OR y OR k)) OR (d OR e) OR f`

>>
>>
a ->  [aORb]  <- b
>>
<< goto left paren
>>
>>
>>
>>
>>
x ->  [xORy]  <- y
>>
[xORy] ->  [[xORy]ORk]  <- k
>>
<< goto left paren
>>
<< goto left paren
>>
c ->  [cOR[[xORy]ORk]]  <- [[xORy]ORk]
>>
>>
>>
d ->  [dORe]  <- e
>>
<< goto left paren
>>
[dORe] ->  [[dORe]ORf]  <- f
<<
[cOR[[xORy]ORk]] ->  [[cOR[[xORy]ORk]]OR[[dORe]ORf]]  <- [[dORe]ORf]
<<
[aORb] ->  [[aORb]OR[[cOR[[xORy]ORk]]OR[[dORe]ORf]]]  <- [[cOR[[xORy]ORk]]OR[[dORe]ORf]]

------

Original:  `horse OR dog OR x OR banana OR cow OR fish OR cat`
>>
case (horse && dog): null-sets.
 < horse >
>>
case 'horse': inherited right set:
 < x >
>>
case 'banana': inherited left set:
 < x >
>>
case 'cow': inherited left set:
 < x >
>>
case 'fish': inherited left set:
 < x >
>>
created union set
x ->  [xORcat]  <- cat
^^^ returning node [xORcat]

*/

// make clean && make assert_index && lldb assert_index
// make clean && make indexer && ./indexer data/cacm/
// sudo lsof -i -P | grep LISTEN | grep :$8080
