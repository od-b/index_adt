
# Experiment 1:
Simple time-based tests through get_time(), comparing index_tree to index_map.
The tests will be done using assert index, meaning single term queries only.
A timer collects cumulative time spent within index_query() only, and prints the result.
While these tests by no means will be indicative as to which is the 'better' option, 
the results could possible give an idea of what tests should be performed at a later stage of development.

All tests will be performed with flags: -O2 -g.

### parameters:
test 1:
  WORD_LENGTH ( 10 )
  NUM_ITEMS ( 200 )
  NUM_DOCS ( 500 )

test 2:
  WORD_LENGTH ( 10 )
  NUM_ITEMS ( 500 )
  NUM_DOCS ( 2000 )

test 3:
  WORD_LENGTH ( 40 )
  NUM_ITEMS ( 200 )
  NUM_DOCS ( 500 )

------------------ index_tree.c -------------------
1.
queries took a total of 178458 μs                             
queries took a total of 150633 μs
queries took a total of 151839 μs
queries took a total of 151722 μs
queries took a total of 156317 μs
---
2.
queries took a total of 7736915 μs
queries took a total of 7862573 μs
---
3.
queries took a total of 63305 μs
queries took a total of 63364 μs
queries took a total of 61463 μs
queries took a total of 60665 μs

------------------ index_map.c -------------------
1.
queries took a total of 246347 μs
queries took a total of 223138 μs
queries took a total of 224561 μs
queries took a total of 221872 μs
queries took a total of 224945 μs
--- 
2.
queries took a total of 12707335 μs
queries took a total of 12803705 μs
---
3.
queries took a total of 49954 μs
queries took a total of 47778 μs                             
queries took a total of 47100 μs
queries took a total of 48634 μs

--------------

### Thoughts & Theories:
Interesting results despite the simple testing methods. This will need further testing with a much wider range of parameters and proper profiling, as opposed to just time prints.
Initial testing has however already demonstrated that the different implementations scale very differently
depending on parameter weights. 
The pure tree implementation seems to pull ahead with when there are many items (words), and the map implementation seems to show its strength with a smaller set of words. Could this be the fault of the map implementation at hand? Perhaps the hashing function?
Intuitively, the result do make sense, given that a BST is generally a superior structure of choice for accessing "the needle in the haystack".
As the number of buckets in a map increase, the average access time appears to get worse in sync.

### Conclusion:
* Further, more sophisticated testing needed.
* Assert_index is not an ideal testing ground for any method, and as soon as the parser is implemented, 
the implementations should be compared using real; as opposed to randomly generated, data.


