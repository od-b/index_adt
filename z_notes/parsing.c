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

### BNF Grammar:
  <query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
  <andterm> ::=  <orterm> | <orterm> "AND" <andterm>
  <orterm>  ::=  <term> | <term> "OR" <orterm>
  <term>    ::=  "(" <query> ")" | <word>
*/

/* #### Interpretation:
  * All operators ["ANDNOT", "AND", "OR"], must have adjacent terms.

  * The operators "OR" and "AND" may be 'chained' without parantheses, 
    e.g. `(a AND b AND c AND d) OR (x OR y OR z)` is a valid query.

  * The "ANDNOT" operator may not be chained without the use of parantheses,
    e.g. `x ANDNOT y ANDNOT z` would be invalid.
    In the event the above point is wrongly interpreted, the same result could be acquired by the query `x ANDNOT (y OR z)`.

  * If using more than one type of operator within a query, the different operators must be separated through the use 
    of parantheses, in order to specify the desired order of evaluation.
    E.g. `cat AND dog OR rat AND horse OR mouse` is not a valid query, and should instead be:
    `(cat AND dog) OR (rat AND horse) OR mouse`, or
    `(cat AND (dog OR rat)) AND (horse OR mouse)`, etc.
*/