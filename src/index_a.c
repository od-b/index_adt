#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"
// #include "list.h" -- included through index.h

#include <string.h>     /* strcmp, etc */
// #include <stdio.h> -- included through common.h
// #include <stdlib.h> -- included through printing.h

typedef struct index {
    set_t  *indexed_words;      /* set containing all i_word_t within the index */
    list_t  *indexed_files;      /* list to store all indexed paths. mainly used for freeing memory. */
} index_t;

typedef struct indexed_file {
    char    *path;               
    set_t  *words;
} i_file_t;

typedef struct indexed_word {
    char    *word;
    set_t  *files_with_word;
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


/* --- COMPARE FUNCTIONS --- */

/* compares two i_file_t objects by their ->path */
int compare_i_files_by_path(i_file_t *a, i_file_t *b) {
    return strcmp(a->path, b->path);
}

/* compares two i_word_t objects by their ->word */
int compare_i_words_by_string(i_word_t *a, i_word_t *b) {
    return strcmp(a->word, b->word);
}

/* compares two i_word_t objects by how many files they appear in */
int compare_i_words_by_n_occurances(i_word_t *a, i_word_t *b) {
    return (
        set_size((set_t*)(a)->files_with_word)
        - set_size((set_t*)(b)->files_with_word)
    );
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}


/* --- CREATION, DESTRUCTION --- */

/* frees the indexed word, it's word string, and set. Does not affect set content. */
static void destroy_indexed_word(i_word_t *indexed_word) {
    free(indexed_word->word);
    set_destroy(indexed_word->files_with_word);
    free(indexed_word);
}

/* frees the indexed file, it's path string, and set. Does not affect set content. */
static void destroy_indexed_file(i_file_t *indexed_file) {
    free(indexed_file->path);
    set_destroy(indexed_file->words);
    free(indexed_file);
}

index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));

    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    new_index->indexed_words = set_create((cmpfunc_t)compare_i_words_by_string);
    new_index->indexed_files = list_create((cmpfunc_t)compare_i_files_by_path);

    if (new_index->indexed_words == NULL || new_index->indexed_files == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    return new_index;
}

void index_destroy(index_t *index) {
    set_iter_t *word_iter = set_createiter(index->indexed_words);
    list_iter_t *file_iter = list_createiter(index->indexed_files);

    if (word_iter == NULL || file_iter == NULL) {
        ERROR_PRINT("out of memory");
    }

    /* free the content of all indexed_words */
    i_word_t *indexed_word;
    while ((indexed_word = (i_word_t*)set_next(word_iter)) != NULL) {
        destroy_indexed_word(indexed_word);
    }

    /* free the content of all indexed_files  */
    i_file_t *indexed_file;
    while ((indexed_file = (i_file_t*)list_next(file_iter)) != NULL) {
        destroy_indexed_file(indexed_file);
    }

    /* destroy iterators and the data structure shells themselves */
    set_destroyiter(word_iter);
    set_destroy(index->indexed_words);

    list_destroyiter(file_iter);
    list_destroy(index->indexed_files);


    /* lastly, free the index. All memory should now be reclaimed by the system. */
    free(index);

    if (PINFO) printf("cleanup done\n");
}

/*
 * initialize a struct i_file_t using the given path.
 * set indexed_file->path to a copy of the given path.
 * creates an empty set @ indexed_file->words, with cmpfunc_t := compare_i_words_by_string
 */
static i_file_t *create_indexed_file(char *path) {
    i_file_t *new_indexed_path = malloc(sizeof(i_file_t));
    if (new_indexed_path == NULL) {
        return NULL;
    }

    new_indexed_path->path = copy_string(path);
    if (new_indexed_path->path == NULL) {
        return NULL;
    }

    /* create an empty set to store indexed words */
    new_indexed_path->words = set_create((cmpfunc_t)compare_i_words_by_string);
    if (new_indexed_path->words == NULL) {
        return NULL;
    }

    return new_indexed_path;
}

/* 
 * initializes a new indexed word.
 * Does not copy the word, nor create the files_with_word attribute.
 */
static i_word_t *create_tmp_indexed_word(char *word) {
    i_word_t *tmp_word = malloc(sizeof(i_word_t));
    if (tmp_word == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    tmp_word->word = word;
    tmp_word->files_with_word = NULL;
    return tmp_word;
}

/*
 * initialize the attributes of a "temp" i_word_t.
 * replaces its ->word with a permanent copy of that word. the original word is unchanged.
 * creates an empty ->files_with_word, with cmpfunc_t := compare_i_files_by_path
 */
static void initialize_indexed_word(i_word_t *indexed_word) {
    char *word_cpy = copy_string(indexed_word->word);

    indexed_word->word = word_cpy;
    indexed_word->files_with_word = set_create((cmpfunc_t)compare_i_files_by_path);

    if (indexed_word->word == NULL || indexed_word->files_with_word == NULL) {
        ERROR_PRINT("out of memory");
    }

}

void index_addpath(index_t *index, char *path, list_t *words) {
    list_iter_t *words_iter = list_createiter(words);

    /* i_file_t to store the path + words currently being indexed */
    i_file_t *new_i_file = create_indexed_file(path);
    free(path);

    /* add newly indexed file to the centralized list of indexed files */
    int8_t list_add_ok = list_addlast(index->indexed_files, new_i_file);

    if (words_iter == NULL || new_i_file == NULL || !list_add_ok) {
        ERROR_PRINT("out of memory");
    }

    int n_dup_words = 0;
    int n_new_words = 0;

    /* iterate over all words in the provided list */
    void *elem;
    while ((elem = list_next(words_iter)) != NULL) {
        /* create a new indexed word */
        i_word_t *tmp_i_word = create_tmp_indexed_word((char*)elem);

        /* add to the main set of indexed words. Store return to determine whether word is duplicate. */
        i_word_t *curr_i_word = (i_word_t*)set_put(index->indexed_words, tmp_i_word);

        /* check whether word is a duplicate by comparing adress to the temp word */
        if (&curr_i_word->word != &tmp_i_word->word) {
            if (PINFO) {
                n_dup_words += 1;
                // printf("DUP: %s\n", curr_i_word->word);
            }
            /* word is duplicate. free the struct. */
            free(tmp_i_word);
        } else {
            if (PINFO) {
                n_new_words += 1;
                // printf("new: %s\n", curr_i_word->word);
            }
            /* not a duplicate, finalize initialization of the tmp word, making it permanent. */
            initialize_indexed_word(tmp_i_word);
        }

        /* free word from list */
        free(elem);

        /* 
         * curr_i_word is now either the existing indexed word, or the newly created one.
        */

        /* add the file index to the current index word */
        set_add(curr_i_word->files_with_word, new_i_file);

        /* add word to the file index */
        set_add(new_i_file->words, curr_i_word);
    }

    if (PINFO) {
        printf("added file with path: '%s'\n", new_i_file->path);
        printf("  new words in file: %d\n", n_new_words);
        printf("  duplicate/common words in file: %d\n", n_dup_words);
    }

    /* cleanup */
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
    i_word_t *search_word = create_tmp_indexed_word(query_word);
    i_word_t *indexed_word = set_get(index->indexed_words, search_word);
    free(search_word);

    if (indexed_word == NULL) {
        /* no files with the word */
        return;
    }

    /* iterate over files that contain indexed word */
    set_iter_t *file_iter = set_createiter(indexed_word->files_with_word);

    i_file_t *indexed_file;
    double score = 0.0;
    while ((indexed_file = set_next(file_iter))) {
        score += 0.1;
        list_addlast(results, create_query_result(indexed_file->path, score));
    }

    set_destroyiter(file_iter);
}

list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    unsigned long long t_start;
    if (PTIME) t_start = gettime();

    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);

    // if (PINFO) {
    //     printf("query string = ");
    //     print_query_string(query);
    //     // return results;
    // }

    list_iter_t *query_iter = list_createiter(query);
    int n_terms = list_size(query);

    if (query_iter == NULL || results == NULL) {
        return respond_with_errmsg("out of memory", errmsg);
    }

    if (n_terms == 1) {
        /* TODO: validate search word */
        add_query_results(index, results, list_next(query_iter));
        list_destroyiter(query_iter);

        if (PINFO) {
            print_results(results, query);
        }
        if (PTIME) {
            unsigned long long t_end = gettime();
            printf("query took %llu μs\n", t_end-t_start);
        }
        return results;
    }
    return respond_with_errmsg("silence warning", errmsg);
}


/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
/* cd code/C/eksamen23/exam_precode/ && make clean && make indexer && ./indexer data/cacm/  */