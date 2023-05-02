
# Assumptions

## indexer / index interaction

#### Deallocation of strings accessed by both programs  
It is the job of the program using the index to free all and any query_result returned by the index,
but the path pointer within each returned query_result will not be modified or deallocated outside of the index.

Given the instruction that index.c is to free given words / paths - they will not be deallocated or 
modified by the indexer after handing them over to index.

Simply put - no strings going in or out of the index are deallocated through other means than index_destroy

* A file directory is inheritly an ordered set. No two paths within a directory may be equal, and the path strings
are all compareable through the `<`, `>`, `=` operators. As a direct consequence of this, all paths indexed through index_addpath 
will be, compareable by either `<` or `>`, but never `=`. 
Put short, the index assumes no given path will be added more than once, and the index will not check for this.

Digression: 
  If the index was to support updating paths, - say when changes have made to a file at an indexed path, 
  the index design would likely be drastically different. 
  Following the specifications within the assignment, the index will never have to remove any part of 
  its structure and still remain operational. Say the index was to support such a feature, using a map as 
  the 'core' structure of such an index would likely be more appealing than in its current state.
  Removal or alteration map entries is typically much simpler than for a node within a SBBST.

index_addpath will be duplicate. The index will not perform any comparison 
will inheritly be an ordered set, given that no folder within a directory can contain duplicate 
in the sense that 




```
A hashmap works like this (this is a little bit simplified, but it illustrates the basic mechanism):

It has a number of "buckets" which it uses to store key-value pairs in. Each bucket has a unique number - that's what identifies the bucket. When you put a key-value pair into the map, the hashmap will look at the hash code of the key, and store the pair in the bucket of which the identifier is the hash code of the key. For example: The hash code of the key is 235 -> the pair is stored in bucket number 235. (Note that one bucket can store more then one key-value pair).

When you lookup a value in the hashmap, by giving it a key, it will first look at the hash code of the key that you gave. The hashmap will then look into the corresponding bucket, and then it will compare the key that you gave with the keys of all pairs in the bucket, by comparing them with equals().

Now you can see how this is very efficient for looking up key-value pairs in a map: by the hash code of the key the hashmap immediately knows in which bucket to look, so that it only has to test against what's in that bucket.

Looking at the above mechanism, you can also see what requirements are necessary on the hashCode() and equals() methods of keys:

If two keys are the same (equals() returns true when you compare them), their hashCode() method must return the same number. If keys violate this, then keys that are equal might be stored in different buckets, and the hashmap would not be able to find key-value pairs (because it's going to look in the same bucket).

If two keys are different, then it doesn't matter if their hash codes are the same or not. They will be stored in the same bucket if their hash codes are the same, and in this case, the hashmap will use equals() to tell them apart.

``` https://stackoverflow.com/questions/6493605/how-does-a-java-hashmap-handle-different-objects-with-the-same-hash-code/6493946#6493946