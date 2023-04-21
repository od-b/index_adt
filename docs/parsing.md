# Parser

<term>    ::=  "(" <query> ")"  |  <word>
<query>   ::=  <andterm>  |  <andterm> "ANDNOT" <query>
<andterm> ::=  <orterm>  |  <orterm> "AND" <andterm>
<orterm>  ::=  <term>  |  <term> "OR" <orterm>


### Interpretation:
* <term> can either be <word>, or nested <query>. If <term> is <query>, <query> must be wrapped in paranthesis.
* <query> can either be <andterm>, or <andterm> "ANDNOT" <query>. And so: 
  if (<query> != <andterm>) {
    => <query> == [<andterm>, "ANDNOT", "(" <query> ")" || <word> ]
  }

...
Valid syntax?

: [<term>, <orterm>, <term>]
=> yes, but if <term> == <query>, syntax must be exactly ["(" <query> ")", <orterm>, "(" <query> ")"]


: [<term>, <term>, <term>]
=> technically yes, since <orterm> == <term>


## Argument: "OR" can be completely ignored in terms of logic according to the BNF syntax

From the defined grammar, we have:  
>  ´<orterm> ::= <term> | <term> "OR" <orterm>´  

From wikipedia.org/wiki/Backus-Naur_form, we have:  
>  ´::=´ means that the symbol on the left must be replaced with the expression on the right  

I.e., "::=" is a denotion for left hand value assingment
and is logically equivalent to ´x = y´ (plaintext: "x is now y"), where 'x' requires a specific type of value.  

We have from the assignment text that the terminal defined as "OR" symbolizes Union within the index.

let p = <orterm>, q = <term>;  
let "OR" be denoted as '∪';

We then have that:
1. p ::= q | q ∪ p
2. => ∀p, q ∈ p,          (since p either is equal to q or contains q.)
3. q ∪ p == p ∪ q,        (associative property of Union)
4. (q | q ∪ p) == q ∪ p
5. p ::= q ∪ p            (inf. on 1, 4)
6. p == p
7. (p ::= q) == (p ::= q | q ∪ p)
8. (<orterm> ::= <term>)
  == (<orterm> ::= (<term> | (<term> "OR" <orterm>)))

∴ <term> <term> == <term> "OR" <term>



s
: ["(" <query> ")", (" <query> ")"]
=> no, since <query> 
=> no, <query> cannot be == <query> without <andterm>


: [<andterm>, ]

