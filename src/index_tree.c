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
#include <math.h>   // included for log


/******************************************************************************
 *                                                                            *
 *      Section 0: Definitions, Structs, Functions passed by reference        *
 *                                                                            *
 ******************************************************************************/


#define DUMMY_CMPFUNC  &compare_pointers

typedef struct iword iword_t;
typedef struct idocument idocument_t;


/* Type of index */
struct index {
    set_t    *iwords;       // set of all indexed words
    iword_t  *iword_buf;    // buffer of one iword for searching and adding words
    parser_t *parser;
    set_t    *query_words;  // temp set used to contain <word>'s being parsed
    int       n_docs;
};

/* Type of indexed word */
struct iword {
    char   *word;
    set_t  *in_docs;  // set of paths where ->word can be found
};

/* Type of indexed document */
struct idocument {
    char  *path;
    map_t *terms;   // {'char *word' => 'int *freq'}
};

/* strcmp wrapper */
int strcmp_iwords(iword_t *a, iword_t *b) {
    return strcmp(a->word, b->word);
}

/* Compares two iword_t based on the size of their path sets */
int compare_iwords_by_occurance(iword_t *a, iword_t *b) {
    return (set_size(a->in_docs) - set_size(b->in_docs));
}

int compare_idocs_by_path(idocument_t *a, idocument_t *b) {
    return strcmp(a->path, b->path);
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    if (b->score < a->score) return -1;
    if (a->score < b->score) return 1;
    return 0;
}

/* used by the parser to search within the index. */
set_t *get_iword_docs(index_t *index, char *word) {
    index->iword_buf->word = word;
    iword_t *result = set_get(index->iwords, index->iword_buf);

    if (result) {
        set_add(index->query_words, result);
        return result->in_docs;
    }
    return NULL;
}

/* FOR TESTING */
int index_n_words(index_t *index) {
    return set_size(index->iwords);
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

    index->parser = parser_create((void *)index, (search_func_t)get_iword_docs);
    if (!index->parser) {
        free(index);
        free(index->iwords);
        free(index->iword_buf);
        return NULL;
    }
    index->iword_buf->word = NULL;
    index->iword_buf->in_docs = NULL;

    index->n_docs = 0;
    index->query_words = NULL;

    return index;
}

void index_destroy(index_t *index) {
    printf("destroying index ... \n");
    int n_freed_words = 0;
    int n_freed_docs = 0;

    /* Documents are shared within sets of indexed words,
     * so put them in a set priort to freeing to avoid any hocus pocus
     * such as double free or potentially dismembering trees.
     */
    set_t *all_docs = set_create(compare_pointers);
    set_iter_t *iword_iter = set_createiter(index->iwords);

    if (!all_docs || !iword_iter) {
        // ERROR_PRINT("failed to allocate memory\n");
        return;
    }

    /* free the set of indexed words while creating a joint set of documents. */
    while (set_hasnext(iword_iter)) {
        iword_t *curr = set_next(iword_iter);
        set_iter_t *doc_iter = set_createiter(curr->in_docs);
        if (!doc_iter) {
            // ERROR_PRINT("failed to allocate memory\n");
            return;
        }

        /* add to set of all docs */
        while (set_hasnext(doc_iter)) {
            set_add(all_docs, set_next(doc_iter));
        }
        set_destroyiter(doc_iter);

        /* free the iword & its members */
        set_destroy(curr->in_docs);
        free(curr->word);
        free(curr);
        n_freed_words++;
    }
    set_destroyiter(iword_iter);
    set_destroy(index->iwords);

    set_iter_t *all_docs_iter = set_createiter(all_docs);
    if (!all_docs_iter) {
        // ERROR_PRINT("failed to allocate memory\n");
        return;
    }

    /* free the set of all documents */
    while (set_hasnext(all_docs_iter)) {
        idocument_t *doc = set_next(all_docs_iter);
        /* destroy the documents map of terms */
        map_destroy(doc->terms, NULL, free);
        free(doc->path);
        free(doc);
        n_freed_docs++;
    }
    set_destroyiter(all_docs_iter);
    set_destroy(all_docs);

    /* free index & co */
    parser_destroy(index->parser);
    free(index->iword_buf);
    free(index);

    printf("index_destroy: Freed %d documents, %d unique words\n",
        n_freed_docs, n_freed_words);
}

void index_addpath(index_t *index, char *path, list_t *tokens) {
    /*
     * Not certain how a malloc failure should be handled, and especially
     * not within this functions, seeing as there's no return value.
    */
    if (list_size(tokens) == 0) {
        // DEBUG_PRINT("given path with no tokens: %s\n", path);
        return;
    }

    list_iter_t *tok_iter = list_createiter(tokens);
    idocument_t *doc = malloc(sizeof(idocument_t));
    if (!doc || !tok_iter) {
        // ERROR_PRINT("malloc failed\n");
        return;
    }

    doc->terms = map_create((cmpfunc_t)strcmp, hash_string);
    if (!doc->terms) {
        // ERROR_PRINT("malloc failed\n");
        return;
    }

    index->n_docs++;
    doc->path = path;

    while (list_hasnext(tok_iter)) {
        char *tok = list_next(tok_iter);

        /* try to add the word to the index, using word_buf to allow comparison */
        index->iword_buf->word = tok;
        iword_t *iword = set_tryadd(index->iwords, index->iword_buf);

        if (iword == index->iword_buf) {
            /* first index entry for this word. initialize it as an indexed word. */
            iword->word = tok;
            iword->in_docs = set_create((cmpfunc_t)compare_idocs_by_path);
            if (!iword->in_docs) {
                // ERROR_PRINT("malloc failed\n");
                return;
            }

            /* Since the search word was added, create a new iword for searching. */
            index->iword_buf = malloc(sizeof(iword_t));
            if (!index->iword_buf) {
                // ERROR_PRINT("malloc failed\n");
                return;
            }
            index->iword_buf->in_docs = NULL;
        } else {
            /* index is already storing this word. free the duplicate string. */
            free(tok);
        }

        /* check if word already has occured within the document */
        int *freq = map_get(doc->terms, iword->word);

        if (!freq) {
            /* create a value entry for { word : freq } in the document map */
            freq = malloc(sizeof(int));
            if (!freq) {
                return;
            }
            *freq = 1;
            map_put(doc->terms, iword->word, freq);
        } else {
            /* increment the frequency of the word */
            *freq += 1;
        }

        /* add path to the indexed words set. */
        set_add(iword->in_docs, doc);
    }

    list_destroyiter(tok_iter);

    // printf("\nindex: docs = %d, unique words = %d\n", index->n_docs, set_size(index->iwords));
}


/******************************************************************************
 *                                                                            *
 *                   Section 2: Query Parsing & Response                      *
 *                                                                            *
 ******************************************************************************/


/*
 * Returns a sorted list of query results, created from each path in the given set.
 * Calculates score through a naive implementation of if-idf
 */
static list_t *format_query_results(index_t *index, set_t *docs) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);
    set_iter_t *docs_iter = set_createiter(docs);

    if (!query_results || !docs_iter) {
        goto alloc_error;
    }

    double n_total_docs = (double)index->n_docs;

    while (set_hasnext(docs_iter)) {
        query_result_t *q_result = malloc(sizeof(query_result_t));
        idocument_t *doc = set_next(docs_iter);

        set_iter_t *qword_iter = set_createiter(index->query_words);
        if (!qword_iter || !q_result) {
            if (q_result) {
                free(q_result);
            }
            goto alloc_error;
        }

        /* Naive implementation of the tf-idf scoring algorithm.
         * Cross references all search terms with terms in the result document
         */
        while (set_hasnext(qword_iter)) {
            iword_t *curr = set_next(qword_iter);
            int *tf = map_get(doc->terms, curr->word);

            if (tf) {
                /* document has the term. Calculate tf-idf and add to score */
                double idf = log(n_total_docs / (double)set_size(curr->in_docs));
                q_result->score += (double)(*tf) * idf;
            }
            /* Notes:
             * 1) the doc will always get a score from this, as it cannot be here without a matching token
             * 2) division by zero may not occur, as all indexed words must stem from a document
            */
        }
        set_destroyiter(qword_iter);

        /* assign the document path query result, then add it to the list of results */
        q_result->path = doc->path;
        if (!list_addlast(query_results, q_result)) {
            goto alloc_error;
        }
    }
    set_destroyiter(docs_iter);

    /* sort the results by score */
    list_sort(query_results);

    return query_results;

alloc_error:
    if (docs_iter) {
        set_destroyiter(docs_iter);
    }
    if (query_results) {
        void *res;
        while ((res = list_popfirst(query_results)) != NULL) {
            free(res);
        }
        list_destroy(query_results);
    }
    return NULL;
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
    return ret_list;
}

// sudo lsof -i -P | grep LISTEN | grep :$8080
