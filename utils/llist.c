#include <utils/llist.h>
#include <output/output.h>

//Return new list_node on success
//NULL on memory allocation failure
struct llist_node *llist_append(struct llist *list, void *data) 
{
        acquire_mutex(&(list->mutex));

    struct llist_node *new_node = (struct llist_node *)kmalloc(sizeof(struct llist_node));
    if (new_node == NULL)
        return NULL;
    new_node->next = NULL;
    new_node->prev = NULL;
    new_node->list = list;
    new_node->data = data;

    //First entry in list
    if (list->tail == NULL) {
        list->tail = new_node;
        list->head = new_node;
    } else {
        list->tail->next = new_node;
        new_node->prev = list->tail;
        list->tail = new_node;
    }
    list->count++; 
    release_mutex(&(list->mutex));

    return new_node;
}

//Copy contents of list_src to dst_src using copy_data_func
void llist_copy(struct llist *list_src, struct llist *list_dst, void*  (*copy_data_func)(void *data,void *user_data),void *user_data)
{
    struct llist_node *n_ptr = list_src->head;
    void *new_data;
    acquire_mutex(&(list_src->mutex));
    while(n_ptr != NULL)
    {
        new_data = copy_data_func(n_ptr->data,user_data);

        if (llist_append(list_dst,new_data) == NULL)
            panic("Error in copying list");
        n_ptr = n_ptr->next;
    }
    release_mutex(&(list_src->mutex));

}

//Free list while calling data specific freeing function on every node's data ptr
void llist_free(struct llist *list, void (*free_func)(void *data)) 
{
    struct llist_node *n_ptr;
    acquire_mutex(&(list->mutex));

    if (list == NULL)
        return;
    
    n_ptr = list->head;

    while(n_ptr != NULL)
    {
        free_func(n_ptr->data);
        kfree(n_ptr);
        n_ptr = n_ptr->next;
    }
    kfree(list);
    release_mutex(&(list->mutex));

    //kprintf("llist done\n");
}

//Return new list_node on success
//NULL on memory allocation failure
struct llist_node *llist_prepend(struct llist *list, void *data) 
{
    struct llist_node *new_node = (struct llist_node *)kmalloc(sizeof(struct llist_node));
    if (new_node == NULL)
        return NULL;
    acquire_mutex(&(list->mutex));

    new_node->next = NULL;
    new_node->prev = NULL;
    new_node->list = list;
    new_node->data = data;

    //First entry in list
    if (list->head == NULL) {
        list->tail = new_node;
        list->head = new_node;
    } else {
        list->head->prev = new_node;
        new_node->next = list->head;
        list->head = new_node;
    }
    list->count++;
    release_mutex(&(list->mutex));
    return new_node;
}

struct llist* llist_new() 
{
    struct llist* n_llist = (struct llist*) kmalloc(sizeof(struct llist));
    if (n_llist == NULL)
        return NULL;
    init_mutex(&(n_llist->mutex));
    n_llist->head = NULL;
    n_llist->tail = NULL;
    n_llist->count = 0;
    return n_llist;
}