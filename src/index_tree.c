#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"
// #include "list.h" -- included through index.h

#include <string.h>     /* strcmp, etc */
// #include <stdio.h> -- included through common.h
// #include <stdlib.h> -- included through printing.h

/*
 * Implementation of an index ADT with a BST-based set at its core
*/

/* indexed path. Contains a char* path, and a set_t *i_words_at_path */
typedef struct i_path {
    char    *path;               
    set_t   *i_words_at_path;    /* set of *to all words (i_word_t) contained in the file at path location  */
} i_path_t;

/* indexed word. Contains a char* word, and a set_t *i_paths_with_word */
typedef struct i_word {
    char    *word;
    set_t   *i_paths_with_word;  /* set of *to all i_path_t which contain this i_word */
} i_word_t;

typedef struct index {
    set_t    *i_words;       /* set containing all i_word_t within the index */
    i_word_t *search_word;   /* variable i_word_t for internal searching purposes */
} index_t;

/* --- DEBUGGING TOOLS --- */

#define PINFO  0
#define PTIME  0


static void print_query_string(list_t *query) {
    list_iter_t *query_iter = list_createiter(query);

    char *query_term;
    while ((query_term = list_next(query_iter)) != NULL) {
        printf("%s", query_term);
    }
    printf("\n");
    list_destroyiter(query_iter);
}

/* debugging function to print result details */
static void print_results(list_t *results, list_t *query) {
    list_iter_t *iter = list_createiter(results);
    query_result_t *result;

    printf("\nFound %d results for query '", list_size(results));
    print_query_string(query);
    printf("' = {\n");

    int n = 0;
    while ((result = list_next(iter)) != NULL) {
        printf(" result #%d = {\n   score: %lf\n", n, result->score);
        printf("   path: %s\n }\n", result->path);
        n++;
    }
    printf("}\n");

    list_destroyiter(iter);
}


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

/* frees the indexed word, it's word string, and set. Does not affect set content. */
static void destroy_i_word(i_word_t *i_word) {
    free(i_word->word);
    set_destroy(i_word->i_paths_with_word);
    free(i_word);
}

/* frees the indexed file, it's path string, and set. Does not affect set content. */
static void destroy_i_path(i_path_t *i_path) {
    free(i_path->path);
    set_destroy(i_path->i_words_at_path);
    free(i_path);
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

    return new_index;
}

void index_destroy(index_t *index) {
    i_path_t *curr_i_path;
    i_word_t *curr_i_word;
    set_iter_t *i_path_iter;
    set_iter_t *i_word_iter;

    /* create a set of all paths to avoid attempting to free paths multiple times */
    set_t *all_i_paths = set_create((cmpfunc_t)compare_i_paths_by_string);

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

    if (i_word->i_paths_with_word == NULL) {
        /* word was not a duplicate. Initialize the i_word. */
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
    index->search_word->word = NULL;
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

    /* i_path_t to store the path + words currently being indexed */
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

void *respond_with_errmsg(char *msg, char **dest) {
    *dest = copy_string(msg);
    return NULL;
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

static void add_query_results(index_t *index, list_t *results, char *query_word) {
    i_word_t *search_result = index_search(index, query_word);

    if (search_result == NULL) {
        return;
    }

    /* iterate over i_paths that contain indexed word */
    set_iter_t *file_iter = set_createiter(search_result->i_paths_with_word);

    i_path_t *i_path;
    double score = 0.0;
    while ((i_path = set_next(file_iter))) {
        score += 0.1;
        list_addlast(results, create_query_result(i_path->path, score));
    }
    set_destroyiter(file_iter);
}

list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);

    list_iter_t *query_iter = list_createiter(query);
    int n_terms = list_size(query);

    if (query_iter == NULL || results == NULL) {
        return respond_with_errmsg("out of memory", errmsg);
    }

    if (n_terms == 1) {
        /* TODO: validate search word */
        add_query_results(index, results, list_next(query_iter));
        list_destroyiter(query_iter);
        return results;
    }
    return respond_with_errmsg("n_terms > 1", errmsg);
}


/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
/* cd code/C/eksamen23/exam_precode/ && make clean && make indexer && ./indexer data/cacm/  */
