#include "index.h"
#include "common.h"
#include "printing.h"
#include "set.h"
#include "map.h"

#include <string.h>     /* strcmp, etc */

/*
 * to verify a method to skip word list duplicates through sort + cmpfunc,
 * should check if this is faster than _contains functions or making a new set.
 * 
 * Status: Tested and working with assert_index and all data in data/cacm/
 * 
 * Possible disadvantages: 
 *  list needs to be sorted
 *  list is added in sorted order, could put strain on tree rotation
 * 
 * Conclusion:
 *  working in terms of logic. extensive testing needed.
 * 
 */
static void skip_list_dups(char *path, list_t *words) {
    list_sort(words);

    /* sort skip test start */

    list_iter_t *iter_ = list_createiter(words);
    set_t *all_words_ = set_create((cmpfunc_t)strcmp);

    char *word_;
    while ((word_ = list_next(iter_)) != NULL) {
        char *word_cpy = copy_string(word_);
        set_add(all_words_, word_cpy);
    }
    list_destroyiter(iter_);

    set_iter_t *all_words_iter_ = set_createiter(all_words_);

    printf("CORRECT SET OF WORDS: \n");
    while ((word_ = set_next(all_words_iter_)) != NULL) {
        printf("%s, ", word_);
    }

    int expected_ = set_size(all_words_);
    int found_ = 0;

    printf("\nFOUND SET OF WORDS: \n");


    /* ACTUAL METHOD TO TEST: */
    list_iter_t *words_iter = list_createiter(words);

    char *curr_word = list_next(words_iter);
    char *prev_word;

    while (curr_word != NULL) {
        /* -- loop content start -- */

        printf("%s, ", curr_word);
        found_++;

        /* -- loop content end -- */

        /* update prev word, increment curr word */
        prev_word = curr_word;
        curr_word = list_next(words_iter);

        /* while current word is equal to the previous word, skip words */
        while ((curr_word != NULL) && (strcmp(prev_word, curr_word) == 0)) {
            prev_word = curr_word;
            curr_word = list_next(words_iter);
        }
    }

    if (found_ != expected_) {
        ERROR_PRINT("\nadded %d elems. Expected %d\n", found_, expected_);
    }
    printf("\n\n");

    /* cleanup */
    list_destroyiter(words_iter);
}
