''' 
Simple program to automate sending queries through localhost

Required modules:
* psutil
* selenium
* english_words -> pip install english_words, docs: https://pypi.org/project/english-words/

Required executables:
* chrome
* chrome webdriver
'''


##################### SETTINGS ######################
DISPLAY_WINDOW = True   # keep chromedriver hidden or not
HOST = "http://localhost:8080/"

SLEEP_INTERVAL = 0.1    # interval between queries
N_QUERIES = 10     # queries to post before quitting app
N_TERMS = 2         # how many chained ´(<word> <op> <word>) <op> ... `
SIMPLE_TERM = True  # single word queries

SYNTAX_STRESSTEST = False   # try to make the program segfault through faulty syntax
STRESS_A = int(2)   # min amount of invalid stuff per stressquery
STRESS_B = int(5)   # max amount of invalid stuff per stressquery
#######################################################


import sys
import time
import psutil
from random import randint
from selenium import webdriver
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options
from english_words import get_english_words_set


WORDS = list(get_english_words_set(['gcide'], True, True))
# gcide => Words found in GNU Collaborative International Dictionary of English 0.53
# https://pypi.org/project/english-words/

WORDS_LEN = int(len(WORDS) - 1)

def word():
    return WORDS[randint(0, WORDS_LEN)]

def operator():
    match (randint(0, 2)):
        case 0:
            return "OR"
        case 1:
            return "AND"
        case 2:
            return "ANDNOT"

def term():
    return f'({word()} {operator()} {word()})'

if __name__ == '__main__':
    print(f'run_querys.py: running random queries from set of {WORDS_LEN} words')

    service=Service("WebDrivers_path\chromedriver.exe")  # auto find chromedriver
    options = Options()

    if not DISPLAY_WINDOW:
        options.add_argument('--headless')
        options.add_argument('--disable-gpu')

    # init the chromedriver
    driver = webdriver.Chrome(options=options, service=service)
    driver.get(HOST)

    for n in range(N_QUERIES):
        sys.stdout.write(f'\r{n} / {N_QUERIES-1}')
        sys.stdout.flush()

        search_box = driver.find_element(By.NAME, "query")
        query = ''

        if (SYNTAX_STRESSTEST):
            # invalid queries to test syntax control
            query += f' {term()} {operator()} '
            for i in range(randint(STRESS_A, STRESS_B)):
                match randint(0, 3):
                    case 0:
                        query += f' {operator()} '
                    case 1:
                        query += f' ( {term()} ) {operator()} ( '
                    case 2:
                        query += f' ( {term()} {operator()} ( {word()} ) '
                    case 3:
                        query += f' {operator()} ({term()} {operator()} {word()}) {operator()} {term()} {operator()} '
            query += f' {term()} '
        elif (SIMPLE_TERM):
            query += f'{term()}'
        else:
            query += f'{word()} {operator()}'
            for j in range(0, N_TERMS - 1):
                query += f' ({term()} {operator()} {word()}) {operator()} '
            query += f' {word()} '

        # send, submit, sleep
        search_box.send_keys(query)
        search_box.submit()
        search_box = driver.find_element(By.NAME, "query")
        search_box.clear()
        time.sleep(SLEEP_INTERVAL)

    driver.quit()	# terminate the chromedriver

