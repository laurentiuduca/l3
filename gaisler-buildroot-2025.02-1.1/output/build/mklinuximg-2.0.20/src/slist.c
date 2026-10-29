/*
 * Singly linked list implementation
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

#include <stdlib.h>
#include <stdio.h>
#include "slist.h"

/* LIST interface */

void slist_init(struct slist *list, int offset)
{
	list->head = list->tail = NULL;
	list->ofs = offset;
	list->size = 0;
}

void slist_empty(struct slist *list, void (*dealloc)(void*))
{
        if (dealloc) {
                void *prev = NULL;
                void *entry = list->head;
                while (entry) {
                        prev = entry;
                        entry = SLIST_FIELD(list, entry);
                        dealloc(prev);
                }
        }

	list->head = list->tail = NULL;
	list->size = 0;
}

void slist_add_head(struct slist *list, void *entry)
{
	SLIST_FIELD(list, entry) = list->head;
	if ( list->head == NULL )
		list->tail = entry;
	list->head = entry;
	list->size++;
}

void slist_add_tail(struct slist *list, void *entry)
{
	if ( list->tail == NULL ) {
		list->head = entry;
	} else {
		SLIST_FIELD(list, list->tail) = entry;
	}
	SLIST_FIELD(list, entry) = NULL;
	list->tail = entry;
	list->size++;
}

void slist_add_multiple_tail(struct slist *list, void *entries)
{
	void *curr;

	/* Fixup end of current list */
	if ( list->tail == NULL ) {
		list->head = entries;
	} else {
		SLIST_FIELD(list, list->tail) = entries;
	}

	/* Find last entry (tail) of new entries */
	curr = entries;
	while ( SLIST_FIELD(list, curr) ) {
		curr = SLIST_FIELD(list, curr);
		list->size++;
	}

	/* Fixup end of new list */
	list->tail = curr;
}

void slist_join_tail(struct slist *list, struct slist *list2)
{
	printf("SLIST_JOIN_TAIL: Calling unimplemented function\n");
	exit(0);
}

void slist_remove_head(struct slist *list)
{
        if (list->head) {
                list->head = SLIST_FIELD(list, list->head);
                if ( list->head == NULL ) {
                        list->tail = NULL;
                }
                list->size--;
        }
}

void slist_remove(struct slist *list, void *entry)
{
	void **prevptr = &list->head;
	void *curr, *prev;

	prev = NULL;
	curr = list->head;
	while ( curr != entry ) {
		prev = curr;
		prevptr = &SLIST_FIELD(list, curr);
		curr = SLIST_FIELD(list, curr);
	}
	*prevptr = SLIST_FIELD(list, entry);
	if ( list->tail == entry )
		list->tail = prev;
	list->size--;
}

void slist_remove_next(struct slist *list, void *entry)
{
	void *next;

	next = SLIST_FIELD(list, entry);

	// Remove Next
	SLIST_FIELD(list, entry) = SLIST_FIELD(list, next);

	if ( list->tail == next )
		list->tail = entry;
	list->size--;
}
