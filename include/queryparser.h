#ifndef QUERYPARSER_H
#define QUERYPARSER_H

#include "list.h"
#include "set.h"

/* Enumerated response codes */
typedef enum parser_status {
    SKIP_PARSE,
    PARSE_READY,
    ALLOC_FAILED,
    SYNTAX_ERROR,
} parser_status_t;


struct parser;
/* Type of query parser */
typedef struct parser parser_t;

/*
 * Type of search_func, function pointer/reference.
 * Such a func takes in a void and char pointer, and returns a set.
 */
typedef set_t *(*search_func_t)(void *, char *);

/*
 * Creates and returns a new parser.
 * The parser will use the given search_func to determine results.
 * 'parent' will be stored and passed as the first parameter of the 
 * given search_func, when called internally.
 */
parser_t *parser_create(void *parent, search_func_t search_func);

/*
 * Destroys the given parser.
 */
void parser_destroy(parser_t *parser);

/*
 * Scans / Identifies tokens and checks for grammar errors. 
 * Returns a status code parser_status_t.
 */
parser_status_t parser_scan(parser_t *parser, list_t *tokens);

/*
 * Returns the current error message of the parser
 */
char *parser_get_errmsg(parser_t *parser);

/*
 * Initializises the parsing process.
 * Will return NULL unless parser_scan_tokens is called prior to this.
 * Returns a newly created set containing any results. The set may be empty.
 */
set_t *parser_get_result(parser_t *parser);


#endif
