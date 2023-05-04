''' 
  Simple program to automate sending queries through localhost

Required modules:
* selenium
* english_words -> pip install english_words, docs: https://pypi.org/project/english-words/

Required executables:
* chrome
* chrome webdriver
'''


##################### SETTINGS ######################

DISPLAY_WINDOW = True
HOST = "http://localhost:8080/"

SLEEP_INTERVAL = 0.1
N_QUERIES = 100
N_TERMS = 1   # how many chained ´(<word> <op> <word>) <op> ... `
SIMPLE_TERM = True

SYNTAX_STRESSTEST = False   # try to make the program segfault through faulty syntax
STRESS_A = int(2)   # minrange of stuff * each query
STRESS_B = int(4)   # maxrange of stuff * each query

#######################################################


import sys
import time
import psutil
from random import randint as randint
from selenium import webdriver
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options
from english_words import get_english_words_set


service=Service("WebDrivers_path\chromedriver.exe")  # auto find chromedriver
options = Options()

if not DISPLAY_WINDOW:
    options.add_argument('--headless')
    options.add_argument('--disable-gpu')

driver = webdriver.Chrome(options=options, service=service)
driver.get(HOST)


# set up word list
WORDS = list(get_english_words_set(['web2'], True, True))
WORDS_LEN = int(len(WORDS) - 1)

# define operators
OPERATORS = ["AND", "ANDNOT", "OR"]
OPERATORS_LEN = int(len(OPERATORS) - 1)


def word():
    return WORDS[randint(0, WORDS_LEN)]

def operator():
    return OPERATORS[randint(0, OPERATORS_LEN)]

def term():
    return f'({word()} {operator()} {word()})'


print(f'run_querys.py: running random queries from set of {WORDS_LEN} words')

for n in range(N_QUERIES):
    sys.stdout.write(f'\r{n} / {N_QUERIES-1}')
    sys.stdout.flush()

    search_box = driver.find_element(By.NAME, "query")
    query = ''
    if (SYNTAX_STRESSTEST):
        query += f' {term()} '
        for i in range(randint(STRESS_A, STRESS_B)):
            match randint(0, 3):
                case 0:
                    query += f' {operator()} '
                case 1:
                    query += f' ( {term()} ) {operator()} ( '
                case 2:
                    query += f' ( {term()} {operator()} ( {term()} ) '
                case 3:
                    query += f' {operator()} ({term()} {operator()} {term()}) {operator()} {term()} {operator()} '
        query += f' {term()} '
    elif (SIMPLE_TERM):
        query += f'{term()}'
    elif (N_TERMS == 1):
        query += f'{term()} {operator()} {word()}'
    else:
        for j in range(0, N_TERMS - 1):
            query += f' ({term()} {operator()} {word()}) {operator()} '
        query += f' {word()} '


    search_box.send_keys(query)
    search_box.submit()
    search_box = driver.find_element(By.NAME, "query")
    search_box.clear()
    time.sleep(SLEEP_INTERVAL)

driver.quit()

print("\nDone, terminating indexer")

for proc in psutil.process_iter():
    if proc.name() == "indexer":
        proc.terminate()

