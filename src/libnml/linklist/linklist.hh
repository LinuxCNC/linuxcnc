/********************************************************************
* Description: linklist.hh
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
* $Revision$
* $Author$
* $Date$
********************************************************************/

#ifndef LINKED_LIST_HH
#define LINKED_LIST_HH

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifdef __cplusplus
}
#endif
enum IS_EMPTY {
    LIST_EMPTY,
    LIST_NOT_EMPTY
};

enum LIST_SIZING_MODE {
    DELETE_FROM_HEAD,
    DELETE_FROM_TAIL,
    STOP_AT_MAX,
    NO_MAXIMUM_SIZE
};

class LinkedListNode {
  public:
    void *data;
    size_t size;
    int id;
    int copied;
    LinkedListNode *next;
    LinkedListNode *last;
    friend class LinkedList;
      LinkedListNode(void *_data, size_t _size);
     ~LinkedListNode();
};

class LinkedList {
  protected:
    LinkedListNode * head;
    LinkedListNode *tail;
    LinkedListNode *current_node;
    LinkedListNode *extra_node;
    int next_node_id;
  public:
    int get_current_id();
    /* 

     */
    int list_size;
    /* 

     */
    int max_list_size;
    /* 

     */
    LIST_SIZING_MODE sizing_mode;
    /* 

     */
    void set_list_sizing_mode(int, LIST_SIZING_MODE);
    /* 
       Sets a sizing mode and the maximum number of nodes allowed on the
       list. The sizing mode determines what happens when there is an attempt 
       to add another node to the list after it has reached the
       _maximum_size. The following are the possible values for _new_mode:

       DELETE_FROM_TAIL: Remove one node from the tail of the list to make
       room for the new node.

       DELETE_FROM_HEAD: Remove one node from the head of the list to make
       room for the new node.

       STOP_AT_MAX: Return -1 if an attempt is made to add a new node when
       the list is full.

       NO_MAXIMUM_SIZE: Allow the list to grow until all available memory is
       used up. */

    void set_max_list_size(int);
    /* 

     */
    size_t last_size_retrieved;
    /* 

     */
    int delete_data_not_copied;
    /* 

     */
    void *last_data_retrieved;
    /* 

     */
    int last_copied_retrieved;
    /* 

     */
    void *retrieve_head();
    /* 

     */
    void *retrieve_tail();
    /* 

     */
    size_t last_size_stored;
    /* 

     */
    void *last_data_stored;
    /* 

     */

    int store_at_head(void *_data, size_t _size, int _copy);
    /* Creates a new node and places it at the beginning of the list. If
       _copy is nonzero then this function will malloc _size bytes and copy
       _size bytes from the address starting at _data there and the get
       functions will return a pointer to the copy of the object. If _copy is 
       zero then the _data pointer will be stored and the get functions will
       return a pointer to the original object.

       Returns a positive integer id that can be used to select this node
       later if successful or -1 if an error occurred. */

    int store_at_tail(void *_data, size_t _size, int _copy);
    /* Creates a new node and places it at the end of the list. If _copy is
       nonzero then this function will malloc _size bytes and copy _size
       bytes from the address starting at _data there and the get functions
       will return a pointer to the copy of the object. If _copy is zero then 
       the _data pointer will be stored and the get functions will return a
       pointer to the original object.

       Returns a positive integer id that can be used to select this node
       later if successful or -1 if an error occurred. */

    int store_after_current_node(void *_data, size_t _size, int _copy);
    /* Creates a new node and places it after the current node. If _copy is
       nonzero then this function will malloc _size bytes and copy _size
       bytes from the address starting at _data there and the get functions
       will return a pointer to the copy of the object. If _copy is zero then 
       the _data pointer will be stored and the get functions will return a
       pointer to the original object.

       Returns a positive integer id that can be used to select this node
       later if successful or -1 if an error occurred. */

    int store_before_current_node(void *_data, size_t _size, int _copy);
    /* Creates a new node and places it before the current node. If _copy is
       nonzero then this function will malloc _size bytes and copy _size
       bytes from the address starting at _data there and the get functions
       will return a pointer to the copy of the object. If _copy is zero then 
       the _data pointer will be stored and the get functions will return a
       pointer to the original object.

       Returns a positive integer id that can be used to select this node
       later if successful or -1 if an error occurred. */

    int get_newest_id() {
	return (next_node_id - 1);
    }
    void *get_head();
    /* 
       Get the address of the first object on the list and set the current
       node to the beginning of the list.

       If the list is empty get_head returns null. Depending on how the
       object was stored the address this function returns may be the address 
       of the original object or of a copy. */

    void *get_tail();
    /* 
       Get the address of the object at the end of the list and set the
       current node to the end of the list. If the list is empty get_tail
       returns null. Depending on how the object was stored the address this
       function returns may be the address of the original object or of a
       copy. */

    void *get_next();
    /* 
       Get the address of the next object on the list and move the current
       node one step closer to the tail.. If the list is empty get_tail
       returns null. Depending on how the object was stored the address this
       function returns may be the address of the original object or of a
       copy. */

    void *get_last();
    /* 
       Get the address of the previous object on the list and move the
       current node one step closer to the head.. If the list is empty
       get_tail returns null. Depending on how the object was stored the
       address this function returns may be the address of the original
       object or of a copy. */

    void *find_node(int _node_number);
    /* 

     */
    void delete_node(int _id);
    /* 
       Delete the node with the associated id. */

    void delete_current_node();
    /* 
       Remove the current node from the list and free any memory associated
       with it. Some extra pointers keep track of the node that was before
       and after the deleted node so that the next call to get_next or
       get_last will return the same object as if the current node was not
       deleted. */

    void *get_by_id(int _id);
    /* 

     */
    void *get_first_newer(int _id);
    /* 

     */
    void *get_last_newer(int _id);
    /* 

     */
    IS_EMPTY is_empty();
    /* 

     */
    void flush_list();
    /* 

     */
    void delete_members();
    /* 

     */

    LinkedList();
    ~LinkedList();

  private:
    LinkedList(LinkedList & list);	// Don't copy me.
};

#endif /* LINKED_LIST_HH */
