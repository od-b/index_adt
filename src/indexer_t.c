#include "index.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

const char *F_WORDS = "512";
const char *OUT_DIR = "./prof/";

/*
 * Stripped indexer.c for testing build times.
 * Doesn't clean up index or file list but os will
 * reclaim it on exit anyhow.
 * 
 * No code within this file should be considered original work.
 * Refer to indexer.c for authors.
 * 
*/

static void print_to_csv(FILE *f, int n_indexed_files, long long unsigned t_time) {
    /* n = files indexed, time spent @ last 1000 */
    fprintf(f, "%d, %.0f\n", n_indexed_files, ((float)(t_time) / 1000));
}

int main(int argc, char **argv) {
    char *relpath, *fullpath, *root_dir;
    list_t *files, *words;
    list_iter_t *iter;
    index_t *idx;

    if (argc != 3) {
        printf("usage: indexer <dir> <n_files>\n");
        return 1;
    }

    root_dir = argv[1];
    const int n_files = atoi(argv[2]);

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
    if (idx == NULL) { 
        printf("Failed to create index\n");
        return 1;
    }

    iter = list_createiter(files);

    int progress = 0;
    printf("Found %d files in dir, indexing up to %d\n", list_size(files), n_files);

    char *fname = concatenate_strings(5, OUT_DIR, argv[2], "x", F_WORDS, ".csv");
    FILE *f = fopen(fname, "w");
    free(fname);

    unsigned long long cum_time = 0;
    unsigned long long time_a = gettime();
    unsigned long long time_z;

    while (list_hasnext(iter) && (++progress < n_files)) {
        if (progress % 1000 == 0) {
            time_z = (gettime() - time_a);
            print_to_csv(f, progress, time_z);

            printf("\rIndexing doc # %d", progress);
            fflush(stdout);
            cum_time += time_z;
            time_a = gettime();
        }

        relpath = list_next(iter);
        fullpath = concatenate_strings(2, root_dir, relpath);

        words = list_create((cmpfunc_t)strcmp);
        tokenize_file(fullpath, words);

        index_addpath(idx, relpath, words);

        free(fullpath);
        list_destroy(words);
    }

    printf("\rIndexed %d docs, quitting\n", progress);
    printf("\rCumulative time: %.0f\n", ((float)(cum_time) / 1000));

    return 0;
}
