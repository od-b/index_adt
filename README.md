
# General
The program intends to comply with C99 standards.
Tested on:
* GNU/Linux Ubuntu 22.04, AMD x86_64
* Mac OS 13.2.1, ARM64


## Index implementations / variants
* index_aa: Uses the treeset provided within the precode for indexed words
* index_rb: Uses a red-black binary search tree for indexed words
* index_aa_var: Similar to index_aa in many ways. Refactored structure for storing term frequency. Does not utilize a 'document' struct. Alternative take on result formatting.


## queryparser.c
Implementation of a token scanner & parser for a given index ADT.
Used by all implemented index variants.
The parser supports OR, AND, ANDNOT operations, 
and allows specifying the desired order of evaluation through parentheses.
In the event of chained operator/term sequences without parentheses,
the parser defaults to a left-->right evaluation order

Will work with any index ADT, given that it can provide a function pointer which takes
in a void pointer (index) and search term, then return a set which the parser may
perform operations on (search_func_t).
The content of its sets are not of relevance to the parser, but all provided sets must share cmpfunc. 
In the event a search term has no result, a NULL pointer should instead be returned by the index. 
The parser will not mutated or destroy any sets given to it by the parser.

Application: 
1. Scan tokens, check status response.
2. a: Given the heads up, parser_get_result may be called in order to complete the parsing.
2. b: Given a syntax error response, parser_get_errmsg will provide reason in plaintext.


## Testing & Utility
* time_index: build through `make time_index`. 
  Usage: `time_index` `dir` `k_files` `query_src` `k_queries`
  Bit of a lazy approach, but The OUT_DIR constant at the top of the source file must be set prior to compilation.

* assertive_queryparser.c
  Asserts and prints during the parsing process. 
  Causes memory leaks and is mainly intended to visualize the parsing process.


# Precode Changes:
  A binary search tree is inheritly an ordered set, and this makes it an excellent structure for a set ADT.
  When attempting to add an item to a tree, the tree will have to traverse to the node where the item is to be added, aborting the process of creating a new node if an equal item already exists. 
  This process consists of two main components, traversion and insertion operations. This effectively eliminates the need to call a search function prior to the attempt of adding an element. As such, if element `x` is to be added to the set, the procedure `if set does not contain x, add x` can simply be `add x`, and the implementation should; and does, e.g. aatreeset, ignore duplicates. Doing so, the tree only has to be traversed once.

  **Opinions on why `set_add` not providing a return value is a design flaw**
  For one, there is no way for the caller to identify a potential memory allocation error.

  Secondly, using a short example. Say we have a struct A, defined as:
  struct A = { int a, list *b };

  We want to store a set of A. Creating the set, we provide a comparison function which compares on A->a.
  Seeing as set_add() does not return __any__ value, and set_contains() returns a boolean as opposed to (NULL | *elem) the following problem arises:
  We have no way to access b unless A is stored in an additional data structure outside of the set, effectively rendering the set redundant.
  This is not a technical limitation, but one of a header limiting the use case of its implementation.
  While this could be solved by implementing an additional header for sets, e.g., `set++.h`, the approach was rather taken to expand on the existing set header, as the changes needed to allow for far more flexible sets are extremely minor.

  Prior to development, a decision had to be made to allow for the desired implementation - either alter set_contains & set_add, create a separate set header, or expand the header. Neither option is particularly attractive, however - it is the belief of the author that expanding the defined header is the least intrusive option.

  *The set header was expanded wit the following functions*
  * set_tryadd():
    Add an element. If succesful, returns value will be the given elem.
    If elem is a duplicate, returns the duplicate elem.
    Note: ideally, set_add() could simply be altered to prove a return value.

  * set_get()
    Get an existing element from the set.
    Returns NULL if set does not contain the element.

