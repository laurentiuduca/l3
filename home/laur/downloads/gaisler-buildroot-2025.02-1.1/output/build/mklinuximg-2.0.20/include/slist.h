/*
 * Singly linked list help functions
 *
 * (C) Copyright 2009 Frontgrade Gaisler AB
 *
 *  Implements a singly linked list with head and tail
 *  pointers for fast insertions/deletions to head and
 *  tail in list.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#ifndef __LIB_SLIST_H__
#define __LIB_SLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/*! List description, Singly link list with head and tail pointers. */
struct slist {
	void	*head;	/*!< First entry in queue */
	void	*tail;	/*!< Last entry in queue */
	int	ofs;	/*!< Offset into head and tail to find next field */
	int     size;   /*!< Size of the list */
};

typedef struct slist slist_t;

/* Static initialization of list */
#define SLIST_INITIALIZER(type, field) {NULL, NULL, offsetof(type, field), 0}

/* Return the first element in list */
#define SLIST_HEAD(list, type) ((type *)(list)->head)

/* Return the last element in list */
#define SLIST_TAIL(list, type) ((type *)(list)->tail)

/* Get the next pointer of an entry */
#define SLIST_FIELD(list, entry) (*(void **)((char *)(entry) + (list)->ofs))

/* Return the next emlement in list */
#define SLIST_NEXT(list, entry, type) ((type *)(SLIST_FIELD(list, entry)))

/* Get size of list */
#define SLIST_SIZE(list) ((const int)(list)->size)

/* Iterate through all entries in list */
#define SLIST_FOR_EACH(list, entry, type) \
	for(entry=SLIST_HEAD(list, type); entry; entry=SLIST_NEXT(list, entry, type))

/*! Initialize a list during runtime 
 *
 * \param list The list to initialize
 * \param offset The number of bytes into the entry structure the next pointer is found
 */
extern void slist_init(struct slist *list, int offset);

/*! Clear list */
extern void slist_empty(struct slist *list, void (*dealloc)(void*));

/*! Add entry to front of list */
extern void slist_add_head(struct slist *list, void *entry);

/*! Add entry to end of list */
extern void slist_add_tail(struct slist *list, void *entry);

/*! Add multiple entries to end of list */
extern void slist_add_multiple_tail(struct slist *list, void *entries);

/*! Add all entries of list 'list2' to the tail of list 'list' 
 * The lists must be of the same type (or at least have the same offset
 * to the next pointer).
 */
extern void slist_join_tail(struct slist *list, struct slist *list2);

/*! Remove entry from front of list */
extern void slist_remove_head(struct slist *list);

/*! Remove entry from anywhere in list */
extern void slist_remove(struct slist *list, void *entry);

/*! Remove next entry in list (the entry after 'entry') */
extern void slist_remove_next(struct slist *list, void *entry);

#ifdef __cplusplus
}
#endif

#endif
