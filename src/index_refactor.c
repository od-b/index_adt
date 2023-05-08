/*
 * Index ADT implementation relying mainly on the use of tree-based set(s).
 * In an attempt to improve readability, the code within this file is split into sections.
 * Sections 1 & 2 consist solely of functions.
 */

#include "index.h"
#include "common.h"
#include "queryparser.h"
#include "set.h"
#include "map.h"
// #include "assert.h"
// #include "printing.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <math.h>   // included for log


/******************************************************************************
 *                                                                            *
 *      Section 0: Definitions, Structs, Functions passed by reference        *
 *                                                                            *
 ******************************************************************************/


#define DUMMY_CMPFUNC  &compare_pointers


/* Type of indexed word */
typedef struct iword {
    char   *term;
    set_t  *paths;  // set of paths where ->word can be found
    map_t  *tf;     // path -> freq
} iword_t;


/* Type of index */
struct index {
    set_t    *indexed_words;       // set of all indexed words
    iword_t  *iword_buf;    // buffer of one iword for searching and adding words
    parser_t *parser;
    set_t    *query_words;  // temp set used to contain <word>'s being parsed
    int       n_docs;
};


/* strcmp wrapper */
int strcmp_iwords(iword_t *a, iword_t *b) {
    return strcmp(a->term, b->term);
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    if (b->score < a->score) return -1;
    if (a->score < b->score) return 1;
    return 0;
}

/* used by the parser to search within the index. */
set_t *get_iword_docs(index_t *index, char *term) {
    index->iword_buf->term = term;
    iword_t *result = set_get(index->indexed_words, index->iword_buf);

    if (result) {
        set_add(index->query_words, result);
        return result->paths;
    }
    return NULL;
}

/* TESTFUNC */
int index_uniquewords(index_t *index) {
    return set_size(index->indexed_words);
}

/******************************************************************************
 *                                                                            *
 *            Section 1: Index Creation, Building, Destruction                *
 *                                                                            *
 ******************************************************************************/

index_t *index_create() {
    index_t *index = malloc(sizeof(index_t));
    if (!index) {
        return NULL;
    }

    /* create the 'core' index set to contain iwords */
    index->indexed_words = set_create((cmpfunc_t)strcmp_iwords);
    if (!index->indexed_words) {
        free(index);
        return NULL;
    }

    index->iword_buf = malloc(sizeof(iword_t));
    if (!index->iword_buf) {
        set_destroy(index->indexed_words);
        free(index);
        return NULL;
    }

    index->parser = parser_create((void *)index, (term_func_t)get_iword_docs);
    if (!index->parser) {
        set_destroy(index->indexed_words);
        free(index->iword_buf);
        free(index);
        return NULL;
    }
    index->iword_buf->term = NULL;
    index->iword_buf->paths = NULL;
    index->iword_buf->tf = NULL;

    index->n_docs = 0;
    index->query_words = NULL;

    return index;
}

void index_destroy(index_t *index) {
    return;
    /* test implementation, rework of destroy TODO */
    // printf("destroying index ... \n");
    // int n_freed_words = 0;
    // int n_freed_docs = 0;
    // set_t *all_docs = set_create(compare_pointers);
    // set_iter_t *iword_iter = set_createiter(index->indexed_words);
    // printf("index_destroy: Freed %d documents, %d unique words\n",
    //     n_freed_docs, n_freed_words);
}

void index_addpath(index_t *index, char *path, list_t *tokens) {
    /*
     * Not certain how a malloc failure should be handled, and especially
     * not within this functions, seeing as there's no return value.
     * This seems like a function that absolutely should have one ...
     */
    if (list_size(tokens) == 0) {
        return;
    }

    list_iter_t *tok_iter = list_createiter(tokens);
    if (!tok_iter) {
        return;
    }

    index->n_docs++;

    while (list_hasnext(tok_iter)) {
        char *tok = list_next(tok_iter);

        /* try to add the word to the index, using word_buf to allow comparison */
        index->iword_buf->term = tok;
        iword_t *iword = set_tryadd(index->indexed_words, index->iword_buf);

        if (iword == index->iword_buf) {
            /* first index entry for this word. initialize it as an indexed word. */
            iword->term = tok;
            iword->paths = set_create((cmpfunc_t)strcmp);
            iword->tf = map_create((cmpfunc_t)strcmp, hash_string);

            /* Since the search word was added, recreate buffer. */
            index->iword_buf = malloc(sizeof(iword_t));

            if (!iword->paths || !iword->tf || !index->iword_buf) {
                return;
            }
            index->iword_buf->paths = NULL;
        } else {
            free(tok);
        }

        if (set_contains(iword->paths, path)) {
            /* duplicate word within document */
            unsigned short *tf = map_get(iword->tf, path);
            if (*tf < USHRT_MAX) {
                *tf += 1;
            }
        } else {
            /* add path to the indexed words set. */
            set_add(iword->paths, path);
            printf("added %s\n", path);
            /* allocate a tf pointer */
            unsigned short *tf = malloc(sizeof(unsigned short));
            if (tf) {
                *tf = 1;
                map_put(iword->tf, path, tf);
            }
        }
    }

    list_destroyiter(tok_iter);
}


/******************************************************************************
 *                                                                            *
 *                   Section 2: Query Parsing & Response                      *
 *                                                                            *
 ******************************************************************************/

int compare_qresults_by_path(query_result_t *a, query_result_t *b) {
    return strcmp(a->path, b->path);
}

/*
 * Returns a sorted list of query results, created from each path in the given set.
 */
static list_t *format_query_results(index_t *index, set_t *paths) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);
    map_t *result_map = map_create((cmpfunc_t)strcmp, hash_string);
    set_iter_t *qword_iter = set_createiter(index->query_words);

    if (!query_results || !qword_iter || !result_map) {
        return NULL;
    }

    double n_total_docs = (double)index->n_docs;
    double idf, tf_idf;
    unsigned short *tf;
    char *path;
    query_result_t *q_result;
    iword_t *iword;
    set_t *uni_paths;
    set_iter_t *uni_iter;

    /* iterate over indexed query words */
    while (set_hasnext(qword_iter)) {
        iword = set_next(qword_iter);

        /* query word paths U result paths */
        uni_paths = set_union(iword->paths, paths);
        if (!uni_paths) {
            return NULL;
        }

        if (set_size(uni_paths)) {
            uni_iter = set_createiter(uni_paths);
            if (!uni_iter) {
                return NULL;
            }

            /* for all paths in union, create result and/or update tf-idf */
            while (set_hasnext(uni_iter)) {
                path = set_next(uni_iter);

                /* calculated tf/idf */
                tf = map_get(iword->tf, path);

                idf = log(n_total_docs / (double)set_size(iword->paths));
                tf_idf = ((double)*tf) * idf;

                /* check if a result struct is already created for path */
                q_result = map_get(result_map, path);

                if (!q_result) {
                    /* new result, create it, set path etc */
                    q_result = malloc(sizeof(query_result_t));
                    if (!q_result) {
                        return NULL;
                    }
                    q_result->path = path;
                    q_result->score = tf_idf;

                    /* put in map of results */
                    map_put(result_map, path, q_result);
                    if (!list_addlast(query_results, q_result)) {
                        return NULL;
                    }
                } else {
                    /* update existing result */
                    q_result->score += tf_idf;
                }
            }
            set_destroyiter(uni_iter);
        }
        set_destroy(uni_paths);
    }
    map_destroy(result_map, NULL, NULL);

    /* sort the results by score */
    list_sort(query_results);

    return query_results;
}

list_t *index_query(index_t *index, list_t *tokens, char **errmsg) {
    if (!list_size(tokens)) {
        *errmsg = "empty query";
        return NULL;
    }

    list_t *ret_list = NULL;
    set_t *results = NULL;

    /* create a set to store <word> token i_words, if any */
    index->query_words = set_create((cmpfunc_t)strcmp_iwords);
    if (!index->query_words) {
        *errmsg = "index failed to allocate memeory";
        return NULL;
    }

    /* give tokens to the parser for scanning */
    switch (parser_scan(index->parser, tokens)) {
        case (ALLOC_FAILED):
            /* no clue if this would even print */
            *errmsg = "index failed to allocate memeory";
            break;
        case (SYNTAX_ERROR):
            *errmsg = parser_get_errmsg(index->parser);
            break;
        case (SKIP_PARSE):
            ret_list = list_create(DUMMY_CMPFUNC);
            break;
        case (PARSE_READY):
            /* parse is ready, proceed to get results */
            results = parser_get_result(index->parser);

            if (!results) {
                *errmsg = "index failed to allocate memeory";
            } else if (!set_size(results)) {
                /* query produced an empty set, return an empty list */
                ret_list = list_create(DUMMY_CMPFUNC);
            } else {
                /* nonempty set, format results */
                ret_list = format_query_results(index, results);
            }
            break;
    }

    /* clean up and return */
    if (results) {
        set_destroy(results);
    }
    if (index->query_words) {
        set_destroy(index->query_words);
    }

    /* ret_list will be NULL on error */
    return ret_list;
}

// sudo lsof -i -P | grep LISTEN | grep :$8080
