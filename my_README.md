

# Precode changes

### General modifications to make the precode style more consistant:  
  * Removed spaces between declaration and parameters for functions and defines that takes in parameters  
  * Added brackets to many loops and if-statements where i believe it makes the code more readable  


### Logical or variable type changes
  #### list.h, linkedlist.c
  * int to unsigned int for list->size and subsequently list_size(), as a list cannot have negative size
  * Removed several else statements where the function would return on a true condition



# Misc. Notes
### questions
Is it possible to know within index.c when the program caller is finished adding files?

### index.c  
Given that index_create does not take in a cmpfunc, i will be working under the assumption
that all index entries' content is comparable through compare_strings, and that all nodes in index_addpath( , , words) 
contain elements with arrays of characters
