#include "index.h"
#include "common.h"
#include "printing.h"
#include "map.h"
#include "tree.h"
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
    tree_t  *indexed_words;      /* tree containing all indexed_word_t within the index */
    tree_t  *indexed_files;      /* list to store all indexed paths. mainly used for freeing memory. */
} index_t;

typedef struct indexed_file {
    void    *path;               
    tree_t  *words;
} indexed_file_t;

typedef struct indexed_word {
    void    *word;
    tree_t  *files_with_word;
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
        tree_size((tree_t*)(a)->files_with_word) - 
        tree_size((tree_t*)(b)->files_with_word)
    );
}

index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));

    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* create the index tree with a specialized compare func for the words */
    new_index->indexed_words = tree_create((cmpfunc_t)compared_indexed_words_by_string);
    new_index->indexed_files = tree_create((cmpfunc_t)compared_indexed_files_by_path);

    if (new_index->indexed_words == NULL || new_index->indexed_files == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    return new_index;
}

/*
 * initialize a indexed_word_t using the given word
 * set indexed_word->path to a copy of the given path.
 * creates and empty tree_t @ indexed_word->files_with_word, with cmpfunc_t := compared_indexed_files_by_path
 */
static indexed_word_t *create_indexed_word(char *word) {
    indexed_word_t *new_indexed_word = malloc(sizeof(indexed_word_t));
    if (new_indexed_word == NULL) return NULL;

    size_t word_len = strlen(word) + 1;     /* needed malloc size */
    new_indexed_word->word = malloc(word_len);
    if (new_indexed_word->word == NULL) return NULL;

    /* copy the string to the allocated memory */
    strncpy(new_indexed_word->word, word, word_len);

    return new_indexed_word;
}

/*
 * initialize a indexed_file_t using the given path.
 * set indexed_file->path to a copy of the given path.
 * creates an empty tree @ indexed_file->words, with cmpfunc_t := compared_indexed_words_by_string
 */
static indexed_file_t *create_indexed_file(char *path) {
    indexed_file_t *new_indexed_path = malloc(sizeof(indexed_file_t));
    if (new_indexed_path == NULL) return NULL;

    size_t path_len = strlen(path) + 1;     /* needed malloc size */
    new_indexed_path->path = malloc(path_len);
    if (new_indexed_path->path == NULL) return NULL;

    /* copy the string to the allocated memory */
    strncpy(new_indexed_path->path, path, path_len);

    /* create an empty tree to store indexed words */
    new_indexed_path->words = tree_create((cmpfunc_t)compared_indexed_words_by_string);
    if (new_indexed_path->words == NULL) return NULL;

    return new_indexed_path;
}


void index_destroy(index_t *index) {
    /* free the content of indexed_words, then the tree itself */
    tree_iter_t *word_iter = tree_createiter(index->indexed_words, 1);
    indexed_word_t *indexed_word;

    while ((indexed_word = (indexed_word_t*)tree_next(word_iter)) != NULL) {
        /* free word, then destroy the tree of files. the file path strings are freed by the next loop. */
        free(indexed_word->word);
        tree_destroy(indexed_word->files_with_word);
    }
    tree_destroyiter(word_iter);


    /* free the content of indexed_files, then the tree itself */
    tree_iter_t *file_iter = tree_createiter(index->indexed_files, 1);
    indexed_file_t *indexed_file;

    while ((indexed_file = (indexed_file_t*)tree_next(file_iter)) != NULL) {
        /* free path, then destroy the tree of words. the words strings are freed by the previous loop. */
        free(indexed_file->path);
        tree_destroy(indexed_file->words);
    }
    tree_destroyiter(file_iter);

    /* destroy the index trees, then free the index */
    tree_destroy(index->indexed_words);
    tree_destroy(index->indexed_files);
    free(index);
}

void index_addpath(index_t *index, char *path, list_t *words) {
    /* create iter for list of words */
    list_iter_t *words_iter = list_createiter(words);

    /* create indexed_file_t from path. free path. */
    indexed_file_t *new_indexed_file = create_indexed_file(path);
    free(path);

    if (words_iter == NULL || new_indexed_file == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    /* add index file to index */
    int8_t file_index_added = tree_add(index->indexed_files, (void*)new_indexed_file);
    if (file_index_added == -1) {
        ERROR_PRINT("out of memory");
        return NULL;
    } else if (file_index_added == 0) {
        ERROR_PRINT("duplicate file '%s' this should never happen.", path);
        return NULL;
    }
    /* ... else, index file was succesfully added. */

    /* iterate over all words in the provided list */
    void *word;
    while ((word = list_next(words_iter)) != NULL) {
        indexed_word_t *current_index_word;

        /* check in tree if duplicate or not */
        void *duplicate = tree_contains(index->indexed_words, word);
        if (duplicate == NULL) {
            /* first file containing the word. add to index of words.*/
            indexed_word_t *current_index_word = create_indexed_word((char*)word);
        } else {
            /* word is already indexed. */
            current_index_word = (indexed_word_t*)duplicate;
        }

        /* free word from list */
        free(word);

        /* add the file index to the current index word */
        if (tree_add(current_index_word->files_with_word, (void*)new_indexed_file) == -1) {
            ERROR_PRINT("out of memory");
            return NULL;
        }

        /* add word to the file index */
        if (tree_add(new_indexed_file->words, (void*)current_index_word) == -1){
            ERROR_PRINT("out of memory");
            return NULL;
        }
    }

    /* cleanup */
    list_destroyiter(words_iter);
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

