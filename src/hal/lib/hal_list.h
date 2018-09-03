#ifndef HAL_LIST_H
#define HAL_LIST_H

// HAL doubly-linked list
// private API - obtained by including hal_priv.h


/** HAL "list element" data structure.
    This structure is used to implement generic double linked circular
    lists.  Such lists have the following characteristics:
    1) One "dummy" element that serves as the root of the list.
    2) 'next' and 'previous' pointers are never NULL.
    3) Insertion and removal of elements is clean and fast.
    4) No special case code to deal with empty lists, etc.
    5) Easy traversal of the list in either direction.
    This structure has no data, only links.  To use it, include it
    inside a larger structure.
*/
typedef struct {
    shmoff_t next;			/* next element in list */
    shmoff_t prev;			/* previous element in list */
} hal_list_t;

/** These functions are used to manipulate double-linked circular lists.
    Every list entry has pointers to the next and previous entries.
    The pointers are never NULL.  If an entry is not in a list its
    pointers point back to itself (which effectively makes it a list
    with only one entry)

    'list_init_entry()' sets the pointers in the list entry to point
    to itself - making it a legal list with only one entry. It should
    be called when a list entry is first allocated.

    'list_prev()' and 'list_next()' simply return a pointer to the
    list entry that precedes or follows 'entry' in the list. If there
    is only one element in the list, they return 'entry'.

    'list_add_after()' adds 'entry' immediately after 'prev'.
    Entry must be a single entry, not in a larger list.

    'list_add_before()' adds 'entry' immediately before 'next'.
    Entry must be a single entry, not in a larger list.

    'list_remove_entry()' removes 'entry' from any list it may be in.
    It returns a pointer to the next entry in the list.  If 'entry'
    was the only entry in the list, it returns 'entry'.
*/
static inline void dlist_init_entry(hal_list_t * entry);
/* static inline hal_list_t *list_prev(const hal_list_t * entry); */
/* static inline hal_list_t *list_next(const hal_list_t * entry); */
static inline void dlist_add_after(hal_list_t * entry, hal_list_t * prev);
static inline void dlist_add_before(hal_list_t * entry, hal_list_t * next);
static inline hal_list_t *dlist_remove_entry(hal_list_t * entry);

/**
 * dlist_for_each	-	iterate over a list
 * @pos:	the &hal_list_t to use as a loop counter.
 * @head:	the head for your list.
 */
#define dlist_for_each(pos, head) \
    for (pos = SHMPTR((head)->next); pos != (head);	\
	 pos = SHMPTR(pos->next))


/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &hal_list_t to use as a loop counter.
 * @n:		another &hal_list_t to use as temporary storage
 * @head:	the head for your list.
 */
#define dlist_for_each_safe(pos, n, head) \
    for (pos = SHMPTR((head)->next), n = SHMPTR(pos->next); pos != (head); \
	     pos = n, n = SHMPTR(pos->next))

/**
 * dlist_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define dlist_entry(ptr, type, member)		\
    container_of(ptr, type, member)

#define dlist_first_entry(ptr, type, member)	\
    dlist_entry(SHMPTR((ptr)->next), type, member)

#define dlist_next_entry(pos, member)				\
    dlist_entry(SHMPTR((pos)->member.next), typeof(*(pos)), member)



/**
 * dlist_for_each_entry  -       iterate over list of given type
 * @pos:        the type * to use as a loop counter.
 * @head:       the head for your list.
 * @member:     the name of the list_struct within the struct.
 */
#define dlist_for_each_entry(pos, head, member)				\
    for (pos = dlist_entry(SHMPTR((head)->next), typeof(*pos), member);	\
	 &pos->member != (head);					\
	 pos = dlist_entry(SHMPTR(pos->member.next), typeof(*pos), member))

/**
 * dlist_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:        the type * to use as a loop cursor.
 * @n:          another type * to use as temporary storage
 * @head:       the head for your list.
 * @member:     the name of the list_head within the struct.
 */
#define dlist_for_each_entry_safe(pos, n, head, member)			\
    for (pos = dlist_first_entry(head, typeof(*pos), member),		\
	     n = dlist_next_entry(pos, member);				\
	 &pos->member != (head);					\
	 pos = n, n = dlist_next_entry(n, member))

static inline hal_list_t *dlist_prev(const hal_list_t * entry)
{
    /* this function is only needed because of memory mapping */
    return (hal_list_t *) SHMPTR(entry->prev);
}

static inline hal_list_t *dlist_next(const hal_list_t * entry)
{
    /* this function is only needed because of memory mapping */
    return (hal_list_t *) SHMPTR(entry->next);
}

static inline shmoff_t dlist_empty(const hal_list_t *head)
{
    return dlist_next(head) == head;
}

static inline shmoff_t dlist_empty_careful(const hal_list_t *head)
{
    hal_list_t *next = dlist_next(head);
    return (next == head) && (next == dlist_prev(head));
}
static inline void dlist_init_entry(hal_list_t * entry)
{
    shmoff_t entry_n;

    entry_n = SHMOFF(entry);
    entry->next = entry_n;
    entry->prev = entry_n;
}

static inline void dlist_add_after(hal_list_t * entry, hal_list_t * prev)
{
    shmoff_t entry_n, prev_n, next_n;
    hal_list_t *next;

    /* messiness needed because of memory mapping */
    entry_n = SHMOFF(entry);
    prev_n = SHMOFF(prev);
    next_n = prev->next;
    next = (hal_list_t *)SHMPTR(next_n);
    /* insert the entry */
    entry->next = next_n;
    entry->prev = prev_n;
    prev->next = entry_n;
    next->prev = entry_n;
}

static inline void dlist_add_before(hal_list_t * entry, hal_list_t * next)
{
    shmoff_t entry_n, prev_n, next_n;
    hal_list_t *prev;

    /* messiness needed because of memory mapping */
    entry_n = SHMOFF(entry);
    next_n = SHMOFF(next);
    prev_n = next->prev;
    prev = (hal_list_t *)SHMPTR(prev_n);
    /* insert the entry */
    entry->next = next_n;
    entry->prev = prev_n;
    prev->next = entry_n;
    next->prev = entry_n;
}

static inline hal_list_t *dlist_remove_entry(hal_list_t * entry)
{
    shmoff_t entry_n;
    hal_list_t *prev, *next;

    /* messiness needed because of memory mapping */
    entry_n = SHMOFF(entry);
    prev = (hal_list_t *)SHMPTR(entry->prev);
    next = (hal_list_t *)SHMPTR(entry->next);
    /* remove the entry */
    prev->next = entry->next;
    next->prev = entry->prev;
    entry->next = entry_n;
    entry->prev = entry_n;
    return next;
}

#endif /* HAL_LIST_H */
