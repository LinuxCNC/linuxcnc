#ifndef AVLTREE_H
#define AVLTREE_H

/** This file, 'avltree.h', defines an implementation of AVL balanced
    binary trees.  Binary trees allow fast searching, insertion,
    and deletion of data by maintaining the data in a sorted form
    and doing binary searches.  Ordinary binary trees can get out of
    balance, with some branches much deeper than others.  If data is
    added in sorted or partially sorted order, the tree can get
    completely out of balance and turn into a slow linear search.  AVL
    trees avoid this by dynamically re-balancing the tree if adding
    or deleting data causes it to become distorted.
*/

/**
* Author:    John Kasunich
*            <jmkasunich AT users DOT sourceforge DOT net>
* License:   GPL Version 2
* System:    Linux
*
* Copyright (c) 1994 All rights reserved.
* Revised and released under GPL 2005
*
* Last change:
* $Revision$
* $Author$
* $Date$
*
*/

/** This implementation separates the data and the tree.  The tree
    consists of one avltree structure for the tree and one avltree_node
    structure for each data item.  The nodes are linked together to
    form the branches of the tree, and each node points to the
    corresponding data item.  Although this adds one more level of
    indirection to data accesses, it allows the tree manipulation
    code to be independant of the data items.  As long as a function
    is supplied to compare two data items, the types, formats, and
    storage of the data is irrelevant.  In addition, two or more
    trees can be built which refer to the same data.  This allows
    the data to be sorted on more than one key.

    This code can run in user space or in kernel space.  If in kernel
    space, it uses kmalloc() and kfree() for memory allocation, in user
    space it uses the normal malloc() and free().

    These functions are NOT re-entrant for calls that reference the
    same tree.  Interrupting an avltree_find() call and then calling
    avltree_insert() on the same tree may lead to data corruption.
    Possible solutions are:

    1) application does not make re-entrant tree manipulation calls
    2) application wraps a mutex arount tree manipulation calls
    3) these functions are revised to wrap a mutex around tree
       manipulation calls

    IMO, building mutexes into this code would penalize all users
    of this library, even though most would not need them.  Only
    multi-threaded applications need to worry about re-entrancy.
    Instead, option 1 or 2 should be used.
*/

/********************/
/* Data Structures  */
/********************/

/* struct used to refer to a complete tree */

typedef struct avltree {
    struct avltree_node *root;			/* root node */
    int (*cmp_func)( void *data1, void *data2 );/* compare function */
} avltree;

/************************/
/* Function Prototypes  */
/************************/

/**  avltree_create(cmp_func) is used to initialize a tree.  The ordering
     of the data is determined by 'cmp_func'.  This user supplied function
     is called with pointers to two data items.  It must compare the items
     and return as follows:

     If data1 < data2   return value < 0
     If data1 = data2   return value = 0
     If data1 > data2   return value > 0

     If 'cmp_func' is NULL or create_tree() can't allocate memory for the
     tree structure, it will return NULL.  Otherwise it returns a pointer
     to the new tree structure.  All subsequent calls accessing the tree
     will pass that pointer as an argument.

*/
avltree *avltree_create(int(*cmp_func)(void *data1, void *data2));

/**  avltree_destroy(tree) is used to destroy a tree.  When it returns, all
     of the tree's internal data structures have been cleared and freed,
     and 'tree' no longer points to a valid tree structure.
*/
void avltree_destroy(avltree *tree);

/**  avltree_insert(tree,data) is used to add data item 'data' to a tree.
     If a duplicate item is already in the tree, it returns a pointer to
     the duplicate.  If it can't allocate memory for the tree node, it
     returns NULL.  Otherwise, it inserts a node that points to 'data',
     and returns 'data'.
*/
void *avltree_insert(avltree *tree, void *data);

/** avltree_delete(tree, data) is used to remove a data item from the tree.
    If an item matching 'data' is not in the tree, it returns NULL.
    Otherwise, it removes the tree node that points to the matching item,
    and returns a pointer to that item.
*/
void *avltree_delete(avltree *tree, void *data);

/** avltree_find(tree, data) is used to locate a data item in the tree.
    If an item matching 'data' is not in the tree, it returns NULL.
    Otherwise, returns a pointer to the matching item.
*/
void  *avltree_find(avltree *tree, void *data);

/** avltree_first(tree) is used to locate the first data item in the tree.
    If the tree is empty, it returns NULL.  Otherwise it returns a pointer
    to the first item in the tree.
*/
void  *avltree_first(avltree *tree);

/** avltree_last(tree) is used to locate the last data item in the tree.
    If the tree is empty, it returns NULL.  Otherwise it returns a pointer
    to the last item in the tree.
*/
void  *avltree_last(avltree *tree);

/** avltree_next(tree, data) is used to locate the data item that follows
    'data' in the tree.  If 'data' matches the last item in the tree, or
    if 'data' would come after the last item, it returns NULL.  Otherwise,
    it returns a pointer to the item that follows 'data'.  It is not
    neccessary for there to be an item that matches 'data'.  For example,
    assume that a tree contains the items "A", "C", "E", and that the
    compare function is a normal string compare.  avltree_next(tree, "C")
    and avltree_next(tree, "D") would both return a pointer to "E", while
    avltree_next(tree,"E") and avltree_next(tree,"F") would both return
    NULL.
*/
void  *avltree_next(avltree *tree, void *data);

/** avltree_prev(tree, data) is used to locate the data item that precedes
    'data' in the tree.  It works just like avltree_next().
*/
void  *avltree_prev(avltree *tree, void *data);

#endif
