

# Precode: Thoughts, Issues, Changes, Notes

## Thoughts:
  * treeset.c and aatreeset.c being merged set/list implementations greatly limits the possibilites
  of using several different type sets within one file.  
    If set.h was renamed treeset.h, it would allow for a different mapset.c (etc) to exist within the same namespace.
    The drawback of such a change would be reducing the abstraction level of the interface, however,
    is this abstraction level more important than finding a suitable set implementation for the task?¨

## Issues:


## Changes:
All commited changes to the precode.
This list does not including makefile changes, or additions to common.c/h.

### General modifications that do not affect logic:  
  * Removed spaces between declaration and parameters for functions and defines that takes in parameters  
  * Added brackets to many loops and if-statements where i believe it makes the code more readable  
  * Updated several inaccurate descriptions. E.g.; list_create.h, 
  * Renamed linkedlist.c to doublylinkedlist.c for clarity

### Misc:
  * update ERROR_PRINT && DEBUG_PRINT to include line. Can be turned off by not defining LINE_PRINT.

### Logical or variable type changes

  * Changed 'set_add()' to provide a return value
    **Definitions**
      * 'tree' := 'binary search tree data structure'
      * 'list' := 'linked list data structure'

    **When adding an item to a set ADT, the following operations must in some way be performed:**
      1) __check if set already contains the item, through cmpfunc__
      2) __add item if step 1 confirms item does not exist in set__

    **Context**:
      A binary search tree is inheritly an ordered set, and this makes it an excellent structure for a set ADT.
      When attempting to add an item to a tree, the tree will have to traverse to the node where the item is to be added, and abort the  process of creating a new node if an equal item already exists. 
      This process simultaneously performs operations 1) and 2), defined above. This effectively eliminates the need to manually check 'tree_contains()' prior to adding an item, seeing as the logic within 'tree_contains()' and 'tree_add()' are logically identical, apart from 'tree_add()' potentially inserting a new node at the position.

    **Demonstrating why 'set_add()' not including a return value is problematic:**
      We have an arbitraty struct defined as 'A' containing two pointers, 'a' and 'b'. a holds a single pointer, b holds many pointers.
      We have an arbitraty tree-based set defined as 'S', containing nodes with pointers to A as their item/data/value. S uses a cmpfunc that compares its elements according to *A->a.

      We recieve item 'x' and 'y'. We want to organize x and y within an A, so that A->a = x. We create A, set A->a = x, then add it to the tree. 
      At this point, we could check whether x was a duplicate item or not by getting the set size. However, we also want to add y to A->b.
      **If x already existed within the tree, we have no convenient way of accessing A->b at this point.**

      **Solution** => 'set_add(A)' returns:
        1) a void pointer to the elem in the tree. This can be either the elem which was given, or the existing elem, if duplicate.
        2) NULL on failure; i.e., out of memory

      We can now create a temp A, append x to A->a, and depending on set_add(A) response - we can either free the the temp A, or keep the newly created A as a node's item, within the set. Since we get the original A in return if there is one, we can access A->b from there. Thus, an easy way of storing structs in sets, without any changes to the existing code/precode.

      Seeing as 'set_add()' is a void call, this change does not affect any existing calls or functionality to set_add() within the existing precode. Simultaneously, it provides optional, crucial functionality for whenever needed.




Each file holds a tree with pointers to the words within the file.

Each word holds a list of files it appears in.


