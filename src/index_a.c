#include "index.h"
#include "common.h"
#include "printing.h"
#include "tree.h"
// #include "list.h" -- included through index.h

#include <string.h>     /* strcmp, etc */
// #include <stdio.h> -- included through common.h
// #include <stdlib.h> -- included through printing.h



#define PINFO  0


typedef struct index {
    tree_t  *indexed_words;      /* tree containing all i_word_t within the index */
    list_t  *indexed_files;      /* list to store all indexed paths. mainly used for freeing memory. */
} index_t;

typedef struct indexed_file {
    char    *path;               
    tree_t  *words;
} i_file_t;

typedef struct indexed_word {
    char    *word;
    tree_t  *files_with_word;
} i_word_t;


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
        tree_size((tree_t*)(a)->files_with_word)
        - tree_size((tree_t*)(b)->files_with_word)
    );
}

// typedef struct query_result {
//     char *path;
//     double score;
// } query_result_t;

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}

/* frees the indexed word, it's word string, and tree. Does not affect tree content. */
static void destroy_indexed_word(i_word_t *indexed_word) {
    free(indexed_word->word);
    tree_destroy(indexed_word->files_with_word);
    free(indexed_word);
}

/* frees the indexed file, it's path string, and tree. Does not affect tree content. */
static void destroy_indexed_file(i_file_t *indexed_file) {
    free(indexed_file->path);
    tree_destroy(indexed_file->words);
    free(indexed_file);
}

index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));

    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index tree with a specialized compare func for the words */
    new_index->indexed_words = tree_create((cmpfunc_t)compare_i_words_by_string);
    new_index->indexed_files = list_create((cmpfunc_t)compare_i_files_by_path);

    if (new_index->indexed_words == NULL || new_index->indexed_files == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    return new_index;
}

void index_destroy(index_t *index) {
    tree_iter_t *word_iter = tree_createiter(index->indexed_words, 1);
    list_iter_t *file_iter = list_createiter(index->indexed_files);

    if (word_iter == NULL || file_iter == NULL) {
        ERROR_PRINT("out of memory");
    }

    /* free the content of all indexed_words */
    i_word_t *indexed_word;
    while ((indexed_word = (i_word_t*)tree_next(word_iter)) != NULL) {
        destroy_indexed_word(indexed_word);
    }

    /* free the content of all indexed_files  */
    i_file_t *indexed_file;
    while ((indexed_file = (i_file_t*)list_next(file_iter)) != NULL) {
        destroy_indexed_file(indexed_file);
    }

    /* destroy iterators and the data structure shells themselves */
    tree_destroyiter(word_iter);
    tree_destroy(index->indexed_words);

    list_destroyiter(file_iter);
    list_destroy(index->indexed_files);

    /* lastly, free the index. All memory should now be reclaimed by the system. */
    free(index);
}

/*
 * initialize a struct i_file_t using the given path.
 * set indexed_file->path to a copy of the given path.
 * creates an empty tree @ indexed_file->words, with cmpfunc_t := compare_i_words_by_string
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

    /* create an empty tree to store indexed words */
    new_indexed_path->words = tree_create((cmpfunc_t)compare_i_words_by_string);
    if (new_indexed_path->words == NULL) {
        return NULL;
    }

    return new_indexed_path;
}

/* 
 * creates and returns an indexed word with a borrowed string pointer. 
 * The i_word_t has files_with_word == NULL.
 */
static i_word_t *create_tmp_indexed_word(char* word) {
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
 * initialize the attributes of an i_word_t.
 * replaces its word with a permanent copy.
 * creates an empty tree @ indexed_word->files_with_word, with cmpfunc_t := compare_i_files_by_path
 */
static void initialize_indexed_word(i_word_t *indexed_word) {
    char *word_cpy = copy_string(indexed_word->word);

    indexed_word->word = word_cpy;
    indexed_word->files_with_word = tree_create((cmpfunc_t)compare_i_files_by_path);

    if (indexed_word->word == NULL || indexed_word->files_with_word == NULL) {
        ERROR_PRINT("out of memory");
    }

}

void index_addpath(index_t *index, char *path, list_t *words) {
    /* create iter for list of words */
    list_iter_t *words_iter = list_createiter(words);

    /* create i_file_t from path. free path. */
    i_file_t *new_i_file = create_indexed_file(path);

    /* add newly indexed file to the list of indexed files */
    int8_t list_add_ok = list_addlast(index->indexed_files, new_i_file);

    if (words_iter == NULL || new_i_file == NULL || !list_add_ok) {
        ERROR_PRINT("out of memory");
    }

    /* as a copy of the path is in the indexed file, free the given path */
    free(path);

    /* iterate over all words in the provided list */
    void *elem;
    while ((elem = list_next(words_iter)) != NULL) {
        /* create a new indexed word */
        i_word_t *tmp_i_word = create_tmp_indexed_word((char*)elem);

        /* add to the main tree of indexed words. Store return to determine whether word is duplicate. */
        i_word_t *curr_i_word = (i_word_t*)tree_add(index->indexed_words, tmp_i_word);

        /* check whether word is a duplicate by comparing adress to the temp word */
        if (&curr_i_word->word != &tmp_i_word->word) {
            if (PINFO)
                printf("DUP: %s\n", curr_i_word->word);
            /* word is duplicate. free the struct. */
            free(tmp_i_word);
        } else {
            if (PINFO)
                printf("new: %s\n", curr_i_word->word);
            /* not a duplicate, finalize initialization of the tmp word. (no longer temporary) */
            initialize_indexed_word(tmp_i_word);
        }

        /* free word from list */
        free(elem);

        /* 
         * curr_i_word is now either the existing indexed word, or the newly created one.
        */

        /* add the file index to the current index word */
        tree_add(curr_i_word->files_with_word, new_i_file);

        /* add word to the file index */
        tree_add(new_i_file->words, curr_i_word);
    }

    /* cleanup */
    list_destroyiter(words_iter);
}

void *respond_with_errmsg(char *msg, char **dest) {
    *dest = copy_string(msg);
    return NULL;
}

// typedef struct query_result {
//     char *path;
//     double score;
// } query_result_t;

query_result_t *create_query_result(char *path, double score) {
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
    i_word_t *indexed_word = tree_contains(index->indexed_words, search_word);
    free(search_word);

    if (indexed_word == NULL) {
        /* no files with the word */
        return;
    }

    /* iterate over files that contain indexed word */
    tree_iter_t *file_iter = tree_createiter(indexed_word->files_with_word, 1);

    i_file_t *indexed_file;
    double score = 0.0;
    while ((indexed_file = tree_next(file_iter))) {
        score += 0.1;
        list_addlast(results, create_query_result(indexed_file->path, score));
    }

    tree_destroyiter(file_iter);
}

/*
 * debugging function to print result details 
 */
static void print_results(list_t *results, list_t *query) {
    if (!PINFO) return;

    list_iter_t *query_iter = list_createiter(query);
    char *buf[255];
    int n = 0;

    char *query_term;
    while ((query_term = list_next(query_iter)) != NULL) {
        size_t len = strlen(query_term);
        for (size_t i = 0; i < len; i++, n++) {
            buf[n] = &query_term[i];
        }
    }
    buf[n] = "\0";
    list_destroyiter(query_iter);

    list_iter_t *iter = list_createiter(results);
    query_result_t *result;

    printf("Found %d results for query '%s' = {", list_size(results), *buf);

    n = 0;
    while ((result = list_next(iter)) != NULL) {
        printf(" result #%d = {\n   score: %lf\n", n, result->score);
        printf("   path: %s\n }\n", result->path);
        n++;
    }
    printf("}\n");

    list_destroyiter(iter);
}


/*
 * Performs the given query on the given index.  If the query
 * succeeds, the return value will be a list of paths (query_result_t). 
 * If there is an error (e.g. a syntax error in the query), an error 
 * message is assigned to the given errmsg pointer and the return value
 * will be NULL.
 */
list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    list_t *results = list_create((cmpfunc_t)compare_query_results_by_score);
    list_iter_t *query_iter = list_createiter(query);

    if (query_iter == NULL || results == NULL) {
        return respond_with_errmsg("out of memory", errmsg);
    }

    char *query_word;
    if (list_size(query) == 1) {
        query_word = list_next(query_iter);
        /* TODO: validate search word */
        add_query_results(index, results, query_word);
        list_destroyiter(query_iter);

        print_results(results, query);
        return results;
    }

    // while ((query_word = list_next(query_iter)) != NULL) {
    //     printf("\nquery_word: '%s'\n", query_word);
    // }

    /* 
        query   ::= andterm
                | andterm "ANDNOT" query
        andterm ::= orterm
                | orterm "AND" andterm
        orterm  ::= term
                | term "OR" orterm
        term    ::= "(" query ")"
                | <word> 
    */
    return respond_with_errmsg("silence warning", errmsg);
}


/* make clean && make assert_index && lldb assert_index */
