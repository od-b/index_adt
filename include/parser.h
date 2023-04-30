#ifndef PARSER_H
#define PARSER_H

// #include "common.h"
#include "list.h"
#include "set.h"

/* 
 * Enumerated status codes:
 * SKIP_PARSE
 * PARSE_READY
 * ALLOC_FAILED
 * SYNTAX_ERROR
 */
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
 * type of search func used by the parser 
 * takes in a void and char pointer, returns a set.
 */
typedef set_t *(*search_func_t)(void *, char *);

/*
 * Creates and returns a new parser.
 * the parser will use the given search_func to determine results.
 * 'parent' will be stored and passed as the first parameter of the 
 * given search_func when called internally.
 */
parser_t *parser_create(void *parent, search_func_t search_func);

/*
 * Destroys the given parser
 */
void parser_destroy(parser_t *parser);

/*
 * Identifies tokens and checks for grammar errors. 
 * Returns a status code parser_status_t
 */
parser_status_t parser_scan_tokens(parser_t *parser, list_t *tokens);

/*
 * returns the current error message of the parser
 */
char *parser_get_errmsg(parser_t *parser);

/*
 * initializises the parsing process.
 * will return NULL unless parser_scan_tokens is called prior to this.
 * Returns a newly created set containing any results. The set may be empty.
 */
set_t *parser_get_result(parser_t *parser);


#endif
