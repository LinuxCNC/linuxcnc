#ifndef HAL_LIST_H
#define HAL_LIST_H

// HAL doubly-linked list
// private API - obtained by including hal_priv.h


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
static inline void list_init_entry(hal_list_t * entry);
static inline hal_list_t *list_prev(hal_list_t * entry);
static inline hal_list_t *list_next(hal_list_t * entry);
static inline void list_add_after(hal_list_t * entry, hal_list_t * prev);
static inline void list_add_before(hal_list_t * entry, hal_list_t * next);
static inline hal_list_t *list_remove_entry(hal_list_t * entry);


static inline hal_list_t *list_prev(hal_list_t * entry)
{
    /* this function is only needed because of memory mapping */
    return (hal_list_t *) SHMPTR(entry->prev);
}

static inline hal_list_t *list_next(hal_list_t * entry)
{
    /* this function is only needed because of memory mapping */
    return (hal_list_t *) SHMPTR(entry->next);
}

static inline void list_init_entry(hal_list_t * entry)
{
    int entry_n;

    entry_n = SHMOFF(entry);
    entry->next = entry_n;
    entry->prev = entry_n;
}

static inline void list_add_after(hal_list_t * entry, hal_list_t * prev)
{
    int entry_n, prev_n, next_n;
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

static inline void list_add_before(hal_list_t * entry, hal_list_t * next)
{
    int entry_n, prev_n, next_n;
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

static inline hal_list_t *list_remove_entry(hal_list_t * entry)
{
    int entry_n;
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
