#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"
#include <ctype.h>
// #include "map.h"
// #include "list.h" -- included through index.h

#include <string.h>     /* strcmp, etc */
// #include <stdio.h> -- included through common.h
// #include <stdlib.h> -- included through printing.h


#define UNUSED(x) { (void)(x); }

#define T_WORD    0
#define O_ANDNOT  1
#define O_AND     2
#define O_OR      3
#define P_OPEN    -1
#define P_CLOSE   -2


/* indexed path. Contains a char* path, and a set_t *i_words_at_path */
typedef struct i_path {
    char     *path;               
    set_t    *i_words_at_path;    /* set of *to all words (i_word_t) contained in the file at path location  */
} i_path_t;

/* indexed word. Contains a char* word, and a set_t *i_paths_with_word */
typedef struct i_word {
    char     *word;
    set_t    *i_paths_with_word;  /* set of *to all i_path_t which contain this i_word */
    /* set_t *contained_with? */
} i_word_t;

typedef struct index {
    set_t    *i_words;        /* set containing all i_word_t within the index */
    i_word_t *search_word;    /* variable i_word_t for internal searching purposes */
} index_t;


/* --- COMPARE FUNCTIONS --- */

/* compares two i_path_t objects by their ->path */
int compare_i_paths_by_string(i_path_t *a, i_path_t *b) {
    return strcmp(a->path, b->path);
}

/* compares two i_word_t objects by their ->word */
int compare_i_words_by_string(i_word_t *a, i_word_t *b) {
    return strcmp(a->word, b->word);
}

/* compares two i_word_t objects by how many files they appear in */
int compare_i_words_by_n_occurances(i_word_t *a, i_word_t *b) {
    return (
        set_size((set_t*)(a)->i_paths_with_word)
        - set_size((set_t*)(b)->i_paths_with_word)
    );
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}


/* --- CREATION, DESTRUCTION --- */

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

/*
 * initialize a struct i_path_t using the given path.
 * set i_path->path to a copy of the given path.
 * creates an empty set @ i_path->i_words_at_path, with cmpfunc_t := compare_i_words_by_string
 */
static i_path_t *create_i_path(char *path) {
    i_path_t *new_i_path = malloc(sizeof(i_path_t));
    if (new_i_path == NULL) {
        return NULL;
    }

    new_i_path->path = copy_string(path);
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

/* 
 * Search for an indexed word within index->i_words. If it does not exist, it will be added and initialized.
 * returns the existing, or newly created, i_word_t.
 */
static i_word_t *get_or_add(index_t *index, char *word) {
    index->search_word->word = word;

    /* try to add the i_word, then compare to determine result */
    i_word_t *i_word = set_put(index->i_words, index->search_word);

    if (i_word == index->search_word) {
        /* word was not a duplicate entry. Initialize the i_word. */
        i_word->word = copy_string(word);
        i_word->i_paths_with_word = set_create((cmpfunc_t)compare_i_paths_by_string);

        /* Since the search word was added, create a new i_word for searching. */
        index->search_word = malloc(sizeof(i_word_t));

        if (i_word->word == NULL || i_word->i_paths_with_word == NULL || index->search_word == NULL) {
            ERROR_PRINT("out of memory");
            return NULL;
        }
        index->search_word->i_paths_with_word = NULL;
    }

    /* reset the search word to NULL */
    index->search_word->word = NULL;

    /* return i_word, be it the newly added one or the existing i_word */
    return i_word;
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

void index_addpath(index_t *index, char *path, list_t *words) {
    list_iter_t *words_iter = list_createiter(words);

    /* i_path_t to store the path + pointers to (or new) words being indexed */
    i_path_t *new_i_path = create_i_path(path);

    if (words_iter == NULL || new_i_path == NULL) {
        ERROR_PRINT("out of memory");
    }

    /* iterate over all words in the provided list */
    void *elem;
    while ((elem = list_next(words_iter)) != NULL) {
        i_word_t *curr_i_word = get_or_add(index, elem);
        free(elem);

        /* add the file index to the current index word */
        set_add(curr_i_word->i_paths_with_word, new_i_path);

        /* add word to the file index */
        set_add(new_i_path->i_words_at_path, curr_i_word);
    }

    /* cleanup */
    free(path);
    list_destroyiter(words_iter);
}

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


/* --- PARSER --- */

static void parse_query(list_t *tokens, list_t *results) {
    // Base case: a single word query
    if (list_size(tokens) == 1) {
        return;
    }

    // check for ANDNOT
    // evaluate the left term and remove it from the tokens list
    // evaluate the right term and remove it from the tokens list

    // check for AND
    // evaluate the left term and remove it from the tokens list
    // evaluate the right term and remove it from the tokens list

    // check for OR
    // evaluate the left term and append its results to the results list
    // evaluate the right term and append its results to the results list
}

/*
 * returns the defined type of token:
    T_WORD    =>    0
    O_ANDNOT  =>    1
    O_AND     =>    2
    O_OR      =>    3
    P_OPEN    =>    -1
    P_CLOSE   =>    -2
 */
static int word_type(char *token) {
    if (token[0] == '(')
        return P_OPEN;
    if (token[0] == ')')
        return P_CLOSE;

    if (isupper(token[0])) {
        if (strcmp(token, "AND") == 0)
            return O_AND;
        if (strcmp(token, "OR") == 0)
            return O_OR;
        if (strcmp(token, "ANDNOT") == 0)
            return O_ANDNOT;
    }

    return T_WORD;
}

/* 
 * Checks whether a query is syntactically valid. Returns a bool.
 */
static int is_valid_query(list_t *tokens, int n_tokens, char **errmsg) {
    list_iter_t *q_iter = list_createiter(tokens);
    if (q_iter == NULL) {
        return 0;
    }

    int n_par_open = 0, n_par_close = 0, last_operator = 0;
    int prev_type, curr_type;
    char *curr = list_next(q_iter);
    curr_type = prev_type = word_type(curr);

    if (n_tokens % 2 == 0) {
        /* Any given valid query will always have an odd number of tokens. Reasoning:
        * '<word> <word>' results in the indexer adding an "OR" inbetween.
        * As a consequence of this, all words must (or will, for "OR"), be connected by an operator.
        * Every operator must have adjacent terms, and parantheses must occur in multiples of 2.
        * => for every word, operator or parantheses added in addition to the 'root' word, n_tokens will 
        * be incremented by 2 if query is valid, and |tokens| will therefore never be even */
        *errmsg = "Undefined syntax error. Check your query, or see the documentation for usage.";
        goto error;
    }

    if (n_tokens == 1) {
        /* single term word query - boolean validity check */
        if (curr_type == T_WORD) {
            list_destroyiter(q_iter);
            return 1;
        }
        *errmsg = "Single term queries must be a word.";
        goto error;
    }

    if (curr_type == P_OPEN) {
        n_par_open++;
    } else if (curr_type != T_WORD) {
        *errmsg = "Syntax Error: Search must begin with an opening paranthesis or word.";
        goto error;
    }

    /* iterate over the rest of the query to look for errors */
    while ((curr = list_next(q_iter)) != NULL) {
        curr_type = word_type(curr);

        if (curr_type < 0) {
            /* current token is a paranthesis. forget the last operator. */
            last_operator = 0;

            if (curr_type == P_OPEN) {
                n_par_open++;
            } else if (curr_type == P_CLOSE) {
                if (prev_type == P_OPEN) {
                    *errmsg = "Syntax Error: Empty parantheses.";
                    goto error;
                }
                n_par_close++;
            }
        } else if (curr_type > 0) {
            /* current token is an operator */
            if (last_operator && (last_operator != curr_type)) {
                *errmsg = "Syntax Error: Different types of operators must be separated by parantheses.";
                goto error;
            }
            if (prev_type > 0) {
                *errmsg = "Syntax Error: Adjacent operators";
                goto error;
            }
            /* remember current operator for when the next operator comes */
            last_operator = curr_type;
        }
        prev_type = curr_type;
    }

    if ((curr_type != T_WORD) && (curr_type != P_CLOSE)) {
        if (curr_type == P_OPEN) 
            *errmsg = "Syntax Error: Query may not end with an opening paranthesis.";
        else
            *errmsg = "Syntax Error: Query may not end with an operator.";
        goto error;
    } 

    if (n_par_open != n_par_close) {
        *errmsg = "Syntax Error: Opening and closing paranthesis counts do not match.";
        goto error;
    } 

    list_destroyiter(q_iter);
    DEBUG_PRINT("Syntax Error: Query passed all tests and should be valid\n");
    return 1;

error:
    /* typically wont use goto, but i believe it improves readability here */
    list_destroyiter(q_iter);
    return 0;
}


list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    /* set a default error message so we can simply return in case of memory error */
    *errmsg = "index out of memory";
    const int total_n_tokens = list_size(query);

    /* check the entire query for syntax errors */
    if (!is_valid_query(query, total_n_tokens, errmsg)) {
        return NULL;
    }

    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);

    /* case for single term queries */
    if (total_n_tokens == 1) {
        char *term = list_popfirst(query);

        if (results == NULL) {
            return NULL;
        }

        i_word_t *search_result = index_search(index, term);
        if (search_result != NULL) {
            /* iterate over i_paths that contain indexed word */
            set_iter_t *file_iter = set_createiter(search_result->i_paths_with_word);

            double score = 0.0;
            i_path_t *i_path;
            while ((i_path = set_next(file_iter)) != NULL) {
                score += 0.1;
                list_addlast(results, create_query_result(i_path->path, score));
            }
            set_destroyiter(file_iter);

            if (list_size(results) != 1) {
                /* sort results by score */
                list_sort(results);
            }
        }
        free(term);
        return results;
    }

    *errmsg = "n_terms > 1";
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
