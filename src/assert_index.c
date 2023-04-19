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

#define WORD_LENGTH ( 40 )
#define NUM_ITEMS ( 200 )
#define NUM_DOCS ( 500 )
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
    unsigned long long t_time = 0;

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
            unsigned long long t_start;
            if (PTIME) t_start = gettime();

            result = index_query(ind, query, &errmsg);

            if (PTIME) {
                t_time += (gettime() - t_start);
            }
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

    if (PTIME) printf("queries took a total of %llu μs\n", t_time);
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

    DEBUG_PRINT("Running a series of single term queries to validate the index...\n");
    validate_index(ind);
    DEBUG_PRINT("Success!\n");


    index_destroy(ind);

    /* Cleanup */
    for (i = 0; i < NUM_DOCS; i++) {
        doc_destroy(&docs[i]);
    }
}


/* 
Experiment:
Simple time-based tests, comparing index_A to index_B.
The tests will be done using assert index, meaning single term queries only.
A timer collects cumulative time spent within index_query() only, and prints the result.
While these tests by no means will be indicative as to which is the 'better' option, 
the results could possible give an idea of what tests should be performed at a later stage of development.

All tests will be performed with flags: -O2 -g

------------------ index_A.c -------------------
#define WORD_LENGTH ( 10 )
#define NUM_ITEMS ( 200 )
#define NUM_DOCS ( 500 )

queries took a total of 178458 μs                             
queries took a total of 150633 μs
queries took a total of 151839 μs
queries took a total of 151722 μs
queries took a total of 156317 μs

---

#define WORD_LENGTH ( 10 )
#define NUM_ITEMS ( 500 )
#define NUM_DOCS ( 2000 )
queries took a total of 7736915 μs
queries took a total of 7862573 μs

---

#define WORD_LENGTH ( 40 )
#define NUM_ITEMS ( 200 )
#define NUM_DOCS ( 500 )
queries took a total of 63305 μs
queries took a total of 63364 μs
queries took a total of 61463 μs
queries took a total of 60665 μs

------------------ index_B.c -------------------
#define WORD_LENGTH ( 10 )
#define NUM_ITEMS ( 200 )
#define NUM_DOCS ( 500 )
queries took a total of 246347 μs
queries took a total of 223138 μs
queries took a total of 224561 μs
queries took a total of 221872 μs
queries took a total of 224945 μs

--- 

#define WORD_LENGTH ( 10 )
#define NUM_ITEMS ( 500 )
#define NUM_DOCS ( 2000 )
queries took a total of 12707335 μs
queries took a total of 12803705 μs

---

#define WORD_LENGTH ( 40 )
#define NUM_ITEMS ( 200 )
#define NUM_DOCS ( 500 )
queries took a total of 49954 μs
queries took a total of 47778 μs                             
queries took a total of 47100 μs
queries took a total of 48634 μs


--------------

Thoughts & Theories:
Interesting results despite the simple testing methods. This will need further testing with a much wider range of parameters.
Initial testing has however already demonstrated that the different implementations scale very differently
depending on parameter weights. The pure tree implementation (A) pulls ahead by a mile with a large number of items - 
however, the map implementation (B), seems to show its strength with smaller sets. 
The result make sense, given that a BST is generally a superior structure of choice for accessing "the needle in the haystack".
As the number of buckets in a map increase, the average access time gets worse in sync.

Conclusion:
* Further, more sophisticated testing needed.
* Assert_index is not an ideal testing ground for any method, and as soon as the parser is implemented, 
the implementations should be compared using real; as opposed to randomly generated, data.

*/
