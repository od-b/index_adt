/* README - queryparser.c
 *
 * Implementation of a token scanner & parser for a given index ADT.
 * The parser supports OR, AND, ANDNOT operations, 
 * and allows specifying the desired order of evaluation through parentheses.
 * In the event of chained operator/term sequences without parentheses,
 * the parser defaults to a left-->right evaluation order
 * 
 * Will work with any index ADT, given that it can provide a function pointer which takes
 * in a void pointer (index) and search term, then return a set which the parser may
 * perform operations on (search_func_t).
 * The content of its sets are not of relevance to the parser, but all provided sets must share cmpfunc. 
 * In the event a search term has no result, a NULL pointer should instead be returned by the index. 
 * The parser will not mutated or destroy any sets given to it by the parser.
 * 
 * Usage: 
 * Scan tokens, check status response.
 * If given the heads up, parser_get_result may be called in order to complete the parsing.
 * If given a syntax error response, parser_get_errmsg will provide reason in plaintext.
 * 
*/
