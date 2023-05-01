
#### Asserting the query parser:
Implemented `assertive_queryparser.c`, which completes essential assertions throughout the program.

Prior to evaluating the parsing process, there was extensive, albeit manual, attempts to catch all the corner cases for syntax validation.
After hundreds of attempts to cheat the syntax control, it is the firm belief of the author that the scanning routine *will* catch invalid syntax prior to any parsing attempt.

In addition to the assertions done, the strategy to verify correct order of evaluation was through string concatination.
Unlike the main implementation of the query parser, the assertive variant stores given tokens within the nodes,
and combines their strings after each completed operation. 
This allows fast visual inspection of each completed query, as confirming through checking the files can be tedious.

The below figure showcase an example output of a query, as printed to the terminal by the assertive parser.
Note that each line after the first shift symbol is a recursive call, and the shifts indicate a move right or left within the nodes.
In order to limit the recursion depth, the main variant has loop to always traverse right until it find a term or right parenthesis.
For testing purposes, however, being able to visualize the recursion stack was preferable.
Each completed operation is wrapped within brackets, and so the result should be "identical" to the query.

[parser_scan]: scan of 21 tokens completed in 0.06600ms
[D][src/assertive_queryparser.c:610]: [query_validated]
[q_tokens]	`(a) OR (b ANDNOT (c AND d)) OR ((e) AND f)`
[q_nodes]	`a OR (b ANDNOT (c AND d)) OR (e AND f)`
>>
>>
>>
>>
>>
[term_and] new set: c &-->  [cANDd]  <--& d
>>
<<< goto l_paren
>>
<<< goto l_paren
>>
[term_andnot] new set: b /-->  [bANDNOT[cANDd]]  <--/ [cANDd]
>>
>>
>>
[term_and] new set: e &-->  [eANDf]  <--& f
>>
<<< goto l_paren
<<
[term_or] new set: [bANDNOT[cANDd]] U-->  [[bANDNOT[cANDd]]OR[eANDf]]  <--U [eANDf]
<<
[term_or] new set: a U-->  [aOR[[bANDNOT[cANDd]]OR[eANDf]]]  <--U [[bANDNOT[cANDd]]OR[eANDf]]
^^ returning node: [aOR[[bANDNOT[cANDd]]OR[eANDf]]]

Comparing the last node's string to the query, we can deduct that the order of evaluation was correctly interpreted by the parser.
The above strategy may not be classified as a scientific approach. 
Despite of this, when considered along with the assertions done by assert_index, checking the resulting documents, and dozens of assertions done within the parser code - the parser does paint a picture of functioning as intended.


----
