
/**
 linklist.cc allocates blocks of memory and keeps track of them in a list
 of pointers..
 LinkedList->data contains the address of the memory block
 LinkedList->size is the size of the memory block
 LinkedList->id   a unique ID for each 'node'
 LinkedList->next pointer to the next node ?
 LinkedList->last pointer to the previous node

*/
extern "C" {
#include <stdlib.h>		/* malloc() */
#include <string.h>		/* memcpy() */
#include <stdio.h>		/* fprintf(), stderr */
}
#include "linklist.hh"		/* class LinkedList */
LinkedListNode::LinkedListNode(void *_data, size_t _size)
{
    data = _data;
    size = _size;
    next = (LinkedListNode *) NULL;
    last = (LinkedListNode *) NULL;
}

LinkedListNode::~LinkedListNode()
{
}

LinkedList::LinkedList()
{
    head = (LinkedListNode *) NULL;
    tail = (LinkedListNode *) NULL;
    current_node = (LinkedListNode *) NULL;
    extra_node = (LinkedListNode *) NULL;
    lastDataRetrieved = NULL;
    lastSizeRetrieved = 0;
    lastCopiedRetrieved = 0;
    listSize = 0;
    nextNodeId = 1;
    deleteDataNotCopied = 0;
    extra_node = new LinkedListNode(NULL, 0);
    maxListSize = 0;
    sizingMode = NO_MAXIMUM_SIZE;
}

LinkedList::~LinkedList()
{
    flushList();
    if (NULL != extra_node) {
	delete extra_node;
	extra_node = (LinkedListNode *) NULL;
    }
}

/** Used once by print.cc rcs_fputs() for printing to a list */
void LinkedList::setListSizingMode(int _new_max_size,
    LIST_SIZING_MODE _new_sizing_mode)
{
    maxListSize = _new_max_size;
    sizingMode = _new_sizing_mode;
}

/** Called by LinkedList destructor and delete_members() 
    But nothing outside this file uses it */
void LinkedList::flushList()
{
    LinkedListNode *next_node;
    current_node = head;
    while (NULL != current_node) {
	next_node = current_node->next;
	if ((current_node->copied || deleteDataNotCopied)
	    && (NULL != current_node->data)) {
	    free(current_node->data);
	}
	delete current_node;
	current_node = next_node;
    }
    if (lastCopiedRetrieved) {
	if (lastDataRetrieved != NULL) {
	    free(lastDataRetrieved);
	    lastDataRetrieved = NULL;
	    lastSizeRetrieved = 0;
	}
    }
    head = (LinkedListNode *) NULL;
    tail = (LinkedListNode *) NULL;
    listSize = 0;
    lastDataStored = NULL;
    lastSizeStored = 0;
}

/** Used by CMS::internal_retrieve_diag_info() TCPMEM::get_diagnostics_info()
    and by the interp via NML_INTERP_LIST::clear() */
void LinkedList::deleteMembers()
{
    int old_delete_data_not_copied = deleteDataNotCopied;
    deleteDataNotCopied = 1;
    flushList();
    deleteDataNotCopied = old_delete_data_not_copied;
}

/** Called by NML_INTERP_LIST::get() */
/** Removes the first node of the list and frees the memory */
void *LinkedList::retrieveHead()
{
    LinkedListNode *next_node;

    if (NULL != head) {
	if (lastCopiedRetrieved) {
	    if (NULL != lastDataRetrieved) {
		free(lastDataRetrieved);
		lastDataRetrieved = NULL;
		lastSizeRetrieved = 0;
	    }
	}
	lastDataRetrieved = head->data;
	lastSizeRetrieved = head->size;
	lastCopiedRetrieved = head->copied;
	next_node = head->next;
	delete head;
	head = next_node;
	if (NULL != head) {
	    head->last = (LinkedListNode *) NULL;
	} else {
	    tail = (LinkedListNode *) NULL;
	}
	listSize--;
	return (lastDataRetrieved);
    }
    return (NULL);
}

/* Called by NML::prefix_format_chain() */
int LinkedList::storeAtHead(void *_data, size_t _size, int _copy)
{
    LinkedListNode *new_head;
    LinkedListNode *old_tail = tail;

    if (listSize >= maxListSize) {
	switch (sizingMode) {
	case DELETE_FROM_TAIL:
	    if (NULL != tail) {
		tail = tail->last;
		if (NULL != tail) {
		    tail->next = (LinkedListNode *) NULL;
		} else {
		    head = (LinkedListNode *) NULL;
		    delete old_tail;
		    listSize = 0;
		    break;
		}
		delete old_tail;
		listSize--;
	    }
	    break;

	case NO_MAXIMUM_SIZE:
	    break;

	case DELETE_FROM_HEAD:
	case STOP_AT_MAX:
	default:
	    return (-1);
	}
    }

    if (_copy) {
	lastDataStored = malloc(_size);
	memcpy(lastDataStored, _data, _size);
	lastSizeStored = _size;
	new_head = new LinkedListNode(lastDataStored, _size);
    } else {
	lastDataStored = _data;
	lastSizeStored = _size;
	new_head = new LinkedListNode(_data, _size);
    }
    if (NULL != new_head) {
	new_head->copied = _copy;
	new_head->id = nextNodeId++;
	if (NULL == head) {
	    head = new_head;
	    if (NULL != tail) {
		return (-1);
	    }
	    tail = new_head;
	} else {
	    head->last = new_head;
	    new_head->last = (LinkedListNode *) NULL;
	    new_head->next = head;
	    head = new_head;
	}
	listSize++;
	return (head->id);
    } else {
	return (-1);
    }
}

/** Called by numerous functions */
int LinkedList::storeAtTail(void *_data, size_t _size, int _copy)
{
    LinkedListNode *new_tail;
    LinkedListNode *old_head = head;

    if (listSize >= maxListSize) {
	switch (sizingMode) {
	case DELETE_FROM_HEAD:
	    if (NULL != head) {
		head = head->next;
		if (NULL != head) {
		    head->last = (LinkedListNode *) NULL;
		} else {
		    head = (LinkedListNode *) NULL;
		    delete old_head;
		    listSize = 0;
		    break;
		}
		delete old_head;
		listSize--;
	    }
	    break;

	case NO_MAXIMUM_SIZE:
	    break;

	case DELETE_FROM_TAIL:
	case STOP_AT_MAX:
	default:
	    fprintf(stderr, "LinkedList: Invalid list sizing mode.\n");
	    return (-1);
	}
    }

    if (_copy) {
	lastDataStored = malloc(_size);
	memcpy(lastDataStored, _data, _size);
	lastSizeStored = _size;
	new_tail = new LinkedListNode(lastDataStored, _size);
    } else {
	lastDataStored = _data;
	lastSizeStored = _size;
	new_tail = new LinkedListNode(lastDataStored, _size);
    }
    if (NULL != new_tail) {
	new_tail->copied = _copy;
	new_tail->id = nextNodeId++;
	if (NULL == tail) {
	    tail = new_tail;
	    if (NULL != head) {
		fprintf(stderr,
		    "LinkedList: Tail is NULL but head is not.\n");
		return (-1);
	    }
	    head = new_tail;
	} else {
	    tail->next = new_tail;
	    new_tail->last = tail;
	    new_tail->next = (LinkedListNode *) NULL;
	    tail = new_tail;
	}
	listSize++;
	return (tail->id);
    } else {
	fprintf(stderr,
	    "LinkedList: Couldn't create new node to store_at_tail.\n");
	return (-1);
    }
}

/** Used by numerous functions */
void *LinkedList::getHead()
{
    current_node = head;
    if (NULL != current_node) {
	return (current_node->data);
    } else {
	return (NULL);
    }
}

/** Called by CMS::internal_retrieve_diag_info() TCPMEM::get_diagnostics_info() */
void *LinkedList::getTail()
{
    current_node = tail;
    if (NULL != current_node) {
	return (current_node->data);
    } else {
	return (NULL);
    }
}

/** Used by numerous functions */
void *LinkedList::getNext()
{
    if (NULL != current_node) {
	current_node = current_node->next;
    }
    if (NULL != current_node) {
	return (current_node->data);
    } else {
	return (NULL);
    }
}

/** Used by numerous functions */
void LinkedList::deleteNode(int _id)
{
    LinkedListNode *temp;

    temp = head;
    while (NULL != temp) {
	if (temp->id == _id) {
	    listSize--;
	    if (temp == current_node) {
		if (NULL != extra_node) {
		    extra_node->next = current_node->next;
		    extra_node->last = current_node->last;
		    current_node = extra_node;
		}
	    }
	    if (NULL != temp->next) {
		temp->next->last = temp->last;
	    } else {
		tail = temp->last;
	    }
	    if (NULL != temp->last) {
		temp->last->next = temp->next;
	    } else {
		head = temp->next;
	    }
	    if ((temp->copied || deleteDataNotCopied)
		&& (NULL != temp->data)) {
		free(temp->data);
	    }
	    delete temp;
	    break;
	}
	temp = temp->next;
    }
}

/** Used by numerous functions */
void LinkedList::deleteCurrentNode()
{
    if (NULL != current_node && (current_node != extra_node)) {
	LinkedListNode *temp;
	temp = current_node;
	if (NULL != extra_node) {
	    extra_node->next = current_node->next;
	    extra_node->last = current_node->last;
	    current_node = extra_node;
	}
	if (NULL != temp->next) {
	    temp->next->last = temp->last;
	} else {
	    tail = temp->last;
	}
	if (NULL != temp->last) {
	    temp->last->next = temp->next;
	} else {
	    head = temp->next;
	}
	if ((temp->copied || deleteDataNotCopied) && (NULL != temp->data)) {
	    free(temp->data);
	}
	delete temp;
	listSize--;
    }
}

/** Used by LOCMEM::LOCMEM() only */
int LinkedList::getCurrentId()
{
    if (current_node == NULL) {
	return (-1);
    }
    return (current_node->id);
}

// Constructor defined private to prevent copying.
LinkedList::LinkedList(LinkedList & list)
{
}
