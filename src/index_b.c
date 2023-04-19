#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"
#include "map.h"
// #include "list.h" -- included through index.h

#include <string.h>     /* strcmp, etc */
// #include <stdio.h> -- included through common.h
// #include <stdlib.h> -- included through printing.h


#define P_LEFT     1
#define P_RIGHT    2
#define T_OR       3
#define T_AND      4
#define T_ANDNOT   5


typedef struct index {
    map_t *indexed_words;       /* map containing all i_word_t within the index. Key = "<word>" */
} index_t;

typedef struct i_word {
    set_t *paths_with_word;     /* set of all i_path_t that contains this word */
    char  *word;
} i_word_t;

typedef struct i_path {
    set_t *words_in_file;       /* set of all i_word_t that is contained within the file at this path */
    char  *path;
} i_path_t;


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
int compare_i_paths_by_path(i_path_t *a, i_path_t *b) {
    return strcmp(a->path, b->path);
}

/* compares two i_word_t objects by their ->word */
int compare_i_words_by_word(i_word_t *a, i_word_t *b) {
    return strcmp(a->word, b->word);
}


/* compares two i_word_t objects by how many files they appear in */
int compare_i_words_by_n_occurances(i_word_t *a, i_word_t *b) {
    return (
        set_size((set_t*)(a)->paths_with_word)
        - set_size((set_t*)(b)->paths_with_word)
    );
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}

index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));

    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    new_index->indexed_words = map_create((cmpfunc_t)strcmp, hash_string);

    if (new_index->indexed_words == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    return new_index;
}

void index_destroy(index_t *index) {
    return;
}

void index_addpath(index_t *index, char *path, list_t *words) {
    /* create a set to contain string pointers */
    set_t *words_at_path = set_create((cmpfunc_t)strcmp);

    char *path_cpy = strdup(path);

    /* iterate over words from file @ path */
    list_iter_t *words_iter = list_createiter(words);

    while (list_hasnext(words_iter)) {
        char *word = list_next(words_iter);
        i_word_t *paths_with_word = map_get(index->indexed_words, word);

        /* if the word is not in the index already, add it */
        if (paths_with_word == NULL) {
            paths_with_word = set_create((cmpfunc_t)strcmp);
            map_put(index->indexed_words, word, paths_with_word);
        }

        /* add the path to the words set of paths 
         * this call assumes set will not add duplicates. otherwise set_contains() should be called first. */
        set_add(paths_with_word, path_cpy);
    }

    /* cleanup */
    list_destroyiter(words_iter);
    free(path);
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

static int is_special_term(char *term) {
    if (strcmp(term, "(") == 0) {
        return P_LEFT;
    } else if (strcmp(term, ")") == 0) {
        return P_RIGHT;
    } else if (strcmp(term, "OR") == 0) {
        return T_OR;
    } else if (strcmp(term, "AND") == 0) {
        return T_AND;
    } else if (strcmp(term, "ANDNOT") == 0) {
        return T_ANDNOT;
    }
    return 0;
}

/* split a query into parsable subqueries */
static list_t *parse_query(index_t *index, list_t *query, char **errmsg) {
    list_iter_t *query_iter = list_createiter(query);
    int n_terms = list_size(query);

    int special_term;
    char *term;

    while (list_hasnext(query_iter)) {
        special_term = is_special_term(term);
        if (!special_term && n_terms == 1) {
            return;
        }
    }
}


list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    unsigned long long t_start;
    if (PTIME) t_start = gettime();

    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);
    list_iter_t *query_iter = list_createiter(query);

    int n_terms = list_size(query);

    if (query_iter == NULL || results == NULL) {
        return respond_with_errmsg("out of memory", errmsg);
    }

    return respond_with_errmsg("silence warning", errmsg);
}


/* make clean && make assert_index && lldb assert_index */
/* make clean && make indexer && ./indexer data/cacm/ */
/* cd code/C/eksamen23/exam_precode/ && make clean && make indexer && ./indexer data/cacm/  */
