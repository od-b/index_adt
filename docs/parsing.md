
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
