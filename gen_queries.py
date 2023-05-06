''' 
Simple program to generate queries to a textfile
uses the format `(a AND b) ANDNOT (x OR c)` for all generated queries

'''

import sys
from random import randint
from english_words import get_english_words_set


def operator():
    match (randint(0, 2)):
        case 0:
            return "OR"
        case 1:
            return "AND"
        case 2:
            return "ANDNOT"

def word():
    match (randint(0, 1)):
        case 0:
            return WORDS[randint(0, WORDS_LEN)]
        case 1:
            return str(randint(0, NUM_RANGE))



if __name__ == '__main__':
    WORDS = list(get_english_words_set(['gcide'], True, True))
    # gcide => Words found in GNU Collaborative International Dictionary of English 0.53
    # https://pypi.org/project/english-words/

    WORDS_LEN = int(len(WORDS) - 1)
    NUM_RANGE = int(300000)  # range of random integers inserted

    if len(sys.argv) < 4:
        print("usage: gen_queries.py <outfile> <n_queries> <query_maxlen>")
        exit()

    outfile = sys.argv[1]
    n_queries = int(sys.argv[2])
    query_maxlen = int(sys.argv[3])

    maxlen = 0

    with open(f'./{outfile}', 'w') as out:
        for _ in range(n_queries):
            txt = f'{word()}'
            for _ in range(randint(0, query_maxlen-1)):
                txt += f' {operator()} {word()}'

            l = len(txt)
            if (l > maxlen):
                maxlen = l

            out.write(f'{txt}\n')
        out.close()

    print(f'char size of longest query = {maxlen+2}')
    print(f'NOTE: generation typically has ≈ 1 error per 5000 queries')
