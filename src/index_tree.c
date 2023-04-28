/*
 * Index ADT implementation relying mainly on the use of tree-based set(s)
*/


#include "index.h"
#include "common.h"
#include "printing.h"
#include "map.h"
#include "set.h"    // aatreeset || rbtreeset
#include "pile.h"   // simple stack

#include <ctype.h>
#include <string.h>


#define ERRMSG_MAXLEN  254 
#define L_PAREN   -2
#define R_PAREN   -1
#define OP_OR      1
#define OP_AND     2
#define OP_ANDNOT  3
#define TERM       0


struct qnode;
typedef struct qnode qnode_t;


/* type of indexed word */
typedef struct i_word {
    char      *word;
    set_t     *paths;          /* set of paths where this word can be found */
} i_word_t;

/* type of index */
typedef struct index {
    set_t     *i_words;        /* set of all indexed words */
    i_word_t  *word_buf;       /* i_word for searching / appending purposes */
    char      *errmsg_buf;     /* buffer to ease customization of error messages */
    int        print_once;
    int        run_number;
} index_t;

/* type of query node */
struct qnode {
    int        type;
    set_t     *product;        /* result of a completed term. will be a set or NULL. */
    int        free_product;   /* track if the product points to an indexed set or a gen2 set, e.g. union. */
    qnode_t   *r_paren;        /* L_PAREN only. Holds a pointer to the respective right parenthesis */
    qnode_t   *left;
    qnode_t   *right;
};


/* 
 * functions that require preempty declaration will be listed here
*/

static void query_printnodes(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS);
static list_t *get_query_results(qnode_t *node);
static qnode_t *term_andnot(qnode_t *oper);
static qnode_t *term_and(qnode_t *oper);
static qnode_t *term_or(qnode_t *oper);
static qnode_t *parse_query(qnode_t *node);
static qnode_t *splice_nodes(qnode_t *l, qnode_t *r);
static void destroy_product(qnode_t *term);


/*
 *  -------- Section 1: Index Creation, Destruction and Building --------
*/


/* compares two i_word_t objects by their ->word */
int compare_i_words_by_string(i_word_t *a, i_word_t *b) {
    return strcmp(a->word, b->word);
}

/* compares two i_word_t objects by how many files(paths) they appear in */
int compare_i_words_by_n_occurances(i_word_t *a, i_word_t *b) {
    return (set_size(a->paths) - set_size(b->paths));
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}


// static void print_arr(int *arr, int n) {
//     for (int i = 0; i < n; i++) {
//         // printf("%d:  %d\n", i, arr[i]);
//         printf("%d", arr[i]);
//         if (i == n-1) {
//             printf("\n");
//         } else {
//             printf(", ");
//         }
//     }
// }

index_t *index_create() {
    index_t *ind = malloc(sizeof(index_t));
    if (!ind) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    ind->i_words = set_create((cmpfunc_t)compare_i_words_by_string);
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

/* destroys the product set of a node, unless the set is from a word. Does not destroy the term. */
static void destroy_product(qnode_t *term) {
    if (term && (term->product && term->free_product)) {
        set_destroy(term->product);
        term->product = NULL;
    }
}

/*
 * * Identifies a majority of typical issues regarding query syntax.
 * Converts the tokens to query nodes. <word> tokens are terminated at this stage,
 * and will be assigned a pointer to a set of paths, given that the word is contained within
 * the index. The product of <word> will otherwise be a null-pointer.
 */
static qnode_t *format_tokens(index_t *index, list_t *tokens) {
    list_iter_t *tok_iter = list_createiter(tokens);
    pile_t *paranthesis_pile = pile_create();
    map_t *searched_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (!paranthesis_pile || !searched_words || !tok_iter) {
        return NULL;
    }

    qnode_t *leftmost, *prev, *prev_nonpar, *node, *l_paren;
    char *errmsg, *token;
    i_word_t *i_word;
    int i; // iteration count, to improve msg on error.

    /* initialize all declared pointers to NULL */
    leftmost = prev = prev_nonpar = node = l_paren = NULL;
    errmsg = token = NULL;
    i_word = NULL;

    for (i = 1; (!errmsg && list_hasnext(tok_iter)); i++) {
        token = list_next(tok_iter);
        node = malloc(sizeof(qnode_t));
        if (!node) {
            return NULL;
        }

        node->free_product = 1;
        node->product = NULL;
        node->right = NULL;
        node->r_paren = NULL;

        /* perform checks and assign node members depending on the token. */

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
                }
            } else {
                /* link the left paranthesis to its right counterpart */
                l_paren->r_paren = node;
            }
        } else if (isupper(token[0])) {
            /* find type of operator */
            if (strcmp(token, "OR") == 0)
                node->type = OP_OR;
            else if (strcmp(token, "AND") == 0)
                node->type = OP_AND;
            else if (strcmp(token, "ANDNOT") == 0)
                node->type = OP_ANDNOT;
            else {
                /* default case to control that all given uppercase tokens are operators */
                errmsg = "indexer error. Index expexted all uppercase tokens to be OR | AND | ANDNOT";
                break;
            }

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
            if (prev_nonpar && (prev_nonpar->type == TERM)) {
                errmsg = "Adjacent words";
            } else {
                if (map_haskey(searched_words, token)) {
                    /* Duplicate <word> within this query. 
                    * Get result from map instead of searching set again. */
                    node->product = map_get(searched_words, token);
                } else {
                    /* search index for word, add result to node & map of searched words */
                    index->word_buf->word = token;
                    i_word = set_get(index->i_words, index->word_buf);

                    /* set product to the i_word paths, or NULL if <word> is not indexed. */
                    node->product = (i_word) ? (i_word->paths) : (NULL);
                    map_put(searched_words, token, node->product);
                }
                /* mark this node to not have its set destroyed, as that would break the index :^) */
                node->free_product = 0;
            }
            node->type = TERM;
            prev_nonpar = node;
        }

        /* Attach node to the chain */
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
        query_printnodes(NULL, leftmost, 1, 1, 1);

        /* clean up nodes */
        qnode_t *tmp;
        while (leftmost) {
            tmp = leftmost;
            leftmost = leftmost->right;
            free(tmp);
        }
        leftmost = NULL;    // it was NULL, and now its double NULL

        /* format error message and store it in the designated buffer */
        snprintf(index->errmsg_buf, ERRMSG_MAXLEN, "<br>Error around token %d, <b>'%s'</b> ~ %s.", i, token, errmsg);
    } else {
        /* syntax seems ok! pop all/any parantheses pairs expanding the entire width of the query to ease parsing */
        while (leftmost->r_paren == node) {
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

/* 
 * removes a left & right node without dismembering any adjacent nodes.
 * will cause a segfault if theres no node inbetween left & right.
 * returns the node to the right of left node.
 */
static qnode_t *splice_nodes(qnode_t *l, qnode_t *r) {
    if (l == r) {
        DEBUG_PRINT("splice_nodes: left == right\n");
    }

    l->right->left = l->left;
    if (l->left) {
        l->left->right = l->right;
    }

    r->left->right = r->right;
    if (r->right) {
        r->right->left = r->left;
    }

    qnode_t *new_left = l->right;
    free(l);
    free(r);

    return new_left;
}

static qnode_t *term_andnot(qnode_t *oper) {
    return NULL;
}

static qnode_t *term_and(qnode_t *oper) {
    return NULL;
}

static qnode_t *term_or(qnode_t *oper) {
    if (oper->type == TERM) {
        return oper;
    }

    qnode_t *r_node = oper->right;
    if (oper->right->type != TERM) {
        r_node = parse_query(r_node);
        // return term_or(self);
    }

    qnode_t *l_node = oper->left;
    qnode_t *self = r_node->left;

    set_t *lp = l_node->product;     // left product
    set_t *rp = r_node->product;     // right product

    /* perform various checks to see if an union operation can be circumvented. */
    if (!(lp || rp)) {
        self->product = NULL;
    } 
    else if (!rp) {
        self->product = lp;
        self->free_product = l_node->free_product;
    } 
    else if (!lp) {
        self->product = rp;
        self->free_product = r_node->free_product;
    } 
    else if (lp == rp) {
        /* Duplicate sets from equal <word> leaves, e.g. `a OR a` */
        self->product = lp;
        self->free_product = 0;
    } 
    else {
        /* Both products are non-empty sets, and an union operation is nescessary. */
        self->product = set_union(lp, rp);

        /* free any sets created by the parser that are no longer needed */
        if (l_node->free_product)
            set_destroy(lp);
        if (r_node->free_product)
            set_destroy(rp);
    }

    /* consume the terms and return self as a term. */
    self->type = TERM;
    return splice_nodes(l_node, r_node);
}

/*
 * <query>   ::=  <andterm>  |  <andterm> "ANDNOT" <query>
 * <andterm> ::=  <orterm>  |  <orterm> "AND" <andterm>
 * <orterm>  ::=  <term>  |  <term> "OR" <orterm>
 * <term>    ::=  "(" <query> ")"  |  <word>
*/

/* <term> ::= '(' <query> ')' | <word> */
static qnode_t *parse_query(qnode_t *node) {

    /* base case: nothing more to parse */
    if (!(node->left || node->right)) {
        return node;
    }

    switch (node->type) {
        case L_PAREN:
            parse_query(splice_nodes(node, node->r_paren));
            break;
        case OP_OR:
            term_or(node);
            break;
        case OP_AND:
            term_and(node);
            break;
        case OP_ANDNOT:
            term_andnot(node);
            break;
    }

    if (node->right)
        return parse_query(node->right);
    else if (node->left)
        return parse_query(node->left);
    else
        return node;
}

/* ((a OR b) OR (c OR d OR e)) OR (f OR g OR h) */
/* ((a AND b) OR (c OR d OR e)) ANDNOT (f OR g AND h) */

list_t *index_query(index_t *index, list_t *tokens, char **errmsg) {
    if (!list_size(tokens)) {
        *errmsg = "empty query";
        return NULL;
    }

    qnode_t *leftmost = format_tokens(index, tokens);
    if (!leftmost) {
        if (index->errmsg_buf) {
            *errmsg = index->errmsg_buf;
        } else {
            *errmsg = "out of memory";
        }
        return NULL;
    }

    qnode_t *parsed = parse_query(leftmost);
    if (!parsed) {
        *errmsg = "parsing error";
        return NULL;
    }

    list_t *query_results = get_query_results(parsed);
    if (!query_results) {
        *errmsg = "out of memory";
    }/* else {
        score_query_results(query_result);
    }*/

    destroy_product(parsed);
    free(parsed);

    return query_results;
}

static list_t *get_query_results(qnode_t *node) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);

    if (query_results && node->product && set_size(node->product)) {
        set_iter_t *path_iter = set_createiter(node->product);
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
//         query_printnodes(tokens, leftmost, 1, 1, 0);
//         printf("     ________Times________\n");
//         printf(" %11s %14s \n", "Run No.", "Time (μs)");
//         index->print_once = list_size(tokens);
//         index->run_number = 0;
//     }
//     index->run_number++;
//     printf("%11d %14llu\n", index->run_number, t_end);
//     return leftmost;
// }

static void query_printnodes(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS) {
    qnode_t *n = leftmost;
    if (DETAILS) {
        char *msg;
        printf("%6s   %13s\n", "type", "token");
        int c = 97;
        while (n != NULL) {
            switch (n->type) {
                case 0:
                    printf("%6d   %13c\n", n->type, c);
                    c = (c == 122) ? (97) : (c+1);
                    break;
                case 1:
                    msg = "OR";
                    break;
                case 2:
                    msg = "AND";
                    break;
                case 3:
                    msg = "ANDNOT";
                    break;
                case -2:
                    msg = "(";
                    break;
                case -1:
                    msg = ")";
                    break;
                default:
                    ERROR_PRINT("unknown type %d\n", n->type);
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
                case 0:
                    printf("%c", (char)c);
                    c = (c == 122) ? (97) : (c+1);
                    break;
                case 1:
                    printf(" OR ");
                    break;
                case 2:
                    printf(" AND ");
                    break;
                case 3:
                    printf(" ANDNOT ");
                    break;
                case -2:
                    printf("(");
                    break;
                case -1:
                    printf(")");
                    break;
                default:
                    printf("_ERROR_");
                    break;
            }
            n = n->right;
        }
        printf("`\n");
    }
}

/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
/* sudo lsof -i -P | grep LISTEN | grep :$8080 */

