#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"
#include "map.h"
// #include "list.h" -- included through index.h

#include <string.h>     /* strcmp, etc */
// #include <stdio.h> -- included through common.h
// #include <stdlib.h> -- included through printing.h

typedef struct index {
    map_t   *i_words;      /* set containing all i_word_t within the index */
} index_t;

typedef struct i_path {
    char    *path;               
    set_t   *i_words_at_path;
} i_path_t;

typedef struct i_word {
    char    *word;
    set_t   *i_paths_with_word;
} i_word_t;


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


/* --- COMPARE | HASH FUNCTIONS --- */

unsigned long hash_i_word_string(i_word_t *i_word) {
    return hash_string(i_word->word);
}

/* compares two i_path_t objects by their ->path */
int compare_i_paths_by_path(i_path_t *a, i_path_t *b) {
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
static void destroy_indexed_word(i_word_t *indexed_word) {
    free(indexed_word->word);
    set_destroy(indexed_word->i_paths_with_word);
    free(indexed_word);
}

/* frees the indexed file, it's path string, and set. Does not affect set content. */
static void destroy_indexed_file(i_path_t *indexed_file) {
    free(indexed_file->path);
    set_destroy(indexed_file->i_words_at_path);
    free(indexed_file);
}

index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));

    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    new_index->i_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (new_index->i_words == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    return new_index;
}

void index_destroy(index_t *index) {
    // set_iter_t *word_iter = set_createiter(index->i_words);

    // if (word_iter == NULL) {
    //     ERROR_PRINT("out of memory");
    // }

    // /* free the content of all i_words */
    // i_word_t *indexed_word;
    // while ((indexed_word = (i_word_t*)set_next(word_iter)) != NULL) {
    //     destroy_indexed_word(indexed_word);
    // }

    // /* destroy iterators and the data structure shells themselves */
    // set_destroyiter(word_iter);
    // map_destroy(index->i_words);


    /* lastly, free the index. All memory should now be reclaimed by the system. */
    free(index);

    if (PINFO) printf("cleanup done\n");
}

/*
 * initialize a struct i_path_t using the given path.
 * set indexed_file->path to a copy of the given path.
 * creates an empty set @ indexed_file->words, with cmpfunc_t := compare_i_words_by_string
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

    /* create an empty set to store indexed words */
    new_i_path->i_words_at_path = set_create((cmpfunc_t)compare_i_words_by_string);
    if (new_i_path->i_words_at_path == NULL) {
        return NULL;
    }

    return new_i_path;
}

/*
 * creates a new i_word_t. Copies the given word, sets a pointer to the copy @ ->word,
 * creates an empty set @ ->i_paths_with_word, with cmpfunc_t := compare_i_paths_by_path
 */
static i_word_t *create_i_word(char *word) {
    i_word_t *new_i_word = malloc(sizeof(i_word_t));
    char *word_cpy = copy_string(word);

    if (new_i_word == NULL || word_cpy == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    new_i_word->word = word_cpy;
    new_i_word->i_paths_with_word = set_create((cmpfunc_t)compare_i_paths_by_path);

    if (new_i_word->word == NULL || new_i_word->i_paths_with_word == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    return new_i_word;
}


void index_addpath(index_t *index, char *path, list_t *words) {
    /* i_path_t to store the path + words currently being indexed */
    i_path_t *new_i_path = create_i_path(path);
    list_iter_t *words_iter = list_createiter(words);
    char *curr_word;

    /* iterate over the given list of words */
    while ((curr_word = list_next(words_iter)) != NULL) {
        /* check if index map contains the word already */
        i_word_t *curr_i_word = map_get(index->i_words, (void *)curr_word);

        if (curr_i_word == NULL) {
            /* word is not in index, create it */
            curr_i_word = create_i_word(curr_word);
            map_put(index->i_words, curr_i_word->word, curr_i_word);
        }

        /* add a pointer to curr_i_path to the words set of i_paths. */
        set_add(curr_i_word->i_paths_with_word, new_i_path);

        /* add a pointer to curr_i_word to the paths set of i_words. */
        set_add(new_i_path->i_words_at_path, curr_i_word);

        /* this strategy should ensure each string, be it for file or path, 
         * is only ever stored once in memory. 
         * All files containing a word, will share the same i_word pointer, 
         * and vice versa for i_words. */
        free(curr_word);
    }
    free(path);

    /* cleanup */
    list_destroyiter(words_iter);
}


/* --- query interaction --- */

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
    result->path = copy_string(path);
    result->score = score;

    return result;
}

list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);

    list_iter_t *query_iter = list_createiter(query);
    int n_terms = list_size(query);

    if (query_iter == NULL || results == NULL) {
        return respond_with_errmsg("out of memory", errmsg);
    }

    if (n_terms == 1) {
        /* get word from map. if null, return empty list */

        i_word_t *found_i_word = map_get(index->i_words, list_next(query_iter));
        list_destroyiter(query_iter);

        if (found_i_word != NULL) {
            set_iter_t *path_iter = set_createiter(found_i_word->i_paths_with_word);
            i_path_t *curr_i_path;
            double score = 0.0;

            /* add all results to the list */
            while ((curr_i_path = set_next(path_iter)) != NULL) {
                score += 0.15;
                list_addlast(results, create_query_result(curr_i_path->path, score));
            }
            set_destroyiter(path_iter);

            /* sort results by query score */
            list_sort(results);
        }
        return results;
    }
    return respond_with_errmsg("silence warning", errmsg);
}

// if (PINFO) {
//     print_results(results, query);
// }
// if (PTIME) {
//     unsigned long long t_end = gettime();
//     printf("query took %llu μs\n", t_end-t_start);
// }

/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
/* cd code/C/eksamen23/exam_precode/ && make clean && make indexer && ./indexer data/cacm/  */
