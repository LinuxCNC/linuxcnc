#ifndef LINKED_LIST_HH
#define LINKED_LIST_HH

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifdef __cplusplus
}
#endif
enum LIST_SIZING_MODE {
    DELETE_FROM_HEAD,
    DELETE_FROM_TAIL,
    STOP_AT_MAX,
    NO_MAXIMUM_SIZE
};

/** this is more of a struct..
    But still need to add some comments... */
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
    int nextNodeId;
  public:
    /** Deletes the current node
        Used by CMS_SERVER & CMS_SERVER_REMOTE_XXX_PORT */
    void deleteCurrentNode();

    /** Flushes the list regardless of data still tagged uncopied */
    void deleteMembers();

    /** Deletes the specified node and frees the memory if it is tagged
        as copied.
        Used by NML and CMS classes */
    void deleteNode(int _id);

    /** Flushes all the nodes that have been copied out and frees up memory */
    void flushList();

    /** Returns the ID of the current node */
    int getCurrentId();

    /** Returns the first member of the list without deleting it. 
    Called CMS_SERVER, CMS_SERVER_XXX_PORT, & NML classes */
    void *getHead();

    /** Returns the next member of the list */
    void *getNext();

    /** Returns the last member of the list 
    Used by CMS & TCPMEM for diagnostics...*/
    void *getTail();

    /** Returns the first member of the list and deletes the node. */
// Recommend: caller uses getHead() then deleteCurrentNode()
    void *retrieveHead();

    /** sets the limit on the list size and determines what happens when the list size hits maxListSize */
    void setListSizingMode(int _size, LIST_SIZING_MODE);

    /** Sets the limit to the list size */
//    void setMaxListSize(int _size);

    /** Stores the node at the begining of the list and may remove the
        tail node depending on the status of sizingMode
    Used by NML::prefix_format_chain() */
    int storeAtHead(void *_data, size_t _size, int _copy);

    /** Stores the node at the end of the list - May remove head node...
    Used by CMS, NML and others. */
    int storeAtTail(void *_data, size_t _size, int _copy);

    /** Constructor */
      LinkedList();

     /** Destructor */
     ~LinkedList();

/** Public attributes - Most of these should be hidden */

    /** Current size of the list */
    int listSize;

    /** Max size of the list */
    int maxListSize;

    /** See LISTING_SIZE_MODE for values */
    LIST_SIZING_MODE sizingMode;

    size_t lastSizeRetrieved;
    int deleteDataNotCopied;
    void *lastDataRetrieved;
    int lastCopiedRetrieved;
    size_t lastSizeStored;
    void *lastDataStored;

  private:
      LinkedList(LinkedList & list);	// Is this required ??
};

#endif /* LINKED_LIST_HH */
