/* 
 * Authors:
 * Magnus Stenhaug <magnus.stenhaug@uit.no> 
 * Erlend Helland Graff <erlend.h.graff@uit.no> 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "index.h"
#include "list.h"
#include "set.h"
#include "printing.h"

#define WORD_LENGTH ( 5 )
#define NUM_ITEMS ( 40 )
#define NUM_DOCS ( 40 )
#define PTIME  1

typedef struct document {
    set_t *terms;
    char path[20];
} document_t;

static document_t docs[NUM_DOCS];


/* Generates a random sequence of characters given a seed */
char *generate_string(unsigned int *seed) {
    int i;
    int len;
    char *s;

    len = (rand_r(seed) % WORD_LENGTH) + 1;

    /* Generate a random string of characters */
    s = calloc(sizeof(char), len + 1);
    for (i = 0 ; i < len; i++) {
        s[i] = 'a' + (rand_r(seed) % ('z' - 'a'));
    }

    return s;
}

/* Generates a list of words which acts a a document */
void initialize_document(document_t *doc, unsigned int seed) {
    int i;
    // list_t *words; // note: unused
    char *word;

    sprintf(doc->path, "document_%d.txt", seed);
    doc->terms = set_create(compare_strings);

    for (i = 0; i < NUM_ITEMS; i++) {
        word = generate_string(&seed);
		
        if (set_contains(doc->terms, word)) {
            free(word);
        } else {
            set_add(doc->terms, word);
        }
    }
}

/* Releases the memory used */
void doc_destroy(document_t *doc) {	
    set_iter_t *iter;
	
    iter = set_createiter(doc->terms);
    while (set_hasnext(iter)) {
        free(set_next(iter));
    }

    set_destroyiter(iter);
    set_destroy(doc->terms);
}

/* Runs a series of queries and validates the index */
void validate_index(index_t *ind) {
    int i, hitCount;
    // set_t *w; // note: unused
    list_t *query;
    set_iter_t *iter;
    list_t *result;
    char *errmsg, *term;
    query_result_t *res;

    query = list_create(compare_strings);

    /* Validate that all words returns the document */
    for (i = 0; i < NUM_DOCS; i++) {
        iter = set_createiter(docs[i].terms);

        while (set_hasnext(iter)) {
            /* Add to query */
            term = (char *)set_next(iter);
            list_addfirst(query, term);

            /* Run the query */
            result = index_query(ind, query, &errmsg);
            if (result == NULL) {
                ERROR_PRINT("Query resulted in the following error: %s", errmsg);
            }

            /* Validate that the path is in the result set */
            hitCount = 0;
            while (list_size(result) > 0) {
                res = list_popfirst(result);
                if (strcmp(res->path, docs[i].path) == 0) {
                    hitCount++;
                }
                free(res);
            }
            list_destroy(result);

            if (hitCount == 0){
                ERROR_PRINT("Document was not returned: term=%s path=%s", term, docs[i].path);
            }
            list_popfirst(query);
        }
        set_destroyiter(iter);
    }
    list_destroy(query);
}

int main(int argc, char **argv) {
    int i;
    index_t *ind;
    list_t *words;
    set_iter_t *iter;

    /* Create index */
    ind = index_create();

    /* Generate random documents */
    for (i = 0; i < NUM_DOCS; i++) {
        initialize_document(&docs[i], i);

        words = list_create(compare_strings);
        iter = set_createiter(docs[i].terms);

        while (set_hasnext(iter)) {
            list_addfirst(words, strdup((char *)set_next(iter)));
        }

        set_destroyiter(iter);
        index_addpath(ind, strdup(docs[i].path), words);
        list_destroy(words);
    }
    // unsigned long long t_start;
    // if (PTIME) t_start = gettime();

    // DEBUG_PRINT("Running a series of single term queries to validate the index...\n");
    // validate_index(ind);
    // DEBUG_PRINT("Success!\n");

    // if (PTIME) {
    //     unsigned long long t_end = gettime();
    //     printf("query took %llu μs\n", t_end-t_start);
    // }

    // index_destroy(ind);

    /* Cleanup */
    for (i = 0; i < NUM_DOCS; i++) {
        doc_destroy(&docs[i]);
    }
}


/* initial, simple testing of validate_index

// rbtree vs aatree -- single term queries -- all debug flags, 5 runs:

Run one:
WORD_LENGTH ( 5 )
NUM_ITEMS ( 500 )
NUM_DOCS ( 100 )

rbtreeset:
query took 74242 μs
query took 76714 μs
query took 73065 μs
query took 73142 μs
query took 72773 μs

aatreeset:
query took 62218 μs
query took 62924 μs
query took 60335 μs
query took 68264 μs
query took 63969 μs

Run two:
#define WORD_LENGTH ( 10 )
#define NUM_ITEMS ( 2000 )
#define NUM_DOCS ( 200 )

rbtreeset:
query took 750011 μs
query took 751826 μs
query took 739653 μs
query took 758962 μs
query took 809426 μs

aatreeset:
query took 636134 μs
query took 632887 μs
query took 640650 μs
query took 648631 μs
query took 675516 μs

Notes: aatree faster for single word queries using random words. Scaling seems ≈ the same for both.

Run three:
#define WORD_LENGTH ( 5 )
#define NUM_ITEMS ( 200 )
#define NUM_DOCS ( 2000 )

rbtreeset:
query took 10219844 μs
query took 10302288 μs
query took 10212572 μs
query took 10260200 μs
query took 10264125 μs

aatreeset:
query took 8644670 μs
query took 8698660 μs
query took 8813897 μs
query took 8711854 μs
query took 9802243 μs

Note: gap seems almost constant as a percentage.

Testing with -O2 and no debug flags

#define WORD_LENGTH ( 5 )
#define NUM_ITEMS ( 200 )
#define NUM_DOCS ( 2000 )

rbtreeset:
query took 9291487 μs
query took 9061225 μs
query took 9169199 μs
query took 9078361 μs
query took 9302959 μs

aatreeset:
query took 8141901 μs
query took 8310149 μs
query took 8126537 μs
query took 8127447 μs
query took 8407227 μs

Note: fairly consistent 10% gain from the aatree.

*/
