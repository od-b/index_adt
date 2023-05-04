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
#include <math.h>


/******************************************************************************
 *                                                                            *
 *      Section 0: Definitions, Structs, Functions passed by reference        *
 *                                                                            *
 ******************************************************************************/


#define DUMMY_CMPFUNC  &compare_pointers

typedef struct iword iword_t;
typedef struct idocument idocument_t;
typedef struct idocument_term idocument_term_t;


/* Type of index */
struct index {
    set_t    *iwords;       // set of all indexed words
    iword_t  *iword_buf;    // buffer of one iword for searching and adding words
    parser_t *parser;
    int       n_docs;
};

/* Type of indexed word */
struct iword {
    char   *word;
    set_t  *idocs;  // set of paths where ->word can be found
};

/* Type of indexed document */
struct idocument {
    char  *path;
    map_t *terms;
};

/* Type of term within indexed document */
struct idocument_term {
    iword_t  *iword;  // pointer to the indexed word, for word string & global frequency
    int       freq;   // frequency of term within the parent document
};

/* strcmp wrapper */
int strcmp_iwords(iword_t *a, iword_t *b) {
    return strcmp(a->word, b->word);
}

/* Compares two iword_t based on the size of their path sets */
int compare_iwords_by_occurance(iword_t *a, iword_t *b) {
    return (set_size(a->idocs) - set_size(b->idocs));
}

int compare_idocs_by_path(idocument_t *a, idocument_t *b) {
    return strcmp(a->path, b->path);
}

int compare_query_results_by_score(query_result_t *a, query_result_t *b) {
    if (b->score < a->score) return -1;
    if (a->score < b->score) return 1;
    return 0;
}

set_t *get_iword_docs(index_t *index, char *word) {
    index->iword_buf->word = word;
    iword_t *result = set_get(index->iwords, index->iword_buf);

    if (result)
        return result->idocs;
    return NULL;
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
    index->iword_buf->idocs = NULL;
    index->n_docs = 0;
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
        set_iter_t *doc_iter = set_createiter(curr->idocs);
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
        set_destroy(curr->idocs);
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

int index_addpath(index_t *index, char *path, list_t *tokens) {
    /*
     * Not certain how a malloc failure should be handled, and especially
     * not within this functions, seeing as there's no return value.
    */
    if (list_size(tokens) == 0) {
        // DEBUG_PRINT("given path with no tokens: %s\n", path);
        return 1;
    }

    list_iter_t *tok_iter = list_createiter(tokens);
    idocument_t *doc = malloc(sizeof(idocument_t));
    if (!doc || !tok_iter) {
        // ERROR_PRINT("malloc failed\n");
        return 0;
    }

    doc->terms = map_create((cmpfunc_t)strcmp, hash_string);
    if (!doc->terms) {
        // ERROR_PRINT("malloc failed\n");
        return 0;
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
            iword->idocs = set_create((cmpfunc_t)compare_idocs_by_path);
            if (!iword->idocs) {
                // ERROR_PRINT("malloc failed\n");
                return 0;
            }

            /* Since the search word was added, create a new iword for searching. */
            index->iword_buf = malloc(sizeof(iword_t));
            if (!index->iword_buf) {
                // ERROR_PRINT("malloc failed\n");
                return 0;
            }
            index->iword_buf->idocs = NULL;
        } else {
            /* index is already storing this word. free the duplicate string. */
            free(tok);
        }

        /* update the existing document term, or create it if it's the first occurance */
        idocument_term_t *term = map_get(doc->terms, iword->word);
        if (!term) {
            term = malloc(sizeof(idocument_term_t));
            if (!term) {
                // ERROR_PRINT("malloc failed\n");
                return 0;
            }
            term->freq = 1;
            term->iword = iword;
            map_put(doc->terms, iword->word, term);
        } else {
            term->freq++;
        }

        /* add path to the indexed words set. */
        set_add(iword->idocs, doc);
    }

    list_destroyiter(tok_iter);

    if (set_size(index->iwords) >= WORDS_LIMIT) {
        printf("\nindex: docs = %d, unique words = %d\n", index->n_docs, set_size(index->iwords));
        return 0;
    }
    return 1;
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
static list_t *format_query_results(index_t *index, list_t *tokens, set_t *docs) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);
    set_t *search_terms = set_create((cmpfunc_t)strcmp);
    list_iter_t *tok_iter = list_createiter(tokens);
    set_iter_t *docs_iter = set_createiter(docs);

    if (!query_results || !search_terms || !tok_iter || !docs_iter) {
        goto alloc_error;
    }

    /* Create a set from the token words.
     * Operators/parantheses will not be indexed, but having them in the set 
     * increase the amount of dead map calls within the main loop. 
     */
    while (list_hasnext(tok_iter)) {
        char *tok = list_next(tok_iter);
        /* Filter out nonterms */
        switch (tok[0]) {
            case (')'): break;
            case ('('): break;
            case ('A'): break;
            case ('O'): break;
            default: set_add(search_terms, tok);
        }
    }
    list_destroyiter(tok_iter);

    double n_total_docs = (double)index->n_docs;

    while (set_hasnext(docs_iter)) {
        query_result_t *q_result = malloc(sizeof(query_result_t));
        idocument_t *doc = set_next(docs_iter);

        set_iter_t *s_terms_iter = set_createiter(search_terms);
        if (!s_terms_iter || !q_result) {
            if (q_result) free(q_result);
            goto alloc_error;
        }

        /* Naive implementation of the tf-idf scoring algorithm.
         * Cross references all search terms with terms in the result doc 
         */
        while (set_hasnext(s_terms_iter)) {
            idocument_term_t *docterm = map_get(doc->terms, set_next(s_terms_iter));
            if (docterm) {
                /* Since doc has the term, it's a relevant term. Calculate tf-idf and add to score */
                q_result->score += (
                    (double)docterm->freq   // tf
                    * log(n_total_docs / (double)set_size(docterm->iword->idocs))  // idf
                );
            }
            /* Notes: 
             * 1) the doc will always get a score from this, as it cannot be here without a matching token
             * 2) division by zero may not occur, as all indexed words must stem from a document
            */
        }
        set_destroyiter(s_terms_iter);

        /* assign the document path query result, then add it to the list of results */
        q_result->path = doc->path;
        if (!list_addlast(query_results, q_result)) {
            goto alloc_error;
        }
    }

    /* cleanup */
    set_destroyiter(docs_iter);
    set_destroy(search_terms);

    /* sort the results by score */
    list_sort(query_results);

    return query_results;

alloc_error:
    if (tok_iter) list_destroyiter(tok_iter);
    if (docs_iter) set_destroyiter(docs_iter);
    if (search_terms) set_destroy(search_terms);
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

    /* Scan query tokens with the parser */
    switch (parser_scan(index->parser, tokens)) {
        case (ALLOC_FAILED):
            *errmsg = "index failed to allocate memeory";
            return NULL;
        case (SYNTAX_ERROR):
            *errmsg = parser_get_errmsg(index->parser);
            return NULL;
        case (SKIP_PARSE):
            return list_create(DUMMY_CMPFUNC);
        case (PARSE_READY):
            break;
    }

    /* parse is ready, proceed to get results */
    set_t *results = parser_get_result(index->parser);

    /* if the query produced an empty set, return an empty list */
    if (!set_size(results)) {
        set_destroy(results);
        return list_create(DUMMY_CMPFUNC);
    }

    /* non-empty result set. Create list of query results */
    list_t *query_results = format_query_results(index, tokens, results);

    /* The set returned by parser is no longer needed. */
    set_destroy(results);

    /* if all is well, return the results. */
    if (query_results) {
        return query_results;
    } else {
        *errmsg = "index failed to allocate memeory";
        return NULL;
    }
}



// make clean && make assert_index && lldb assert_index
// make clean && make indexer && ./indexer data/cacm/
// sudo lsof -i -P | grep LISTEN | grep :$8080
/* a OR b OR ((c) OR k OR (y OR l)) OR x OR (j) OR k OR (x OR y) OR j */
