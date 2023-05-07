

/*
 * --- REFERENCES ---

 * Many of the functions within this file contain pieces of code copied
 * from within the provided precode. The functions are typically either
 * slightly or completely modified.
 * 
 * References will be given in the following format: 
 *  `REFERENCE: <function name> @ <src/c file>`
 * 
 * All functions with a reference may be considered to be copied
 * in its entirety from that source, for the sake of simplicity.
 * If there are no changes commited to the copied function, it will be noted.
 * 
 * No code within this file is taken, copied or based on sources outside of 
 * the precode.
 * 
*/

/* 
 * Program to profile the index while logging the results to files.
 *
 * NOTE:
 * Considering the program is immediately terminated post-test,
 * the program does not correctly clean up after itself.
 * OS will reclaim any memory, so it would for the purpose of testing
 * code other than index_destroy specifically - be pointless.
 * 
*/

#include "index.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static const char *TEST_ID = "MIX";
static const char *OUT_DIR = "./prof/";

#define QUERY_MAXLEN 3000


/*
 *  `REFERENCE: <is_reserved_word> @ <indexer.c>`
 *  copied in its entirety
 */
static int is_reserved_word(char *word) {
    if (strcmp(word, "ANDNOT") == 0)
        return 1;
    else if (strcmp(word, "AND") == 0)
        return 1;
    else if (strcmp(word, "OR") == 0)
        return 1;
    else if (strcmp(word, "(") == 0)
        return 1;
    else if (strcmp(word, ")") == 0)
        return 1;
    else
        return 0;
}

/*
 *  `REFERENCE: <is_reserved_char> @ <indexer.c>`
 *  copied in its entirety
 */
static int is_reserved_char(char a) {
    if (isspace(a)) {
        return 1;
    }

    switch (a) {
        case '(':
            return 1;
        case ')':
            return 1;
        default:
            return 0;
    }
}

/*
 *  `REFERENCE: <substring> @ <indexer.c>`
 *  copied in its entirety
 */
static char *substring(char *start, char *end) {
    char *s = malloc(end-start+1);
    if (s == NULL) {
        printf("out of memory\n");
        goto end;
    }

    strncpy(s, start, end-start);
    s[end-start] = 0;

end:
    return s;
}

/*
 *  `REFERENCE: <tokenize_query> @ <indexer.c>`
 *  copied in its entirety
 */
static list_t *tokenize_query(char *query) {
    char *term;
    list_t *processed;
    processed = list_create(compare_strings);

    while (*query != '\0') {
        if (isspace(*query)) {
            /* Ignore whitespace */
            query++;
            continue;
        } else if (*query == '(') {
            list_addlast(processed, strdup("("));
            query++;
        } else if (*query == ')') {
            list_addlast(processed, strdup(")"));
            query++;
        } else {
            /* Get length of term */
            char *s;
            for (s = query; (!is_reserved_char(*s)) && (*s != '\0'); s++);

            /* Copy term */
            term = substring(query, s);
            query = s;

            /* add to list */
            list_addlast(processed, term);
        }
    }

    return processed;
}

/*
 *  `REFERENCE: <preprocess_query> @ <indexer.c>`
 *  copied in its entirety
 */
static list_t *preprocess_query(char *query) {
    char *word, *c, *prev;
    list_t *tokens;
    list_t *processed;
    list_iter_t *iter;

    /* Create tokens */
    tokens = tokenize_query(query);
    processed = list_create(compare_strings);
    prev = NULL;

    iter = list_createiter(tokens);
    while (list_hasnext(iter)) {
        word = list_next(iter);

        /* Is a word */
        if (!is_reserved_word(word)) {

            /* Convert to lowercase */
            for (c = word; *c; c++) {
                *c = tolower(*c);
                // assert(islower(*c));
            }

            /* Adjacent words */
            if (prev != NULL && !is_reserved_word(prev)) {
                list_addlast(processed, strdup("OR"));
            }
        }
        /* Add to processed tokens */
        list_addlast(processed, word);
        prev = word;
    }

    list_destroyiter(iter);
    list_destroy(tokens);

    return processed;
}



/*
 * * * * * * * TESTING & UTILITY FUNCTIONS * * * * * * *
 * apart from sections of main, these are non-copied.
 * assumes memory is available, but cleans up, apart from the index.
*/

typedef struct query_time {
    int ntokens;
    int nresults;
    unsigned long long time;
} query_time_t;

int compare_rand(void *a, void *b) {
    int r = rand() % 3;
    switch (r) {
        case (0):
            return -1;
        case (1):
            return 1;
        case (2):
            return 0;
    }
}

/* the most cursed function name */
int sort_timeresult_by_nresults(query_time_t *a, query_time_t *b) {
    /* sort by n results, otherwise n tokens if equal */
    if (a->nresults != b->nresults)
        return (a->nresults - b->nresults);
    return (a->ntokens - b->ntokens);
}

int compare_lists_by_size(list_t *a, list_t *b) {
    return (list_size(a) - list_size(b));
}

static void print_to_csv(FILE *out, int n, long long unsigned t_time) {
    fprintf(out, "%d, %llu\n", n, t_time);
}

/* returns a 2D list of queries from a csv */
static list_t *load_queries(FILE *csv_in, int n_queries) {
    int count = 0;
    char *errmsg;

    list_t *queries = list_create((cmpfunc_t)compare_lists_by_size);

    /* read queries from file and feed them to the index */
    while (!feof(csv_in) && (count++ < n_queries)) {
        char query[(size_t)QUERY_MAXLEN];
        query[(size_t)QUERY_MAXLEN - 1] = 0;

        /* scan until first newline */
        if (fscanf(csv_in, "%[^\n]", query) != 1) {
            break;
        }
        char *q_dup = strdup(query);

        list_t *tokens = preprocess_query(q_dup);
        list_addlast(queries, tokens);

        /* eat the newline */
        fgetc(csv_in);
    }
    return queries;
}

/* feed queries to the index */
static list_t *run_queries(index_t *idx, list_t *queries) {
    int n_queries = list_size(queries);
    int n_errors = 0;
    char *errmsg;

    list_t *time_results = list_create((cmpfunc_t)sort_timeresult_by_nresults);
    list_iter_t *query_iter = list_createiter(queries);

    printf("Running %d queries ...\n", n_queries);

    /* read queries from file and feed them to the index */
    while (list_hasnext(query_iter)) {
        list_t *tokens = list_next(query_iter);

        unsigned long long seg_start = gettime();
        list_t *results = index_query(idx, tokens, &errmsg);
        unsigned long long seg_end = (gettime() - seg_start);

        /* in case there is an error query in the generated file */
        if (results) {
            /* create the time result and add to list */
            query_time_t *time_result = malloc(sizeof(query_time_t));
            time_result->time = seg_end;
            time_result->ntokens = list_size(tokens);
            time_result->nresults = list_size(results);
            list_addlast(time_results, time_result);

            list_iter_t *result_iter = list_createiter(results);
            while (list_hasnext(result_iter)) {
                free(list_next(result_iter));
            }
        } else {
            n_errors++;
        }

        list_iter_t *tok_iter = list_createiter(tokens);

        /* prints below can be uncommented to check query was correctly read 
         * would not recommend doing this with a huge amount of queries. */

        // printf("query = `");
        while (list_hasnext(tok_iter)) {
            char *tok = list_next(tok_iter);
            // printf("%s ", tok);
            free(tok);
        }
        // printf("`\n");

        list_destroyiter(tok_iter);
        list_destroy(tokens);
    }

    /* typical query generation has ≈ 4 errors in 20000. may be the gcide lib, idk. */
    printf("no. errors in set: %d\n", n_errors);

    return time_results;
}

static void print_time_results_to_csv(list_t *time_results, FILE *csv_ntokens, FILE* csv_nresults) {
    list_iter_t *iter = list_createiter(time_results);

    /* print results based on n tokens first since list is sorted that way by default */
    while (list_hasnext(iter)) {
        query_time_t *curr = list_next(iter);
        print_to_csv(csv_ntokens, curr->ntokens, curr->time);   // n = tokens
    }
    list_destroyiter(iter);

    /* sort the time results by how many results (paths) the query returned */
    list_sort(time_results);

    iter = list_createiter(time_results);

    /* print to csv based on n_results */
    while (list_hasnext(iter)) {
        query_time_t *curr = list_next(iter);
        print_to_csv(csv_nresults, curr->nresults, curr->time);  // n = results

        /* free the struct */
        free(curr);
    }
}

/* start the process of timing queries */
static void init_timed_queries(index_t *idx, char *query_src, char *str_n_queries, char *k_files) {
    /* load queries from csv */
    FILE *csv_in = fopen(query_src, "r");
    if (!csv_in) {
        perror("failed to open query src");
        return;
    }

    int n_queries = atoi(str_n_queries) * 1000;

    list_t *queries = load_queries(csv_in, n_queries);
    fclose(csv_in);

    /* sort by sub-list size */
    list_sort(queries);

    /* run the queries */
    list_t *time_results = run_queries(idx, queries);


    /* create & open csv docs */

    /* file to print time from n tokens in query */
    char *out_n_toks = concatenate_strings(4, OUT_DIR, "q_ntokens_", TEST_ID, ".csv");
    FILE *csv_ntokens = fopen(out_n_toks, "w");
    free(out_n_toks);

    /* file to print time from n results of a query */
    char *out_n_results = concatenate_strings(4, OUT_DIR, "q_nresults_", TEST_ID, ".csv");
    FILE *csv_nresults = fopen(out_n_results, "w");
    free(out_n_results);

    /* print results to csv */
    print_time_results_to_csv(time_results, csv_ntokens, csv_nresults);

    fclose(csv_ntokens);
    fclose(csv_nresults);
}

/*
 * REFERENCE: main @ indexer.c
 * ./profile_index data/cacm/ 1 queries.csv 20
 */
int main(int argc, char **argv) {
    char *relpath, *fullpath, *root_dir, *query_src, *k_files;
    unsigned long long cum_time, seg_start, seg_time;
    list_t *files, *words;
    list_iter_t *iter;
    index_t *idx;

    srand(time(NULL));

    /* queries are read separated by newlines, regardless of source format 
     * k_ values are given in thousands
    */
    if (argc != 5) {
        printf("usage: indexer <dir> <k_files> <query_src> <k_queries>\n");
        return 1;
    }

    root_dir = argv[1];
    k_files = argv[2];
    query_src = argv[3];

    const int n_files = atoi(k_files) * 1000;

    if (n_files <= 0) {
        printf("ERROR: invalid file count\n");
        return 1;
    }

    /* Check that root_dir exists and is directory */
    if (!is_valid_directory(root_dir)) {
        printf("ERROR: invalid root_dir '%s'\n", root_dir);
        return 1;
    }

    printf("\nFinding files at %s \n", root_dir);

    files = find_files(root_dir);
    idx = index_create();
    if (!idx) { 
        printf("ERROR: Failed to create index\n");
        return 1;
    }

    /* create & open csvs for build time */

    char *outpath_nfiles = concatenate_strings(4, OUT_DIR, "build_nfiles_", TEST_ID, ".csv");
    FILE *csv_nfiles = fopen(outpath_nfiles, "w");
    free(outpath_nfiles);

    char *outpath_nwords = concatenate_strings(4, OUT_DIR, "build_nwords_", TEST_ID, ".csv");
    FILE *csv_nwords = fopen(outpath_nwords, "w");
    free(outpath_nwords);

    /* build time variables */
    cum_time = 0;
    seg_start = gettime();

    int progress = 0;
    printf("Found %d files in dir, indexing up to %d ...\n", list_size(files), n_files);
    iter = list_createiter(files);

    while (list_hasnext(iter) && (progress < n_files)) {
        if (++progress % 500 == 0) {
            seg_time = (gettime() - seg_start);
            cum_time += seg_time;

            print_to_csv(csv_nfiles, progress, seg_time);   // n = files
            print_to_csv(csv_nwords, index_uniquewords(idx), seg_time);  // n = unique words

            printf("\rIndexing doc # %d", progress);
            fflush(stdout);

            /* get time again to ignore any time spent printing */
            seg_start = gettime();
        }

        relpath = list_next(iter);
        fullpath = concatenate_strings(2, root_dir, relpath);

        words = list_create((cmpfunc_t)strcmp);
        tokenize_file(fullpath, words);

        index_addpath(idx, relpath, words);

        free(fullpath);
        list_destroy(words);
    }

    printf("\nDone indexing %d docs\n", progress);
    printf("Cumulative build time: %.0fms\n", ((float)(cum_time) / 1000));

    init_timed_queries(idx, query_src, argv[4], k_files);

    printf("test_index done -- terminating\n");

    return 0;
}
