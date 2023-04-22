/* definitions to allow referencing without vscode going mad */
struct set; typedef struct set set_t;
struct list; typedef struct list list_t;
struct map; typedef struct map map_t;
struct term; typedef struct term term_t;


/* trying to understand the grammar by imagining the nonterminals/productions as structs */

/* # Parser

* Should the parser be an integral component of the index, or a separate entity?

### Syntax tree
     ANDNOT
    /      \
  AND       OR
 /   \     /   \ 
a     b   c     d

-> "(a AND b) ANDNOT (c OR d)"
---
     <and>
    /      \
  AND       OR
 /   \     /   \ 
a     b   c     d

### BNF Grammar:
  <query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
  <andterm> ::=  <orterm> | <orterm> "AND" <andterm>
  <orterm>  ::=  <term> | <term> "OR" <orterm>
  <term>    ::=  "(" <query> ")" | <word>
*/

struct query;
typedef struct query query_t;

struct and_term;
typedef struct and_term and_term_t;

struct or_term;
typedef struct or_term or_term_t;

struct term;
typedef struct term term_t;

typedef struct query {
    struct and_term* andterm;   // first andterm in the query
    struct query* query;        // next query after "ANDNOT", NULL if not present
} query_t;

typedef struct and_term {
    struct or_term* orterm;     // first orterm in the andterm
    struct and_term* andterm;   // next andterm after "AND", NULL if not present
} and_term_t;

typedef struct or_term {
    struct term* term;          // first term in the orterm
    struct or_term* or_term;    // next orterm after "OR", NULL if not present
} or_term_t;

typedef struct term {
    struct query* query;        // query within a parentheses
    char* word;                 // The actual word, NULL if a query within a parentheses
} term_t;


/* #### Interpretation:
  * All operators ["ANDNOT", "AND", "OR"], must have adjacent words or subqueries.
  * All subqueries must be contained within parantheses.
  * The operators "OR" and "AND" may be 'chained' without parantheses, E.g. (cat AND dog AND horse),
    while the "ANDNOT" operator may not.
  * In the event of using more than one type of operator for a search, the different operators must be 
    separated through the use of parantheses, in order to specify the desired order of evaluation.
    E.g. `cat AND dog OR rat AND horse OR mouse` is not a valid query, and should instead be:
    `(cat AND dog) OR (rat AND horse) OR mouse`, or
    `(cat AND (dog OR rat)) AND (horse OR mouse)`, etc.
*/