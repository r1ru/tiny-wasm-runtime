#include "list.h"

void list_elem_init(list_elem_t *elem) {
    elem->next = NULL;
    elem->prev = NULL;
}

bool list_is_linked(list_elem_t *elem) {
    return elem->next != NULL;
}

static void list_insert(list_elem_t *prev, list_elem_t *next, list_elem_t *new) {
    new->prev = prev;
    new->next = next;
    next->prev = new;
    prev->next = new;
}

void list_push_back(list_t *list, list_elem_t *elem) {
    list_insert(list->prev, list, elem);
}

list_elem_t *list_tail(list_t *list) {
    list_t *tail = list->prev;

    if(tail == list)
        return NULL;

    return tail;
}

list_elem_t *list_pop_tail(list_t *list) {
    list_t *tail = list->prev;

    if(tail == list)
        return NULL;
    
    // delete element
    tail->prev->next = tail->next;
    list->prev = tail->prev;

    return tail;
}

list_elem_t *list_get_elem(list_t *list, size_t idx) {
    list_elem_t *e = list;

    do {
        e = e->prev;
        if(e == list)
            return NULL;
    } while(idx--);
    
    return e;
}