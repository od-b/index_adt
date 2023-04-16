#include "index.h"
#include "common.h"
#include "printing.h"
#include "map.h"
#include "set.h"
#include "list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* cmpfuncs:
    compare_strings
    compare_pointers
*/

// typedef struct query_result {
//     char *path;
//     double score;
// } query_result_t;


typedef struct index {
    set_t  *indexed_words;      /* tree containing all indexed_word_t within the index */
    set_t  *indexed_files;      /* list to store all indexed paths. mainly used for freeing memory. */
} index_t;

typedef struct indexed_file {
    void   *path;               
    set_t  *words;
} indexed_file_t;

typedef struct indexed_word {
    void   *word;
    set_t  *files_with_word;
} indexed_word_t;


/* specialized compare funcs */

int compared_indexed_files_by_path(indexed_file_t *a, indexed_file_t *b) {
    return strcmp(a->path, b->path);
}

int compared_indexed_words_by_string(indexed_word_t *a, indexed_word_t *b) {
    return strcmp(a->word, b->word);
}

int compared_indexed_words_by_n_occurances(indexed_word_t *a, indexed_word_t *b) {
    return (
        set_size((set_t*)(a)->files_with_word) - 
        set_size((set_t*)(b)->files_with_word)
    );
}


index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));

    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index set with a specialized compare func for the words */
    new_index->indexed_words = set_create((cmpfunc_t)compared_indexed_words_by_string);
    new_index->indexed_files = set_create((cmpfunc_t)compared_indexed_files_by_path);

    if (new_index->indexed_words == NULL || new_index->indexed_files == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    return new_index;
}

void index_destroy(index_t *index) {
    /* free the content of indexed_words, then the set itself */
    set_iter_t *word_iter = set_createiter(index->indexed_words);
    indexed_word_t *indexed_word;

    while ((indexed_word = (indexed_word_t*)set_next(word_iter)) != NULL) {
        free(indexed_word->word);
        set_destroy(indexed_word->files_with_word);
    }
    set_destroyiter(word_iter);
    set_destroy(index->indexed_words);


    /* free the content of indexed_files, then the set itself */
    set_iter_t *file_iter = set_createiter(index->indexed_files);
    indexed_file_t *indexed_file;

    while ((indexed_file = (indexed_file_t*)set_next(file_iter)) != NULL) {
        free(indexed_file->path);
        set_destroy(indexed_file->words);
    }
    set_destroyiter(file_iter);
    set_destroy(index->indexed_files);

}

/*
 * Adds the given path to the given index, and index the given
 * list of words under that path.
 * NOTE: It is the responsibility of index_addpath() to deallocate (free)
 *       'path' and the contents of the 'words' list.
 */
void index_addpath(index_t *index, char *path, list_t *words) {
    list_iter_t *words_iter = list_createiter(words);
    if (words_iter == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }
}

/*
 * Performs the given query on the given index.  If the query
 * succeeds, the return value will be a list of paths (query_result_t). 
 * If there is an error (e.g. a syntax error in the query), an error 
 * message is assigned to the given errmsg pointer and the return value
 * will be NULL.
 */
list_t *index_query(index_t *index, list_t *query, char **errmsg) {
    // list_t *path_list = list_create()
    /* TODO */
    // return path_list;
}

