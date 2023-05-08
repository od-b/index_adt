#ifndef QUERYPARSER_H
#define QUERYPARSER_H

#include "list.h"
#include "set.h"


struct parser;
/* Type of parser */
typedef struct parser parser_t;

/* Enumerated response codes */
typedef enum parser_status {
    SKIP_PARSE,
    PARSE_READY,
    ALLOC_FAILED,
    SYNTAX_ERROR,
} parser_status_t;


/*
 * Type of term function
 * Takes in two pointers. The first being the parent/handler,
 * and the second being the term to be produced.
 * Returns a pointer to the result of the term.
 */
typedef set_t *(*term_func_t)(void *, char *);

/*
 * Creates and returns a newly created parser.
 * The parser will use the given search_func to determine results.
 * 'parent' will be stored and passed as the first parameter of the 
 * given search_func when called.
 */
parser_t *parser_create(void *parent, term_func_t term_func);

/*
 * Destroys the given parser.
 */
void parser_destroy(parser_t *parser);

/*
 * Initializes the parser using the given list of tokens.
 * if PARSE_READY is returned, parser_get_result may be called.
 * Returns a status code ~ parser_status_t.
 */
parser_status_t parser_scan(parser_t *parser, list_t *tokens);

/*
 * Returns the result scanned tokens given that PARSE_READY was returned
 * Returns a newly created set containing any results, or NULL on error.
 */
set_t *parser_get_result(parser_t *parser);

/*
 * Returns the last set error message from scanning.
 */
char *parser_get_errmsg(parser_t *parser);


#endif
