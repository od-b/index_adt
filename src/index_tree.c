/*
 * Index ADT implementation done almost solely through the use of tree-based sets 
 * TODO: [README LINK]
*/

#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"

#include <ctype.h>
#include <string.h>


#define UNUSED(x) { (void)(x); }

enum term_type {
    WORD      =  0,
    OP_ANDNOT =  1,
    OP_AND    =  2,
    OP_OR     =  3,
    P_OPEN    = -1,
    P_CLOSE   = -2,
    SUBQUERY  = -3
};

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


/* --- COMPARISON | HELPER FUNCTIONS --- */

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

/* --- CREATION | DESTRUCTION --- */

/*
 * initializes and returns a new query result 
 */
static query_result_t *create_query_result(char *path, double score) {
    query_result_t *result = malloc(sizeof(query_result_t));
    if (result == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    result->path = path;
    result->score = score;

    return result;
}

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

    /* strdup calls malloc so should technically call != NULL before adding, but it feels like overkill. */

    return new_index;
}

void index_destroy(index_t *index) {
    i_path_t *curr_i_path;
    i_word_t *curr_i_word;
    set_iter_t *i_path_iter;
    set_iter_t *i_word_iter;

    /* Seeing as the sets contain shared pointers to i_paths,
     * create a set of all paths to avoid dismembering any set nodes */
    set_t *all_i_paths = set_create((cmpfunc_t)compare_i_paths_by_string);

    if (all_i_paths == NULL) {
        ERROR_PRINT("out of memory.");
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
    list_iter_t *words_iter = list_createiter(words);
    i_path_t *new_i_path = create_i_path(path);

    if (words_iter == NULL || new_i_path == NULL) {
        ERROR_PRINT("out of memory");
    }

    /* iterate over all words in the provided list */
    void *word;
    while ((word = list_next(words_iter)) != NULL) {
        index->search_word->word = word;

        /* try to add the word to the index */
        i_word_t *i_word = set_put(index->i_words, index->search_word);

        if (i_word == index->search_word) {
            /* search word was added. initialize it as an indexed word. */
            // i_word->word = copy_string(word);
            i_word->word = word;
            i_word->i_paths_with_word = set_create((cmpfunc_t)compare_i_paths_by_string);

            /* Since the search word was added, create a new i_word for searching. */
            index->search_word = malloc(sizeof(i_word_t));

            if (i_word->word == NULL || i_word->i_paths_with_word == NULL || index->search_word == NULL) {
                ERROR_PRINT("out of memory");
            }
            index->search_word->i_paths_with_word = NULL;
        } else {
            /* free the duplicate word */
            free(word);
        }

        /* reset the search word to NULL */
        index->search_word->word = NULL;

        /* add the file index to the current index word */
        set_add(i_word->i_paths_with_word, new_i_path);

        /* add word to the file index */
        set_add(new_i_path->i_words_at_path, i_word);
    }
    list_destroyiter(words_iter);
}


/* --- PARSER --- 
 * `(graph AND theory) ANDNOT (dot OR math)``
 * 
*/

static int token_type(char *str) {
    /* find the type of word and store as an int, (<word> / "AND" / "(" / etc)*/
    if (str[0] == '(')
        return P_OPEN;
    if (str[0] == ')')
        return P_CLOSE;
    if (strcmp(str, "AND") == 0)
        return OP_AND;
    if (strcmp(str, "OR") == 0)
        return OP_OR;
    if (strcmp(str, "ANDNOT") == 0)
        return OP_ANDNOT;
    return WORD;
}

struct term;
typedef struct term term_t;

/* The type of term. a term can have one of:
 * ->word  : string to search for within files, with or without a related operator.
 * ->query : collection of terms enclosed within parantheses)
 * ->operator / paranthesis type. 
 */
struct term {
    int type;
    int level;
    char *word;
    term_t *query;
    term_t *left;
    term_t *right;
};

/* 
 * Checks whether a query is syntactically valid. Returns a bool.
 * Edge cases still remain to be checked, but this serves as an initial filter for 
 * common errors before attempting to parse the query.
 */
static int is_valid_query(term_t *leftmost_token, char **errmsg) {
    int n_par_open = 0, n_par_close = 0, last_operator = 0;

    if (leftmost_token->right == NULL) {
        /* single term word query - boolean validity check */
        if (leftmost_token->type == WORD) {
            return 1;
        }
        *errmsg = "Single term queries must be a word.";
        return 0;
    }
    /* there's atleast 3 tokens */

    term_t *token, *curr = leftmost_token;

    if (leftmost_token->type == P_OPEN) {
        n_par_open++;
    } else if (leftmost_token->type != WORD) {
        *errmsg = "Syntax Error: Search must begin with an opening paranthesis or word.";
        return 0;
    }

    /* iterate over the rest of the query to check for errors */
    while (curr->right != NULL) {
        curr = curr->right;

        if (curr->type < 0) {
            /* current token is a paranthesis. forget the last operator. */
            last_operator = 0;

            if (curr->type == P_OPEN) {
                n_par_open++;
                if (curr->right == NULL) {
                    *errmsg = "Syntax Error: Query cannot end with an opening paranthesis.";
                    return 0;
                } else if (curr->right->type > 0) {
                    *errmsg = "Syntax Error: Queries may not begin with an operator.";
                    return 0;
                }
            } else if (curr->type == P_CLOSE) {
                n_par_close++;
                if (curr->left->type == P_OPEN) {
                    *errmsg = "Syntax Error: Empty parantheses.";
                    return 0;
                }
                if (n_par_open < n_par_close) {
                    *errmsg = "Syntax Error: Unmatched paranthesis.";
                    return 0;
                }
                if (curr->left->type > 0) {
                    *errmsg = "Syntax Error: A paranthesis may not be closed directly after an operator.";
                    return 0;
                }
            }
        } else if (curr->type > 0) {
            /* current token is an operator */
            if (last_operator) {
                if (curr->type == OP_ANDNOT) {
                    *errmsg = "Syntax Error: The ANDNOT operator cannot be chained without the use of parantheses.";
                    return 0;
                }
                if (last_operator != curr->type) {
                    *errmsg = "Syntax Error: Different types of operators must be separated by parantheses.";
                    return 0;
                }
                /* case of chained AND/OR operators. 
                 * check for correct parantheses use, if any exist. */
                int has_par_left = 0;
                int has_par_right = 0;
                /* if chained, and there is a paranthesis on one side, 
                 * require parantheses on both. (either open/close) 
                 */
                token = curr;
                while (token->left != NULL) {
                    token = token->left;
                    if (token->type < 0) {
                        has_par_left = 1;
                        break;
                    }
                }
                token = curr;
                while (token->right != NULL) {
                    token = token->right;
                    if (token->type < 0) {
                        has_par_right = 1;
                        break;
                    }
                }
                if (has_par_left != has_par_right) {
                    *errmsg = "Syntax Error: Use parantheses when chaining AND/OR operators in combined queries.";
                    return 0;
                }
            }

            if (curr->left->type > 0) {
                *errmsg = "Syntax Error: Adjacent operators";
                return 0;
            }

            /* operator cannot be enclosed in parantheses; e.g. `cat (AND) dog`, as operators alone are not a <term>. */
            if ((curr->left->type == P_OPEN) && (curr->right->type == P_CLOSE)) {
                *errmsg = "Syntax Error: Operator has no connected term.";
                return 0;
            }

            /* validate that first non-paranthesis token on left side of operator is a word */
            token = curr;
            while (token->type != WORD) {
                token = token->left;
                if ((token == NULL) || (token->type > 0)) {
                    *errmsg = "Syntax Error: 1 Operator has no connected term | Adjacent operators.";
                    return 0;
                }
            }
            /* remember current operator for when the next operator comes */
            last_operator = curr->type;
        }
    }

    /* for last token, verify that an operator is not the first non-paranthesis. */
    while (curr->type != WORD) {
        curr = curr->left;
        if ((curr == NULL) || (curr->type > 0)) {
            *errmsg = "Syntax Error: 2 Operator has no connected term | Adjacent operators.";
            return 0;
        }
    }

    if (n_par_open != n_par_close) {
        *errmsg = "Syntax Error: Opening and closing paranthesis do not match.";
        return 0;
    }

    DEBUG_PRINT("Syntax Error: Query passed all tests and should be syntactically valid.\n");
    return 1;
}

static void destroy_terms(term_t *leftmost_term) {
    term_t *curr = leftmost_term;
    term_t *tmp;

    while (curr != NULL) {
        tmp = curr;
        if (tmp->query != NULL) {
            destroy_terms(tmp->query);
        }
        free(tmp);
        curr = curr->right;
    }
}

static term_t *create_token_terms(list_t *query) {
    list_iter_t *q_iter = list_createiter(query);
    if (q_iter == NULL) {
        return NULL;
    }
    term_t *prev = NULL, *first = NULL;

    char *elem;
    while ((elem = list_next(q_iter)) != NULL) {
        term_t *token = malloc(sizeof(term_t));
        if (token == NULL) {
            return NULL;
        }

        if (first == NULL) {
            first = token;
            token->left = NULL;
        } else {
            prev->right = token;
            token->left = prev;
        }

        token->type = token_type(elem);
        if (token->type == WORD) {
            token->word = elem;
        } else {
            token->word = elem;     /* FOR TESTING */
            // token->word = NULL;
        }
        token->query = NULL;
        token->right = NULL;
        prev = token;
    }
    list_destroyiter(q_iter);
    return first;
}

static set_t *parse_query(index_t *index, term_t *term) {
    term_t *curr = term;
    // (a AND b) OR ((c OR d) ANDNOT (x OR y))
    // a OR ((b AND c) ANDNOT (x OR y))
    UNUSED(curr);


    /* detect if we are in an enclosed parantheses without other parantheses */
    /* if there is a nested parantheses, enter it. */
    return NULL;
}

static int analyze_depth(term_t *leftmost_token) {
    term_t *curr = leftmost_token;
    int level = 0;
    int max_level = 0;

    while (curr != NULL) {
        curr->level = level;
        if (curr->type == P_OPEN) {
            level += 1;
            if (level > max_level) {
                max_level = level;
            }
        } else if (curr->type == P_CLOSE) { 
            level -= 1;
            curr->level = level;
        }
        curr = curr->right;
    }
    return max_level;
}

list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    term_t *leftmost_token = create_token_terms(query);

    UNUSED(parse_query);
    UNUSED(index_search);
    UNUSED(create_query_result);

    int max_depth = analyze_depth(leftmost_token);

    term_t *curr = leftmost_token;
    while (curr != NULL) {
        printf("'%s'->level = %d\n", curr->word, curr->level);
        curr = curr->right;
    }

    printf("max_depth=%d\n", max_depth);

    if (!is_valid_query(leftmost_token, errmsg)) {
        destroy_terms(leftmost_token);
        return NULL;
    }

    *errmsg = "valid query.";
    destroy_terms(leftmost_token);
    return NULL;
}






/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
/* cd code/C/eksamen23/exam_precode/ && make clean && make indexer && ./indexer data/cacm/  */

/*
  <query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
  <andterm> ::=  <orterm> | <orterm> "AND" <andterm>
  <orterm>  ::=  <term> | <term> "OR" <orterm>
  <term>    ::=  "(" <query> ")" | <word>
*/


// static void print_query_string(list_t *query) {
//     list_iter_t *query_iter = list_createiter(query);
// 
//     char *query_term;
//     while ((query_term = list_next(query_iter)) != NULL) {
//         printf("%s", query_term);
//     }
//     printf("\n");
//     list_destroyiter(query_iter);
// }

// /* debugging function to print result details */
// static void print_results(list_t *results, list_t *query) {
//     list_iter_t *iter = list_createiter(results);
//     query_result_t *result;
// 
//     printf("\nFound %d results for query '", list_size(results));
//     print_query_string(query);
//     printf("' = {\n");
// 
//     int n = 0;
//     while ((result = list_next(iter)) != NULL) {
//         printf(" result #%d = {\n   score: %lf\n", n, result->score);
//         printf("   path: %s\n }\n", result->path);
//         n++;
//     }
//     printf("}\n");
// 
//     list_destroyiter(iter);
// }



// const int total_n_tokens = list_size(query);

// // /* check the entire query for syntax errors */
// // if (!is_valid_query(query, total_n_tokens, errmsg)) {
// //     return NULL;
// // }

// list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);

// /* case for single term queries */
// if (total_n_tokens == 1) {
//     char *term = list_popfirst(query);

//     if (results == NULL) {
//         return NULL;
//     }

//     i_word_t *search_result = index_search(index, term);
//     if (search_result != NULL) {
//         /* iterate over i_paths that contain indexed word */
//         set_iter_t *file_iter = set_createiter(search_result->i_paths_with_word);

//         double score = 0.0;
//         i_path_t *i_path;
//         while ((i_path = set_next(file_iter)) != NULL) {
//             score += 0.1;
//             list_addlast(results, create_query_result(i_path->path, score));
//         }
//         set_destroyiter(file_iter);

//         if (list_size(results) != 1) {
//             /* sort results by score */
//             list_sort(results);
//         }
//     }
//     free(term);
//     return results;
// }


// if (list_size(query) % 2 == 0) {
//     /* Any given valid query will always have an odd number of tokens. Reasoning:
//     * '<word> <word>' results in the indexer adding an "OR" inbetween.
//     * As a consequence of this, all words must (or will, for "OR"), be connected by an operator.
//     * Every operator must have adjacent terms, and parantheses must occur in multiples of 2.
//     * => for every word, operator or parantheses added in addition to the 'root' word, n_tokens will 
//     * be incremented by 2 if query is valid, and |tokens| will therefore never be even */
//     *errmsg = "Invalid use of parantheses or operators.";
//     return NULL;
// }


/*
    unsigned long long t_start, t_time;
    t_start = gettime();
    if (!is_valid_query(leftmost_token, errmsg)) {
        destroy_terms(leftmost_token);
        return NULL;
    }
    t_time = (gettime() - t_start);
    printf("validity checks took a total of %llu μs\n", t_time);

*/