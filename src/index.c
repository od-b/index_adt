#include "index.h"
#include "common.h"
#include "printing.h"
#include "map.h"

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
    tree_t *indexed_words;      /* tree containing all words within the index */
    int **file_paths;       /* array of pointers to all file paths */
    map_t *entry_map;       /* map of all words. each maps value is a tree of files its contained in */
} index_t;

/*
 * entry for the index
*/
typedef struct index_entry {
    void *map_key;      /* file name */
    tree_t *words;      /* tree of words in the file */
} index_entry_t;



/*
 * Creates a new, empty index.
 */
index_t *index_create() {
    index_t *new_index = malloc(sizeof(index_t));

    if (new_index == NULL) {
        ERROR_PRINT("out of memory");
        return NULL;
    }

    // new_index->x = NULL;
    // new_index->y = NULL;
    // new_index->z = NULL;

    return new_index;
}

/*
 * Destroys the given index.  Subsequently accessing the index will
 * lead to undefined behavior.
 */
void index_destroy(index_t *index);

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

    /* create a tree containing all words within the file */
    tree_t *file_words = tree_create(compare_strings);


    /* add all words to the index tree. Free the elems. */
    void *elem;
    while ((elem = list_next(words_iter)) != NULL) {
        char *word = (* char)elem;
        tree_add(file_words, elem);
        tree_add(index->all_words, elem);
        free(elem);
    }

    list_destroyiter(words_iter);

    /* hash the filepath */


    free(path);
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

