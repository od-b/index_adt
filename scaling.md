# Pretext: 
* Size data is gathered considering the indexer app as a whole, not specifically the index ADT and its child processes. A general overview of the code within the http/indexer files suggest allocation done by http server is constant. Indexer allocation size varies from the tokenization of files and path names, but considering the index ADT directly controls deallocation of these tokens, the memory used by indexer.app will be be interpreted as memory used by the index ADT.


## Data Set
All samples will be gathered using the same data set, or subsets of this set.
The data set chosen is a portion of English wikipedia dump from from april 2007, consisting of static html files.
Source: <https://dumps.wikimedia.org/other/static_html_dumps/April_2007/en/>, at `wikipedia-en-html.0.7z`.
The size of the set is ≈1.1gb compressed and ≈16gb extracted.

There are a number of reasons for using this set to perform profiling and experiments.
During development it was discovered that words such as `bjørn` will be imported as the two words `bj` and `rn` by the tokenizer, and subsequently index. This would be fine, if it was not for the fact that indexer tokenizes queries in a different manner. Due to this discrepancy, any search for 'bjørn' will come up empty. Following this observation, performing the tests on a English set seemed intuitive, rather than using a set of documents containing mostly Norwegian terms, as proposed by the assignment paper.
Apart from this, the files within the set are of varying size, providing a more 'natural' setting than data with fairly uniform file sizes, such as the provided 'cacm' set.


## General Approach
In order to predict a growth pattern for size/time scaling based on input (indexed files/words), having a wide range of input sizes to compare with is crucial. The test unit, a macbook air with 8gb of memory, would be able to index fairly large sets of data utilizing swap memory. Doing so, however, would potentially skew comparisons when trying to compare time scaling to smaller sets. The upper bound for testing the index size/time scaling will therefore be determined by the available memory. In an ideal setting the testing would be performed on a unit with more memory, to allow a wider range of samples. The goal will be to get samples at increasing rates as far as memory allows it, hopefully establishing a pattern indicating how the index implementation scales with input size.


## Defining input size
Given two functions:
T(n) = 'time complexity',
S(n) = 'size complecity',
Where `n` = 'input size'.
One crucial question still remains - how to define input size. The variants described below are two different takes.


## Variant 1:
  `n` = 'number of indexed paths'
  This approach will ignore the word count, indexing documents until it has reached the desired value of `n`.
  As the document count increases, the time complexity of performing set operations for queries should follow.
  Given a large number of documents, the discrepancies caused by the varying document size should 'even out'.


## Variant 2:
  `n` = 'unique words contained within the index'
  Each document within the test set contain a variable number of words.
  The index is built to search for documents through 'word terms', and the core structure within the index is one containing words. Disregarding edge cases such as all documents being equal or only indexing one document, the size of the 'words' structure within the index will always be greater than the number of terms within any individual document. Typically, it will be bigger by an enormous factor.

  To allow for these tests to be performed, the index was temporary altered to check the unique word count after each path was added, returning a stop signal to indexer if at or above `n`. This way, the chosen data set will be the same for all tests, while `n` decides how far to proceed within the data set. The values of `n` listed at each individual test will be the actual value of `n`, not the preset limit - in order not to stop in the middle of a document.
  The immediate consequense of this approach is that the number of indexed documents will be far greater between each incrementation of `n`, considering each successive document will on average contain fewer unique terms.





# n=500


# n=1000
# n=2000
# n=4000
# n=8000
# n=16000
# n=32000
# n=64000
# n=128000


# 2000 files


# 



# 597304 unique words over 50000 documents
Physical footprint:         2.6G
Physical footprint (peak):  2.6G

