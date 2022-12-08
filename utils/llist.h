#ifndef _LLIST_H
#define _LLIST_H
#include <locks/mutex.h>
#include <include/types.h>
#include <mm/mm.h>

//struct llist;

struct llist_node {
    void *data;
    struct llist_node *prev;
    struct llist_node *next;
    struct llist *list;
};

struct llist {
    struct llist_node *head;
    struct llist_node *tail;
    uint64_t count;
    struct mutex mutex;
};

struct llist* llist_new();
struct llist_node *llist_prepend(struct llist *list, void *data);
void llist_free(struct llist *list, void (*free_func)(void *data));
void llist_copy(struct llist *list_src, struct llist *list_dst, void*  (*copy_data_func)(void *data,void *user_data),void *user_data);
struct llist_node *llist_append(struct llist *list, void *data);

#endif