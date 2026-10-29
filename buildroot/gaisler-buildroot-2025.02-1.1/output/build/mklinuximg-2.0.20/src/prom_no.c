/* PROM Node Operations
 *
 * Copyright (C) 2011 Frontgrade Gaisler AB
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <common.h>
#include <prom.h>
#include <prom_no.h>
#ifdef CONFIG_WATCHDOG
#include <prom_watchdog.h>
#endif
#include <string.h>

/* Next node. node=0 means get first node. */
int no_nextnode(int node)
{
#ifdef CONFIG_WATCHDOG
	watchdog_ping(PING_ID_PROM_NO);
#endif

	struct node *n = (struct node *)node;

	if (node == 0) {
		if (node_root == NULL)
			return -1;
		else
			return (int)node_root;
	} else
		return (int)n->sibling;
}

/* Get first child of a node */
int no_child(int node)
{
	struct node *n = (struct node *)node;

	if (node == 0)
		return -1;
	else
		return (int)n->child;
}

static struct prop_std *_prop_next(struct prop_std *p)
{
	struct propa_ptr *pa;

	/* Next Property */
	if ((p->options & PO_NONEXT) == 0)
		return (struct prop_std *)p->next;

	pa = (struct propa_ptr *)p;
	/* Property Array */
	if (pa->options & PO_END)
		p = NULL;
	else if (pa->options & PO_PTR)
		p = (void *)((unsigned long)p + sizeof(struct propa_ptr));
	else
		/* Property array with data inplace */
		p = (void *)&pa->value + ((p->length + 0x3) & ~0x3);
	return p;
}

static struct prop_std *prop_next(struct prop_std *p)
{
	do {
		p = _prop_next(p);
	} while(p && (p->options & PO_REMOVED));

	return p;
}

static struct prop_std *prop_find(struct node *n, const char *name)
{
	struct prop_std *p;

	if (n == NULL)
		return NULL;

	p = (struct prop_std *)n->props;
	while (p != NULL) {
		if (_strcmp((char *)name, p->name) == 0)
			return p;
		p = prop_next(p);
	}

	return NULL;
}

/* Add property last in node, last property must be "next-pointer" property */
void prop_add(struct node *n, struct prop *newprop)
{
	struct prop_std *p, *next;
	struct prop **pp;

	/* Find Last Pointer */
	pp = &n->props;
	p = (struct prop_std *)*pp;
	if (p != NULL) {
		while ((next = prop_next(p)) != NULL)
			p = next;
		pp = &p->next;
	}

	*pp = newprop;
}

/* Delete named property of node */
void prop_del(struct node *node, char *name)
{
	struct prop_std *p;

	p = prop_find(node, name);
	if (p)
		p->options |= PO_REMOVED;
}

/* A copy of source is created dynamically and added to the node,
 * If outprop is specified then it will be assigned the created prop.
 */
void prop_add_dup(struct node *n, struct prop_std *src, int size,
			struct prop_std **outprop)
{
	struct prop_std *prop;

	prop = (struct prop_std *)prom_malloc(size);
	_memcpy((void *)prop, (void *)src, size);
	prop_add(n, (struct prop *)prop);

	if (outprop)
		*outprop = prop;
}
/* Get node property by name */
int no_getprop(int node, const char *name, char *val)
{
	struct prop_std *p;
	void *value;

	p = prop_find((struct node *)node, name);
	if (p == NULL)
		return -1;

	value = &p->next;
	if ((p->options & PO_NONEXT) == 0)
		value += sizeof(struct prop *);
	if (p->options & PO_PTR)
		value = *(void **)value;
	_memcpy(val, value, p->length);
	return 1;
}

/* Get node property length */
int no_proplen(int node, const char *name)
{
	struct prop_std *p;

	p = prop_find((struct node *)node, name);
	if (p)
		return p->length;
	else
		return -1;
}

/* Set node property: not implemented */
int no_setprop(int node, const char *name, char *val, int len)
{
	return -1;
}

/* Next property of a node */
char *no_nextprop(int node, char *name)
{
	struct prop_std *p;
	struct node *n;

	if (node == 0)
		return NULL;
	if (!name || name[0] == '\0') {
		/* Return name of first property */
		n = (struct node *)node;
		if (!n->props)
			return NULL;
		return n->props->name;
	}

	p = prop_find((struct node *)node, name);
	if (p == NULL)
		return NULL;
	p = prop_next(p);
	if (p)
		return p->name;
	else
		return NULL;
}

void node_add_child(struct node *node, struct node *child)
{
	struct node **np = &node->child;

	while (*np)
		np = &((*np)->sibling);
	*np = child;
}
