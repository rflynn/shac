/* vim: set ts=4: */

#include <stdio.h>		/* printf, fprintf */
#include <stdlib.h>		/* free, stddef.h */
#include <string.h>		/* memcpy */
#include <assert.h>
#include "llist.h"

/* creates list node */
/* returns NULL on failure */
list_node *list_node_create(const void *data, const size_t bytes)
{
	list_node *node = NULL;

#ifdef DEBUG_LIST
	assert(data != NULL);
#endif
	
	if (NULL == (node = malloc(sizeof(list_node)))) {
#ifdef DEBUG_LIST
		fprintf(stderr, "%s:%d:list_node_create(): could not allocate %d bytes for node!\n",
			__FILE__, __LINE__, (int)sizeof(list_node));
#endif
		return NULL;
	}

	if (NULL == (node->data = malloc(bytes))) {
#ifdef DEBUG_LIST
		fprintf(stderr, "%s:%d:list_node_create(): could not allocate %d bytes for node->data!\n",
			__FILE__, __LINE__, (int)bytes);
#endif
		free(node);
		return NULL;
	}

	/* make a copy of data into node->data */
	memcpy(node->data, data, bytes);

	node->prev = NULL;
	node->next = NULL;

	return node;
}

/* creates deep copy of data into list node */
/* returns NULL on failure */
list_node *list_node_create_deep(const void *data, void *(*deepcopy)(const void *))
{
	list_node *node = NULL;
	void *deep = NULL;

#ifdef DEBUG_LIST
	assert(NULL != data);
	assert(NULL != deepcopy);
#endif

	if (NULL == (deep = deepcopy(data))) {
#ifdef DEBUG_LIST
		fprintf(stderr, "%s:%d:list_node_create_deep() could not create deep copy of data at %p\n",
			__FILE__, __LINE__, data);
#endif
		return NULL;
	}

	if (NULL == (node = malloc(sizeof(list_node)))) {
#ifdef DEBUG_LIST
		fprintf(stderr, "%s:%d:list_node_create(): could not allocate %d bytes for node!\n",
			__FILE__, __LINE__, sizeof(list_node));
#endif
		return NULL;
	}

	node->data = deep;
	node->prev = NULL;
	node->next = NULL;

	return node;
}

/* initialize list head */
list_head *list_head_create()
{
	list_head * head = NULL;
	size_t mallen = 0;
	
	mallen = sizeof(list_head);
	if (NULL == (head = malloc(mallen))) {
#ifdef DEBUG_LIST
		fprintf(stderr, "%s:%d:list_head_create() could not allocate %d bytes for head!\n",
			__FILE__, __LINE__, (int)mallen);
#endif
		return NULL;
	}

	head->first = NULL;
	head->last = NULL;
	head->nodes = 0;

	return head;
}

#if 0
void *list_dupe(void *head)
{
	list_head *orig, *dupe;
	list_node *node, *dupenode;
#ifdef DEBUG_LIST
	assert(NULL != head);
#endif
	orig = head;

	if (NULL == (dupe = list_head_create())) {
#ifdef DEBUG_LIST
		fprintf(stderr, "%s:%d:list_dupe() could not create list_head 'dupe'!\n",
			__FILE__, __LINE__);
#endif
		return NULL;
	}

	dupe->nodes = orig->nodes;
	
	for (node = list_first(orig); node != NULL; node = list_node_next(node)) {
		if (NULL == (newnode = list_append(dupe, node))) { /* make a copy */

		}
	}

	return (void *)dupe;
}
#endif

/* frees memory used by a node */
/* returns 1 on success, 0 on failure */
int list_node_free(list_node *node, void (*free_data)(void *))
{
#ifdef DEBUG_LIST
	assert(NULL != node);
#endif

	node->prev = NULL;
	node->next = NULL;

#if 0
	fprintf(stderr, "%s:%d:list_node_free(): freeing node->data node: %p, data: %p, free_data: %p\n",
		__FILE__, __LINE__, (void *)node, (void *)node->data, (void *)free_data);
#endif

	if (NULL != free_data) {
		/* free up data structure with function pointer */
		free_data(node->data);
	} else {
		free(node->data);
	}

#if 0
	fprintf(stderr, "%s:%d:list_node_free(): freeing node (%p)\n",
		__FILE__, __LINE__, (void *)node);
#endif

	free(node);

	return 1;
}

/* release the list_head. you must obviously get rid of the list before you do this */
/* this is useful if you concatenate 2 lists and want to free the second list's head
but keep the nodes intact */
void list_head_free(list_head *head)
{
#ifdef DEBUG_LIST
	assert(NULL != head);
#endif

	if (NULL == head)
		return;

	free(head);
}
/* prepend new node into list */
/* returns pointer to node on success, NULL on failure */
list_node *list_prepend(list_head *head, list_node *node)
{

#ifdef DEBUG_LIST
	assert(NULL != head);
	assert(NULL != node);
#endif

	if (NULL != head->first)
		head->first->prev = node;

	if (NULL == head->last)
		head->last = node;

	node->next = head->first;
	head->first = node;

	head->nodes++;
	
	return node;

}


/* append new node into list */
/* returns pointer to node on success, NULL on failure */
list_node *list_append(list_head *head, list_node *node)
{
#ifdef DEBUG_LIST
	assert(NULL != head);
	assert(NULL != node);
#endif

	if (NULL != head->last) {
		/* check that we're not appending the same node consecutively */
#if 0
		if (head->last == node) {
			fprintf(stderr, "%s:%d:list_append(%p, %p) you're appending the same node twice, creating an infinitely long list!\n",
				__FILE__, __LINE__, (void *)head, (void *)node);
			exit(1);
		}
#endif
#if 0
	printf("list_append()%d head->last->next(%p)\n",
		__LINE__, (void *)head->last->next);
#endif
		head->last->next = node;
	}

	if (NULL == head->first)
		head->first = node;

	node->prev = head->last;
	head->last = node;

	head->nodes++;
	
	return node;

}

/* return data if node isn't null */
void *list_node_data(list_node *v)
{
	if (NULL == v)
		return NULL;
	return v->data;
}

/* returns 1 on success */
void list_remove(list_head *head, list_node *node, void (*free_data)(void *))
{
#ifdef DEBUG_LIST
	assert(NULL != head);
	assert(NULL != node);
#endif

	/* point prev node or first next */
	if (node->prev == NULL)
		head->first = node->next;
	else
		node->prev->next = node->next;

	/* point next node or last prev */
	if (node->next == NULL)
		head->last = node->prev;
	else
		node->next->prev = node->prev;

	list_node_free(node, free_data);

}

/* frees entire list and list_head */
/* returns 1 on success, 0 on failure */
void list_free(list_head *head, void (*free_data)(void *))
{
#ifdef DEBUG_LIST
	assert(NULL != head);
#endif
	list_nodes_free(head, free_data);
	list_head_free(head);
}

/* frees entire list structure but not the head */
/* free_data may be NULL, list_node_free handles it */
int list_nodes_free(list_head *head, void (*free_data)(void *))
{
	list_node *curr = NULL, *tmp = NULL;

#ifdef DEBUG_LIST
	assert(NULL != head);
#endif

	curr = list_first(head);

	while (NULL != curr) {
		/* prev on the head will be NULL, not a circular list */
		tmp = list_node_next(curr);
		if (0 == list_node_free(curr, free_data)) {
#ifdef DEBUG_LIST
			fprintf(stderr, "%s:%d:list_nodes_free() error freeing curr(%p) with free_data()!\n",
				__FILE__, __LINE__, (void *)curr);
			exit(1);
#endif
			return 0;
		}
		curr = tmp;
	}

	list_first(head) = NULL;
	list_last(head) = NULL;
	list_size(head) = 0;

	return 1;

}

/* print an entire list */
void list_dump(list_head *head, void (*data_print)(const void *))
{
	list_node *curr;

#ifdef DEBUG_LIST
	assert(head != NULL);
#endif

	list_head_dump(head);

	for (curr = head->first; curr != NULL; curr = curr->next)
		list_node_dump(curr, data_print);

}

/* apply a function to all members of the list */
void list_map(list_head *head, void (*mapped)(list_node *))
{
	list_node *curr = NULL;

#ifdef DEBUG_LIST
	assert(head != NULL);
	assert(mapped != NULL);
#endif

	for (curr = head->first; curr != NULL; curr = curr->next)
		mapped(curr);

}

/* concatenate list b onto the end of list a */
/* NOTE: this does not make a copy of b, and it does alter b, b will work going first->last after this, but not in reverse */
/* if you do this, make sure to free *only the head* of list b */
void list_concat(list_head *a, list_head *b) {

#ifdef DEBUG_LIST
	assert(NULL != a);
	assert(NULL != b);
#endif

	if (list_size(a))
		a->last->next = list_first(b);
	else
		list_first(a) = list_first(b);
	if (list_size(b))
		b->first->prev = list_last(a);
	list_last(a) = list_last(b); /* */
	list_size(a) += list_size(b); /* reflect count */
}

/* print a single head */
void list_head_dump(list_head *head)
{

#ifdef DEBUG_LIST
	assert(head != NULL);
#endif

	fprintf(stdout, "list_head(%p){\n\tnodes: %d\n\tfirst: %p\n\tlast: %p\n}\n",
		(void *)head, (int)head->nodes, (void *)head->first, (void *)head->last);

}

/* print a single node */
void list_node_dump(list_node *node, void (*data_print)(const void *))
{

#ifdef DEBUG_LIST
	assert(node != NULL);
	assert(data_print != NULL);
#endif

	fprintf(stdout, "list_node(%p){\n", (void *)node);

	if (NULL != node)
		fprintf(stdout, "\tprev: %p\n\tnext: %p\n",
 			(void *)node->prev, (void *)node->next);

	if (data_print != NULL)
		data_print(node->data);

	fprintf(stdout, "}\n");

}

/* returns found node on success, NULL on failure */
list_node* list_search(list_node *node, int (*cmp)(const void *, const void *), const void *find)
{
	list_node *curr = NULL;

#ifdef DEBUG_LIST
	assert(cmp != NULL);
	assert(find != NULL);
#endif

	/* if the list is empty or an non-existant node is passed to us, that's a
	 * big negative good buddy */
	if (NULL == node)
		return NULL;

	for (curr = node; curr != NULL; curr = curr->next)
		if (0 == cmp(curr->data, find)) /* callback matches */
			return curr;

	return NULL;
}

/* returns found node on success, NULL on failure */
list_node* list_search_reverse(list_node *node, int (*cmp)(const void *, const void *), const void *find)
{
	list_node *curr = NULL;

#ifdef DEBUG_LIST
	assert(node != NULL);
	assert(cmp != NULL);
	assert(find != NULL);
#endif

	for (curr = node; curr != NULL; curr = curr->prev)
	{
		/* callback matches */
		if (0 == (*cmp)(curr->data, find)) {
			return curr;
		}
	}

	return NULL;
}

/* a callback you can pass to list_dump() if node->data is a char array */
void list_dump_cb_chars(const void *v)
{
	const char *c = v;
	printf("\tdata(%p){\n", v);
	if (NULL != v)
		printf("\t\t\"%s\"\n", c);
	printf("}\n");
}

