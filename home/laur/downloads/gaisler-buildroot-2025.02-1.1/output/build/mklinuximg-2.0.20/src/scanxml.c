/*
 * This program generates C code from an XML file containing instructions on
 * adding, changing and removing nodes and properties in the prom tree.
 *
 * (C) Copyright 2013 Frontgrade Gaisler AB
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

/*
 * NOTE: The policy on any failure is to print out an error message and abort
 * the program. Memory is in general not freed as most data structures are used
 * until the program finishes or is aborted.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <expat.h>
#include <ctype.h>

#include "slist.h"

#ifdef XML_UNICODE
#error "UTF-16 configuration of expat is not supported"
#endif

#define BUFSIZE 4096

#define IERR "INTERNAL ERROR: "

#define MATCH_INDEX_ALL	"-1"
#define CHFIXUP_DUMMY	"-1"
#define PROP_NAME	"name"

#define DEBUG_SECTION_HEADER \
	"/*\n================ Debug output ==========================\n"
#define DEBUG_SECTION_DIVIDER \
	"============================================================\n"

enum matchtype {
	MATCHTYPE_CORE = 0,
	MATCHTYPE_BUS,
	MATCHTYPE_ROOT,
	MATCHTYPE_NONE,
};

enum action {
	ACTION_ADD = 0,
	ACTION_REMOVE,
	ACTION_MATCH,
	ACTION_PLACEHOLDER,
};

enum proptype {
	PROPTYPE_EMPTY = 0,
	PROPTYPE_INT,
	PROPTYPE_STRING,
};

enum pvaltype {
	PVALTYPE_INT = 0,	/* belongs to PROPTYPE_INT */
	PVALTYPE_COREHANDLE,	/* belongs to PROPTYPE_INT */
	PVALTYPE_NODEHANDLE,	/* belongs to PROPTYPE_INT */
	PVALTYPE_STRING,	/* belongs to PROPTYPE_STRING */
};

/* Node handle target */
enum nhtarget {
	NHTARGET_REF,	/* The nodehandle target is a named nodelabel */
	NHTARGET_CORE	/* The nodehandle target is the node of the core */
};

/* Node label type */
enum nltype {
	NLTYPE_UNKNOWN	= 0,      /* Unknown */
	NLTYPE_NODE	= 1 << 0, /* Node label */
	NLTYPE_CORE	= 1 << 1, /* Core label */
};

#define NLTYPE_ALL (NLTYPE_NODE | NLTYPE_CORE)

enum proptype pvaltype_to_proptype(enum pvaltype type)
{
	switch(type) {
	case PVALTYPE_INT:
	case PVALTYPE_COREHANDLE:
	case PVALTYPE_NODEHANDLE:
		return PROPTYPE_INT;
	case PVALTYPE_STRING:
	default:
		return PROPTYPE_STRING;
	}
}

struct propval {
	/* Type of this propval.  */
	enum pvaltype type;

	/* Pointer to value of this propval
	 * For corehandle: either ref string or NULL for current core
	 */
	char *value;

	void *next; /* slist next pointer in parents propval list */

	void *intnext;  /* slist next in priv->intlist */
	int intindex;

	enum nhtarget nhtarget;
	void *nhnext;  /* slist next in priv->nodehandles and match->nhfixups */
};

struct property {
	enum action action;
	int id; /* Running id for properties */

	enum proptype type; /* list type - some types collapsed to others */
	char *name;

	struct slist values; /* Can be empty for an empty property */

	void *next; /* slist next under owning match/node */

	/* Code generation */
	void *gnext; /* slist next in global list of properties */
	int gindex;
};

struct node {
	struct slist properties;
	struct slist nodes;

	enum action action;
	int id; /* Running id for nodes */
	char *name;

	char *nodelabel;
	enum nltype nltype;

	void *next; /* slist next under owning match/node */

	/* Code generation */
	void *gnext; /* slist next in global list of nodes */
	int gindex;
	struct slist nhfixups;
};

/* Match types are mutually exclusive */
struct match {
	struct node *placeholder;

	enum matchtype matchtype;
	enum action action;
	int id; /* Running id for matches */

	/* Vendor/device type core match */
	char *vendor;
	char *device;
	char *index;

	/* Coreindex type core match */
	char *coreindex;

	/* Bus match */
	char *bus;

	/* Code generation */
	void *next; /* slist next pointer */
};

struct stackframe {
	struct taginfo *ti;

	void *data; /* Data structure being built by corresponding tag */

	void *parent_frame; /* slist next pointer */
};

struct scanpriv {
	XML_Parser parser;

	int depth;
	int aborted;

	struct slist stack;

	struct match *currmatch;

	struct slist matches;
	int matchcount;
	int nodecount;
	int propcount;

	/* text recording */
	int trecord;
	char *tbuf; /* Internal buffer */
	int tbuflen;
	char *text; /* Pointer into tbuf */
	int textlen;

	struct slist intlist;
	struct slist nodehandles;
	int intcount;

	/* Code generation */
	int nhcount;

	/* Debug flags */
	int dbg_parsing;
	int dbg_parse_tree;
};

static void initpriv(struct scanpriv *priv) {
	memset((char *)priv, 0, sizeof(struct scanpriv));
	priv->depth = 0;

	slist_init(&priv->stack, offsetof(struct stackframe, parent_frame));
	slist_init(&priv->matches, offsetof(struct match, next));
	slist_init(&priv->intlist, offsetof(struct propval, intnext));
	slist_init(&priv->nodehandles, offsetof(struct propval, nhnext));
}


/* ============================================================ */
/* Tag attributes */

#define MATCH_VENDOR	"vendor"
#define MATCH_DEVICE	"device"
#define MATCH_INDEX	"index"
#define MATCH_COREINDEX	"coreindex"
#define MATCH_BUS	"bus"

/* ============================================================ */
/* Unchanging (after initialization) tag info */

enum tag {
	TAG_MATCHES = 0,
	TAG_MATCHCORE,
	TAG_DELCORE,
	TAG_MATCHBUS,
	TAG_CORELABEL,
	TAG_ADDNODE,
	TAG_ADDPROP,
	TAG_DELPROP,
	TAG_INT,
	TAG_COREHANDLE,
	TAG_STRING,
	TAG_MATCHROOT,
	TAG_NODELABEL,
	TAG_NODEHANDLE,
	NUMBER_OF_TAGS		/* MUST be last one */
};

#define ATTR_NAME	"name"
#define ATTR_REF	"ref"

typedef int (*starth_t)(struct scanpriv *priv, struct stackframe *frame,
			const char **attr);
typedef	int (*endh_t)(struct scanpriv *priv, struct stackframe *frame);

typedef	void (*docfunc_t)(struct taginfo *ti);

struct taginfo {
	enum tag tag;
	const char *name;
	const char *docstr;
	docfunc_t docfunc;
	int validchildren[NUMBER_OF_TAGS];
	int validparent;

	starth_t starthandler;
	endh_t endhandler;
};

/* Global taginfo array, not to be changed after initialization */
struct taginfo taginfos[NUMBER_OF_TAGS];

/* ============================================================ */
/* Stack functions */

static int stack_empty(struct scanpriv *priv)
{
	return SLIST_SIZE(&priv->stack) == 0;
}

static int stack_push(struct scanpriv *priv, struct taginfo *ti)
{
	struct stackframe *current;

	if (!ti) {
		fprintf(stderr, IERR "Stack frames must have ti\n");
		return 1;
	}

	current = calloc(1, sizeof(struct stackframe));
	if (!current) {
		fprintf(stderr, "Could not allocate stack frame\n");
		return 1;
	}

	current->ti = ti;

	slist_add_head(&priv->stack, current);

	return 0;
}

static struct stackframe *stack_pop(struct scanpriv *priv)
{
	struct stackframe *current = NULL;
	struct slist *stack = &priv->stack;

	if (stack_empty(priv)) {
		fprintf(stderr, IERR "Trying to pop empty stack!\n");
	} else {
		current = SLIST_HEAD(stack, struct stackframe);
		slist_remove_head(stack);
	}

	return current;
}

static struct stackframe *stack_peek(struct scanpriv *priv)
{
	return SLIST_HEAD(&priv->stack, struct stackframe);
}

static struct stackframe *stack_parent(struct scanpriv *priv,
				       struct stackframe *frame)
{
	if (!frame) {
		fprintf(stderr, IERR "Trying to get parent of NULL frame!\n");
		return NULL;
	}
	return SLIST_NEXT(&priv->stack, frame, struct stackframe);
}

enum tag get_upper_tag(struct scanpriv *priv, struct stackframe *frame)
{
	return stack_parent(priv, frame)->ti->tag;
}

static void *get_upper_data(struct scanpriv *priv, struct stackframe *frame)
{
	return stack_parent(priv, frame)->data;
}

static struct node *get_parent_from_stack(struct scanpriv *priv,
					  struct stackframe *currframe)
{
	enum tag tag = get_upper_tag(priv, currframe);

	if (tag != TAG_MATCHCORE && tag != TAG_MATCHBUS && tag != TAG_ADDNODE &&
	    tag != TAG_MATCHROOT) {
		fprintf(stderr, IERR "cannot get parent from type %d\n", tag);
		return NULL;
	}
	return (struct node *)get_upper_data(priv, currframe);
}

static struct property *get_property_from_stack(struct scanpriv *priv,
						struct stackframe *currframe)
{
	enum tag tag = get_upper_tag(priv, currframe);

	if (tag != TAG_ADDPROP) {
		fprintf(stderr, IERR "cannot get property from type %d\n",
			tag);
		return NULL;
	}
	return (struct property *)get_upper_data(priv, currframe);
}

/* ============================================================ */
/* General helpers */

/* Returns non-0 if string is not a valid number in its entirety */
static int string_to_int(int *dest, const char *str)
{
	char *end;
	long val = strtol(str, &end, 0);

	if (dest)
		*dest = (int)val;

	/* Returns error if zero length string or not entire string used */
	return (*str == '\0') || (*end != '\0');
}

/*
 * Returns whether str could be a macro define that will be resolved later by
 * the C compiler.
 */
static int valid_macro_string(const char *str)
{
	if (!isascii(*str) || !(isalpha(*str) || *str == '_'))
		return 0; /* First character can not be number */

	for (str++; *str; str++)
		if (!isascii(*str) || !(isalnum(*str) || *str == '_'))
			return 0; /* Found bad character */

	return 1;
}

#define STRINGTYPE_INT		0x01
#define STRINGTYPE_MACRO	0x02
#define STRINGTYPE_INT_OR_MACRO	(STRINGTYPE_INT | STRINGTYPE_MACRO)

/* Checks flags in an OR fashion unless no flags are chosen */
static int check_string_type(const char *str, unsigned types)
{
	if (!types)  /* No checks */
		return 0;
	if ((types | STRINGTYPE_INT) && !string_to_int(NULL, str))
		return 0;
	if ((types | STRINGTYPE_MACRO) && valid_macro_string(str))
		return 0;

	fprintf(stderr, "String \"%s\" is not one of: ", str);
	if (types | STRINGTYPE_INT)
		fprintf(stderr, "int, ");
	if (types | STRINGTYPE_MACRO)
		fprintf(stderr, "macro constant, ");
	return 1;
}

static char *dup_string(const char *str)
{
	char *dup;

	dup = strdup(str);
	if (!dup)
		fprintf(stderr,
			"Could not allocate memory for duplication of \"%s\"\n",
			str);
	return dup;
}

static char *dup_string_type(const char *str, unsigned types)
{
	if (check_string_type(str, types))
		return NULL;

	return dup_string(str);
}

enum tag pvaltype_to_tag(enum pvaltype pvaltype)
{
	switch (pvaltype) {
	case PVALTYPE_STRING:
		return TAG_STRING;
	case PVALTYPE_INT:
		return TAG_INT;
	case PVALTYPE_COREHANDLE:
		return TAG_COREHANDLE;
	case PVALTYPE_NODEHANDLE:
		return TAG_NODEHANDLE;
	}
	return NUMBER_OF_TAGS;
}

/* ============================================================ */
/* Parse tree building */

/*
 * No NULL values allowed
 */
static struct property *new_property(struct scanpriv *priv, struct node *parent,
				     enum action action, const char *name)
{
	struct property *p;

	/* Step 1 - Allocate */
	p = calloc(1, sizeof(struct property));
	if (!p) {
		fprintf(stderr, "Could not allocate property\n");
		return NULL;
	}

	p->name = dup_string(name);
	if (!p->name)
		return NULL;

	/* Step 2 - Initialize */
	slist_init(&p->values, offsetof(struct propval, next));
	p->action = action;

	/* Step 3 - Build tree */
	slist_add_tail(&parent->properties, p);

	return p;
}

static struct propval *new_propval(struct scanpriv *priv, struct property *p,
				   enum pvaltype pvtype, const char *val)
{
	struct propval *pv;
	enum proptype ptype = pvaltype_to_proptype(pvtype);

	if (p->type == PROPTYPE_EMPTY) {
		p->type = ptype;
	} else if (p->type != ptype) {
		fprintf(stderr,
			"Array properties cannot contain different types\n");
		return NULL;
	}

	/* Step 1 - Allocate */
	pv = calloc(1, sizeof(struct propval));
	if (!pv) {
		fprintf(stderr, "Could not allocate propval\n");
		return NULL;
	}

	if (val) {
		pv->value = dup_string(val);
		if (!pv->value)
			return NULL;
	}

	/* Step 2 - Initialize */
	pv->type = pvtype;

	/* Step 3 - Build tree */
	slist_add_tail(&p->values, pv);

	return pv;
}

/* No arguments are allowed to be NULL */
static struct propval *new_stringval(struct scanpriv *priv, struct property *p,
				     const char* val)
{
	return new_propval(priv, p, PVALTYPE_STRING, val);
}

/* No arguments are allowed to be NULL */
static struct propval *new_intval(struct scanpriv *priv, struct property *p,
				  const char* val)
{
	if(check_string_type(val, STRINGTYPE_INT))
		return NULL;

	return new_propval(priv, p, PVALTYPE_INT, val);
}

/* ref is are allowed to be NULL */
static struct propval *new_handleval(struct scanpriv *priv,
					 struct stackframe *frame,
					 struct property *p, const char* ref,
					 enum nhtarget target,
					 enum pvaltype pvaltype)
{
	struct propval *pv;
	pv = new_propval(priv, p, pvaltype, ref);
	if (!pv)
		return NULL;

	pv->nhtarget = target;
	slist_add_tail(&priv->nodehandles, pv);

	return pv;
}

/* NULL parent and name is OK for placeholder nodes */
static struct node *new_node(struct scanpriv *priv, struct node *parent,
			     enum action action, const char *name)
{
	struct node *n;

	/* Step 1 - Allocate */
	n = calloc(1, sizeof(struct node));
	if (!n) {
		fprintf(stderr, "Could not allocate node\n");
		return NULL;
	}

	if (name) {
		n->name = dup_string(name);
		if (!n->name)
			return NULL;
	}

	/* Step 2 - Initialize */
	slist_init(&n->nodes, offsetof(struct node, next));
	slist_init(&n->properties, offsetof(struct property, next));
	slist_init(&n->nhfixups, offsetof(struct propval, nhnext));
	n->action = action;

	if (name) {
		struct property *p;
		struct propval *pv;

		/* Add implicit name property... */
		p = new_property(priv, n, ACTION_ADD, PROP_NAME);
		if (!p)
			return NULL;
		/* ...with the name as value */
		pv = new_stringval(priv, p, name);
		if (!pv)
			return NULL;
	}


	/* Step 3 - Build tree */
	if (parent)
		slist_add_tail(&parent->nodes, n);

	return n;
}

static struct match *new_match(struct scanpriv *priv, enum matchtype matchtype,
			       enum action action)
{
	struct match *m;

	m = calloc(1, sizeof(struct match));
	if (!m) {
		fprintf(stderr, "Could not allocate match\n");
		return NULL;
	}

	m->placeholder = new_node(priv, NULL, ACTION_PLACEHOLDER, NULL);
	if (!m->placeholder)
		return NULL;

	m->matchtype = matchtype;
	m->action = action;
	slist_add_tail(&priv->matches, m);

	return m;
}

static struct match *new_core_match(struct scanpriv *priv, enum action action)
{
	struct match *m;

	m = new_match(priv, MATCHTYPE_CORE, action);

	return m;
}

/*
 * The index argument can be NULL to match all indices. No other NULL values
 * allowed.
 */
static struct match *new_vendev_match(struct scanpriv *priv, enum action action,
				      const char *vendor, const char *device,
				      const char *index)
{
	struct match *m;

	m = new_core_match(priv, action);
	if (!m)
		return NULL;

	m->vendor = dup_string_type(vendor, STRINGTYPE_INT_OR_MACRO);
	m->device = dup_string_type(device, STRINGTYPE_INT_OR_MACRO);
	m->index = dup_string_type((index ? index : MATCH_INDEX_ALL),
				   STRINGTYPE_INT);
	if (!m->vendor || !m->device || !m->index)
		return NULL;

	return m;
}

/*
 * No NULL values allowed
 */
static struct match *new_coreindex_match(struct scanpriv *priv,
					 enum action action,
					 const char *coreindex)
{
	struct match *m;

	m = new_core_match(priv, action);
	if (!m)
		return NULL;

	m->coreindex = dup_string_type(coreindex, STRINGTYPE_INT);
	if (!m->coreindex)
		return NULL;

	return m;
}

static struct match *new_bus_match(struct scanpriv *priv, enum action action,
				   const char *bus)
{
	struct match *m;

	m = new_match(priv, MATCHTYPE_BUS, action);
	if (m)
		m->bus = dup_string_type(bus, STRINGTYPE_INT);

	return m;
}

static struct match *new_root_match(struct scanpriv *priv, enum action action,
				    const char *bus)
{
	struct match *m;

	m = new_match(priv, MATCHTYPE_ROOT, action);

	return m;
}

/* ============================================================ */
/* Documentation */

static void doc_matchcore(struct taginfo *ti)
{
	printf("   Matches a core. There are two variants:\n");
	printf("   1) Matching on vendor and device (and optionally index)\n");
	printf("      A mandatory \"%s\" attribute is set to match the core's AMBA Plug&Play\n", MATCH_VENDOR);
	printf("      vendor number and a mandatory \"%s\" attribute is set to match the\n", MATCH_DEVICE);
	printf("      core's AMBA Plug&Play device number. The values for these two can be\n");
	printf("      written wither in two possible ways, either as numbers (C style decimal,\n");
	printf("      octal and hexadecimal notations are supported) or as the defined\n");
	printf("      constants in include/ambapp_ids.h. If there is more than one core in the\n");
	printf("      system with the same vendor:device ID, an optional \"%s\" attribute can\n", MATCH_INDEX);
	printf("      be set to select which one of those counting from 0 in the AMBA Plug&Play\n");
	printf("      scan order. If the \"%s\" attribute is not set all cores with the given\n", MATCH_INDEX);
	printf("      vendor:device ID are matched.\n");
	printf("   2) Matching on coreindex\n");
	printf("      A mandatory \"%s\" attribute is set to the AMBA Plug&Play index\n", MATCH_COREINDEX);
	printf("      number of the core.\n");
}

static void doc_delcore(struct taginfo *ti)
{
	struct taginfo *mc = &taginfos[TAG_MATCHCORE];

	printf("   Deletes a core from the prom tree. There are two variants with \"%s\",\n", MATCH_VENDOR);
	printf("   \"%s\" and optional \"%s\" attributes on one hand and a \"%s\"\n",
	       MATCH_DEVICE, MATCH_INDEX, MATCH_COREINDEX);
	printf("   attribute on the other hand, just as for the <%s> tag.\n", mc->name);
}

static void doc_corelabel(struct taginfo *ti)
{
	struct taginfo *ch = &taginfos[TAG_COREHANDLE];

	printf("   A mandatory \"name\" attribute sets a label for the parent core that is used\n");
	printf("   by <%s> tags added elsewhere to be able to reference that parent\n", ch->name);
	printf("   core. Only one corelabel can be set per match and the label name needs to be\n");
	printf("   unique among all corelabels and nodelabels.\n");
}

static void doc_addnode(struct taginfo *ti)
{
	struct taginfo *an = &taginfos[TAG_ADDNODE];

	printf("   Adds a child node to the prom tree node of the matched core, or parent node\n");
	printf("   when nested withing an <%s>. The value of the mandatory \"%s\"\n", an->name, ATTR_NAME);
	printf("   attribute names the node. Furthermore, this adds, under this node, a string\n");
	printf("   property with the name \"name\" with that same value.\n");
}

static void doc_addprop(struct taginfo *ti)
{
	struct taginfo *tint = &taginfos[TAG_INT];
	struct taginfo *tch = &taginfos[TAG_COREHANDLE];
	struct taginfo *tstr = &taginfos[TAG_STRING];
	struct taginfo *tnh = &taginfos[TAG_NODEHANDLE];

	printf("   Adds a property. A mandatory \"%s\" attribute names the property. A property\n", ATTR_NAME);
	printf("   can be of three different kinds: empty, integer or string. A property\n");
	printf("   becomes empty if it has no child tags. Child tags of type <%s>, <%s>\n", tint->name, tch->name);
	printf("   or <%s> makes the property integer type, whereas child tags of type\n", tnh->name);
	printf("   <%s> makes it string type. Setting more than one child tag makes the\n", tstr->name);
	printf("   property an array of values, but it can only have integer type or string\n");
	printf("   type values, not both.\n");
	printf("      If the name attribute matches an existing property of the core, the new\n");
	printf("   value will replace the old value. This can even change the property from one\n");
	printf("   type to another.\n");
}

static void doc_delprop(struct taginfo *ti)
{
	printf("   Deletes a property. A mandatory \"%s\" attribute names the property that is\n", ATTR_NAME);
	printf("   to be deleted.\n");
}

static void doc_int(struct taginfo *ti)
{
	printf("   An integer value of an added property. The character data between the\n");
	printf("   starting <%s> and the ending </%s> needs to be a valid integer (C style\n", ti->name, ti->name);
	printf("   decimal, octal and hexadecimal notations are supported).\n");

}

static void doc_corehandle(struct taginfo *ti)
{
	struct taginfo *cl = &taginfos[TAG_CORELABEL];

	printf("   An integer value of an added property that contains the address of the prom\n");
	printf("   tree node of the referenced core. To reference a core that is labeled using\n");
	printf("   the <%s> tag, the \"%s\" attribute should have the value of that\n", cl->name, ATTR_REF);
	printf("   label. If the \"%s\" attribute is omitted, the corehandle will reference the\n", ATTR_REF);
	printf("   core that the property is a descendant of.\n");
}

static void doc_string(struct taginfo *ti)
{
	printf("   An string value of an added property. The character data between the\n");
	printf("   starting <%s> and the ending </%s> is taken as the string value.\n", ti->name, ti->name);
}

static void doc_matchbus(struct taginfo *ti)
{
	printf("   Matches a bus. A mandatory \"%s\" attribute names the bus number to match\n", MATCH_BUS);
}

static void doc_matchroot(struct taginfo *ti)
{
	printf("   Matches the root node.\n");
}

static void doc_nodelabel(struct taginfo *ti)
{
	struct taginfo *ch = &taginfos[TAG_NODEHANDLE];
	const char *nh_name = ch->name;

	printf("   A mandatory \"name\" attribute sets a label for the parent node that is used\n");
	printf("   by <%s> tags added elsewhere to be able to reference that parent\n", nh_name);
	printf("   node. Only one nodelabel can be set per node and the label name needs to be\n");
	printf("   unique among all nodelabels and corelabels.\n");
}

static void doc_nodehandle(struct taginfo *ti)
{
	struct taginfo *cl = &taginfos[TAG_NODELABEL];

	printf("   An integer value of an added property that contains the address of the prom\n");
	printf("   tree node of the referenced node. To reference a node that is labeled using\n");
	printf("   the <%s> tag, the \"%s\" attribute should have the value of that\n", cl->name, ATTR_REF);
	printf("   label.\n");
}

static void doc_print()
{
	struct taginfo *ti;
	struct taginfo *tj;
	int i, j;

	printf("The xml file format consists of the following tags:\n");
	for (i = 0; i < NUMBER_OF_TAGS; i++) {
		ti = &taginfos[i];
		printf("- <%s>\n", ti->name);
	}
	printf("\n");

	for (i = 0; i < NUMBER_OF_TAGS; i++) {
		ti = &taginfos[i];
		printf("<%s>\n", ti->name);
		if (ti->docfunc) {
			ti->docfunc(ti);
		} else if (ti->docstr) {
			printf("   %s\n", ti->docstr);
		} else {
			fprintf(stderr, IERR "Tag %d has no documentation\n",
				ti->tag);
			return;
		}
		/* printf("\n"); */

		if (ti->tag != TAG_MATCHES) {
			printf("Valid as a child of:\n");
			for (j = 0; j < NUMBER_OF_TAGS; j++) {
				tj = &taginfos[j];
				if (tj->validchildren[i]) {
					printf("- <%s>\n", tj->name);
				}
			}
		}
		/* printf("\n"); */

		if (ti->validparent) {
			printf("Valid children:\n");
			for (j = 0; j < NUMBER_OF_TAGS; j++) {
				tj = &taginfos[j];
				if (ti->validchildren[j]) {
					printf("- <%s>\n", tj->name);
				}
			}
		} else {
			printf("No valid children\n");
		}
		printf("\n");
		/* printf("\n\n"); */
	}
}


/* ============================================================ */
/* Tag handling */

#define for_each_attribute(name, value, i, attr) \
	for (i=0, name = attr[i], value = attr[i+1]; name; i += 2, name = attr[i], value = attr[i+1])

static const char *get_attribute(const char *name, const char **attr)
{
	const char *aname;
	const char *aval;
	int i;

	for_each_attribute (aname, aval, i, attr)
		if (strcmp(name, aname) == 0)
			return aval;

	return NULL; /* Attribute not found */
}

static const char *get_required_attribute(const char *name, const char **attr)
{
	const char *val;

	val = get_attribute(name, attr);
	if(!val) {
		fprintf(stderr, "Missing attribute \"%s\"\n", name);
		return NULL;
	}

	return val;
}

static int start_match(struct scanpriv *priv, struct stackframe *frame,
		       const char **attr)
{
	struct match *m;
	const char *vendor;
	const char *device;
	const char *index;
	const char *coreindex;
	const char *bus;
	enum matchtype matchtype;
	enum action action;

	vendor = get_attribute(MATCH_VENDOR, attr);
	device = get_attribute(MATCH_DEVICE, attr);
	index = get_attribute(MATCH_INDEX, attr);
	coreindex = get_attribute(MATCH_COREINDEX, attr);
	bus = get_attribute(MATCH_BUS, attr);

	if (frame->ti->tag == TAG_MATCHCORE) {
		matchtype = MATCHTYPE_CORE;
		action = ACTION_MATCH;
	} else if (frame->ti->tag == TAG_DELCORE) {
		matchtype = MATCHTYPE_CORE;
		action = ACTION_REMOVE;
	} else if (frame->ti->tag == TAG_MATCHBUS) {
		matchtype = MATCHTYPE_BUS;
		action = ACTION_MATCH;
	} else if (frame->ti->tag == TAG_MATCHROOT) {
		matchtype = MATCHTYPE_ROOT;
		action = ACTION_MATCH;
	} else {
		fprintf(stderr, IERR "Unknown match tag type %d\n",
			frame->ti->tag);
		return 1;
	}

	if (matchtype == MATCHTYPE_CORE) {
		if ((vendor || device || index) && coreindex) {
			fprintf(stderr, "Conflicting core match attributes\n");
			return 1;
		} else if (vendor) {
			/* Vendor/device type match */
			if (!device) {
				fprintf(stderr, "Match attribute \"%s\" missing\n",
					MATCH_DEVICE);
				return 1;
			}
			m = new_vendev_match(priv, action, vendor, device, index);
		} else if (coreindex) {
			/* Coreindex type match */
			m = new_coreindex_match(priv, action, coreindex);
		} else {
			fprintf(stderr, "Match attributes missing\n");
			return 1;
		}
	} else if (matchtype == MATCHTYPE_BUS) {
		if (!bus) {
			fprintf(stderr, "Missing bus attribute\n");
			return 1;
		}
		m = new_bus_match(priv, action, bus);
	} else if (matchtype == MATCHTYPE_ROOT) {
		m = new_root_match(priv, action, bus);
	}

	if (!m)
		return 1;

	priv->currmatch = m;
	frame->data = m->placeholder;

	return 0;
}

static int start_addnode(struct scanpriv *priv, struct stackframe *frame,
			 const char **attr)
{
	struct node *n;
	struct node *parent;
	const char *name;

	parent = get_parent_from_stack(priv, frame);
	if (!parent)
		return 1;

	name = get_required_attribute(ATTR_NAME, attr);
	if (!name)
		return 1;

	n = new_node(priv, parent, ACTION_ADD, name);
	if (!n)
		return 1;

	frame->data = n;

	return 0;
}

static int start_prop(struct scanpriv *priv, struct stackframe *frame,
		      const char **attr)
{
	struct property *p;
	struct node *parent;
	enum action action;
	const char *name;

	if (frame->ti->tag == TAG_ADDPROP) {
		action = ACTION_ADD;
	} else if (frame->ti->tag == TAG_DELPROP) {
		action = ACTION_REMOVE;
	} else {
		fprintf(stderr, IERR "start_prop: invalid tag %d\n",
			frame->ti->tag);
		return 1;
	}

	parent = get_parent_from_stack(priv, frame);
	if (!parent)
		return 1;

	name = get_required_attribute(ATTR_NAME, attr);
	if (!name)
		return 1;

	p = new_property(priv, parent, action, name);
	if (!p)
		return 1;

	frame->data = p;

	return 0;
}

static void start_text_recording(struct scanpriv *priv);
static const char *stop_text_recording(struct scanpriv *priv);

static int trigger_text_recording(struct scanpriv *priv,
				  struct stackframe *frame,
				  const char **attr)
{
	start_text_recording(priv);

	return 0;
}

static int end_int(struct scanpriv *priv, struct stackframe *frame)
{
	struct property *p;
	struct propval *pv;
	const char *text;

	p = get_property_from_stack(priv, frame);
	if (!p)
		return 1;

	text = stop_text_recording(priv);
	pv = new_intval(priv, p, text);
	if (!pv)
		return 1;

	return 0;
}

static int start_corehandle(struct scanpriv *priv, struct stackframe *frame,
			    const char **attr)
{
	struct property *p;
	struct propval *pv;
	const char *ref;
	enum nhtarget nhtarget = NHTARGET_REF;

	p = get_property_from_stack(priv, frame);
	if (!p)
		return 1;

	ref = get_attribute(ATTR_REF, attr); /* Allowed to be NULL */
	if (!ref)
		nhtarget = NHTARGET_CORE;

	pv = new_handleval(priv, frame, p, ref, nhtarget, PVALTYPE_COREHANDLE);
	if (!pv)
		return 1;

	return 0;
}

static int end_string(struct scanpriv *priv, struct stackframe *frame)
{
	struct property *p;
	struct propval *pv;
	const char *text;

	p = get_property_from_stack(priv, frame);
	if (!p)
		return 1;

	text = stop_text_recording(priv);
	pv = new_stringval(priv, p, text);
	if (!pv)
		return 1;

	return 0;
}

static struct node* find_node(struct node *parent,
			      const char *label,
			      unsigned int nltype_mask)
{
	struct node *child;
	struct node *n;

	if (!label || !parent || nltype_mask == NLTYPE_UNKNOWN)
		return NULL;

	/* Check if the current node match the label */
	if (parent->nodelabel && (parent->nltype & nltype_mask) &&
	    strcmp(parent->nodelabel, label) == 0)
		return parent;

	/* And the same for each child */
	SLIST_FOR_EACH (&parent->nodes, child, struct node) {
		n = find_node(child, label, nltype_mask);
		if (n)
			return n;
	}

	return NULL;
}

static struct node *find_any_node_by_name(struct scanpriv *priv,
					  const char *name)
{
	struct match *o;
	struct node *n;

	SLIST_FOR_EACH (&priv->matches, o, struct match) {
		/* Check that no other node use the same label */
		n = find_node(o->placeholder, name, NLTYPE_ALL);
		if (n)
			return n;
	}

	return NULL;
}

static int start_label(struct scanpriv *priv, struct stackframe *frame,
		       const char **attr, enum nltype nltype)
{
	struct node *node = NULL;
	const char *name = get_required_attribute(ATTR_NAME, attr);

	if (!name)
		return 1;

	/* Enforce uniqueness of labels regardless of node label type */
	node = find_any_node_by_name(priv, name);
	if (node) {
		struct taginfo *ti = NULL;

		switch (node->nltype) {
		case NLTYPE_NODE:
			ti = &taginfos[TAG_NODEHANDLE];
			break;
		case NLTYPE_CORE:
			ti = &taginfos[TAG_COREHANDLE];
			break;
		case NLTYPE_UNKNOWN:
			break;
		}
		fprintf(stderr, "A %s with name=\"%s\" is already defined\n",
			ti ? ti->name : "<unknown tag>", name);
		return 1;
	}

	node = get_upper_data(priv, frame);
	if (!node) {
		fprintf(stderr, "%s is not supported by parent tag\n",
				frame->ti->name);
		return 1;
	}

	/* Check if the node already have a label */
	if (node->nodelabel) {
		enum tag upper_tag = get_upper_tag(priv, frame);
		struct taginfo *ti = &taginfos[upper_tag];
		struct taginfo *nlti = &taginfos[TAG_NODELABEL];
		struct taginfo *clti = &taginfos[TAG_CORELABEL];

		if (node->nltype == nltype)
			fprintf(stderr, "Only one <%s> is allowed under <%s>\n",
				frame->ti->name, ti->name);
		else
			fprintf(stderr, "Only one of <%s> or <%s> can be defined under <%s>.\n",
				clti->name, nlti->name, ti->name);

		return 1;
	}

	node->nodelabel = dup_string(name);
	node->nltype = nltype;

	if (!node->nodelabel)
		return 1;

	return 0;
}

static int start_nodelabel(struct scanpriv *priv, struct stackframe *frame,
			   const char **attr)
{
	return start_label(priv, frame, attr, NLTYPE_NODE);
}

static int start_corelabel(struct scanpriv *priv, struct stackframe *frame,
			   const char **attr)
{
	return start_label(priv, frame, attr, NLTYPE_CORE);
}

static int start_nodehandle(struct scanpriv *priv, struct stackframe *frame,
			    const char **attr)
{
	struct property *p;
	struct propval *pv;
	const char *ref;
	enum nhtarget nhtarget = NHTARGET_REF;

	p = get_property_from_stack(priv, frame);
	if (!p)
		return 1;

	ref = get_attribute(ATTR_REF, attr); /* Not allowed to be NULL */
	if (!ref) {
		fprintf(stderr, "Nodehandle must have a 'ref' attribute\n");
		return 1;
	}

	pv = new_handleval(priv, frame, p, ref, nhtarget, PVALTYPE_NODEHANDLE);
	if (!pv)
		return 1;

	return 0;
}

/* int start_xxxx(struct scanpriv *priv, struct stackframe *frame, */
/* 	const char **attr) */
/* { */
/* 	return 0; */
/* } */
/*  */
/* int end_xxxx(struct scanpriv *priv, struct stackframe *frame) */
/* { */
/* 	return 0; */
/* } */

static struct taginfo *init_tag(int tag, const char *name,
				starth_t starth, endh_t endh)
{
	struct taginfo *ti = &taginfos[tag];

	ti->name = name;
	ti->starthandler = starth;
	ti->endhandler = endh;

	return ti;
}

static void set_simple_doc(struct taginfo *ti, const char *docstr)
{
	ti->docstr = docstr;
}

static void set_doc_function(struct taginfo *ti, docfunc_t docfunc)
{
	ti->docfunc = docfunc;
}


static void valid_child(struct taginfo *ti, int childtag)
{
	ti->validchildren[childtag] = 1;
	ti->validparent = 1;
}

/*
 * Initializes taginfos and returns 0 on success.
 *
 * NOTE: Some tags have the same tag parse functions (like TAG_MATCHCORE vs
 * TAG_DELCORE and TAG_ADDPROP vs TAG_DELPROP). However, they do have different
 * sets of valid child types and are differentiated by the different tag type at
 * code generation.
 */
static int init_tags(void) {
	struct taginfo *ti;
	int i;

	/* TI_VALIDCHILD operates on the taginfo for the last TI_INIT */

	ti = init_tag(TAG_MATCHES, "matches", NULL, NULL); /* Nothing to do for top tag */
	set_simple_doc(ti, "Top level tag");
	valid_child(ti, TAG_MATCHCORE);
	valid_child(ti, TAG_DELCORE);
	valid_child(ti, TAG_MATCHBUS);
	valid_child(ti, TAG_MATCHROOT);

	/*
	 * Several tags uses the same tag parse function start_match, but has
	 * different allowed child types and differs at code generation.
	 */
	ti = init_tag(TAG_MATCHCORE, "match-core", start_match, NULL);
	set_doc_function(ti, doc_matchcore);
	valid_child(ti, TAG_ADDNODE);
	valid_child(ti, TAG_ADDPROP);
	valid_child(ti, TAG_DELPROP);
	valid_child(ti, TAG_CORELABEL);
	valid_child(ti, TAG_NODELABEL);

	ti = init_tag(TAG_DELCORE, "del-core", start_match, NULL);
	set_doc_function(ti, doc_delcore);

	ti = init_tag(TAG_MATCHBUS, "match-bus", start_match, NULL);
	set_doc_function(ti, doc_matchbus);
	valid_child(ti, TAG_ADDNODE);

	ti = init_tag(TAG_MATCHROOT, "match-root", start_match, NULL);
	set_doc_function(ti, doc_matchroot);
	valid_child(ti, TAG_ADDNODE);

	ti = init_tag(TAG_CORELABEL, "corelabel", start_corelabel, NULL);
	set_doc_function(ti, doc_corelabel);

	ti = init_tag(TAG_ADDNODE, "add-node", start_addnode, NULL);
	set_doc_function(ti, doc_addnode);
	valid_child(ti, TAG_ADDPROP);
	valid_child(ti, TAG_ADDNODE);
	valid_child(ti, TAG_NODELABEL);

	ti = init_tag(TAG_ADDPROP, "add-prop", start_prop, NULL);
	set_doc_function(ti, doc_addprop);
	valid_child(ti, TAG_INT);
	valid_child(ti, TAG_COREHANDLE);
	valid_child(ti, TAG_STRING);
	valid_child(ti, TAG_NODEHANDLE);

	/*
	 * del-prop uses the same tag parse functions as add-prop, but has no
	 * allowed child types and will differ at code generation having a
	 * different tag
	 */
	ti = init_tag(TAG_DELPROP, "del-prop", start_prop, NULL);
	set_doc_function(ti, doc_delprop);

	ti = init_tag(TAG_INT, "int", trigger_text_recording, end_int);
	set_doc_function(ti, doc_int);

	ti = init_tag(TAG_COREHANDLE, "corehandle", start_corehandle, NULL);
	set_doc_function(ti, doc_corehandle);

	ti = init_tag(TAG_STRING, "string", trigger_text_recording, end_string);
	set_doc_function(ti, doc_string);

	ti = init_tag(TAG_NODELABEL, "nodelabel", start_nodelabel, NULL);
	set_doc_function(ti, doc_nodelabel);

	ti = init_tag(TAG_NODEHANDLE, "nodehandle", start_nodehandle, NULL);
	set_doc_function(ti, doc_nodehandle);

	for (i = 0; i < NUMBER_OF_TAGS; i++) {
		ti = &taginfos[i];
		ti->tag = i;
		if (!ti->name) {
			fprintf(stderr, "Tag with index %d uninitialized\n", i);
			return 1;
		}
		if (!ti->starthandler && !ti->endhandler && ti->tag != TAG_MATCHES) {
			fprintf(stderr, IERR "No handlers for tag %s\n",
				ti->name);
			return 1;
		}
		if (!ti->docstr && !ti->docfunc) {
			fprintf(stderr, IERR "No documentation for tag %s\n",
				ti->name);
			return 1;
		}
	}

	return 0;
}

/* ============================================================ */
/* Generic tag handling */

static int handle_starttag(struct scanpriv *priv, struct taginfo *ti,
			   const char **attr)
{
	/* Check for malformed document */
	if (stack_empty(priv)) {
		if (ti->tag != TAG_MATCHES) {
			fprintf(stderr, "Tag '%s' can not be at top level\n",
				ti->name);
			return 1;
		}
	} else {
		struct taginfo *pti;

		pti = stack_peek(priv)->ti;
		if (!pti->validchildren[ti->tag]) {
			fprintf(stderr, "Tag '%s' can not be child of tag %s\n",
				ti->name, pti->name);
			return 1;
		}
	}

	/* A specific handler function should take care of storing attr's if
	 * necessary */
	if(stack_push(priv, ti)) {
		return 1;
	}

	if (ti->starthandler)
		return ti->starthandler(priv, stack_peek(priv), attr);

	return 0;
}

static int handle_endtag(struct scanpriv *priv, const char *name)
{
	struct taginfo *ti;
	struct stackframe *frame;


	/*
	 * Expat checks for matching tags, so we don't have to match name to a
	 * struct taginfo, but we check sanity nevertheless
	 */
	if (stack_empty(priv)) {
		fprintf(stderr, IERR "Empty stack parsing end tag '%s'\n",
			name);
		return 1;
	}
	frame = stack_peek(priv);
	ti = frame->ti;
	if (strcmp(name, ti->name) != 0) {
		fprintf(stderr, IERR "End tag '%s' != stack top tag '%s'\n",
			name, ti->name);
		return 1;
	}

	if (ti->endhandler)
		if (ti->endhandler(priv, frame))
			return 1;

	free(stack_pop(priv));

	return 0;
}

static void abort_parsing(struct scanpriv *priv)
{
	XML_StopParser(priv->parser, 0);
	priv->aborted = 1;
}

static void starttag(void *data, const char *name, const char **attr)
{
	struct scanpriv *priv = (struct scanpriv *)data;
	int i;
	int handled;
	int err = 0;

	if (priv->aborted)
		return;

	if (priv->dbg_parsing) {
		printf("#");
		for (i = 0; i < priv->depth; i++)
			printf("  ");
		printf("<%s", name);
		for (i = 0; attr[i]; i += 2) {
			printf(" %s=\"%s\"", attr[i], attr[i + 1]);
		}
		printf(">\n");
	}

	/* Handle */
	handled = 0;
	for (i = 0; i < NUMBER_OF_TAGS; i++) {
		struct taginfo *ti = &taginfos[i];

		if (strcmp(name, ti->name) == 0) {
			err = handle_starttag(priv, ti, attr);
			handled = 1;
		}
	}
	if (!handled) {
		fprintf(stderr, "Unknown tag '%s'\n", name);
		err = 1;
	}

	if (err) {
		fprintf(stderr, "Aborting at start tag '%s'\n", name);
		abort_parsing(priv);
	}

	priv->depth++;
}

static void endtag(void *data, const char *name)
{
	struct scanpriv *priv = (struct scanpriv *)data;
	int i;

	if (priv->aborted)
		return;

	priv->depth--;

	if (priv->dbg_parsing) {
		printf("#");
		for (i = 0; i < priv->depth; i++)
			printf("  ");
		printf("</%s>\n", name);
	}

	if (handle_endtag(priv, name)) {
		fprintf(stderr, "Aborting at end tag '%s'\n", name);
		abort_parsing(priv);
	}
}

static void start_text_recording(struct scanpriv *priv)
{
	priv->trecord = 1;

	priv->text = priv->tbuf;
	priv->text[0] = '\0';
	priv->textlen = 0;
}

static const char *stop_text_recording(struct scanpriv *priv)
{
	char *s;

	priv->trecord = 0;

	/* Trim whitespace and point out with text */

	if (priv->textlen > 0) {
		/* Trim at end - text points to the final '\0' at this point */
		for (s = priv->text - 1; isspace(*s); s--)
			; /* Nothing */
		*(s+1) = '\0';

		/* Trim at beginning */
		for (s = priv->tbuf; isspace(*s); s++)
			; /* Nothing */
		priv->text = s;
	} else {
		priv->text = priv->tbuf;
	}

	return priv->text;
}

static void texthandler(void *data, const char *text, int len)
{
	struct scanpriv *priv = (struct scanpriv *)data;

	if (!priv->trecord || priv->aborted)
		return;

	if (priv->textlen + len + 1 > priv->tbuflen) {
		int newlen = priv->tbuflen * 2;
		char *newbuf = malloc(newlen);

		if (!newbuf) {
			fprintf(stderr,
				"Failed to expand character data buffer\n");
			abort_parsing(priv);
		}

		strncpy(newbuf, priv->tbuf, priv->textlen + 1);
		priv->tbuf = newbuf;
		priv->tbuflen = newlen;
		priv->text = priv->tbuf + priv->textlen;
	}

	strncpy(priv->text, text, len);
	priv->text += len;
	priv->textlen += len;
	priv->text[0] = '\0';
}

/* ============================================================ */
/* Preparation for code generation */

static void prindent(int indent)
{
	int i;

	for (i = 0; i < indent; i++)
		printf("   ");
}

static int prepare_int(struct scanpriv *priv, struct propval *pv, int indent)
{
	if (priv->dbg_parse_tree && indent >= 0) {
		prindent(indent);
		printf("int: %s\n", pv->value);
	}

	slist_add_tail(&priv->intlist, pv);
	pv->intindex = priv->intcount++;

	return 0;
}

static int prepare_nodehandle(struct scanpriv *priv, struct propval *pv,
			      int indent)
{
	struct node *n = NULL;
	struct match *m;
	struct taginfo *ti = NULL;
	enum tag tag = pvaltype_to_tag(pv->type);
	enum nltype nltype = NLTYPE_UNKNOWN;

	if (tag == NUMBER_OF_TAGS)
		return 1;

	ti = &taginfos[tag];

	if (priv->dbg_parse_tree) {
		prindent(indent);
		switch (pv->nhtarget) {
		case NHTARGET_REF:
			if (!pv->value) {
				printf("NHTARGET_REF but no ref is set!?\n");
				return 1;
			}
			printf("%s ref=\"%s\"\n", ti->name, pv->value);
			break;
		case NHTARGET_CORE:
			printf("%s to Node %d\n", ti->name,
			       priv->currmatch->placeholder->id);
			break;
		}
	}

	/* Book a place for a dummy value to be filled in later */
	if (prepare_int(priv, pv, -1))
		return 1;

	/* Find match that needs to fill in value later */
	switch (pv->nhtarget) {
	case NHTARGET_CORE:
		n = priv->currmatch->placeholder;
		break;
	case NHTARGET_REF:
		if (!pv->value) {
			fprintf(stderr, "'Missing reference in %s.\n",
				ti->name);
			return 1;
		}

		/* Get the node label type for the property type */
		switch (pv->type) {
		case PVALTYPE_NODEHANDLE:
			nltype = NLTYPE_NODE;
			break;
		case PVALTYPE_COREHANDLE:
			nltype = NLTYPE_CORE;
			break;
		case PVALTYPE_INT:
		case PVALTYPE_STRING:
			return 1;
		}

		/* Try to find a matching node with the label and type */
		SLIST_FOR_EACH (&priv->matches, m, struct match) {
			n = find_node(m->placeholder, pv->value, nltype);
			if (n)
				break;
		}

		if (!n) {
			fprintf(stderr, "%s \"%s\" undefined\n", ti->name,
				pv->value);
			return 1;
		}
		break;
	}

	if (!n)
		return 1;

	slist_add_tail(&n->nhfixups, pv);

	return 0;
}

static int prepare_propval(struct scanpriv *priv, struct propval *pv,
			   int indent)
{
	switch(pv->type) {
	case PVALTYPE_INT:
		return prepare_int(priv, pv, indent);

	case PVALTYPE_COREHANDLE:
	case PVALTYPE_NODEHANDLE:
		return prepare_nodehandle(priv, pv, indent);

	case PVALTYPE_STRING:
		if (priv->dbg_parse_tree) {
			prindent(indent);
			printf("string: \"%s\"\n", pv->value);
		}
		return 0;
	}

	fprintf(stderr, "Unknown propval type %d\n", pv->type);
	return 1;
}

static int prepare_property(struct scanpriv *priv, struct property *p,
			     int indent)
{
	struct propval *pv;

	p->id = priv->propcount++;

	if (priv->dbg_parse_tree) {
		prindent(indent);
		printf("%s property: %s\n",
		       (p->action == ACTION_ADD ? "add" : "remove"), p->name);
	}

	SLIST_FOR_EACH (&p->values, pv, struct propval) {
		if (prepare_propval(priv, pv, indent + 1))
			return 1;
	}

	return 0;
}

static int prepare_node(struct scanpriv *priv, struct node *n, int indent)
{
	struct node *child;
	struct property *p;

	n->id = priv->nodecount++;

	if (priv->dbg_parse_tree) {
		prindent(indent);
		printf("Node %d:", n->id);
		switch (n->action) {
		case ACTION_ADD:
			printf("add \"%s\"\n", n->name);
			break;
		case ACTION_REMOVE:
			printf("remove \"%s\"\n", n->name);
			break;
		case ACTION_MATCH:
		default:
			printf("\n");
		}
	}

	SLIST_FOR_EACH (&n->properties, p, struct property) {
		if (prepare_property(priv, p, indent + 1))
			return 1;
	}

	SLIST_FOR_EACH (&n->nodes, child, struct node) {
		if (prepare_node(priv, child, indent + 1))
			return 1;
	}

	return 0;
}

static int prepare_match(struct scanpriv *priv, struct match *m)
{
	m->id = priv->matchcount++;
	priv->currmatch = m;

	if (priv->dbg_parse_tree) {
		printf("Match %d: ", m->id);
		switch (m->matchtype) {
		case MATCHTYPE_CORE:
			if (m->vendor)
				printf("vendor=%s device=%s index=%s\n",
				       m->vendor, m->device, m->index);
			else if (m->coreindex)
				printf("coreindex=%s\n", m->coreindex);
			break;
		case MATCHTYPE_BUS:
			if (m->bus)
				printf("Bus %s", m->bus);
			printf("\n");
			break;
		case MATCHTYPE_ROOT:
			printf("Root\n");
			break;
		case MATCHTYPE_NONE:
			printf("\n");
			break;
		}
	}

	return prepare_node(priv, m->placeholder, 1);
}

static int prepare(struct scanpriv *priv)
{
	struct match *m;

	SLIST_FOR_EACH (&priv->matches, m, struct match) {
		if(prepare_match(priv, m))
			return 1;
	}
	if (priv->dbg_parse_tree)
		printf(DEBUG_SECTION_DIVIDER);

	return 0;
}


/* ============================================================ */
/* Code generation */

#define FPRINTF(args...)			\
	do {					\
		if (fprintf(args) < 0)		\
			return 1;		\
	} while(0)


static int generate_matchline(FILE *fout, struct scanpriv *priv,
			      struct match *m)
{
	char *flags1;
	char *flags2;

	FPRINTF(fout, "/* Match %d */\t{", m->id);
	flags1 = NULL;
	flags2 = "";
	if (m->matchtype == MATCHTYPE_CORE) {
		if (m->vendor)
			FPRINTF(fout, "%s, %s, %s, 0, 0, ", m->vendor, m->device, m->index);
		else
			FPRINTF(fout, "0, 0, 0, %s, 0, ", m->coreindex);

		flags1 = "XMLMATCH_CORE";
		if (m->action == ACTION_REMOVE)
			flags2 = " | XMLMATCH_REMOVE";
	} else if (m->matchtype == MATCHTYPE_BUS) {
		FPRINTF(fout, "0, 0, 0, 0, %s, ", m->bus);
		flags1 = "XMLMATCH_BUS";
	} else if (m->matchtype == MATCHTYPE_ROOT) {
		FPRINTF(fout, "0, 0, 0, 0, 0, ");
		flags1 = "XMLMATCH_ROOT";
	} else {
		fprintf(stderr, IERR "No matchline generation for matchtype %d\n",
			m->matchtype);
		return 1;
	}

	FPRINTF(fout, "%s%s, ", flags1, flags2);
	FPRINTF(fout, "%d},\n", m->placeholder->id);

	return 0;
}

static int generate_matches(FILE *fout, struct scanpriv *priv)
{
	struct match *m;

	FPRINTF(fout, "/*\n");
	FPRINTF(fout, " * Each entry consists of:\n");
	FPRINTF(fout, " *   vendor, device, index, coreindex, bus, flags,\n");
	FPRINTF(fout, " *   index of placeholder node in xml_nodes[], \n");
	FPRINTF(fout, " */\n");

	FPRINTF(fout, "struct xml_match xml_matches[] = {\n");
	SLIST_FOR_EACH (&priv->matches, m, struct match)
		if (generate_matchline(fout, priv, m))
			return 1;
	FPRINTF(fout, "};\n\n");

	return 0;
}

static int generate_nhfixups_for_node(struct node *parent, FILE *fout)
{
	static int first = 1;
	struct node *child;
	struct propval *pv;

	if (!parent)
		return 0;

	SLIST_FOR_EACH (&parent->nhfixups, pv, struct propval) {
		if (first)
			first = 0;
		else
			FPRINTF(fout, ",");

		FPRINTF(fout, "%d", pv->intindex);
	}

	SLIST_FOR_EACH (&parent->nodes, child, struct node)
		generate_nhfixups_for_node(child, fout);

	return 0;
}

static int generate_nhfixups(FILE *fout, struct scanpriv *priv)
{
	struct match *m;

	FPRINTF(fout, "/* Indices for nodehandles to be fixed up in xml_values[] */\n");
	FPRINTF(fout, "int xml_nodehandles[] = {");

	SLIST_FOR_EACH (&priv->matches, m, struct match)
		generate_nhfixups_for_node(m->placeholder, fout);

	FPRINTF(fout, "};\n\n");

	return 0;
}

static int generate_intlist(FILE *fout, struct scanpriv *priv)
{
	struct propval *pv;
	int first = 1;

	FPRINTF(fout, "/* Integer property values */\n");
	FPRINTF(fout, "int xml_values[] = {");

	SLIST_FOR_EACH (&priv->intlist, pv, struct propval) {
		char *val = NULL;

		if (pv->type == PVALTYPE_INT)
			val = pv->value;
		else if (pv->type == PVALTYPE_COREHANDLE ||
				pv->type == PVALTYPE_NODEHANDLE)
			val = CHFIXUP_DUMMY;
		if (!val)
			continue;

		if (first) {
			first = 0;
		} else {
			FPRINTF(fout, ",");
		}
		FPRINTF(fout, "%s", val);
	}

	FPRINTF(fout, "};\n\n");

	return 0;
}


static int generate_intpropval(FILE *fout, struct scanpriv *priv,
			       struct property *p)
{
	struct propval *pv;
	int count = 0;
	int start = 0;

	SLIST_FOR_EACH (&p->values, pv, struct propval) {
		if (count == 0)
			start = pv->intindex;
		count++;
	}

	FPRINTF(fout, "(char *)(&xml_values[%d]), %d*4", start, count);

	return 0;
}

static int generate_stringpropval(FILE *fout, struct scanpriv *priv,
				   struct property *p)
{
	struct propval *pv;
	int size = 0;
	int first = 1;

	FPRINTF(fout, "\"");
	SLIST_FOR_EACH (&p->values, pv, struct propval) {
		if (first)
			first = 0;
		else
			FPRINTF(fout, "\\0");
		FPRINTF(fout, "%s", pv->value);
		size += strlen(pv->value) + 1;
	}
	FPRINTF(fout, "\", %d", size);

	return 0;
}

static int generate_propline(FILE *fout, struct scanpriv *priv,
			      struct property *p, int last)
{
	char *endstr = last ? "_END" : "";

	FPRINTF(fout, "  /* %d */\t", p->id);

	if (p->action == ACTION_REMOVE) {
		FPRINTF(fout, "PROPA_RM%s(\"%s\"),\n", endstr, p->name);
	} else { /* ACTION_ADD */
		FPRINTF(fout, "PROPA_PTR%s(\"%s\", ", endstr, p->name);
		if (p->type == PROPTYPE_INT) { /* Includes {core|node}handles */
			generate_intpropval(fout, priv, p);
		} else if (p->type == PROPTYPE_STRING) {
			generate_stringpropval(fout, priv, p);
		} else if (p->type == PROPTYPE_EMPTY) {
			FPRINTF(fout, "NULL, 0");
		}
		FPRINTF(fout, "),\n");
	}

	return 0;
}

static int generate_proplines(FILE *fout, struct scanpriv *priv,
			       struct slist *proplist)
{
	struct property *last = SLIST_TAIL(proplist, struct property);
	struct property *p;

	SLIST_FOR_EACH (proplist, p, struct property) {
		generate_propline(fout, priv, p, p == last);
	}

	return 0;
}

static int generate_node_proplines(FILE *fout, struct scanpriv *priv,
				    struct match *m, struct node *n)
{
	struct node *nc;

	FPRINTF(fout, "/* --- Match %d, node %d --- */\n", m->id, n->id);
	generate_proplines(fout, priv, &n->properties);

	SLIST_FOR_EACH (&n->nodes, nc, struct node)
		generate_node_proplines(fout, priv, m, nc);

	return 0;
}

static int generate_proplist(FILE *fout, struct scanpriv *priv)
{
	struct match *m;

	FPRINTF(fout, "struct propa_ptr xml_props[] = {\n");

	SLIST_FOR_EACH (&priv->matches, m, struct match) {
		/* FPRINTF(fout, "/\* ===== Match %d ===== *\/\n", m->id); */
		generate_node_proplines(fout, priv, m, m->placeholder);
	}

	FPRINTF(fout, "};\n\n");

	return 0;
}

static int generate_nodelines(FILE *fout, struct scanpriv *priv,
			       struct node *n, struct node *sibling)
{
	struct node *firstchild;
	struct node *child;
	struct property *firstprop;
	int nhfixcount;

	firstchild = SLIST_HEAD(&n->nodes, struct node);
	firstprop = SLIST_HEAD(&n->properties, struct property);

	FPRINTF(fout, "  /* %d */\t", n->id);
	FPRINTF(fout, "XMLNODE_INIT(");

	if (firstchild)
		FPRINTF(fout, "&xml_nodes[%2d], ", firstchild->id);
        else
		FPRINTF(fout, "NULL,           ");

	if (sibling)
		FPRINTF(fout, "&xml_nodes[%2d], ", sibling->id);
        else
		FPRINTF(fout, "NULL,           ");

	if (firstprop)
		FPRINTF(fout, "&xml_props[%2d], ", firstprop->id);
        else
		FPRINTF(fout, "NULL,           ");

	FPRINTF(fout,"%d, ", SLIST_SIZE(&n->properties));
	/*
	 * Index into list of corehandles that points to the core of the
	 * match. The list of corehandles contains indices to values needs to be
	 * filled in with addresses of the prom node for the core
	 */
	nhfixcount = SLIST_SIZE(&n->nhfixups);
	FPRINTF(fout, "%d, ", priv->nhcount); /* First nhfixup */
	FPRINTF(fout, "%d, ", nhfixcount); /* Number of nhfixups */
	priv->nhcount += nhfixcount;

	if (n->name) {
		FPRINTF(fout, "0),"); /* flags */
		FPRINTF(fout," /* \"%s\" */", n->name);
	} else {
		FPRINTF(fout, "XMLNODE_PLH),"); /* flags */
		FPRINTF(fout," /* placeholder node */");
	}
	FPRINTF(fout,"\n");

	SLIST_FOR_EACH (&n->nodes, child, struct node) {
		struct node *sibling = SLIST_NEXT(&n->nodes, child, struct node);
		generate_nodelines(fout, priv, child, sibling);
	}

	return 0;
}

static int generate_nodelist(FILE *fout, struct scanpriv *priv)
{
	struct match *m;

	FPRINTF(fout, "#define XMLNODE_INIT(c, s, p, pcnt, nhstart, nhcnt, flags) \\\n");
	FPRINTF(fout, "\t{{(struct node *)c,(struct node *)s,(struct prop *)p}, pcnt, nhstart, nhcnt, flags}\n\n");

	FPRINTF(fout, "/*\n");
	FPRINTF(fout, " * Each entry consists of:\n");
	FPRINTF(fout, " *   first child, next sibling, first property, # of propertien, start of nodehandle, # of nodehandles, flags\n");
	FPRINTF(fout, " */\n");

	FPRINTF(fout, "struct xml_node xml_nodes[] = {\n");

	SLIST_FOR_EACH (&priv->matches, m, struct match) {
		FPRINTF(fout, "/* --- Match %d --- */\n", m->id);
		generate_nodelines(fout, priv, m->placeholder, NULL);
	}

	FPRINTF(fout, "};\n\n");

	return 0;
}

static int generate_code(FILE *fout, struct scanpriv *priv)
{
	if (prepare(priv))
		return 1;

	if (priv->dbg_parsing || priv->dbg_parse_tree)
		printf("*/\n");

	if (generate_matches(fout, priv))
		return 1;
	if (generate_nhfixups(fout, priv))
		return 1;
	if (generate_intlist(fout, priv))
		return 1;
	if (generate_proplist(fout, priv))
		return 1;
	if (generate_nodelist(fout, priv))
		return 1;

	FPRINTF(fout, "#define XML_MATCHCOUNT %d\n", priv->matchcount);
	FPRINTF(fout, "int xml_matchcount = XML_MATCHCOUNT;\n");
	FPRINTF(fout, "#define XML_NODECOUNT %d\n", priv->nodecount);
	FPRINTF(fout, "int xml_nodecount = XML_NODECOUNT;\n");

	return 0;
}


/* ============================================================ */
/* Main and helpers */

static void usage() {
	fprintf(stderr, "Usage: scanxml [options] [infile] [outfile]\n\n");
	fprintf(stderr, "\tInfile/outfile can be - for stdin/stdout\n\n");
	fprintf(stderr, "\tOptions:\n");
	fprintf(stderr, "\t-?, --help\tPrint this help text and exit\n");
	fprintf(stderr, "\t-d\t\tPrint xml format documentation and exit\n");
	fprintf(stderr, "\t-v\t\tVerbose debug output\n");
	fprintf(stderr, "\t--\t\tEnd of options\n");

}

int main(int argc, char *argv[])
{
	XML_Parser parser = NULL;
	char *filename;
	FILE *fin = NULL;
	FILE *fout = NULL;
	int err = 1;
	struct scanpriv _priv;
	struct scanpriv *priv = &_priv;
	int final = 0;
	int argi;
	int debug = 0;
	int docprint = 0;

	for (argi = 1; argi < argc && argv[argi][0] == '-'; argi++) {
		char *op = argv[argi];
		if (strcmp(op, "-") == 0) {
			/* stdin infile */
			break;
		} else if (strcmp(op, "--") == 0) {
			/* end of options */
			argi++;
			break;
		} else if (strcmp(op, "-?") == 0 || strcmp(op, "--help") == 0) {
			usage();
			return 1;
		} else if (strcmp(op, "-v") == 0) {
			debug = 1;
		} else if (strcmp(op, "-d") == 0) {
			docprint = 1;
		} else {
			fprintf(stderr, "Invalid option \"%s\"\n\n", op);
			usage();
			return 1;
		}
	}

	initpriv(priv);
	if (debug) {
		priv->dbg_parsing = 1;
		priv->dbg_parse_tree = 1;
	}

	if (priv->dbg_parsing || priv->dbg_parse_tree)
		printf(DEBUG_SECTION_HEADER);

	if (init_tags())
		return 1;

	if (docprint) {
		doc_print();
		return 0;
	}

	if (argi >= argc || strcmp(argv[argi], "-") == 0) {
		fin = stdin;
	} else {
		filename = argv[argi];
		fin = fopen(filename, "r");
		if (!fin) {
			fprintf(stderr, "Could not open input file \"%s\": %s\n",
				filename, strerror(errno));
			goto out;
		}
	}
	argi++;

	if (argi >= argc || strcmp(argv[argi], "-") == 0) {
		fout = stdout;
	} else {
		filename = argv[argi];
		fout = fopen(filename, "w");
		if (!fout) {
			fprintf(stderr, "Could not open output file \"%s\": %s\n",
				filename, strerror(errno));
			goto out;
		}
	}
	argi++;

	if (argi < argc) {
		fprintf(stderr, "Extraneous argument \"%s\"\n\n", argv[argi]);
		usage();
		goto out;
	}

	parser = XML_ParserCreate(NULL);
	if (!parser) {
		fprintf(stderr, "XML parser creation failed\n");
		goto out;
	}
	priv->parser = parser;

	priv->tbuflen = BUFSIZE;
	priv->tbuf = malloc(priv->tbuflen);
	if (!priv->tbuf) {
		fprintf(stderr, "Character data buffer allocation failed\n");
		goto out;
	}
	priv->text = priv->tbuf;
	priv->text[0] = '\0';

	XML_SetElementHandler(parser, starttag, endtag);
	XML_SetCharacterDataHandler(parser, texthandler);
	XML_SetUserData(parser, priv);

	/* Parse loop */
	do {
		int bytes;
		void *buf;

		buf = XML_GetBuffer(parser, BUFSIZE);
		if (!buf) {
			fprintf(stderr, "Failed to allocate buffer\n");
			goto out;
		}

		bytes = fread(buf, sizeof(char), BUFSIZE, fin);
		if (bytes == 0) {
			if (ferror(fin)) {
				fprintf(stderr, "Failed to read file\n");
				goto out;
			} else if (feof(fin)) {
				final = 1;
			}
		}

		if (XML_ParseBuffer(parser, bytes, final) != XML_STATUS_OK) {
			fprintf(stderr,
				"Parse failed at line %lu, column %lu: %s\n",
				XML_GetCurrentLineNumber(parser),
				XML_GetCurrentColumnNumber(parser),
				XML_ErrorString(XML_GetErrorCode(parser)));
			goto out;
		}
	} while (!final);

	if (priv->dbg_parsing)
		printf(DEBUG_SECTION_DIVIDER);

	err = generate_code(fout, priv);
out:
	if (fin && fin != stdin)
		fclose(fin);
	if (fout && fout != stdin)
		fclose(fout);
	if (parser)
		XML_ParserFree(parser);

	return err;
}
