# Rules

query   ::= andterm | andterm "ANDNOT" query
andterm ::= orterm | orterm "AND" andterm
orterm  ::= term | term "OR" orterm
term    ::= "(" query ")" | <\word>

1. Logical connectives must contain exactly one adjacent search word or bracket.
2. If more than one search word is to be included, they must be connected with one of the logical connectives.
3. Different logical connectives must be separated by brackets.
4. If a query contains brackets, there must be an equal amount of closing and opening brackets.
5. Each query containing a bracket, must include a logical connective which itself is not wrapped by a bracket.
6. ANDNOT is left side inclusive, right side exclusive. (e.g.; x ANDNOT y ==> x, but not y)
7. AND | OR is left/right-side neutral. (e.g.; x OR y === y OR x)
