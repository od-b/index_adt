''' generates files containing random english words '''

##################### SETTINGS ######################
OUTDIR = "./data/gcide/"
N_FILES = 3
N_WORDS = 512
WORDS_PER_LINE = 8
#######################################################

from random import randint
from english_words import get_english_words_set

WORDS = list(get_english_words_set(['gcide'], True, True))
# gcide => Words found in GNU Collaborative International Dictionary of English 0.53
# https://pypi.org/project/english-words/

WORDS_LEN = int(len(WORDS) - 1)
N_LINES = int(N_WORDS / WORDS_PER_LINE)

if __name__ == '__main__':
    for i in range(N_FILES):
        with open(f'{OUTDIR}gcide_{i}.html', 'w') as F:
            txt = '<html> \n<pre> \n\n'

            for _ in range(N_LINES):
                for _ in range(WORDS_PER_LINE):
                    txt += f'{WORDS[randint(0, WORDS_LEN)]} '
                txt += '\n'

            F.write(txt + '\n</pre> \n</html> \n')
            F.close()

