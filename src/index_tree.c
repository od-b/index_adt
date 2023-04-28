/*
 * Index ADT implementation done almost solely through the use of tree-based sets 
 * TODO: [README LINK]
*/

#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"
#include "pile.h"
#include "map.h"

#include <ctype.h>
#include <string.h>


/*
 *  -------- Section 1: Index Creation, Destruction and Building --------
*/

static const size_t ERRMSG_MAXLEN = 254;

/* type of indexed word */
typedef struct i_word {
    char     *word;
    set_t    *paths;  /* paths that contain i_word->word */
} i_word_t;

typedef struct index {
    set_t    *i_words;        /* set containing all i_word_t within the index */
    i_word_t *word_buf;    /* variable i_word_t for internal searching purposes */
    int print_once;
    int run_number;
    char *errmsg_buf;
} index_t;


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
    index_t *new_index = malloc(sizeof(index_t));
    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    new_index->i_words = set_create((cmpfunc_t)compare_i_words_by_string);
    new_index->word_buf = malloc(sizeof(i_word_t));
    new_index->errmsg_buf = calloc(ERRMSG_MAXLEN, sizeof(char));
    if (new_index->i_words == NULL || new_index->word_buf == NULL || new_index->errmsg_buf == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }
    new_index->errmsg_buf[ERRMSG_MAXLEN+1] = '\0';
    new_index->word_buf->word = NULL;
    new_index->word_buf->paths = NULL;

    /* for tests etc */
    new_index->print_once = 0;
    new_index->run_number = 0;

    return new_index;
}

void index_destroy(index_t *index) {
    int n_freed_words = 0;
    int n_freed_paths = 0;
    set_iter_t *i_word_iter = set_createiter(index->i_words);

    while (set_hasnext(i_word_iter)) {
        i_word_t *curr = set_next(i_word_iter);
        set_iter_t *path_iter = set_createiter(curr->paths);

        char *path;
        while (set_hasnext(path_iter)) {
            path = set_next(path_iter);
            if (path != NULL) {
                free(path);
                n_freed_paths++;
            }
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
 *  -------- Section 2: Parser & Query --------
*/

struct qnode;
typedef struct qnode qnode_t;

struct qnode {
    int type;
    int product_destroyable;
    set_t *product;     /* if node with type == TERM: set product of term | NULL if empty result */
    qnode_t *r_paren;   /* if node->type == L_PAREN: holds a pointer to the respective right parenthesis */
    qnode_t *left;
    qnode_t *right;
};

/* parser & query declarations to allow organizing function order */

// static qnode_t *test_preprocessor_time(index_t *index, list_t *query, char **errmsg);
static void query_printnodes(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS);
static list_t *get_query_results(qnode_t *node);
static qnode_t *term_andnot(qnode_t *oper);
static qnode_t *term_and(qnode_t *oper);
static qnode_t *term_or(qnode_t *oper);
static qnode_t *parse_query(qnode_t *node);
static qnode_t *splice_nodes(qnode_t *l, qnode_t *r);
static void destroy_product(qnode_t *term);


/* 
 * Defined types of query nodes, to improve readability. Not certain if there's any side effects, 
 * but classified as static consts instead of defines to avoid any potential namespace issues
*/

static const int L_PAREN    = -2;
static const int R_PAREN    = -1;
static const int OP_OR      =  1;
static const int OP_AND     =  2;
static const int OP_ANDNOT  =  3;
static const int TERM       =  0;


/* whether a given node is an operator. Checks for node == NULL. */
static int is_operator(qnode_t *node) {
    return (node == NULL) ? (0) : ((node->type > 0) ? (1) : (0));
}

/* destroys the product set of a node, unless that set belongs to the index. */
static void destroy_product(qnode_t *term) {
    if ((term->product != NULL) && (term->product_destroyable)) {
        set_destroy(term->product);
    }
}

/*
 * Verifies the query syntax.
 * Converts the tokens to query nodes. <word> tokens are terminated at this stage,
 * and will be assigned a pointer to a set of paths if the word is contained within
 * the index. The product will otherwise be a null-pointer.
 * Removes some meaningless parantheses, e.g. '(' <word> ')'
 */
static qnode_t *verify_tokens(index_t *index, list_t *tokens) {
    list_iter_t *tok_iter = list_createiter(tokens);
    pile_t *paranthesis_pile = pile_create();
    map_t *searched_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (paranthesis_pile == NULL || searched_words == NULL || tok_iter == NULL) {
        return NULL;
    }

    qnode_t *prev = NULL, *node = NULL;
    qnode_t *leftmost = NULL;
    int token_n = 0;
    char *msg = NULL, *token = NULL;

    while (list_hasnext(tok_iter)) {
        token_n++;
        token = list_next(tok_iter);
        node = malloc(sizeof(qnode_t));
        if (node == NULL) {
            return NULL;
        }
        node->product_destroyable = 1;
        node->product = NULL;
        node->right = NULL;
        node->r_paren = NULL;

        if (token[0] == '(') {
            node->type = L_PAREN;
            pile_push(paranthesis_pile, node);
        } else if (token[0] == ')') {
            node->type = R_PAREN;
            qnode_t *l_paren = pile_pop(paranthesis_pile);

            if (prev == NULL || l_paren == NULL) {
                msg = "Unmatched closing paranthesis";
                goto error;
            }
            if (prev == l_paren) {
                msg = "Parantheses must contain a query";
                goto error;
            }
            /* If the parantheses only wrap a single <word>,
             * pop them right away as they dont serve any logical purpose */
            if (l_paren->right == prev) {
                if (prev->left->type == TERM) {
                    // for cases like `a AND b (c) OR d`
                    msg = "Adjacent words.";
                    goto error;
                }
                if (l_paren == leftmost) {
                    leftmost = prev;
                } else {
                    l_paren->left->right = prev;
                }
                prev->left = l_paren->left;
                free(node);
                free(l_paren);
                if (list_hasnext(tok_iter)) {
                    continue;
                } else {
                    break;
                }
            } else {
                /* link the left paranthesis to its right counterpart */
                l_paren->r_paren = node;
            }
        } else if (isupper(token[0])) {
            /* token is an operator */
            if (strcmp(token, "OR") == 0)
                node->type = OP_OR;
            else if (strcmp(token, "AND") == 0)
                node->type = OP_AND;
            else if (strcmp(token, "ANDNOT") == 0)
                node->type = OP_ANDNOT;
            else {
                msg = "oops. verify_tokens expected an operator.";
                goto error;
                // add the default just in case, so doesn't turn into a bug.
            }
            /* check first non-paranthesis token on left */
            qnode_t *tmp = prev;
            while ((tmp != NULL) && (tmp->type == L_PAREN || tmp->type == R_PAREN)) {
                tmp = tmp->left;
            }
            if ((tmp != NULL) && (is_operator(tmp))) {
                msg = "Adjacent operators.";
                goto error;
            }
        } else {
            node->type = TERM;

            /* check first non-paranthesis token on left */
            qnode_t *tmp = prev;
            while ((tmp != NULL) && (tmp->type == L_PAREN || tmp->type == R_PAREN)) {
                tmp = tmp->left;
            }
            if ((tmp != NULL) && (tmp->type == TERM)) {
                msg = "Adjacent words.";
                goto error;
            }
            /* If token from this query has already been searched for within the index,
             * get the search result from map instead of searching the whole index again. */
            if (map_haskey(searched_words, token)) {
                node->product = map_get(searched_words, token);
            } else {
                index->word_buf->word = token;
                i_word_t *indexed_word = set_get(index->i_words, index->word_buf);
                node->product = (indexed_word == NULL) ? (NULL) : (indexed_word->paths);
                map_put(searched_words, token, node->product);
            }
            /* mark this node to not have its set destroyed, as that would break the index. */
            node->product_destroyable = 0;
        }

        if (is_operator(node)) {
            if (prev == NULL) {
                msg = "Operators require adjacent terms.";
                goto error;
            }
            if (is_operator(prev)) {
                msg = "Adjacent operators.";
                goto error;
            }
            if (prev->type == L_PAREN) {
                msg = "Operators may not occur directly after an opening paranthesis.";
                goto error;
            }
        }

        /* Attach node to the chain */
        if (leftmost == NULL) {
            leftmost = node;
            node->left = NULL;
        } else {
            prev->right = node;
            node->left = prev;
        }
        prev = node;
    }

    if (pile_size(paranthesis_pile) != 0) {
        msg = "Unmatched opening paranthesis";
        goto error;
    }

    /* pop all/any parantheses pairs expanding the entire width of the query */
    while (leftmost->r_paren == node) {
        qnode_t *new_right = node->left;
        leftmost = splice_nodes(leftmost, node);
        node = new_right;
    }

    if (is_operator(leftmost) || is_operator(node)) {
        msg = "Query may not begin or end with an operator.";
        goto error;
    }

    map_destroy(searched_words, NULL, NULL);
    pile_destroy(paranthesis_pile);
    list_destroyiter(tok_iter);

    return leftmost;

error:
    /* clean up and format the error message before returning */

    if (leftmost != NULL) {
        query_printnodes(NULL, leftmost, 1, 1, 1);
        /* clean up nodes */
        qnode_t *tmp;
        while (leftmost != NULL) {
            tmp = leftmost;
            leftmost = leftmost->right;
            free(tmp);
        }
    }

    list_destroyiter(tok_iter);
    pile_destroy(paranthesis_pile);
    map_destroy(searched_words, NULL, NULL);

    if (msg && token) {
        snprintf(index->errmsg_buf, ERRMSG_MAXLEN, "[Error at token no. %d, '%s']: %s", token_n, token, msg);
    } else {
        /* cannot see how the following would trigger, but anyways. */
        strncpy(index->errmsg_buf, "undefined error", ERRMSG_MAXLEN);
    }
    return NULL;
}

/* 
 * removes a left & right node without dismembering any adjacent nodes.
 * will cause a segfault if theres no node inbetween left & right.
 * returns the node to the right of left node.
 */
static qnode_t *splice_nodes(qnode_t *l, qnode_t *r) {
    qnode_t *new_left = l->right;

    l->right->left = l->left;
    if (l->left != NULL) {
        l->left->right = l->right;
    }

    r->left->right = r->right;
    if (r->right != NULL) {
        r->right->left = r->left;
    }

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
    if (lp == NULL && rp == NULL) {
        self->product = NULL;
    } 
    else if (rp == NULL) {
        self->product = lp;
        self->product_destroyable = l_node->product_destroyable;
    } 
    else if (lp == NULL) {
        self->product = rp;
        self->product_destroyable = r_node->product_destroyable;
    } 
    else if (lp == rp) {
        /* Duplicate sets from equal <word> leaves, e.g. `a OR a` */
        self->product = lp;
        self->product_destroyable = 0;
    } 
    else {
        /* Both products are non-empty sets, and an union operation is nescessary. */
        self->product = set_union(lp, rp);

        /* free any sets created by the parser that are no longer needed */
        if (l_node->product_destroyable)
            set_destroy(lp);
        if (r_node->product_destroyable)
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
    if ((node->left == NULL) && (node->right == NULL)) {
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

    if (node->right != NULL)
        return parse_query(node->right);
    else if (node->left != NULL)
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

    qnode_t *leftmost = verify_tokens(index, tokens);
    if (leftmost == NULL) {
        *errmsg = index->errmsg_buf;
        return NULL;
    }

    qnode_t *result = parse_query(leftmost);

    if (result == NULL) {
        *errmsg = "parsing error";
        return NULL;
    }

    return get_query_results(result);
}

/* misc helper functions */

static list_t *get_query_results(qnode_t *node) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);
    if (query_results == NULL) {
        return NULL;
    }

    if (node->product != NULL) {
        set_iter_t *path_iter = set_createiter(node->product);
        if (path_iter == NULL) {
            return NULL;
        }

        double score = 0.0;
        char *path;

        while ((path = set_next(path_iter)) != NULL) {
            query_result_t *q_result = malloc(sizeof(query_result_t));
            if (q_result == NULL) {
                ERROR_PRINT("out of memory");
                return NULL;
            }
            q_result->path = path;
            q_result->score = score;
            score += 0.1;
            list_addlast(query_results, q_result);
        }
        set_destroyiter(path_iter);
    }

    return query_results;
}


/* 
 * ------- debugging functions -------
*/

// static qnode_t *test_preprocessor_time(index_t *index, list_t *tokens, char **errmsg) {
//     const unsigned long long t_start = gettime();
//     qnode_t *leftmost = verify_tokens(index, tokens, errmsg);
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
        printf("%6s   %13s   %8s\n", "type", "token", "depth");
        int c = 97;
        while (n != NULL) {
            switch (n->type) {
                case 0:
                    printf("%6d   %13c", n->type, (char)c);
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
