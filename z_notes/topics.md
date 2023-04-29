# Intro
* Intro 1400++
* brief summary



# Technical Background
* What is a set
  * definitions
    * infinite 
    * finite -> 
  * All finite sets are countable, but not all countable sets are finite.
  * in math vs. CS
  * Examples of ordered sets:
    * integers
    * Letters in the alphabet
    * Directory paths
    * Maps (CS, real world)
  * Finite set operations
    * |A U B| <= |A| + |T|    ->  either less than, or equal to

* What is an ADT
  * definitions
  * examples
  * index: ADT, but still limited to index words - not integers, etc
  * Pros, Cons -> (report oblig 1)
    * Scaleable, replaceable, non-intertwined, limited
    * Code Reuseability
    * Limited to the defined operations
    * Difficult to implement - considering essential functionality - (reference to changes done within set.h)
    * Limitation by design VS technical limitations
    * Usage requires no indepth knowledge of implementation, but correct does - otherwise assumptions have to be made
    * Rigid due to their defined operations - compared to specialized data structures

<!-- * storing strings - pointers vs. literals - memory & time -->



# Design & Implementation
* intro
  * Initial Design - (idea: structs with circular referencing)
  * changes to set.h (adhering fully to the given set ADT vs. modification)

* building the index
  * index_addpath (more duplicates == fewer i_word_t mallocs)

* accessing the index
  * parser
    goals, limitiations, rules, requirements, logical equivalences, implementation
  * calculating result score
    tf-idf

* index variants
  * differences between implementations
  * How to measure, test and compare? --> create index variants

* initial design plan vs. actualized design



# Experiments
 * intro
  * how to benchmark anything? (trying different options)
  * prototype experiments

 * advanced comparisons
  * building | accessing
  * large sets of files
  * small sets of files
  * big files vs small files?



# Results
 * index_tree VS index_map
  * scaling: time | space
  * strengths | weaknesses, (build, search)



# Discussion
* Attempting an approach vs researching the 'correct way'
* Post implementation thoughts: How i did it  VS  how its typically done
* why the results are like this -- map / tree fundamental differences
* no reference point -> degree of uncertainty regarding the validity of results



# Conclusion
* Difficult to feel confident about any implementation without proper feedback & reference points
* Implementation: What is good, what is bad



# References
* rosen - set theory
* draw.io - resource
* OBLIG 1: Report, tree
