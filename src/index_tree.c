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


/* type of indexed path */
typedef struct i_path {
    char     *path;               
    set_t    *i_words_at_path;    /* set of *to all words (i_word_t) contained in the file at path location  */
} i_path_t;

/* type of indexed word */
typedef struct i_word {
    char     *word;
    set_t    *i_paths_with_word;  /* set of *to all i_path_t which contain this i_word */
    /* set_t *contained_with? */
} i_word_t;

typedef struct index {
    set_t    *i_words;        /* set containing all i_word_t within the index */
    i_word_t *search_word;    /* variable i_word_t for internal searching purposes */
} index_t;


/* --- COMPARISON | HELPER FUNCTIONS --- 
 * TODO: Test if inline improves performance, if yes, by how much and is it worth the memory tradeoff
*/

/* compares two i_path_t objects by their ->path */
int compare_i_paths_by_string(i_path_t *a, i_path_t *b) {
    return strcmp(a->path, b->path);
}

/* compares two i_word_t objects by their ->word */
int compare_i_words_by_string(i_word_t *a, i_word_t *b) {
    return strcmp(a->word, b->word);
}

/* compares two i_word_t objects by how many files(i_paths) they appear in */
int compare_i_words_by_n_occurances(i_word_t *a, i_word_t *b) {
    return (set_size(a->i_paths_with_word) - set_size(b->i_paths_with_word));
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}

/*
 * if a word is indexed, returns its related i_word_t. Otherwise, returns NULL. 
 */
static i_word_t *index_search(index_t *index, char *word) {
    index->search_word->word = word;
    i_word_t *search_result = set_get(index->i_words, index->search_word);
    index->search_word->word = NULL;
    return search_result;
}

static void print_arr(int *arr, int n) {
    for (int i = 0; i < n; i++) {
        // printf("%d:  %d\n", i, arr[i]);
        printf("%d", arr[i]);
        if (i == n-1) {
            printf("\n");
        } else {
            printf(", ");
        }
    }
}

/* --- CREATION | DESTRUCTION --- */

/*
 * returns a newly created i_path, using the given path.
 * creates an empty set at the i_path->i_words_at_path, with the cmpfunc compare_i_words_by_string
 */
static i_path_t *create_i_path(char *path) {
    i_path_t *new_i_path = malloc(sizeof(i_path_t));
    if (new_i_path == NULL) {
        return NULL;
    }

    new_i_path->path = path;
    if (new_i_path->path == NULL) {
        return NULL;
    }

    /* create an empty set to store indexed paths */
    new_i_path->i_words_at_path = set_create((cmpfunc_t)compare_i_words_by_string);
    if (new_i_path->i_words_at_path == NULL) {
        return NULL;
    }

    return new_i_path;
}

index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));
    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    new_index->i_words = set_create((cmpfunc_t)compare_i_words_by_string);
    new_index->search_word = malloc(sizeof(i_word_t));
    if (new_index->i_words == NULL || new_index->search_word == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    new_index->search_word->word = NULL;
    new_index->search_word->i_paths_with_word = NULL;

    UNUSED(print_arr);

    return new_index;
}

void index_destroy(index_t *index) {
    i_path_t *curr_i_path;
    i_word_t *curr_i_word;
    set_iter_t *i_path_iter, *i_word_iter;

    /* Seeing as the sets contain shared pointers to i_paths,
     * create a set of all paths to avoid dismembering any set nodes */
    set_t *all_i_paths = set_create((cmpfunc_t)compare_i_paths_by_string);

    if (all_i_paths == NULL) {
        ERROR_PRINT("out of memory.\n");
    }

    i_word_iter = set_createiter(index->i_words);

    while ((curr_i_word = set_next(i_word_iter)) != NULL) {
        /* iterate over the indexed words set of paths */
        i_path_iter = set_createiter(curr_i_word->i_paths_with_word);

        /* add all paths to the newly created set of paths */
        while ((curr_i_path = set_next(i_path_iter)) != NULL) {
            set_add(all_i_paths, curr_i_path);
        }
        /* cleanup iter, the nested set and word string */
        set_destroyiter(i_path_iter);
        set_destroy(curr_i_word->i_paths_with_word);
        free(curr_i_word->word);
        /* free the i_word struct */
        free(curr_i_word);
    }

    /* destroy iter, then the set of i_words */
    set_destroyiter(i_word_iter);
    set_destroy(index->i_words);

    /* iterate over the set of all paths, freeing each paths' string and set */
    i_path_iter = set_createiter(all_i_paths);
    while ((curr_i_path = set_next(i_path_iter)) != NULL) {
        free(curr_i_path->path);
        set_destroy(curr_i_path->i_words_at_path);
        /* free the i_path struct */
        free(curr_i_path);
    }

    /* free the iter and set of all paths */
    set_destroyiter(i_path_iter);
    set_destroy(all_i_paths);

    /*
     * check: All word and path strings destroyed.
     * check: All i_word_t and i_path_t structs destroyed
     * check: All nested sets contained within i_word/path_t destroyed
     * check: Main set of i_words destroyed
     * Lastly, free the search_word, then the index struct itself.
     */
    free(index->search_word);
    free(index);
    DEBUG_PRINT("All memory allocated by the index is now freed\n");
}

void index_addpath(index_t *index, char *path, list_t *words) {
    /* TODO: test index build with and without list_sort, as well as with different sorts 
     * will no doubt be correlated to n_words, but so will sorting the list.
     * Compared scaling with varied, large sets needed.
     * If the list sort is slower for small sets, but faster for varied/large sets,
     * using the sort method will be the best option, seeing as small sets are rather quick to add regardless.
    */

    list_iter_t *words_iter = list_createiter(words);
    i_path_t *new_i_path = create_i_path(path);

    if (words_iter == NULL || new_i_path == NULL) {
        ERROR_PRINT("out of memory");
    }

    /* sort the list of words to skip duplicate words from the current list,
     * without having to search the entire main set of indexed words */
    // list_sort(words);

    char *word;

    /* iterate over all words in the provided list */
    while ((word = list_next(words_iter)) != NULL) {

        // /* skip all duplicate words within the current list of words */
        // if ((prev_word == NULL) || (strcmp(word, prev_word) != 0)) {

            /* try to add the word to the index, using search_word as a buffer. */
            index->search_word->word = word;
            i_word_t *i_word = set_tryadd(index->i_words, index->search_word);

            if (i_word == index->search_word) {
                /* first index entry for this word. initialize it as an indexed word. */
                i_word->word = word;
                i_word->i_paths_with_word = set_create((cmpfunc_t)compare_i_paths_by_string);

                /* Since the search word was added, create a new i_word for searching. */
                index->search_word = malloc(sizeof(i_word_t));

                if (i_word->word == NULL || i_word->i_paths_with_word == NULL || index->search_word == NULL) {
                    ERROR_PRINT("out of memory");
                }
                index->search_word->i_paths_with_word = NULL;
            } else {
                /* index is already storing this word. free the string. */
                free(word);
            }

            /* reset the search word to NULL. (initialize, rather, if word was previously unseen) */
            index->search_word->word = NULL;

            /* add path to the i_words' set of paths */
            set_add(i_word->i_paths_with_word, new_i_path);

            /* add word to the i_paths' set of words */
            set_add(new_i_path->i_words_at_path, i_word);

        // } else {
        //     // printf("[%s] %s ", path, word);
        //     /* duplicate word within the current list of words, free and continue to next word. */
        //     free(word);
        // }
        // prev_word = word;
    }
    list_destroyiter(words_iter);
}

/* --- PARSER --- 
 `(graph AND theory) ANDNOT (dot OR math)``

  <query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
  <andterm> ::=  <orterm> | <orterm> "AND" <andterm>
  <orterm>  ::=  <term> | <term> "OR" <orterm>
  <term>    ::=  "(" <query> ")" | <word>

*/


const int P_OPEN = -2;
const int P_CLOSE = -1;
const int OP_ANDNOT = 1;
const int OP_AND = 2;
const int OP_OR = 3;
const int WORD = 0;

struct qnode;
typedef struct qnode qnode_t;

struct qnode {
    i_word_t *i_word;
    char *token;
    int depth;
    int pos;
    int id;
    qnode_t *left;
    qnode_t *right;
};

static void print_query(qnode_t *leftmost) {
    qnode_t *n = leftmost;
    printf("\n");
    while (n != NULL) {
        if (n->i_word != NULL) {
            printf("%1s[ i ] %-13s: d=%d\n", "", n->i_word->word, n->depth);
        } else {
            if (n->id < 0) {
                printf("[ %d ] %-13s: d=%d\n", n->id, n->token, n->depth);
            } else {
                printf("%1s[ %d ] %-13s: d=%d\n", "", n->id, n->token, n->depth);
            }
        }
        n = n->right;
    }
}

static void destroy_qnodes(qnode_t *leftmost) {
    qnode_t *tmp, *curr = leftmost;

    while (curr != NULL) {
        tmp = curr;
        curr = curr->right;
        free(tmp->token);
        free(tmp);
    }
}

/*
 * Purpose: Grammar control, paranthesis matching, token to node conversion
 * Todo: Grammar control
 */
static qnode_t *process_query(index_t *index, list_t *query, char **errmsg) {
    qnode_t *leftmost = NULL, *prev = NULL, *node = NULL;
    int depth = 0, par_set_id = -1;

    nstack_t *par_stack = nstack_create();
    if (par_stack == NULL) {
        *errmsg = "out of memory";
        goto error;
    }

    char *token;
    while ((token = list_popfirst(query)) != NULL) {
        node = malloc(sizeof(qnode_t));
        if (node == NULL) {
            free(token);
            *errmsg = "out of memory";
            goto error;
        }

        node->token = token;
        node->depth = depth;
        node->i_word = NULL;

        if (token[0] == '(') {
            node->id = P_OPEN;
            nstack_push(par_stack, node);
            depth++;
        } else if (token[0] == ')') {
            /* perform grammar control upon each closed pair of paranthesis */
            node->id = P_CLOSE;
            qnode_t *par_match = nstack_pop(par_stack);

            if (prev == NULL || par_match == NULL) {
                *errmsg = "Unmatched closing paranthesis";
                goto error;
            }
            if (prev == par_match) {
                *errmsg = "Parantheses without query";
                goto error;
            }
            if (prev->id != WORD) {
                qnode_t *lterm = prev;
                /* verify that first non-paranthesis on left is a not an operator */
                while ((lterm->left != NULL) && (lterm->id <= 0)) {
                    lterm = lterm->left;
                }
                if (lterm->id != WORD) {
                    *errmsg = "Operators require adjacent terms";
                    goto error;
                }
            }

            /* paranthesis syntax should be valid. Assign an id to the matching parantheses */
            par_match->id = node->id = par_set_id;
            par_set_id--;
            node->depth--;
            depth--;
        } else if (strcmp(token, "OR") == 0) {
            node->id = OP_OR;
        } else if (strcmp(token, "AND") == 0) {
            node->id = OP_AND;
        } else if (strcmp(token, "ANDNOT") == 0) {
            node->id = OP_ANDNOT;
        } else {
            node->id = WORD;
            node->i_word = index_search(index, token);
        }

        if ((prev != NULL) && ((prev->id > 0) && (node->id > 0))) {
            *errmsg = "Adjacent operators";
            goto error;
        }

        node->right = NULL;

        if (leftmost == NULL) {
            leftmost = node;
            node->left = NULL;
        } else {
            prev->right = node;
            node->left = prev;
        }
        prev = node;
    }

    if (nstack_height(par_stack) != 0) {
        *errmsg = "unmatched opening paranthesis";
        goto error;
    }

    nstack_destroy(par_stack);
    return leftmost;

error:
    if (par_stack != NULL) {
        nstack_destroy(par_stack);
    }
    if (leftmost != NULL) {
        destroy_qnodes(leftmost);
    }
    if (node != NULL) {
        /* may not have been linked to the chain yet. */
        free(node);
    }
    return NULL;
}

/* for each path @ to the given indexed word, create and add query_result(s) to results */
static void add_result(list_t *results, i_word_t *i_word) {
    set_iter_t *path_iter = set_createiter(i_word->i_paths_with_word);
    double score = 0.0;
    i_path_t *i_path;
    if (path_iter == NULL) {
        ERROR_PRINT("out of memory");
    }

    while ((i_path = set_next(path_iter)) != NULL) {
        query_result_t *q_result = malloc(sizeof(query_result_t));
        if (q_result == NULL) {
            ERROR_PRINT("out of memory");
        }
        q_result->path = i_path->path;
        q_result->score = score;
        score += 0.1;
        list_addlast(results, q_result);
    }
    set_destroyiter(path_iter);
}

// static set_t *eval_or(qnode_t *op_node);
// static set_t *eval_and(qnode_t *op_node);
// static set_t *eval_andnot(qnode_t *op_node);

static void parse_query(index_t *index, list_t *results, qnode_t *leftmost) {
    print_query(leftmost);

    UNUSED(index);
    UNUSED(results);
    UNUSED(results);
}

list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    qnode_t *leftmost = process_query(index, query, errmsg);
    if (leftmost == NULL) {
        /* errmsg will have been set by process query. */
        return NULL;
    }

    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);

    if (leftmost->right == NULL) {
        i_word_t *search_res = leftmost->i_word;
        if (search_res != NULL) {
            add_result(results, search_res);
        }
        free(leftmost);
    } else {
        parse_query(index, results, leftmost);
    }

    return results;
}


/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
