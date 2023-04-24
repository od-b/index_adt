
* Should the parser be an integral component of the index, or a separate entity?

### BNF Grammar
<query>   ::=  <andterm> | <andterm> "ANDNOT" <query>
<andterm> ::=  <orterm> | <orterm> "AND" <andterm>
<orterm>  ::=  <term> | <term> "OR" <orterm>
<term>    ::=  "(" <query> ")" | <word>

"ANDNOT" : difference   => <term> \ <term>
"OR"     : union        => <term> ∪ <term>
"AND"    : intersection => <term> ∩ <term>


#### Interpretation
* All operators `ANDNOT | AND | OR` must have adjacent terms.

* The operators `OR` and `AND` may be used in 'chained' terms without parantheses, 
  as long as the operator within the chain stays the same. 
  Example: `(a AND b AND c AND d) OR (x OR y OR z)` is a valid query; while `a AND b AND c OR d` is not,
  as there would be no way to determine whether `a AND b AND (c OR d)` or `(a AND b AND c) OR d` is the intended evaluation.

* The `ANDNOT` operator may not be used in chain between words without the use of parantheses.
  Example. `x ANDNOT y ANDNOT z` would be invalid. 
  In the event the above point is wrongly interpreted, the same result could be acquired by the query `x ANDNOT (y OR z)`.
  Implementing the query in accordance with this interpretation should limit no functionality.

* If using more than one type of operator within a query, the different operators must be separated through the use 
  of parantheses, in order to specify the desired order of evaluation.
  Example. `cat AND dog OR rat AND horse OR mouse` is not a valid query, and should instead be:
  `(cat AND dog) OR (rat AND horse) OR mouse`, or `(cat AND (dog OR rat)) AND (horse OR mouse)`, etc.


#### Further Notes, Examples
* `a OR (b OR (c AND d)) OR (e AND f)` is valid
* `a OR (b OR (c AND d)) AND (e AND f)` is invalid


#### Logical equivalence examples on valid queries:  
Let `=` be defined as logically equivalent.  

"ANDNOT" : difference   => <term> \ <term>
"OR"     : union        => <term> ∪ <term>
"AND"    : intersection => <term> ∩ <term>

query := `a ANDNOT (a ANDNOT b)`
