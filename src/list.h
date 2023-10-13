#pragma once

#include <stdint.h>
#include <stddef.h>

struct list {
    struct list *next;
    struct list *prev;
};

typedef struct list list_t;
typedef struct list list_elem_t;

#define LIST_INIT(list)                                                         \
    {(list)->next = (list); (list)->prev = (list);}

#define LIST_CONTAINER(elem, container, field)                                  \
    ((container *) ((uint8_t *) (elem) - offsetof(container, field)))

#define LIST_FOR_EACH(elem, list, container, field)                             \
    for (container *elem = LIST_CONTAINER((list)->next, container, field);      \
         &elem->field != (list);                                                \
         elem = LIST_CONTAINER(elem->field.next, container, field))

#define LIST_TAIL(list, container, field)                                       \
    ({                                                                          \
        list_elem_t *__elem = list_tail(list);                                  \
        (__elem) ? LIST_CONTAINER(__elem, container, field) : NULL;             \
    })

#define LIST_POP_TAIL(list, container, field)                                   \
    ({                                                                          \
        list_elem_t *__elem = list_pop_tail(list);                              \
        (__elem) ? LIST_CONTAINER(__elem, container, field) : NULL;             \
    })

#define LIST_GET_ELEM(list, container, field, idx)                              \
    ({                                                                          \
        list_elem_t *__elem = list_get_elem(list, idx);                         \
        (__elem) ? LIST_CONTAINER(__elem, container, field) : NULL;             \
    })

void list_push_back(list_t *list, list_elem_t *elem);
list_elem_t *list_tail(list_t *list);
list_elem_t *list_pop_tail(list_t *list);
list_elem_t *list_get_elem(list_t *list, size_t idx);