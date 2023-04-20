# Rules

query   ::=       andterm  |  andterm "ANDNOT" query
andterm ::=        orterm  |  orterm "AND" andterm
orterm  ::=          term  |  term "OR" orterm
term    ::= "(" query ")"  |  <word>



* Logical connectives must contain exactly one adjacent search word or bracket.
* If more than one search word is to be included, they must be connected with one of the logical connectives.
* Different logical connectives must be separated by brackets.
* If a query contains brackets, there must be an equal amount of closing and opening brackets.
* Each query containing a bracket, must contain exactly one logical connective that is not wrapped by any bracket.
* AND | OR is left/right - neutral. See logical equivalence examples 1.
* ANDNOT is left side inclusive, right side exclusive. e.g. (x ANDNOT y) ==> "x, but not y".
* There may only be __one__ ANDNOT connective within each __individual__ bracket.  
  This rule does not affect search capabilities. See logical equivalence examples 2.

pre_process = '(sand and (Sand OR sAND)) ANDNOT bAND'>

post_process = ['(', 'sand', 'OR', 'and', '(', 'sand', 'OR', 'sand', ')', ')', 'ANDNOT', 'band']

### Logical equivalence examples
=== is defined as logically equivalent.
1. (x OR y) === (y OR x).
2. (x ANDNOT y ANDNOT z) === (x ANDNOT (y OR x)).
<!-- 4. (x ANDNOT y) AND (z ANDNOT b) === (x AND z) ANDNOT ((x AND y) OR (z AND b))  -->

(a ANDNOT (b OR c)) ANDNOT d === a ANDNOT (b OR c OR d)

<!-- ((a AND b) OR (a ANDNOT c)) OR (x ANDNOT y) === my head hurts -->

(a ANDNOT b) AND (x ANDNOT y)
