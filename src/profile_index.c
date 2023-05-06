

/*
 * DISCLAIMER // REFERENCE:

 * Many of the functions within this file contain pieces of code copied
 * from within the provided precode. The functions are typically either
 * slightly or completely modified.
 * 
 * For the sake of simplicity, any function marked with 
 *  `REFERENCE: <function name> @ <src/c file>`
 * may be considered to be copied in its entirety from that source.
 * 
 * No code within this file is taken, copied or based on sources outside of 
 * the precode.
 * 
*/

#include "index.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

const char *F_WORDS = "128";
const char *OUT_DIR = "./prof/";

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

/*
 *  `REFERENCE: <tokenize_file> @ <common.c>`
 */
static list_t *tokenize_queries(const char *path) {
    FILE *f;
    char *c, *word;
    char linebuf[401];

    f = fopen(path, "r");
    if (!f) {
        printf("tokenize_lines: error @ fopen");
        return;
    }

    linebuf[400] = 0;

    list_t *lines = list_create(compare_pointers);
    if (!lines) {
        printf("tokenize_lines: error @ list_create");
    }

    while (!feof(f)) {
        /* scan letters until newline */
        if (fscanf(f, "%400[^\n]", linebuf) != 1) {
            break;
        }

        /* line list */
        list_t *l = list_create(strcmp);

        for (c = linebuf; *c; c++) {
            while (*c == ' ') {
                c++;
            }
            char *wordbuf[101];
            wordbuf[100] = '\0';

            /* add word*/
            for (int i = 0; i < 100; i++) {
                if (isalpha(*c)) {
                    wordbuf[i] = tolower(*c);
                } else if (*c == ' ') {
                    word = strdup(wordbuf);
                    printf("word='%s'\n", word);
                    list_add(l, strdup(wordbuf));
                }
                c++;
                if (!c) {
                    break;
                }
            }
        }
        list_addlast(lines, l);

        char nl = (char)fgetc(f);
        while (nl == '\n') {
            nl = (char)fgetc(f);
        }
    }

    fclose(f);
}

static void print_to_csv(FILE *out, int n, long long unsigned t_time) {
    fprintf(out, "%d, %.0f\n", n, ((float)(t_time) / 1000));
}

static void init_queries(index_t *idx, char *query_src, char *str_n_queries, char *k_files) {
    /* create & open csv doc */
    char *out_path = concatenate_strings(6, OUT_DIR, "query_", str_n_queries, "x", k_files, ".csv");
    FILE *csv_out = fopen(out_path, "w");
    free(out_path);

    const int n_queries = atoi(str_n_queries);

    /* import queries as a 2D list */
    list_t *queries = tokenize_queries(query_src);
    printf("Running %d queries ...\n", n_queries);
}

/*
 * REFERENCE:
 * Most of the code within this function is copied from indexer.c,
 * adding the functionality to create and write to csv files.
 */
int main(int argc, char **argv) {
    char *relpath, *fullpath, *root_dir, *query_src, *k_files;
    unsigned long long cum_time, seg_start, seg_time;
    list_t *files, *words;
    list_iter_t *iter;
    index_t *idx;

    if (argc != 5) {
        printf("usage: indexer <dir> <k_files> <query_src> <n_queries>\n");
        return 1;
    }

    root_dir = argv[1];
    k_files = argv[2];
    query_src = argv[3];

    const int n_files = atoi(k_files) * 1000;

    if (n_files <= 0) {
        printf("invalid file count\n");
        return 1;
    }

    /* Check that root_dir exists and is directory */
    if (!is_valid_directory(root_dir)) {
        printf("invalid root_dir\n");
        return 1;
    }

    printf("\nFinding files at %s \n", root_dir);

    files = find_files(root_dir);
    idx = index_create();
    if (!idx) { 
        printf("Failed to create index\n");
        return 1;
    }

    /* create & open csv for build time */
    char *out_path = concatenate_strings(6, OUT_DIR, "build_", k_files, "x", F_WORDS, ".csv");
    FILE *csv_out = fopen(out_path, "w");
    free(out_path);

    /* build time variables */
    cum_time = 0;
    seg_start = gettime();

    int progress = 0;
    printf("Found %d files in dir, indexing up to %d ...\n", list_size(files), n_files);
    iter = list_createiter(files);

    while (list_hasnext(iter) && (progress < n_files)) {
        if (++progress % 1000 == 0) {
            seg_time = (gettime() - seg_start);
            cum_time += seg_time;

            print_to_csv(csv_out, progress, seg_time);   // n = files
            // print_to_csv(f_words, index_n_words(idx), seg_time);  // n = unique words

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

    printf("\rIndexed %d docs\n", progress);
    printf("\rCumulative time: %.0f\n", ((float)(cum_time) / 1000));

    init_queries(idx, query_src, argv[4], k_files);

    printf("[test_index]: Done, exiting\n");

    return 0;
}
