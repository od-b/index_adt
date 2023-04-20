# Intro
* Intro 1400++
* brief summary


# Technical Background
* what is an ADT -- definition, math/code, pros vs cons -> (report oblig 1)
* storing strings - pointers vs. literals - memory & time
* What is a set -- an abstract definition
* Trees - aatree, rbtree


# Design & Implementation  
* Initial Design - (idea: structs with circular referencing)
* Transition: precode changes
* How to measure, test and compare? --> create index variants
* differences between implementations
* changes to design during development
* building the index: add_path, (more duplicates == fewer i_word_t mallocs)
* parser: limitiations, rules, requirements, implementation


# Experiments
 * intro: comparing the implemented indexes
 * initial, simple experiments
 * advanced comparisons, building | accessing: 
    * large sets of files
    * small sets of files
    * big files vs small files?


# Results
 * index_tree VS index_map:
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
