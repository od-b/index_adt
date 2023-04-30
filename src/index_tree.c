/*
 * Index ADT implementation relying mainly on the use of tree-based set(s).
 * The program is
    // Apple clang version 14.0.3 (clang-1403.0.22.14.1)
    // Target: arm64-apple-darwin22.3.0
    // Thread model: posix
 *
 * In an attempt to improve readability, the code within this file is split into sections.
 * Sections 1 & 2 consist solely of functions.
 *
 * 0. Definitions, struct initialization and functions passed by reference
 * 1. Creating and building the index
 * 2. Query handling, parsing
 *
 */

#include "index.h"
#include "common.h"
#include "printing.h"
#include "parser.h"
#include "set.h"
#include "assert.h"

#include <ctype.h>
#include <string.h>



/*
 *  -------- Section 0: Definitions & Declarations --------
*/

#define DUMMY_CMPFUNC  &compare_pointers


typedef struct iword iword_t;


/* Type of index */
struct index {
    set_t    *iwords;       // set of all indexed words
    iword_t  *iword_buf;    // buffer of one iword for searching and adding words
    parser_t *parser;
};

/* Type of indexed word */
struct iword {
    char   *word;
    set_t  *paths;  // set of paths where ->word can be found
};

/* strcmp wrapper */
int strcmp_iwords(iword_t *a, iword_t *b) {
    return strcmp(a->word, b->word);
}

/* Compares two iword_t based on the size of their path sets */
int compare_iwords_by_occurance(iword_t *a, iword_t *b) {
    return (set_size(a->paths) - set_size(b->paths));
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    return (a->score - b->score);
}

set_t *get_iword_paths(index_t *index, char *word) {
    index->iword_buf->word = word;
    iword_t *result = set_get(index->iwords, index->iword_buf);

    if (result)
        return result->paths;
    return NULL;
}


/*
 * -------- Section 1: Index Creation, Destruction and Building --------
*/

index_t *index_create() {
    index_t *index = malloc(sizeof(index_t));
    if (!index) {
        return NULL;
    }

    /* create the 'core' index set to contain iwords */
    index->iwords = set_create((cmpfunc_t)strcmp_iwords);
    if (!index->iwords) {
        free(index);
        return NULL;
    }

    index->iword_buf = malloc(sizeof(iword_t));
    if (!index->iwords) {
        free(index);
        free(index->iwords);
        return NULL;
    }

    index->parser = parser_create((void *)index, (search_func_t)get_iword_paths);
    if (!index->parser) {
        free(index);
        free(index->iwords);
        free(index->iword_buf);
        return NULL;
    }

    index->iword_buf->word = NULL;
    index->iword_buf->paths = NULL;

    return index;
}

void index_destroy(index_t *index) {
    int n_freed_words = 0;
    int n_freed_paths = 0;

    /* Set to group all paths before freeing them. Avoids any hocus pocus
     * such as double free or potentially dismembering trees */
    set_t *all_paths = set_create((cmpfunc_t)strcmp);
    set_iter_t *iword_iter = set_createiter(index->iwords);

    while (set_hasnext(iword_iter)) {
        iword_t *curr = set_next(iword_iter);
        set_iter_t *path_iter = set_createiter(curr->paths);

        /* add to set of all paths */
        while (set_hasnext(path_iter)) {
            set_add(all_paths, set_next(path_iter));
        }
        set_destroyiter(path_iter);

        /* free the iword & its members */
        set_destroy(curr->paths);
        free(curr->word);
        free(curr);
        n_freed_words++;
    }
    set_destroyiter(iword_iter);
    set_destroy(index->iwords);

    /* free all paths now that they're grouped in one set */
    set_iter_t *all_paths_iter = set_createiter(all_paths);
    while (set_hasnext(all_paths_iter)) {
        free(set_next(all_paths_iter));
        n_freed_paths++;
    }
    set_destroyiter(all_paths_iter);
    set_destroy(all_paths);

    /* free index & co */
    parser_destroy(index->parser);
    free(index->iword_buf);
    free(index);

    DEBUG_PRINT("index_destroy: Freed %d paths, %d words\n", n_freed_paths, n_freed_words);
}

void index_addpath(index_t *index, char *path, list_t *words) {
    /*
     * TODO: test index build with and without list_sort, as well as with different sorts
     * will no doubt be correlated to n_words, but so will sorting the list.
     * Compared scaling with varied, large sets needed.
     * If the list sort is slower for small sets, but faster for varied/large sets,
     * using the sort method will be the best option, seeing as small sets are rather quick to add regardless.
    */

    list_iter_t *words_iter = list_createiter(words);
    if (words_iter == NULL) {
        ERROR_PRINT("malloc failed\n");
    }
    // list_sort(words);

    while (list_hasnext(words_iter)) {
        char *w = list_next(words_iter);

        /* try to add the word to the index, using word_buf to allow comparison */
        index->iword_buf->word = w;
        iword_t *iword = set_tryadd(index->iwords, index->iword_buf);

        if (iword == index->iword_buf) {
            /* first index entry for this word. initialize it as an indexed word. */
            iword->word = w;
            iword->paths = set_create((cmpfunc_t)strcmp);
            if (!iword->paths) {
                ERROR_PRINT("malloc failed\n");
            }

            /* Since the search word was added, create a new iword for searching. */
            index->iword_buf = malloc(sizeof(iword_t));
            if (!index->iword_buf) {
                ERROR_PRINT("malloc failed\n");
            }
            index->iword_buf->paths = NULL;
        } else {
            /* index is already storing this word. free the duplicate string. */
            free(w);
        }

        /* add path to the indexed words set. */
        set_add(iword->paths, path);
    }

    list_destroyiter(words_iter);
}


/*
 *  -------- Section 2: Query Parsing & Response --------
*/

/*
 * Returns a list of query results, created from each path in the given set
 */
static list_t *get_query_results(set_t *paths) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);
    if (!query_results) {
        return NULL;
    }

    set_iter_t *path_iter = set_createiter(paths);
    if (!path_iter) {
        return NULL;
    }
    double score = 0.0;

    while (set_hasnext(path_iter)) {
        query_result_t *q_result = malloc(sizeof(query_result_t));
        if (!q_result) {
            return NULL;
        }
        q_result->path = set_next(path_iter);
        q_result->score = score;
        score += 0.1;
        if (!list_addlast(query_results, q_result)) {
            return NULL;
        }
    }
    set_destroyiter(path_iter);

    return query_results;
}


list_t *index_query(index_t *index, list_t *tokens, char **errmsg) {
    if (!list_size(tokens)) {
        *errmsg = "empty query";
        return NULL;
    }

    switch (parser_scan_tokens(index->parser, tokens)) {
        case (ALLOC_FAILED):
            *errmsg = "index failed to allocate more memeory";
            return NULL;
        case (SYNTAX_ERROR):
            *errmsg = parser_get_errmsg(index->parser);
            return NULL;
        case (SKIP_PARSE):
            return list_create(DUMMY_CMPFUNC);
        case (PARSE_READY):
            break;
    }

    set_t *results = parser_get_result(index->parser);

    if (!set_size(results)) {
        set_destroy(results);
        return list_create(DUMMY_CMPFUNC);
    }

    list_t *query_results = get_query_results(results);
    if (!query_results) {
        *errmsg = "index failed to allocate more memeory";
        return NULL;
    }/* else {
        score_query_results(query_result);
    }*/

    return query_results;
}



// make clean && make assert_index && lldb assert_index
// make clean && make indexer && ./indexer data/cacm/
// sudo lsof -i -P | grep LISTEN | grep :$8080
/* a OR b OR ((c) OR k OR (y OR l)) OR x OR (j) OR k OR (x OR y) OR j */
