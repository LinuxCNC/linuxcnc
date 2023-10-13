//    Copyright 2014 Jeff Epler
//
//    Adapted from <linux/list.h>, Copyright various authors.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of version 2 of the GNU General Public
//    License as published by the Free Software Foundation.
//
//    To the extent that any portions of the following code can be identified
//    as written by Jeff Epler, those portions may be used under the terms of
//    (at your option) any later version of the GPL.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef RTAPI_LIST_H
#define RTAPI_LIST_H

#ifdef __KERNEL__
#include <linux/list.h>
#define rtapi_list_head list_head
#define rtapi_list_for_each(pos,head) list_for_each(pos,head)
#define rtapi_list_entry(ptr,type,member) list_entry(ptr,type,member)
#define rtapi_list_add list_add
#define rtapi_list_add_tail list_add_tail
#define rtapi_list_del list_del
#define RTAPI_INIT_LIST_HEAD INIT_LIST_HEAD
#else

#include <rtapi.h>

RTAPI_BEGIN_DECLS

struct rtapi_list_head {
        struct rtapi_list_head *next, *prev;
};

#define rtapi_list_for_each(pos, head) \
        for (pos = (head)->next; pos != (head); pos = pos->next)
#define rtapi_container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#define rtapi_list_entry(ptr, type, member) \
         rtapi_container_of(ptr, type, member)

static inline void rtapi__list_add(struct rtapi_list_head *new_,
                              struct rtapi_list_head *prev,
                              struct rtapi_list_head *next)
{
        next->prev = new_;
        new_->next = next;
        new_->prev = prev;
        prev->next = new_;
}

static inline void rtapi__list_del(struct rtapi_list_head * prev, struct rtapi_list_head * next)
{
        next->prev = prev;
        prev->next = next;
}

static inline void rtapi_list_add(struct rtapi_list_head *new_, struct rtapi_list_head *head)
{
        rtapi__list_add(new_, head, head->next);
}

static inline void rtapi_list_add_tail(struct rtapi_list_head *new_, struct rtapi_list_head *head)
{
        rtapi__list_add(new_, head->prev, head);
}

static inline void rtapi_list_del(struct rtapi_list_head *entry)
{
        rtapi__list_del(entry->prev, entry->next);
        entry->next = 0;
        entry->prev = 0;
}

static inline void RTAPI_INIT_LIST_HEAD(struct rtapi_list_head *list)
{
        list->next = list;
        list->prev = list;
}

RTAPI_END_DECLS

#endif

#endif
