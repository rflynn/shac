/* vim: set ts=4: */
/*
	i've written this doubly linked list code in a very general fashion
	this should be able to be re-used anywhere
*/

#ifndef LIST_H
#define LIST_H

#define DEBUG_LIST
#if 0
#undef DEBUG_LIST
#endif

typedef struct _list_head list_head; /* the actual, mandatory part of a list */
typedef struct _list_node list_node; /* a list entry */

struct _list_head {
	list_node *first, *last;
	size_t nodes; /* number of items in the list */
	size_t bytes; /* number of total bytes the list data consumes. currently unused */
};

struct _list_node {
	void *data;
	list_node *prev, *next;
};

#define list_first(list)		((list)->first)
#define list_last(list)			((list)->last)
#define list_size(list)			((list)->nodes)
#define list_bytes(list)		((list)->bytes)
#define list_node_prev(node)	((node)->next)
#define list_node_next(node)	((node)->next)

/* list creation and destruction */
list_head *list_head_create(); /* create a list */
void list_free(list_head *, void (*)(void *)); /* free an entire list header and all its node */
int list_node_free(list_node *, void (*)(void *)); /* free a node using an external function */
int list_nodes_free(list_head *, void (*)(void *)); /* empty a list, but keep the head */
#if 0
void *list_dupe(void *); /* duplicate an existing list_head* and all nodes */
#endif
void list_head_free(list_head *); /* free only the head of a list (free nodes before you call this!) */

/* adding and removing elements */
list_node *list_node_create(const void *, const size_t); /* create a new node with size_t bytes from the void * */
list_node *list_node_create_deep(const void *, void *(*)(const void *)); /* create a new node using output from an external "dupe" function */
list_node *list_prepend(list_head *, list_node *); /* add a node to the beginning of a list */
list_node *list_append(list_head *, list_node *); /* add a node to the end of a list */
void *list_node_data(list_node *); /* return pointer to node data */
void list_remove(list_head *, list_node *, void (*)(void *)); /* remove a node, using an external function to free the node data */
void list_concat(list_head *, list_head *); /* destructively joins second list to the end of first list */

/* list examination */
void list_dump(list_head *, void (*)(const void *)); /* display of a list head and all its node to stdout using an external display function */
void list_head_dump(list_head *); /* formatted display of just a list_head to stdout */
void list_node_dump(list_node *, void (*)(const void *)); /* formatted display of list a */
void list_dump_cb_chars(const void *); /* callback for list_dump to print char * node data */

/* performing actions on every member of a list */
void list_map(list_head *, void (*)(list_node *)); /* apply a function to each member of the list, first->last */
list_node *list_search(list_node *, int (*)(const void *, const void *), const void *); /* */
list_node *list_search_reverse(list_node *, int (*)(const void *, const void *), const void *); /* */

#endif
