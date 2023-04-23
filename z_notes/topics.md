# Intro
* Intro 1400++
* brief summary


# Technical Background
* what is an ADT -- definition, math/code, pros vs cons -> (report oblig 1)
* storing strings - pointers vs. literals - memory & time
* What is a set -- an abstract definition
* Trees - aatree, rbtree


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
 * rbtree / aatree - wikipedia
 * set - wikipedia
 * draw.io - resource
 * OBLIG 1: Report, tree
