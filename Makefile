## Authors: 
## Steffen Viken Valvaag <steffenv@cs.uit.no>
## Morten Grønnesby <morten.gronnesby@uit.no>

LIST_SRC=linkedlist.c
MAP_SRC=hashmap.c
SET_SRC=aatreeset.c
INDEX_SRC=index_tree.c
PARSER_SRC=queryparser.c pile.c
# PARSER_SRC=assertive_queryparser.c pile.c

# Directories
INCLUDE_DIR=include
SRC_DIR=src

# Binary targets
# INDEXER=indexer
INDEXER=indexer_t
ASSERT_INDEX=assert_index

# Target source files
INDEXER_SRC=${INDEXER}.c common.c httpd.c $(LIST_SRC) $(MAP_SRC) $(SET_SRC) $(INDEX_SRC) $(PARSER_SRC)
ASSERT_SRC=${ASSERT_INDEX}.c common.c $(LIST_SRC) $(MAP_SRC) $(SET_SRC) $(INDEX_SRC) $(PARSER_SRC)

# Prefix the files with the src folder
INDEXER_SRC := $(patsubst %.c, $(SRC_DIR)/%.c, $(INDEXER_SRC))
ASSERT_SRC := $(patsubst %.c, $(SRC_DIR)/%.c, $(ASSERT_SRC))

# Find all header files
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

# FLAGS = -lm -g -Wall -DDEBUG -DERROR_FATAL -DLINE_PRINT
# FLAGS = -lm -O2 -Wno-unused-result -g -pg
FLAGS = -lm -O2 -Wno-unused-result
.PHONY=all

all: $(INDEXER)

$(INDEXER): $(INDEXER_SRC) $(HEADERS) Makefile
	gcc -o $@ -D_GNU_SOURCE -D_REENTRANT $(INDEXER_SRC) -I$(INCLUDE_DIR) -lpthread $(FLAGS)

$(ASSERT_INDEX): $(ASSERT_SRC) $(HEADERS) Makefile
	gcc -o $@ $(ASSERT_SRC) -I$(INCLUDE_DIR) $(FLAGS)

clean:
	rm -f *~ *.o *.exe *.out *.prof *.stackdump $(INDEXER) $(ASSERT_INDEX)
	rm -rf *.dSYM
