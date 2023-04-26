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
    int print_once;
    int run_number;
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

    /* for tests etc */
    new_index->print_once = 0;
    new_index->run_number = 0;

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

    int num_i_words = set_size(index->i_words);
    int freed_words = 0;

    while ((curr_i_word = set_next(i_word_iter)) != NULL) {
        freed_words++;
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

    int num_i_paths = set_size(all_i_paths);
    int freed_paths = 0;

    /* iterate over the set of all paths, freeing each paths' string and set */
    i_path_iter = set_createiter(all_i_paths);
    while ((curr_i_path = set_next(i_path_iter)) != NULL) {
        freed_paths++;
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
    DEBUG_PRINT("index_destroy: Freed %d/%d paths, %d/%d words\n",
        num_i_paths, freed_paths, num_i_words, freed_words);
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


const int L_PAREN = -2;
const int R_PAREN = -1;
const int OP_ANDNOT = 1;
const int OP_AND = 2;
const int OP_OR = 3;
const int WORD = 0;

struct qnode;
typedef struct qnode qnode_t;

// struct term;
// typedef struct term term_t;
// struct term {
//     int depth;
//     int order;
//     int type;
// };

struct qnode {
    set_t *production;   // if <operator>
    i_word_t *i_word;    // if <word>
    int type;
    int id;
    int depth;
    qnode_t *left;
    qnode_t *right;
};

static qnode_t *test_preprocessor_time(index_t *index, list_t *query, char **errmsg, int *maxdepth);
static void query_print(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS);


static i_word_t *get_indexed_word(index_t *index, char *word) {
    index->search_word->word = word;
    i_word_t *indexed_word = set_get(index->i_words, index->search_word);
    index->search_word->word = NULL;
    return indexed_word;
}

static void destroy_qnodes(qnode_t *leftmost) {
    qnode_t *tmp, *curr = leftmost;

    while (curr != NULL) {
        tmp = curr;
        curr = curr->right;
        if (tmp->production != NULL) {
            set_destroy(tmp->production);
        }
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
static qnode_t *preprocess_query(index_t *index, list_t *query, char **errmsg, int *maxdepth) {
    qnode_t *leftmost = NULL, *prev = NULL, *node = NULL;
    int depth = 0, par_set_id = -1;

    list_iter_t *q_iter = list_createiter(query);
    nstack_t *paranthesis_stack = nstack_create();
    map_t *searched_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (paranthesis_stack == NULL || searched_words == NULL || q_iter == NULL) {
        *errmsg = "out of memory";
        goto error;
    }

    char *token;
    while ((token = list_next(q_iter)) != NULL) {
        node = malloc(sizeof(qnode_t));
        if (node == NULL) {
            *errmsg = "out of memory";
            goto error;
        }
        node->id = 0;
        node->depth = depth;
        node->production = NULL;
        node->right = NULL;

        if (token[0] == '(') {
            node->type = L_PAREN;
            nstack_push(paranthesis_stack, node);
            depth++;
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
            if (prev->type != WORD) {
                qnode_t *tmp = prev;
                /* move a pointer to the first non-paranthesis left of closing paranthesis */
                while (tmp->id != WORD) {
                    tmp = tmp->left;
                    if (tmp == NULL) {
                        *errmsg = "[-1] THIS ERROR CANNOT BE TRIGGERED?";
                        goto error;
                    }
                }
                if (tmp->type != WORD) {
                    *errmsg = "[3] Operators require adjacent terms";
                    goto error;
                }
            }

            /* Passed syntax tests. However, if the parantheses only wrap a single <word>,
             * pop them right away as they dont serve any logical purpose */
            if (opening_par->right == prev) {
                if (prev->left->type == WORD) {
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
                prev->depth -= 1;
                free(node);
                free(opening_par);
                node = NULL;
            } else {
                /* Parantheses contains a non-<word> query. Assign an id to the matching parantheses */
                opening_par->id = node->id = par_set_id;
                par_set_id--;
                node->depth--;
            }
            depth--;
        } else if (strcmp(token, "OR") == 0) {
            node->type = OP_OR;
        } else if (strcmp(token, "AND") == 0) {
            node->type = OP_AND;
        } else if (strcmp(token, "ANDNOT") == 0) {
            node->type = OP_ANDNOT;
        } else {
            node->type = WORD;

            if (prev != NULL) {
                /* check for any left adjacent words, looking past parantheses */
                qnode_t *tmp = prev;
                while ((tmp != NULL) && (tmp->type < 0)) {
                    /* while left is not NULL, a word, or an operator */
                    tmp = tmp->left;
                }
                if ((tmp != NULL) && (tmp->type == WORD)) {
                    *errmsg = "[5] consecutive words.";
                    goto error;
                }
            }
            /* If token from this query has already been searched for within the index,
             * get the search result from map instead of searching the whole index again.
             * Note: Map val be NULL if the word is not indexed. */
            if (map_haskey(searched_words, token)) {
                node->i_word = map_get(searched_words, token);
            } else {
                node->i_word = get_indexed_word(index, token);
                map_put(searched_words, token, node->i_word);
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
                if (node->depth > *maxdepth) {
                    *maxdepth = node->depth;
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
    list_destroyiter(q_iter);

    return leftmost;

error:
    printf("syntax error:\n");
    query_print(NULL, leftmost, 1, 1, 1);
    if (q_iter != NULL)
        list_destroyiter(q_iter);
    if (paranthesis_stack != NULL)
        nstack_destroy(paranthesis_stack);
    if (searched_words != NULL)
        map_destroy(searched_words, NULL, NULL);
    if (leftmost != NULL)
        destroy_qnodes(leftmost);
    return NULL;
}

/* for each path @ to the given indexed word, create and add query_result(s) to results */
static void add_result(list_t *results, set_t *i_paths) {
    set_iter_t *path_iter = set_createiter(i_paths);
    if (path_iter == NULL) {
        ERROR_PRINT("out of memory");
    }

    double score = 0.0;
    i_path_t *i_path;

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

// <query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
// <andterm> ::=  <orterm> | <orterm> "AND" <andterm>
// <orterm>  ::=  <term> | <term> "OR" <orterm>
// <term>    ::=  "(" <query> ")" | <word>

// "ANDNOT" : difference   => <term> \ <term>
// "OR"     : union        => <term> ∪ <term>
// "AND"    : intersection => <term> ∩ <term>


static qnode_t *eval_andnot(qnode_t *node) {
    return NULL;
    UNUSED(node);
}

static qnode_t *eval_and(qnode_t *node) {
    return NULL;
    UNUSED(node);
}

static qnode_t *eval_or(qnode_t *node) {
    return NULL;
    UNUSED(node);
}

static qnode_t *process_query(list_t *results, qnode_t *leftmost, int maxdepth) {
    qnode_t *node = leftmost;

    // for (int d = maxdepth; d >= 0; d--) {
    //     if (node->depth == d) {
    //         switch (node->id) {
    //             case OP_OR:
    //             case OP_AND:
    //             case OP_ANDNOT:
    //             default:
    //                 break;
    //         }
    //     }
    // }

    UNUSED(index);
    UNUSED(results);
    UNUSED(node);
    return NULL;
}


list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    /* step 1: syntax control and formatting */
    int maxdepth = 0;

    // qnode_t *leftmost = preprocess_query(index, query, errmsg, &maxdepth);
    qnode_t *leftmost = preprocess_query(index, query, errmsg, &maxdepth);
    if (leftmost == NULL) {
        return NULL;
    }

    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);
    if (results == NULL) {
        *errmsg = "out of memory";
        return NULL;
    }

    if ((leftmost->right == NULL) && (leftmost->i_word != NULL)) {
        add_result(results, leftmost->i_word->i_paths_with_word);
    } else {
        process_query(results, leftmost, maxdepth);
    }

    destroy_qnodes(leftmost);
    return results;

    UNUSED(test_preprocessor_time);

    *errmsg = "EOF";
    return NULL;
}


/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */



/* 
 * ------- debugging functions -------
*/

static void query_print(list_t *query, qnode_t *leftmost, int ORIGINAL, int FORMATTED, int DETAILS) {
    qnode_t *n = leftmost;
    if (DETAILS) {
        printf("%6s   %13s   %8s   %3s\n", "type", "token", "depth", "id");
        while (n != NULL) {
            char *msg;
            switch (n->type) {
                case WORD:
                    msg = (n->i_word == NULL) ? ("<WORD>") : (n->i_word->word);
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
            printf("%6d   %13s   %8d   %3d\n", n->type, msg, n->depth, n->id);
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
        printf("Formatted: `");
        int c = 97;
        n = leftmost;
        while (n != NULL) {
            switch (n->type) {
                case WORD:
                    if (c == 123) c = 97;
                    printf("%c", (char)c);
                    c++;
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

static qnode_t *test_preprocessor_time(index_t *index, list_t *query, char **errmsg, int *maxdepth) {
    const unsigned long long t_start = gettime();
    qnode_t *leftmost = preprocess_query(index, query, errmsg, maxdepth);
    const unsigned long long t_end = (gettime() - t_start);
    if (leftmost == NULL) {
        return NULL;
    }
    // int n_nodes = 0;
    // qnode_t *tmp = leftmost;
    // while (tmp != NULL) {
    //     n_nodes++;
    //     tmp = tmp->right;
    // }

    if (list_size(query) != index->print_once) {
        printf("Taking Times for Query:\n");
        query_print(query, leftmost, 1, 1, 0);
        printf("     ________Times________\n");
        printf(" %11s %14s \n", "Run No.", "Time (μs)");
        index->print_once = list_size(query);
        index->run_number = 0;
    }
    index->run_number++;
    printf("%11d %14llu\n", index->run_number, t_end);
    return leftmost;
}
