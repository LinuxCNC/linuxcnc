/********************************************************************
* Description: linklist.cc
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: LGPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change: 
* $Revision$
* $Author$
* $Date$
********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>		/* malloc() */
#include <string.h>		/* memcpy() */
#include <stdio.h>		/* fprintf(), stderr */
#ifdef __cplusplus
}
#endif
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
    last_data_retrieved = NULL;
    last_size_retrieved = 0;
    last_copied_retrieved = 0;
    list_size = 0;
    next_node_id = 1;
    delete_data_not_copied = 0;
    extra_node = new LinkedListNode(NULL, 0);
    max_list_size = 0;
    sizing_mode = NO_MAXIMUM_SIZE;
}

LinkedList::~LinkedList()
{
    flush_list();
    if (NULL != extra_node) {
	delete extra_node;
	extra_node = (LinkedListNode *) NULL;
    }
}

void LinkedList::set_list_sizing_mode(int _new_max_size,
    LIST_SIZING_MODE _new_sizing_mode)
{
    max_list_size = _new_max_size;
    sizing_mode = _new_sizing_mode;
}

void LinkedList::flush_list()
{
    LinkedListNode *next_node;
    current_node = head;
    while (NULL != current_node) {
	next_node = current_node->next;
	if ((current_node->copied || delete_data_not_copied)
	    && (NULL != current_node->data)) {
	    free(current_node->data);
	}
	delete current_node;
	current_node = next_node;
    }
    if (last_copied_retrieved) {
	if (last_data_retrieved != NULL) {
	    free(last_data_retrieved);
	    last_data_retrieved = NULL;
	    last_size_retrieved = 0;
	}
    }
    head = (LinkedListNode *) NULL;
    tail = (LinkedListNode *) NULL;
    list_size = 0;
    last_data_stored = NULL;
    last_size_stored = 0;
}

void LinkedList::delete_members()
{
    int old_delete_data_not_copied = delete_data_not_copied;
    delete_data_not_copied = 1;
    flush_list();
    delete_data_not_copied = old_delete_data_not_copied;
}

void *LinkedList::retrieve_head()
{
    LinkedListNode *next_node;

    if (NULL != head) {
	if (last_copied_retrieved) {
	    if (NULL != last_data_retrieved) {
		free(last_data_retrieved);
		last_data_retrieved = NULL;
		last_size_retrieved = 0;
	    }
	}
	last_data_retrieved = head->data;
	last_size_retrieved = head->size;
	last_copied_retrieved = head->copied;
	next_node = head->next;
	delete head;
	head = next_node;
	if (NULL != head) {
	    head->last = (LinkedListNode *) NULL;
	} else {
	    tail = (LinkedListNode *) NULL;
	}
	list_size--;
	return (last_data_retrieved);
    }
    return (NULL);
}

void *LinkedList::retrieve_tail()
{
    LinkedListNode *last_node;

    if (NULL != tail) {
	if (last_copied_retrieved) {
	    if (NULL != last_data_retrieved) {
		free(last_data_retrieved);
		last_data_retrieved = NULL;
		last_size_retrieved = 0;
	    }
	}
	last_data_retrieved = tail->data;
	last_size_retrieved = tail->size;
	last_copied_retrieved = tail->copied;
	last_node = tail->last;
	delete tail;
	tail = last_node;
	if (NULL != tail) {
	    tail->next = (LinkedListNode *) NULL;
	} else {
	    head = (LinkedListNode *) NULL;
	}
	list_size--;
	return (last_data_retrieved);
    }
    return (NULL);
}

int LinkedList::store_at_head(void *_data, size_t _size, int _copy)
{
    LinkedListNode *new_head;
    LinkedListNode *old_tail = tail;

    if (list_size >= max_list_size) {
	switch (sizing_mode) {
	case DELETE_FROM_TAIL:
	    if (NULL != tail) {
		tail = tail->last;
		if (NULL != tail) {
		    tail->next = (LinkedListNode *) NULL;
		} else {
		    head = (LinkedListNode *) NULL;
		    delete old_tail;
		    list_size = 0;
		    break;
		}
		delete old_tail;
		list_size--;
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
	last_data_stored = malloc(_size);
	memcpy(last_data_stored, _data, _size);
	last_size_stored = _size;
	new_head = new LinkedListNode(last_data_stored, _size);
    } else {
	last_data_stored = _data;
	last_size_stored = _size;
	new_head = new LinkedListNode(_data, _size);
    }
    if (NULL != new_head) {
	new_head->copied = _copy;
	new_head->id = next_node_id++;
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
	list_size++;
	return (head->id);
    } else {
	return (-1);
    }
}

int LinkedList::store_at_tail(void *_data, size_t _size, int _copy)
{
    LinkedListNode *new_tail;
    LinkedListNode *old_head = head;

    if (list_size >= max_list_size) {
	switch (sizing_mode) {
	case DELETE_FROM_HEAD:
	    if (NULL != head) {
		head = head->next;
		if (NULL != head) {
		    head->last = (LinkedListNode *) NULL;
		} else {
		    head = (LinkedListNode *) NULL;
		    delete old_head;
		    list_size = 0;
		    break;
		}
		delete old_head;
		list_size--;
	    }
	    break;

	case NO_MAXIMUM_SIZE:
	    break;

	case DELETE_FROM_TAIL:
	case STOP_AT_MAX:
	default:
	    fprintf(stderr, "LinkedList: Invalid list_sizing_mode.\n");
	    return (-1);
	}
    }

    if (_copy) {
	last_data_stored = malloc(_size);
	memcpy(last_data_stored, _data, _size);
	last_size_stored = _size;
	new_tail = new LinkedListNode(last_data_stored, _size);
    } else {
	last_data_stored = _data;
	last_size_stored = _size;
	new_tail = new LinkedListNode(last_data_stored, _size);
    }
    if (NULL != new_tail) {
	new_tail->copied = _copy;
	new_tail->id = next_node_id++;
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
	list_size++;
	return (tail->id);
    } else {
	fprintf(stderr,
	    "LinkedList: Couldn't create new node to store_at_tail.\n");
	return (-1);
    }
}

int LinkedList::store_after_current_node(void *_data, size_t _size, int _copy)
{
    LinkedListNode *new_node;
    LinkedListNode *old_tail = tail;
    LinkedListNode *old_head = head;

    if (list_size >= max_list_size) {
	switch (sizing_mode) {
	case DELETE_FROM_TAIL:
	    if (NULL != tail) {
		tail = tail->last;
		if (NULL != tail) {
		    tail->next = (LinkedListNode *) NULL;
		} else {
		    head = (LinkedListNode *) NULL;
		    delete old_tail;
		    list_size = 0;
		    break;
		}
		delete old_tail;
		list_size--;
	    }
	    break;

	case NO_MAXIMUM_SIZE:
	    break;

	case DELETE_FROM_HEAD:
	    if (NULL != head) {
		head = head->next;
		if (NULL != head) {
		    head->last = (LinkedListNode *) NULL;
		} else {
		    head = (LinkedListNode *) NULL;
		    delete old_head;
		    list_size = 0;
		    break;
		}
		delete old_head;
		list_size--;
	    }
	    break;
	case STOP_AT_MAX:
	default:
	    fprintf(stderr, "LinkedList: Invalid list_sizing_mode.\n");
	    return (-1);
	}
    }

    if (_copy) {
	last_data_stored = malloc(_size);
	memcpy(last_data_stored, _data, _size);
	last_size_stored = _size;
	new_node = new LinkedListNode(last_data_stored, _size);
    } else {
	last_data_stored = _data;
	last_size_stored = _size;
	new_node = new LinkedListNode(last_data_stored, _size);
    }
    if (NULL != new_node) {
	new_node->copied = _copy;
	new_node->id = next_node_id++;
	if (NULL == current_node) {
	    if (tail == NULL) {
		tail = new_node;
		if (NULL != head) {
		    fprintf(stderr,
			"LinkedList: Tail is NULL but the head is not.\n");
		    return (-1);
		}
		head = new_node;
	    }
	    current_node = tail;
	} else {
	    new_node->next = current_node->next;
	    if (current_node == extra_node) {
		new_node->last = current_node->last;
		if (NULL != current_node->last) {
		    current_node->last->next = new_node;
		} else {
		    head = new_node;
		}
	    } else {
		new_node->last = current_node;
	    }
	    current_node->next = new_node;
	    if (NULL != new_node->next) {
		new_node->next->last = new_node;
	    } else {
		tail = new_node;
	    }
	}
	list_size++;
	return (new_node->id);
    } else {
	fprintf(stderr,
	    "LinkedList: Couldn't create new node to store_after_current.\n");
	return (-1);
    }
}

int LinkedList::store_before_current_node(void *_data, size_t _size,
    int _copy)
{
    LinkedListNode *new_node;
    LinkedListNode *old_tail = tail;
    LinkedListNode *old_head = head;

    if (list_size >= max_list_size) {
	switch (sizing_mode) {
	case DELETE_FROM_TAIL:
	    if (NULL != tail) {
		tail = tail->last;
		if (NULL != tail) {
		    tail->next = (LinkedListNode *) NULL;
		} else {
		    head = (LinkedListNode *) NULL;
		    delete old_tail;
		    list_size = 0;
		    break;
		}
		delete old_tail;
		list_size--;
	    }
	    break;

	case NO_MAXIMUM_SIZE:
	    break;

	case DELETE_FROM_HEAD:
	    if (NULL != head) {
		head = head->next;
		if (NULL != head) {
		    head->last = (LinkedListNode *) NULL;
		} else {
		    head = (LinkedListNode *) NULL;
		    delete old_head;
		    list_size = 0;
		    break;
		}
		delete old_head;
		list_size--;
	    }
	    break;

	case STOP_AT_MAX:
	default:
	    fprintf(stderr, "LinkedList: Invalid list_sizing_mode.\n");
	    return (-1);
	}
    }

    if (_copy) {
	last_data_stored = malloc(_size);
	memcpy(last_data_stored, _data, _size);
	last_size_stored = _size;
	new_node = new LinkedListNode(last_data_stored, _size);
    } else {
	last_data_stored = _data;
	last_size_stored = _size;
	new_node = new LinkedListNode(last_data_stored, _size);
    }
    if (NULL != new_node) {
	new_node->copied = _copy;
	new_node->id = next_node_id++;
	if (NULL == current_node) {
	    if (tail == NULL) {
		tail = new_node;
		if (NULL != head) {
		    fprintf(stderr,
			"LinkedList: Tail is NULL but head is not.\n");
		    return (-1);
		}
		head = new_node;
	    }
	    current_node = tail;
	} else {
	    new_node->last = current_node->last;
	    if (current_node == extra_node) {
		new_node->next = current_node->next;
		if (NULL != current_node->next) {
		    current_node->next->last = new_node;
		} else {
		    tail = new_node;
		}
	    } else {
		new_node->next = current_node;
	    }
	    current_node->last = new_node;
	    if (NULL != new_node->last) {
		new_node->last->next = new_node;
	    } else {
		head = new_node;
	    }
	}
	list_size++;
	return (new_node->id);
    } else {
	fprintf(stderr,
	    "LinkedList: Couldn't create new node to store_before_current.\n");
	return (-1);
    }
}

void *LinkedList::get_head()
{
    current_node = head;
    if (NULL != current_node) {
	return (current_node->data);
    } else {
	return (NULL);
    }
}

void *LinkedList::get_tail()
{
    current_node = tail;
    if (NULL != current_node) {
	return (current_node->data);
    } else {
	return (NULL);
    }
}

void *LinkedList::get_next()
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

void *LinkedList::get_last()
{
    if (NULL != current_node) {
	current_node = current_node->last;
    }
    if (NULL != current_node) {
	return (current_node->data);
    } else {
	return (NULL);
    }
}

IS_EMPTY LinkedList::is_empty()
{
    if ((NULL == head) || (NULL == tail) || (list_size == 0)) {
	return (LIST_EMPTY);
    } else {
	return (LIST_NOT_EMPTY);
    }
}

void *LinkedList::get_by_id(int _id)
{
    LinkedListNode *temp;

    temp = head;
    while (NULL != temp) {
	if (temp->id == _id) {
	    return (temp->data);
	}
	temp = temp->next;
    }
    return (NULL);
}

void *LinkedList::get_first_newer(int _id)
{
    current_node = head;
    while (NULL != current_node) {
	if (current_node->id > _id) {
	    return (current_node->data);
	}
	current_node = current_node->next;
    }
    return (NULL);
}

void *LinkedList::get_last_newer(int _id)
{
    current_node = tail;
    while (NULL != current_node) {
	if (current_node->id > _id) {
	    return (current_node->data);
	}
	current_node = current_node->last;
    }
    return (NULL);
}

void LinkedList::delete_node(int _id)
{
    LinkedListNode *temp;

    temp = head;
    while (NULL != temp) {
	if (temp->id == _id) {
	    list_size--;
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
	    if ((temp->copied || delete_data_not_copied)
		&& (NULL != temp->data)) {
		free(temp->data);
	    }
	    delete temp;
	    break;
	}
	temp = temp->next;
    }
}

void LinkedList::delete_current_node()
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
	if ((temp->copied || delete_data_not_copied) && (NULL != temp->data)) {
	    free(temp->data);
	}
	delete temp;
	list_size--;
    }
}

int LinkedList::get_current_id()
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
