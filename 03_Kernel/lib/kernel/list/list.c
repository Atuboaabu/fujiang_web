#include "list.h"
#include "print.h"

void list_init(struct list* _list) {
    _list->head.prev = NULL;
    _list->head.next = &(_list->tail);

    _list->tail.prev = &(_list->head);
    _list->tail.next = NULL;
}

void list_insert_before(struct list_elem* _before_elem, struct list_elem* _elem) {
    _before_elem->prev->next = _elem; 
    _elem->prev = _before_elem->prev;
    _elem->next = _before_elem;
    _before_elem->prev = _elem;
}

void list_push(struct list* _list, struct list_elem* _elem) {
    list_insert_before(_list->head.next, _elem);
}

void list_append(struct list* _list, struct list_elem* _elem) {
    list_insert_before(&(_list->tail), _elem);
}

void list_remove(struct list_elem* _elem) {
    _elem->prev->next = _elem->next;
    _elem->next->prev = _elem->prev;
    _elem->prev = NULL;
    _elem->next = NULL;
}

bool elem_find(struct list* _list, struct list_elem* _elem) {
    struct list_elem* tmp_elem = _list->head.next;
    while (tmp_elem != &(_list->tail)) {
        if (tmp_elem == _elem) {
            return true;
        }
        tmp_elem = tmp_elem->next;
    }
    return false;
}

bool list_empty(struct list* _list) {
    return (_list->head.next == &(_list->tail) ? true : false);
}

uint32_t list_len(struct list* _list) {
    uint32_t _len = 0;
    struct list_elem* tmp_elem = _list->head.next;
    while (tmp_elem != &(_list->tail)) {
        _len++;
        tmp_elem = tmp_elem->next;
    }
    return _len;
}

struct list_elem* list_pop(struct list* _list) {
    struct list_elem* _elem = _list->head.next;
    list_remove(_elem);
    return _elem;
}

struct list_elem* list_traversal(struct list* _list, list_callback _callback, int _arg) {
    struct list_elem* _elem = _list->head.next;
    /* 如果队列为空,就必然没有符合条件的结点,故直接返回NULL */
    if (list_empty(_list)) { 
        return NULL;
    }

   while (_elem != &(_list->tail)) {
        /* _callback 返回 ture 则认为该元素在回调函数中符合条件，命中，故停止继续遍历 */
        if (_callback(_elem, _arg)) {
            return _elem;
        }
        _elem = _elem->next;
    }
    return NULL;
}