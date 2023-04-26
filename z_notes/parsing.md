
* Should the parser be an integral component of the index, or a separate entity?

## BNF Grammar
<query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
<andterm> ::=  <orterm> | <orterm> "AND" <andterm>
<orterm>  ::=  <term> | <term> "OR" <orterm>
<term>    ::=  "(" <query> ")" | <word>

"ANDNOT" : difference   => <term> \ <term>
"OR"     : union        => <term> ∪ <term>
"AND"    : intersection => <term> ∩ <term>


## Interpretation
* All operators `ANDNOT | AND | OR` must have adjacent terms.

* Operators of the same type may be used to 'chain' queries without the use of parantheses.
  Example: `(a AND b AND c AND d) OR (x OR y OR z)` is a valid query; while `a AND b AND c OR d` is not,
  as there would be no way to determine whether `a AND b AND (c OR d)` or `(a AND b AND c) OR d` is the intended evaluation.

* If using more than one type of operator within a (nested) query, the different operators must be separated through the use 
  of parantheses, in order to specify the desired order of evaluation.
  Example. `cat AND dog OR rat AND horse OR mouse` is not a valid query, and should instead be:
  `(cat AND dog) OR (rat AND horse) OR mouse`, or `(cat AND (dog OR rat)) AND (horse OR mouse)`, etc.

### Order of evaluation
1. expressions within parentheses.
2. ANDNOT.
3. AND.
4. OR.
5. Chained operators at equal depth: evaluate from left to right.


## Notes, Examples
* `a OR (b OR (c AND d)) OR (e AND f)` is valid
* `a OR ((b OR (c AND d)) AND e AND f)` is valid


#### Logical equivalence on valid queries:  
Let `=` be defined as logically equivalent.  

"ANDNOT" : difference   => <term> \ <term>
"OR"     : union        => <term> ∪ <term>
"AND"    : intersection => <term> ∩ <term>

query := `a ANDNOT (a ANDNOT b)`


Sure, here are the requested actions for each of the given examples:

1.`a OR b AND c OR d ANDNOT e OR f AND g AND h`
  The correct order of evaluation according to the BNF is: 
  1) Evaluate the ANDNOT expression first, 
  2) Then evaluate any AND expressions, 
  3) Finally, evaluate any OR expressions.

  Applying parentheses according to the order of evaluation, the query becomes:
  `(a OR ((b AND c) OR ((d ANDNOT e) OR (f AND g AND h))))`

2.`a OR b AND c ANDNOT(d OR e) OR f AND g AND h`
  The correct order of evaluation according to the BNF is:
  1) Evaluate any ANDNOT expressions first,
  2) Then evaluate any AND expressions,
  3) Finally, evaluate any OR expressions.

  Applying parentheses according to the order of evaluation, the query becomes:
  `(a OR (((b AND c) ANDNOT (d OR e)) OR (f AND g AND h)))`

3.`(a AND b OR c) ANDNOT(d OR e AND f OR g) OR h`
  The correct order of evaluation according to the BNF is:
  1) Evaluate any parentheses first,
  2) Then evaluate any OR expressions,
  3) Finally, evaluate any ANDNOT expressions and then any OR expressions.

  The query is already fully parenthesized, so no changes are needed.

4.`a OR (b ANDNOT c AND d) OR e AND f OR (g AND h)`
  The correct order of evaluation according to the BNF is:
  1) Evaluate any parentheses first,
  2) Then evaluate any ANDNOT expressions,
  3) Then evaluate any AND expressions, 
  4) Finally, evaluate any OR expressions.

  Applying parentheses according to the order of evaluation, the query becomes:
  `((a OR ((b ANDNOT c) AND d)) OR ((e AND f) OR (g AND h)))`

5.`(a OR b AND c) ANDNOT(d OR e OR f AND g) AND h`
  The correct order of evaluation according to the BNF is:
  1) Evaluate any parentheses first,
  2) Then evaluate any OR expressions,
  3) Finally, evaluate any ANDNOT expressions and then any AND expressions.

  Applying parentheses according to the order of evaluation, the query becomes:
  (((a OR (b AND c)) ANDNOT (d OR e OR (f AND g))) AND h)

6.`a OR b OR c ANDNOT(d AND e OR f AND g) OR h AND i`
  The correct order of evaluation according to the BNF is:
  1) Evaluate any parentheses first,
  2) Then evaluate any OR expressions,
  3) Finally, evaluate any ANDNOT expressions and then any OR expressions.

  Applying parentheses according to the order of evaluation, the query becomes:
  `((a OR b OR ((c ANDNOT d) ANDNOT (e AND f AND g))) OR (h AND i))`

7.`a OR b ANDNOT(c OR d OR e) OR f AND g AND h AND i`
  The correct order of evaluation according to the BNF is:
  1) Evaluate any parentheses first,
  2) Then evaluate any ANDNOT expressions,
  3) Then evaluate any OR expressions,
  4) Finally, evaluate any AND expressions.

  Applying parentheses according to the order of evaluation, the query becomes:
  `(a OR (((b ANDNOT (c OR d OR e)) OR (f AND g AND h)) AND i))`


parse_expression()
    return parse_expression_1(parse_primary(), 0)

parse_expression_1(lhs, min_precedence)
    lookahead := peek next token
    while lookahead is a binary operator whose precedence is >= min_precedence
        op := lookahead
        advance to next token
        rhs := parse_primary ()
        lookahead := peek next token
        while lookahead is a binary operator whose precedence is greater
                 than op's, or a right-associative operator
                 whose precedence is equal to op's
            rhs := parse_expression_1 (rhs, precedence of op + (1 if lookahead precedence is greater, else 0))
            lookahead := peek next token
        lhs := the result of applying op with operands lhs and rhs
    return lhs



(a AND b ANDNOT ((c) AND d ANDNOT e) OR f AND g AND h ANDNOT (i) AND j) AND (k AND (l) OR m AND n ANDNOT (o) ANDNOT p) OR q AND ((r) AND s) OR (t) OR u AND (v) AND w ANDNOT ((x ANDNOT y) ANDNOT (z)) ANDNOT (aa AND bb) OR (cc) OR dd