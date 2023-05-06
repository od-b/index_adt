''' simple program to create html files containing random-ish english words 

    to simulate a variance in number of duplicate words, random numbers are inserted.
    the range of numbers will gradually increase by x * No. generated files.
    e.g. the first file will only contain 0 as numbers, the second 0 | 1 | 2, asf.
    same number range for all N_FILES, see the 'MULTI' constant.
'''

##################### SETTINGS ######################
OUTDIR = "./generated/"
FNAMES = "gcide_" #+{file no}.html
N_FILES = int(20)
N_WORDS = int(64)       # words&integers per file, 50/50 split
LINE_WIDTH = int(8)     # words per line
MAX_FILES = 800000      # defined max upper bound
#######################################################

from random import randint
from english_words import get_english_words_set

WORDS = list(get_english_words_set(['gcide'], alpha=False, lower=False))
# gcide => Words found in GNU Collaborative International Dictionary of English 0.53
# https://pypi.org/project/english-words/

WORDS_LEN = int(len(WORDS) - 1)
N_LINES = int(N_WORDS / LINE_WIDTH)
L_BEGIN = str('<html> \n<pre> \n\n')
L_END = str('\n</pre> \n</html> \n')
PREFIX = str(OUTDIR+FNAMES) # +{file no}.html
MULTI = int((2 * MAX_FILES) / N_FILES)

def main():
    for i in range(N_FILES):
        with open(f'{PREFIX}{i}.html', 'w') as F:
            txt = L_BEGIN
            for _ in range(N_LINES):
                for _ in range(LINE_WIDTH):
                    txt += f'{WORDS[randint(0, WORDS_LEN)]} '
                txt += '\n'
                for _ in range(LINE_WIDTH):
                    txt += f'{randint(0, i*MULTI)} '
                txt += '\n'

            F.write(txt + L_END)
            F.close()


if __name__ == '__main__':
    print(f'{WORDS_LEN} words in set, creating {N_FILES} files @ {OUTDIR}')
    main()
    print("done")

