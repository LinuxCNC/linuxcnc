
extern "C"
{

#include <stdlib.h>		/* malloc() */
#include <string.h>		/* memcpy() */
#include <stdio.h>		/* fprintf(), stderr */

}
#include "linklist.hh"		/* class RCS_LINKED_LIST */
RCS_LINKED_LIST_NODE::RCS_LINKED_LIST_NODE(void *_data, size_t _size)
{
    data = _data;
    size = _size;
    next = (RCS_LINKED_LIST_NODE *) NULL;
    last = (RCS_LINKED_LIST_NODE *) NULL;
}

RCS_LINKED_LIST_NODE::~RCS_LINKED_LIST_NODE()
{
}

RCS_LINKED_LIST::RCS_LINKED_LIST()
{
    head = (RCS_LINKED_LIST_NODE *) NULL;
    tail = (RCS_LINKED_LIST_NODE *) NULL;
    current_node = (RCS_LINKED_LIST_NODE *) NULL;
    extra_node = (RCS_LINKED_LIST_NODE *) NULL;
    last_data_retrieved = NULL;
    last_size_retrieved = 0;
    last_copied_retrieved = 0;
    list_size = 0;
    next_node_id = 1;
    delete_data_not_copied = 0;
    extra_node = new RCS_LINKED_LIST_NODE(NULL, 0);
    max_list_size = 0;
    sizing_mode = NO_MAXIMUM_SIZE;
}

RCS_LINKED_LIST::~RCS_LINKED_LIST()
{
    flush_list();
    if (NULL != extra_node) {
	delete extra_node;
	extra_node = (RCS_LINKED_LIST_NODE *) NULL;
    }
}

/** Used once by print.cc rcs_fputs() for printing to a list */
void RCS_LINKED_LIST::set_list_sizing_mode(int _new_max_size,
    LIST_SIZING_MODE _new_sizing_mode)
{
    max_list_size = _new_max_size;
    sizing_mode = _new_sizing_mode;
}

/** Called by RCS_LINKED_LIST destructor and delete_members() 
    But nothing outside this file uses it */
void RCS_LINKED_LIST::flush_list()
{
    RCS_LINKED_LIST_NODE *next_node;
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
    head = (RCS_LINKED_LIST_NODE *) NULL;
    tail = (RCS_LINKED_LIST_NODE *) NULL;
    list_size = 0;
    last_data_stored = NULL;
    last_size_stored = 0;
}

/** Used by CMS::internal_retrieve_diag_info() TCPMEM::get_diagnostics_info()
    and by the interp via NML_INTERP_LIST::clear() */
void RCS_LINKED_LIST::delete_members()
{
    int old_delete_data_not_copied = delete_data_not_copied;
    delete_data_not_copied = 1;
    flush_list();
    delete_data_not_copied = old_delete_data_not_copied;
}

/** Called by NML_INTERP_LIST::get() */
void *RCS_LINKED_LIST::retrieve_head()
{
    RCS_LINKED_LIST_NODE *next_node;

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
	    head->last = (RCS_LINKED_LIST_NODE *) NULL;
	} else {
	    tail = (RCS_LINKED_LIST_NODE *) NULL;
	}
	list_size--;
	return (last_data_retrieved);
    }
    return (NULL);
}

/* Called by NML::prefix_format_chain() */
int RCS_LINKED_LIST::store_at_head(void *_data, size_t _size, int _copy)
{
    RCS_LINKED_LIST_NODE *new_head;
    RCS_LINKED_LIST_NODE *old_tail = tail;

    if (list_size >= max_list_size) {
	switch (sizing_mode) {
	case DELETE_FROM_TAIL:
	    if (NULL != tail) {
		tail = tail->last;
		if (NULL != tail) {
		    tail->next = (RCS_LINKED_LIST_NODE *) NULL;
		} else {
		    head = (RCS_LINKED_LIST_NODE *) NULL;
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
	new_head = new RCS_LINKED_LIST_NODE(last_data_stored, _size);
    } else {
	last_data_stored = _data;
	last_size_stored = _size;
	new_head = new RCS_LINKED_LIST_NODE(_data, _size);
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
	    new_head->last = (RCS_LINKED_LIST_NODE *) NULL;
	    new_head->next = head;
	    head = new_head;
	}
	list_size++;
	return (head->id);
    } else {
	return (-1);
    }
}

/** Called by numerous functions */
int RCS_LINKED_LIST::store_at_tail(void *_data, size_t _size, int _copy)
{
    RCS_LINKED_LIST_NODE *new_tail;
    RCS_LINKED_LIST_NODE *old_head = head;

    if (list_size >= max_list_size) {
	switch (sizing_mode) {
	case DELETE_FROM_HEAD:
	    if (NULL != head) {
		head = head->next;
		if (NULL != head) {
		    head->last = (RCS_LINKED_LIST_NODE *) NULL;
		} else {
		    head = (RCS_LINKED_LIST_NODE *) NULL;
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
	    fprintf(stderr, "RCS_LINKED_LIST: Invalid list_sizing_mode.\n");
	    return (-1);
	}
    }

    if (_copy) {
	last_data_stored = malloc(_size);
	memcpy(last_data_stored, _data, _size);
	last_size_stored = _size;
	new_tail = new RCS_LINKED_LIST_NODE(last_data_stored, _size);
    } else {
	last_data_stored = _data;
	last_size_stored = _size;
	new_tail = new RCS_LINKED_LIST_NODE(last_data_stored, _size);
    }
    if (NULL != new_tail) {
	new_tail->copied = _copy;
	new_tail->id = next_node_id++;
	if (NULL == tail) {
	    tail = new_tail;
	    if (NULL != head) {
		fprintf(stderr,
		    "RCS_LINKED_LIST: Tail is NULL but head is not.\n");
		return (-1);
	    }
	    head = new_tail;
	} else {
	    tail->next = new_tail;
	    new_tail->last = tail;
	    new_tail->next = (RCS_LINKED_LIST_NODE *) NULL;
	    tail = new_tail;
	}
	list_size++;
	return (tail->id);
    } else {
	fprintf(stderr,
	    "RCS_LINKED_LIST: Couldn't create new node to store_at_tail.\n");
	return (-1);
    }
}

/** Used by numerous functions */
void *RCS_LINKED_LIST::get_head()
{
    current_node = head;
    if (NULL != current_node) {
	return (current_node->data);
    } else {
	return (NULL);
    }
}

/** Called by CMS::internal_retrieve_diag_info() TCPMEM::get_diagnostics_info() */
void *RCS_LINKED_LIST::get_tail()
{
    current_node = tail;
    if (NULL != current_node) {
	return (current_node->data);
    } else {
	return (NULL);
    }
}

/** Used by numerous functions */
void *RCS_LINKED_LIST::get_next()
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
void RCS_LINKED_LIST::delete_node(int _id)
{
    RCS_LINKED_LIST_NODE *temp;

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

/** Used by numerous functions */
void RCS_LINKED_LIST::delete_current_node()
{
    if (NULL != current_node && (current_node != extra_node)) {
	RCS_LINKED_LIST_NODE *temp;
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

/** Used by LOCMEM::LOCMEM() only */
int RCS_LINKED_LIST::get_current_id()
{
    if (current_node == NULL) {
	return (-1);
    }
    return (current_node->id);
}

// Constructor defined private to prevent copying.
RCS_LINKED_LIST::RCS_LINKED_LIST(RCS_LINKED_LIST & list)
{
}
