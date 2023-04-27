/*
 * Index ADT implementation done almost solely through the use of tree-based sets 
 * TODO: [README LINK]
*/

#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"
#include "nstack.h"
#include "map.h"

#include <ctype.h>
#include <string.h>


#define UNUSED(x) { (void)(x); }
#define P_ERROR_QUERY 1


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
} index_t;


/* --- COMPARISON | HELPER FUNCTIONS --- 
 * TODO: Test if inline improves performance, if yes, by how much and is it worth the memory tradeoff
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

/* --- CREATION | DESTRUCTION --- */

index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));
    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    new_index->i_words = set_create((cmpfunc_t)compare_i_words_by_string);
    new_index->word_buf = malloc(sizeof(i_word_t));
    if (new_index->i_words == NULL || new_index->word_buf == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    new_index->word_buf->word = NULL;
    new_index->word_buf->paths = NULL;

    /* for tests etc */
    new_index->print_once = 0;
    new_index->run_number = 0;

    return new_index;
}

void index_destroy(index_t *index) {
    i_word_t *curr_i_word;
    set_iter_t *path_iter, *i_word_iter;

    /* Seeing as the sets contain shared pointers to paths,
     * create a set of all paths to avoid dismembering any set nodes */
    set_t *all_paths = set_create((cmpfunc_t)strcmp);

    if (all_paths == NULL) {
        ERROR_PRINT("out of memory.\n");
    }

    i_word_iter = set_createiter(index->i_words);

    const int n_i_words = set_size(index->i_words);
    int freed_words = 0;

    void *path;
    while ((curr_i_word = set_next(i_word_iter)) != NULL) {
        freed_words++;
        /* iterate over the indexed words set of paths */
        path_iter = set_createiter(curr_i_word->paths);

        /* add all paths to the newly created set of paths */
        while ((path = set_next(path_iter)) != NULL) {
            set_add(all_paths, path);
        }
        /* cleanup iter, the nested set and word string */
        set_destroyiter(path_iter);
        set_destroy(curr_i_word->paths);
        free(curr_i_word->word);
        /* free the i_word struct */
        free(curr_i_word);
    }

    /* destroy iter, then the set of i_words */
    set_destroyiter(i_word_iter);
    set_destroy(index->i_words);

    const int n_paths = set_size(all_paths);
    int freed_paths = 0;

    /* iterate over the set of all paths, freeing each paths' string and set */
    path_iter = set_createiter(all_paths);
    while ((path = set_next(path_iter)) != NULL) {
        freed_paths++;
        free(path);
    }

    /* free the iter and set of all paths */
    set_destroyiter(path_iter);
    set_destroy(all_paths);

    /*
     * check: All word and path strings destroyed.
     * check: All i_word_t and i_path_t structs destroyed
     * check: All nested sets contained within i_word/path_t destroyed
     * check: Main set of i_words destroyed
     * Lastly, free the word_buf, then the index struct itself.
     */
    free(index->word_buf);
    free(index);
    DEBUG_PRINT("index_destroy: Freed %d/%d paths, %d/%d words\n",
        n_paths, freed_paths, n_i_words, freed_words);
}

void index_addpath(index_t *index, char *path, list_t *words) {
    /* TODO: test index build with and without list_sort, as well as with different sorts 
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

                if (i_word->word == NULL || i_word->paths == NULL || index->word_buf == NULL) {
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

static set_t *get_i_word_paths(index_t *index, char *word) {
    index->word_buf->word = word;
    i_word_t *indexed_word = set_get(index->i_words, index->word_buf);
    index->word_buf->word = NULL;

    if (indexed_word == NULL) {
        /* word is not indexed */
        return NULL;
    } else {
        return indexed_word->paths;
    }
}

/* --- PARSER --- 
 `(graph AND theory) ANDNOT (dot OR math)``

  <query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
  <andterm> ::=  <orterm> | <orterm> "AND" <andterm>
  <orterm>  ::=  <term> | <term> "OR" <orterm>
  <term>    ::=  "(" <query> ")" | <word>

*/


const int L_PAREN = -2;
const int R_PAREN = -1;
const int OP_ANDNOT = 1;
const int OP_AND = 2;
const int OP_OR = 3;
const int TERM = 0;

struct qnode;
typedef struct qnode qnode_t;

static qnode_t *test_preprocessor_time(index_t *index, list_t *query, char **errmsg);
static void query_printnodes(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS);

struct qnode {
    set_t *product;
    int type;
    int id;
    qnode_t *left;
    qnode_t *right;
};


static void destroy_query_nodes(qnode_t *leftmost) {
    qnode_t *tmp, *curr = leftmost;

    while (curr != NULL) {
        tmp = curr;
        curr = curr->right;
        free(tmp);
    }
}

/*
 * Multi purpose processor: 
 * Grammar control, paranthesis matching, detecting duplicate words, 
 * converting tokens to nodes, assigning i_words to <word> terms,
 * and the removal of meaningless parantheses if they exist.
 * 
 * General purposes: 
 * Make parsing the tokens easier. Filter out common errors before attempting to terminate operators.
 */
static qnode_t *verify_tokens(index_t *index, list_t *tokens, char **errmsg) {
    list_iter_t *tok_iter = list_createiter(tokens);
    nstack_t *paranthesis_stack = nstack_create();
    map_t *searched_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (paranthesis_stack == NULL || searched_words == NULL || tok_iter == NULL) {
        return NULL;
    }

    qnode_t *prev = NULL, *node = NULL;
    int par_set_id = -1;
    qnode_t *leftmost = NULL;

    char *token;
    while ((token = list_next(tok_iter)) != NULL) {
        node = malloc(sizeof(qnode_t));
        if (node == NULL) {
            return NULL;
        }
        node->id = 0;
        node->product = NULL;
        node->right = NULL;

        if (token[0] == '(') {
            node->type = L_PAREN;
            nstack_push(paranthesis_stack, node);
        } else if (token[0] == ')') {
            node->type = R_PAREN;
            qnode_t *opening_par = nstack_pop(paranthesis_stack);

            /* perform general grammar control upon each closed pair of paranthesis */
            if (prev == NULL || opening_par == NULL) {
                *errmsg = "[1] Unmatched closing paranthesis";
                goto error;
            }
            if (prev == opening_par) {
                *errmsg = "[2] Parantheses without query";
                goto error;
            }

            /* verify that first non-paranthesis on left is a not an operator */
            if (prev->type != TERM) {
                qnode_t *tmp = prev;
                /* move a pointer to the first non-paranthesis left of closing paranthesis */
                while (tmp->id != TERM) {
                    tmp = tmp->left;
                    if (tmp == NULL) {
                        *errmsg = "[-1] THIS ERROR CANNOT BE TRIGGERED?";
                        goto error;
                    }
                }
                if (tmp->type != TERM) {
                    *errmsg = "[3] Operators require adjacent terms";
                    goto error;
                }
            }

            /* Passed syntax tests. However, if the parantheses only wrap a single <word>,
             * pop them right away as they dont serve any logical purpose */
            if (opening_par->right == prev) {
                if (prev->left->type == TERM) {
                    // for cases like `a AND b (c) OR d`
                    *errmsg = "[4] Consecutive words.";
                    goto error;
                }
                if (opening_par == leftmost) {
                    leftmost = prev;
                } else {
                    opening_par->left->right = prev;
                }
                prev->left = opening_par->left;
                free(node);
                free(opening_par);
                node = NULL;
            } else {
                /* Parantheses contains a non-<word> query. Assign an id to the matching parantheses */
                opening_par->id = node->id = par_set_id;
                par_set_id--;
            }
        } else if (strcmp(token, "OR") == 0) {
            node->type = OP_OR;
        } else if (strcmp(token, "AND") == 0) {
            node->type = OP_AND;
        } else if (strcmp(token, "ANDNOT") == 0) {
            node->type = OP_ANDNOT;
        } else {
            node->type = TERM;

            if (prev != NULL) {
                /* check for any left adjacent words, looking past parantheses */
                qnode_t *tmp = prev;
                while ((tmp != NULL) && (tmp->type < 0)) {
                    /* while left is not NULL, a word, or an operator */
                    tmp = tmp->left;
                }
                if ((tmp != NULL) && (tmp->type == TERM)) {
                    *errmsg = "[5] consecutive words.";
                    goto error;
                }
            }
            /* If token from this query has already been searched for within the index,
             * get the search result from map instead of searching the whole index again.
             * Note: Map val be NULL if the word is not indexed. */
            if (map_haskey(searched_words, token)) {
                node->product = map_get(searched_words, token);
            } else {
                node->product = get_i_word_paths(index, token);
                map_put(searched_words, token, node->product);
            }
        }

        /* paranthesis node might have been popped. If so, go to next token */
        if (node != NULL) {
            /* some operator checks */
            if (node->type > 0) {
                if (prev == NULL) {
                    *errmsg = "[6] Operators require adjacent queries.";
                    goto error;
                }
                if (prev->type > 0) {
                    *errmsg = "[7] Adjacent operators.";
                    goto error;
                }
                if (prev->type == L_PAREN) {
                    *errmsg = "[8] Operators may not occur directly after an opening paranthesis.";
                    goto error;
                }
            }

            /* all syntax tests seem okay. link to chain of query nodes. and continue with next token. */
            if (leftmost == NULL) {
                leftmost = node;
                node->left = NULL;
            } else {
                prev->right = node;
                node->left = prev;
            }
            prev = node;
        }
    }

    if (nstack_height(paranthesis_stack) != 0) {
        *errmsg = "[9] Unmatched opening paranthesis";
        goto error;
    }

    /* pop all parantheses pairs expanding the entire width of the query */
    while ((node->type == R_PAREN) && (leftmost->id == node->id)) {
        qnode_t *tmp = leftmost;
        leftmost = leftmost->right;
        leftmost->left = NULL;
        free(tmp);
        tmp = node->left;
        node->left->right = NULL;
        free(node);
        node = tmp;
    }

    if ((leftmost->type > 0) || (node->type > 0)) {
        *errmsg = "[10] Query may not begin or end with an operator.";
        goto error;
    }

    map_destroy(searched_words, NULL, NULL);
    nstack_destroy(paranthesis_stack);
    list_destroyiter(tok_iter);

    return leftmost;

error:
    if (leftmost != NULL)
        query_printnodes(NULL, leftmost, 1, 1, 1);
        destroy_query_nodes(leftmost);
    if (tok_iter != NULL)
        list_destroyiter(tok_iter);
    if (paranthesis_stack != NULL)
        nstack_destroy(paranthesis_stack);
    if (searched_words != NULL)
        map_destroy(searched_words, NULL, NULL);
    return NULL;
}

static list_t *get_query_results(qnode_t *node) {
    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);
    if (results == NULL) {
        return NULL;
    }

    if (node == NULL) {
        DEBUG_PRINT("query == NULL, avoided segfault\n");
        return results;
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
            list_addlast(results, q_result);
        }
        set_destroyiter(path_iter);
    }

    return results;
}

// <query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
// <andterm> ::=  <orterm> | <orterm> "AND" <andterm>
// <orterm>  ::=  <term> | <term> "OR" <orterm>
// <term>    ::=  "(" <query> ")" | <word>

// "ANDNOT" : difference   => <term> \ <term>
// "OR"     : union        => <term> ∪ <term>
// "AND"    : intersection => <term> ∩ <term>

static qnode_t *term_andnot(qnode_t *andterm, qnode_t *query);
static qnode_t *term_and(qnode_t *orterm, qnode_t *andterm);
static qnode_t *term_or(qnode_t *term, qnode_t *orterm);
static qnode_t *parse_query(qnode_t *leftmost);


static void consume_terms(qnode_t *oper) {
    oper->type = TERM;
    qnode_t *l_term = oper->left;
    qnode_t *r_term = oper->right;

    oper->left = l_term->left;
    if (l_term->left != NULL) {
        l_term->left->right = oper;
    }

    oper->right = r_term->right;
    if (r_term->right != NULL) {
        r_term->right->left = oper;
    }
    free(l_term);
    free(r_term);
}

static void pop_parens(qnode_t *l_paren, qnode_t *r_paren) {
    l_paren->right->left = l_paren->left;
    if (l_paren->left != NULL) {
        l_paren->left->right = l_paren->right;
    }
    r_paren->left->right = r_paren->right;
    if (r_paren->right != NULL) {
        r_paren->right->left = r_paren->left;
    }
    free(l_paren);
    free(r_paren);
}

static qnode_t *term_andnot(qnode_t *andterm, qnode_t *query) {
    return NULL;
}

static qnode_t *term_and(qnode_t *orterm, qnode_t *andterm) {
    return NULL;
}

static qnode_t *term_or(qnode_t *term, qnode_t *orterm) {
    qnode_t *self = term->right;

    if (orterm->type == TERM) {
        if ((term->product != NULL) && (orterm->product != NULL)) {
            self->product = set_union(term->product, orterm->product);
        } 
        else if (term->product != NULL) {
            self->product = term->product;
        } 
        else if (orterm->product != NULL) {
            self->product = orterm->product;
        } 
        else {
            self->product = NULL;
        }

        consume_terms(self);
        return self;
    } else if (orterm->type == L_PAREN) {
        return term_or(term, parse_query(orterm));
    }
}

static qnode_t *parse_query(qnode_t *leftmost) {
    /* base case: nothing more to parse */
    if ((leftmost->left == NULL) && (leftmost->right == NULL)) {
        return leftmost;
    }

    qnode_t *L = leftmost;
    qnode_t *R = L->right;

    if (L->type == L_PAREN) {
        qnode_t *R_PAR = R;
        while (L->id != R_PAR->id) {
            R_PAR = R_PAR->right;
        }
        pop_parens(L, R_PAR);
        return parse_query(R);
    }

    switch (R->type) {
        case OP_OR:
            return term_or(L, R);
        case OP_AND:
            return term_and(L, R);
        case OP_ANDNOT:
            return term_andnot(L, R);
        default:
            return parse_query(L);
    }
}

/* ((a OR b) OR (c OR d OR e)) OR (f OR g OR h) */
/* ((a AND b) OR (c OR d OR e)) ANDNOT (f OR g AND h) */

list_t *index_query(index_t *index, list_t *tokens, char **errmsg) {
    qnode_t *leftmost = verify_tokens(index, tokens, errmsg);
    if (leftmost == NULL) {
        return NULL;
    }

    qnode_t *parsed_query = parse_query(leftmost);
    list_t *query_results = get_query_results(parsed_query);
    destroy_query_nodes(leftmost);
    free(parsed_query);

    return query_results;
}


/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
/* sudo lsof -i -P | grep LISTEN | grep :$8080 */

/* 
 * ------- debugging functions -------
*/

static void query_printnodes(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS) {
    qnode_t *n = leftmost;
    if (DETAILS) {
        char *msg;
        printf("%6s   %13s   %8s   %3s\n", "type", "token", "depth", "id");
        int c = 97;
        while (n != NULL) {
            switch (n->type) {
                case TERM:
                    printf("%6d   %13c   %3d\n", n->type, (char)c, n->id);
                    c = (c == 122) ? (97) : (c+1);
                    break;
                case OP_AND:
                    msg = "AND";
                    break;
                case OP_ANDNOT:
                    msg = "ANDNOT";
                    break;
                case OP_OR:
                    msg = "OR";
                    break;
                case L_PAREN:
                    msg = "(";
                    break;
                case R_PAREN:
                    msg = ")";
                    break;
                default:
                    ERROR_PRINT("unknown type %d\n", n->type);
                    break;
            }
            if (n->type != TERM) {
                printf("%6d   %13s   %3d\n", n->type, msg, n->id);
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
                case OP_AND:
                    printf(" AND ");
                    break;
                case OP_ANDNOT:
                    printf(" ANDNOT ");
                    break;
                case OP_OR:
                    printf(" OR ");
                    break;
                case L_PAREN:
                    printf("(");
                    break;
                case R_PAREN:
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

static qnode_t *test_preprocessor_time(index_t *index, list_t *tokens, char **errmsg) {
    const unsigned long long t_start = gettime();
    qnode_t *leftmost = verify_tokens(index, tokens, errmsg);
    const unsigned long long t_end = (gettime() - t_start);
    if (leftmost == NULL) {
        return NULL;
    }

    if (list_size(tokens) != index->print_once) {
        printf("Taking Times for Query:\n");
        query_printnodes(tokens, leftmost, 1, 1, 0);
        printf("     ________Times________\n");
        printf(" %11s %14s \n", "Run No.", "Time (μs)");
        index->print_once = list_size(tokens);
        index->run_number = 0;
    }
    index->run_number++;
    printf("%11d %14llu\n", index->run_number, t_end);
    return leftmost;
}
