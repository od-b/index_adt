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
#include "map.h"

#include <ctype.h>
#include <string.h>
#include <math.h>


/******************************************************************************
 *                                                                            *
 *                   Section 0: Definitions & Declarations                    *
 *                                                                            *
 ******************************************************************************/


#define DUMMY_CMPFUNC  &compare_pointers

typedef struct iword iword_t;
typedef struct idocument idocument_t;
typedef struct idocu_term idocu_term_t;


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

struct idocu_term {
    iword_t  *iword;  // pointer to the indexed word, where we can also find global frequency of the word
    int       freq;   // how frequent the word is within the given document
};

// The tf-idf value increases proportionally to the number of times a word appears in the document
// and is offset by the number of documents in the corpus that contain the word


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
 *           Section 1: Index Creation, Destruction and Building              *
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
        set_iter_t *path_iter = set_createiter(curr->idocs);

        /* add to set of all paths */
        while (set_hasnext(path_iter)) {
            set_add(all_paths, set_next(path_iter));
        }
        set_destroyiter(path_iter);

        /* free the iword & its members */
        set_destroy(curr->idocs);
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

void index_addpath(index_t *index, char *path, list_t *tokens) {
    /*
     * Note: No good way of handling malloc failure without a possible return value,
     * so indexer could react appropriately. 
     * For that reason, there's solely error prints / void returns.
    */

    if (list_size(tokens) == 0) {
        DEBUG_PRINT("given path with no tokens: %s\n", path);
        return;
    }
    index->n_docs++;

    list_iter_t *tok_iter = list_createiter(tokens);
    idocument_t *doc = malloc(sizeof(idocument_t));
    if (!doc || !tok_iter) {
        ERROR_PRINT("malloc failed\n");
    }
    doc->terms = map_create((cmpfunc_t)strcmp, hash_string);
    if (!doc->terms) {
        ERROR_PRINT("malloc failed\n");
    }

    doc->path = path;

    char *prev_tok = NULL;
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
                ERROR_PRINT("malloc failed\n");
            }

            /* Since the search word was added, create a new iword for searching. */
            index->iword_buf = malloc(sizeof(iword_t));
            if (!index->iword_buf) {
                ERROR_PRINT("malloc failed\n");
            }
            index->iword_buf->idocs = NULL;
        } else {
            /* index is already storing this word. free the duplicate string. */
            free(tok);
        }

        /* update the existing document term, or create it if it's the first occurance */
        idocu_term_t *term = map_get(doc->terms, iword->word);
        if (!term) {
            term = malloc(sizeof(idocu_term_t));
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
}


/******************************************************************************
 *                                                                            *
 *                   Section 2: Query Parsing & Response                      *
 *                                                                            *
 ******************************************************************************/


/*
 * Returns a list of query results, created from each path in the given set.
 */
static list_t *get_query_results(index_t *index, list_t *tokens, set_t *docs) {
    list_t *query_results = list_create((cmpfunc_t)compare_query_results_by_score);
    set_t *search_terms = set_create((cmpfunc_t)strcmp);
    list_iter_t *tok_iter = list_createiter(tokens);
    set_iter_t *docs_iter = set_createiter(docs);

    if (!query_results || !search_terms || !tok_iter || !docs_iter) {
        return NULL;
    }

    /* Create a set from the token words.
     * Operators/parantheses will not be registered as terms,
     * But would increase the amount of dead map calls within the main loop.
     */
    while (list_hasnext(tok_iter)) {
        char *tok = list_next(tok_iter);
        /* The most cursed switch */
        switch (tok[0]) {
            case (')'): break;
            case ('('): break;
            case ('A'): break;
            case ('O'): break;
            default: set_add(search_terms, tok);
        }
    }
    list_destroyiter(tok_iter);
    idocu_term_t *docterm = NULL;
    double tf, idf;

    while (set_hasnext(docs_iter)) {
        query_result_t *q_result = malloc(sizeof(query_result_t));
        idocument_t *doc = set_next(docs_iter);

        set_iter_t *s_terms_iter = set_createiter(search_terms);
        if (!s_terms_iter || !q_result) {
            return NULL;
        }

        /* Naive implementation of the tf-idf scoring algorithm.
         * Cross references all search terms with terms in the result doc 
         */
        while (set_hasnext(s_terms_iter)) {
            docterm = map_get(doc->terms, set_next(s_terms_iter));
            if (docterm) {
                /* If doc has the term, it's a relevant term. Calculate tf-idf and add to score */
                tf = (double)docterm->freq;
                idf = log((double)index->n_docs / (double)set_size(docterm->iword->idocs));
                q_result->score += tf * idf;
            }
            /* Notes: 
             * 1) the doc will always get a score from this, as it cannot be here without a matching token
             * 2) division by zero may not occur, as all indexed words must stem from a document
            */
        }
        set_destroyiter(s_terms_iter);

        /* add the document path to the query result, and query result to list of results */
        q_result->path = doc->path;
        list_addlast(query_results, q_result);
    }

    /* cleanup */
    set_destroyiter(docs_iter);
    set_destroy(search_terms);

    /* sort the results by score */
    list_sort(query_results);
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

    list_t *query_results = get_query_results(index, tokens, results);
    set_destroy(results);

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
