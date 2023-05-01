''' 
Simple program to automate sending queries through localhost

* * Required modules_
* selenium
* english_words -> https://pypi.org/project/english-words/

* * Required executables:
* chrome
* chrome webdriver
'''
##################### SETTINGS ######################

DISPLAY_WINDOW = True
HOST = "http://localhost:8080/"

SLEEP_INTERVAL = 1
N_QUERIES = 10
BAR_WIDTH = 100     # bar wont work with N_QUERIES above 100 :^)
N_TERMS = 3   # how many chained ´(<word> <op> <word>) <op> ... `

SYNTAX_STRESSTEST = False   # try to make the program segfault through faulty syntax
STRESS_A = int(2)   # minrange of stuff * each query
STRESS_B = int(4)   # maxrange of stuff * each query

#######################################################


import sys
import time
from random import randint as randint
from selenium import webdriver
from selenium.webdriver.chrome.service import Service
from selenium.webdriver.common.by import By
from selenium.webdriver.chrome.options import Options
from english_words import get_english_words_set


service=Service("WebDrivers_path\chromedriver.exe") # copied this from somewhere. works for me but idk
options = Options()

if not DISPLAY_WINDOW:
    options.add_argument('--headless')      # comment out to show the window
    options.add_argument('--disable-gpu')   # comment out to show the window

driver = webdriver.Chrome(options=options, service=service)
driver.get(HOST)


# set up word list
WORDS = list(get_english_words_set(['web2'], True, True))
WORDS_LEN = int(len(WORDS) - 1)
print(f'n. words in set: {WORDS_LEN}')

# define operators
OPERATORS = ["AND", "ANDNOT", "OR"]
OPERATORS_LEN = int(len(OPERATORS) - 1)

# super sophisticated progress bar
BAR_PER_QUERY = '#' * int(BAR_WIDTH / N_QUERIES)


def word():
    return WORDS[randint(0, WORDS_LEN)]

def operator():
    return OPERATORS[randint(0, OPERATORS_LEN)]

def term():
    return f'({word()} {operator()} {word()})'


# submit queries
for _ in range(N_QUERIES):
    sys.stdout.write(f'{BAR_PER_QUERY}')
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

    else:
        for j in range(0, N_TERMS - 1):
            query += f' ({term()} {operator()} {word()}) '
            query += f' {operator()} '
        query += f' {word()} '


    search_box.send_keys(query)
    search_box.submit()
    search_box = driver.find_element(By.NAME, "query")
    search_box.clear()
    time.sleep(SLEEP_INTERVAL)


print("\nDone")
driver.quit()
