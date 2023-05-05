''' 
Simple program to generate queries to a textfile
uses the format `(a AND b) ANDNOT (x OR c)` for all generated queries

'''

import sys
from random import randint
from english_words import get_english_words_set


# import set of english dictionary words as a list
# WORDS = list(get_english_words_set(['web2'], True, True))
WORDS = list(get_english_words_set(['gcide'], True, True))
WORDS_LEN = int(len(WORDS) - 1)

def word():
    return WORDS[randint(0, WORDS_LEN)]

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("usage: gen_queries.py <outfile> <n_queries> <query_len>")
        exit()

    outfile = sys.argv[1]
    n_queries = int(sys.argv[2])
    query_len = int(sys.argv[3])

    with open(f'./{outfile}', 'a+') as out:
        for _ in range(n_queries):
            txt = f'({word()} AND {word()}) ANDNOT ({word()} OR {word()})'
            for _ in range(query_len-1):
                txt += f' OR (({word()} AND {word()}) ANDNOT ({word()} OR {word()}))'

            out.write(f'{txt}\n')
        out.close()

