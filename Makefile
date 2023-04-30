## Authors: 
## Steffen Viken Valvaag <steffenv@cs.uit.no>
## Morten Grønnesby <morten.gronnesby@uit.no>

# Common sources to build both binaries
# You can change these values to test with different implementations (for example chained hashmap vs open addressing) 
LIST_SRC=linkedlist.c
MAP_SRC=hashmap.c
SET_SRC=aatreeset.c
INDEX_SRC=index_tree.c
PARSER_SRC=index_parser.c pile.c

# Directories
INCLUDE_DIR=include
SRC_DIR=src

# Binary targets
INDEXER=indexer
ASSERT_INDEX=assert_index

# Target source files
INDEXER_SRC=indexer.c common.c httpd.c $(LIST_SRC) $(MAP_SRC) $(SET_SRC) $(INDEX_SRC) $(PARSER_SRC)
ASSERT_SRC=assert_index.c common.c $(LIST_SRC) $(MAP_SRC) $(SET_SRC) $(INDEX_SRC) $(PARSER_SRC)

# Prefix the files with the src folder
INDEXER_SRC := $(patsubst %.c, $(SRC_DIR)/%.c, $(INDEXER_SRC))
ASSERT_SRC := $(patsubst %.c, $(SRC_DIR)/%.c, $(ASSERT_SRC))

# Find all header files
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)


FLAGS = -g -Wall -DDEBUG -DERROR_FATAL -DLINE_PRINT
# FLAGS = -O2 -g

.PHONY=all

all: $(INDEXER)

$(INDEXER): $(INDEXER_SRC) $(HEADERS) Makefile
	gcc -Wall -o $@ -D_GNU_SOURCE -D_REENTRANT $(INDEXER_SRC) -I$(INCLUDE_DIR) -lm -lpthread $(FLAGS)

$(ASSERT_INDEX): $(ASSERT_SRC) $(HEADERS) Makefile
	gcc -o $@ $(ASSERT_SRC) -I$(INCLUDE_DIR) -lm $(FLAGS)

clean:
	rm -f *~ *.o *.exe *.out *.prof *.stackdump $(INDEXER) $(ASSERT_INDEX)
	rm -rf *.dSYM
