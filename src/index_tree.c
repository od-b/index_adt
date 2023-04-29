/*
 * Index ADT implementation relying mainly on the use of tree-based set(s)
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
 *  -------- Section 0: Definitions, Structs, Declarations, Comparision Functions --------
*/

#define ERRMSG_MAXLEN  254 
#define ASSERT_PARSE  1


/* Type of indexed word */
typedef struct i_word {
    char  *word;
    set_t *paths;          /* set of paths where this word can be found */
} i_word_t;

/* Type of index */
typedef struct index {
    set_t    *i_words;        /* set of all indexed words */
    i_word_t *word_buf;       /* i_word for searching / appending purposes */
    char     *errmsg_buf;     /* buffer to ease customization of error messages */
    // debugging
    int        print_once;
    int        run_number;
} index_t;

/* 
 * Defined query node types (qnode_t->type).
 * Note that 'TERM' is used slightly different than within the specified BNF, and describes a 
 * processed node which may be consumed by an operator. E.g. <word> nodes.
 * To allow simple checks, parantheses types are defined < 0, operators > 0.
 */
typedef enum qnode_types {
    TERM      =  0,
    OP_OR     =  1,
    OP_AND    =  2,
    OP_ANDNOT =  3,
    L_PAREN   = -2,
    R_PAREN   = -1
} qnode_types_t;

// /* type of query product */
// typedef struct qproduct {
//     int     freeable;   /* track if the product points to an indexed set or a gen2 set, e.g. union. */
//     set_t  *words;
//     set_t  *paths;
// } qproduct_t;

struct qnode;
typedef struct qnode qnode_t;

/* Type of query node */
struct qnode {
    qnode_types_t type;
    char     *token;
    set_t    *prod;       /* product of a completed term. will be a set or NULL. */
    int       free_prod;  /* track if the product points to an indexed set or a gen2 set, e.g. union. */
    qnode_t  *par;        /* parentheses only. Links respective left/right parentheses */
    qnode_t  *left;
    qnode_t  *right;
    int pos;
};


/* functions that require preemptive declarations */

static qnode_t *format_tokens(index_t *index, list_t *tokens, int *do_parse);
static qnode_t *parse_query(qnode_t *node);
static qnode_t *term_andnot(qnode_t *oper);
static qnode_t *term_and(qnode_t *oper);
static qnode_t *term_or(qnode_t *oper);
static qnode_t *splice_nodes(qnode_t *a, qnode_t *z);
static list_t *get_query_results(qnode_t *node);
static void destroy_product(qnode_t *term);
static int is_operator(qnode_t *node);

/* debugging */
static qnode_t *assert_term_or(qnode_t *a);
static void print_querynodes(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS);
static void print_term(qnode_t *oper, char *op);

/* comparison functions */

/* Wrapper for strcmp, comparing a->word against b->word. */
int strcmp_i_words(i_word_t *a, i_word_t *b) {
    return strcmp(a->word, b->word);
}

/* Compares two i_word_t based by how many files(paths) they appear in */
int compare_i_words_by_n_occurances(i_word_t *a, i_word_t *b) {
    return (set_size(a->paths) - set_size(b->paths));
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}


/*
 *  -------- Section 1: Index Creation, Destruction and Building --------
*/


index_t *index_create() {
    index_t *ind = malloc(sizeof(index_t));
    if (!ind) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    ind->i_words = set_create((cmpfunc_t)strcmp_i_words);
    ind->word_buf = malloc(sizeof(i_word_t));
    ind->errmsg_buf = calloc((ERRMSG_MAXLEN + 1), sizeof(char));

    if (!ind->i_words || !ind->word_buf || !ind->errmsg_buf) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    ind->word_buf->word = NULL;
    ind->word_buf->paths = NULL;

    /* for tests etc */
    ind->print_once = 0;
    ind->run_number = 0;

    return ind;
}

void index_destroy(index_t *index) {
    int n_freed_words = 0;
    int n_freed_paths = 0;
    set_iter_t *i_word_iter = set_createiter(index->i_words);
    set_iter_t *path_iter;
    set_t *all_paths = set_create((cmpfunc_t)strcmp);

    while (set_hasnext(i_word_iter)) {
        i_word_t *curr = set_next(i_word_iter);
        path_iter = set_createiter(curr->paths);

        while (set_hasnext(path_iter)) {
            /* add to set of all paths to avoid any double free hocus pocus */
            set_add(all_paths, set_next(path_iter));
        }
        set_destroyiter(path_iter);

        /* free the i_word & its members */
        set_destroy(curr->paths);
        free(curr->word);
        free(curr);
        n_freed_words++;
    }
    set_destroyiter(i_word_iter);
    set_destroy(index->i_words);

    /* free the set of all paths */
    path_iter = set_createiter(all_paths);
    while (set_hasnext(path_iter)) {
        free(set_next(path_iter));
        n_freed_paths++;
    }
    set_destroyiter(path_iter);
    set_destroy(all_paths);

    /* free index & co */
    free(index->word_buf);
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
        ERROR_PRINT("out of memory");
        return;
    }

    /* sort the list of words to skip duplicate words from the current list,
     * without having to search the entire main set of indexed words */
    // list_sort(words);

    char *word;

    /* iterate over all words in the provided list */
    while ((word = list_next(words_iter)) != NULL) {

        // /* skip all duplicate words within the current list of words */
        // if ((prev_word == NULL) || (strcmp(word, prev_word) != 0)) {

            /* try to add the word to the index, using word_buf as a buffer. */
            index->word_buf->word = word;
            i_word_t *i_word = set_tryadd(index->i_words, index->word_buf);

            if (i_word == index->word_buf) {
                /* first index entry for this word. initialize it as an indexed word. */
                i_word->word = word;
                i_word->paths = set_create((cmpfunc_t)strcmp);

                /* Since the search word was added, create a new i_word for searching. */
                index->word_buf = malloc(sizeof(i_word_t));

                if (i_word->paths == NULL || index->word_buf == NULL) {
                    ERROR_PRINT("out of memory");
                }
                index->word_buf->paths = NULL;
            } else {
                /* index is already storing this word. free the string. */
                free(word);
            }

            /* reset the search word to NULL. (initialize, rather, if word was previously unseen) */
            index->word_buf->word = NULL;

            /* add path to the i_words' set of paths */
            set_add(i_word->paths, path);

        // } else {
        //     // printf("[%s] %s ", path, word);
        //     /* duplicate word within the current list of words, free and continue to next word. */
        //     free(word);
        // }
        // prev_word = word;
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

    int do_parse = 0;   // skip parsing altogether if no words are contained in the index
    qnode_t *leftmost = format_tokens(index, tokens, &do_parse);
    if (!leftmost) {
        if (index->errmsg_buf) {
            *errmsg = index->errmsg_buf;
        } else {
            *errmsg = "out of memory";
        }
        return NULL;
    }

    if (ASSERT_PARSE) {
        print_querynodes(tokens, leftmost, 1, 0, 0);
    }

    qnode_t *parsed = NULL;
    if (do_parse) {
        parsed = parse_query(leftmost);
        if (!parsed) {
            *errmsg = "parsing error";
            return NULL;
        }
    }

    list_t *query_results = get_query_results(parsed);
    if (!query_results) {
        *errmsg = "out of memory";
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
 * * Identifies a majority of typical issues regarding query syntax.
 * Converts the tokens to query nodes. <word> tokens are terminated at this stage,
 * and will be assigned a pointer to a set of paths, given that the word is contained within
 * the index. The product of <word> will otherwise be a null-pointer.
 */
static qnode_t *format_tokens(index_t *index, list_t *tokens, int *do_parse) {
    list_iter_t *tok_iter = list_createiter(tokens);
    pile_t *paranthesis_pile = pile_create();
    map_t *searched_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (!paranthesis_pile || !searched_words || !tok_iter) {
        return NULL;
    }

    qnode_t *leftmost, *prev, *prev_nonpar, *node, *l_paren;
    char *errmsg, *token;
    i_word_t *i_word;

    /* initialize all declared pointers to NULL */
    leftmost = prev = prev_nonpar = node = l_paren = NULL;
    errmsg = token = NULL;
    i_word = NULL;
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
                    index->word_buf->word = token;
                    i_word = set_get(index->i_words, index->word_buf);

                    /* set product to the i_word paths, or NULL if <word> is not indexed. */
                    node->prod = (i_word) ? (i_word->paths) : (NULL);
                    map_put(searched_words, token, node->prod);
                }
                /* mark this node to not have its set destroyed, as that would break the index :^) */
                node->free_prod = 0;
                prev_nonpar = node;
                if (node->prod) 
                    *do_parse = 1;
                // DEBUGGING
                // if (ASSERT_PARSE) {
                //     node->token = (node->prod) ? (node->token) : ("X");
                // }
            }
        }

        /* Link node and the previous node, or set as leftmost */
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

    /* if errmsg, avoid overwriting it. else, perform final checks. */
    if (!errmsg) {
        if (is_operator(prev_nonpar)) {
            errmsg = "Query may not begin or end with an operator";
        } else if (pile_size(paranthesis_pile)) {
            errmsg = "Unmatched opening paranthesis";
        }
    }

    if (errmsg) {
        /* clean up and format the error message  */
        print_querynodes(NULL, leftmost, 1, 1, 1);

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
    } else {
        /* syntax seems ok! pop all/any parantheses pairs expanding the entire width of the query to ease parsing */
        while (leftmost->par == node) {
            node = node->left;
            leftmost = splice_nodes(leftmost, node->right);
        }
    }

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
    if (oper->right->type != TERM) {
        return parse_query(c);
    }

    set_t *l_prod = a->prod;
    set_t *r_prod = c->prod;

    /* perform checks to see if an union operation can be circumvented. */
    if (!l_prod && !r_prod) {
        oper->prod = NULL;
        printf("set operation avoided with (%s && %s): null-sets.\n", a->token, c->token);
    } else if (!r_prod || (l_prod == r_prod)) {
        /* Combined trigger. either left set is NULL, or sets are duplicate 
         * pointers from from equal <word> leaves, e.g. `a OR a` */
        oper->prod = l_prod;
        oper->free_prod = a->free_prod;
        printf("set operation avoided with '%s': inherited left set:\n", c->token);
    } else if (!l_prod) {
        /* inherit the product of the right term */
        oper->prod = r_prod;
        oper->free_prod = c->free_prod;
        printf("set operation avoided with '%s': inherited right set:\n", a->token);
    } else {
        /* Both products are non-empty sets, and an union operation is nescessary. */
        oper->prod = set_union(l_prod, r_prod);
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

/* turn a node into a list of query_result_t. returns an empty list if node==NULL. */
static list_t *get_query_results(qnode_t *node) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);
    if (!query_results) {
        return NULL;
    }

    if (node && (node->prod && set_size(node->prod))) {
        set_iter_t *path_iter = set_createiter(node->prod);
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

// static qnode_t *test_preprocessor_time(index_t *index, list_t *tokens, char **errmsg) {
//     const unsigned long long t_start = gettime();
//     qnode_t *leftmost = format_tokens(index, tokens, errmsg);
//     const unsigned long long t_end = (gettime() - t_start);
//     if (leftmost == NULL) {
//         return NULL;
//     }
//     if (list_size(tokens) != index->print_once) {
//         printf("Taking Times for Query:\n");
//         print_querynodes(tokens, leftmost, 1, 1, 0);
//         printf("     ________Times________\n");
//         printf(" %11s %14s \n", "Run No.", "Time (μs)");
//         index->print_once = list_size(tokens);
//         index->run_number = 0;
//     }
//     index->run_number++;
//     printf("%11d %14llu\n", index->run_number, t_end);
//     return leftmost;
// }

/* note: <orterm>  ::=  <term>  |  <term> "OR" <orterm> */
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

static void print_querynodes(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS) {
    qnode_t *n = leftmost;
    if (DETAILS) {
        char *msg;
        printf("%6s   %13s\n", "type", "token");
        int c = 97;
        while (n != NULL) {
            switch (n->type) {
                case TERM:
                    printf("%6d   %13c\n", n->type, c);
                    c = (c == 122) ? (97) : (c+1);
                    break;
                case OP_OR:
                    msg = "OR";
                    break;
                case OP_AND:
                    msg = "AND";
                    break;
                case OP_ANDNOT:
                    msg = "ANDNOT";
                    break;
                case L_PAREN:
                    msg = "(";
                    break;
                case R_PAREN:
                    msg = ")";
                    break;
            }
            if (n->type != TERM) {
                printf("%6d   %13s\n", n->type, msg);
            }
            n = n->right;
        }
    }

    if (ORIGINAL && query != NULL) {
        list_iter_t *tok_iter = list_createiter(query);
        printf("Original:  `");
        while (list_hasnext(tok_iter)) {
            char *tok = list_next(tok_iter);
            if (isupper(tok[0])) {
                printf(" %s ", tok);
            } else {
                printf("%s", tok);
            }
        }
        list_destroyiter(tok_iter);
        printf("`\n");
    }

    if (FORMATTED) {
        n = leftmost;
        printf("Formatted: `");
        int c = 97;
        while (n != NULL) {
            switch (n->type) {
                case TERM:
                    printf("%c", (char)c);
                    c = (c == 122) ? (97) : (c+1);
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

/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
/* sudo lsof -i -P | grep LISTEN | grep :$8080 */

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
set operation avoided with (horse && dog): null-sets.
 < horse > 
>>
set operation avoided with 'horse': inherited right set:
 < x > 
>>
set operation avoided with 'banana': inherited left set:
 < x > 
>>
set operation avoided with 'cow': inherited left set:
 < x > 
>>
set operation avoided with 'fish': inherited left set:
 < x > 
>>
created union set
x ->  [xORcat]  <- cat
^^^ returning node [xORcat]


*/
