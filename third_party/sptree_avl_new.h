/*
 * Copyright (C) 2012 Mail.RU
 * Copyright (C) 2010 Teodor Sigaev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SPTREE_H_
#define _SPTREE_H_

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <third_party/qsort_arg_new.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#ifndef SPTREE_NODE_SELF
/*
 * user could suggest pointer's storage himself
 */
typedef u_int32_t spnode_t;
#define SPNIL (0x7fffffff)
#define SPINDEXMASK (0x7fffffff)
#define SPFLAGMASK (0x80000000)
#define MAX_DEPTH (48)

typedef struct sptree_node_pointers {
    u_int32_t    left;   /* sizeof(spnode_t) >= sizeof(sptree_node_pointers.left) !!! */
    u_int32_t    right;
} sptree_node_pointers;

#define GET_SPNODE_LEFT(snp) ((snp)->left & SPINDEXMASK)
#define SET_SPNODE_LEFT(snp, v) ((snp)->left = (((snp)->left & SPFLAGMASK) | (v)))
#define GET_SPNODE_RIGHT(snp) ((snp)->right & SPINDEXMASK)
#define SET_SPNODE_RIGHT(snp, v) ((snp)->right = (((snp)->right & SPFLAGMASK) | (v)))
#define GET_SPNODE_BALANCE(snp) (((snp)->right >> 31) ? 1 : ((snp)->left >> 31) ? -1 : 0)
#define SET_SPNODE_BALANCE(snp, v) ((snp)->left = (((snp)->left & SPINDEXMASK) | ((v) < 0 ? SPFLAGMASK : 0))), ((snp)->right = (((snp)->right & SPINDEXMASK) | ((v) > 0 ? SPFLAGMASK : 0)))

#endif /* SPTREE_NODE_SELF */

#define    _GET_SPNODE_LEFT(n)         GET_SPNODE_LEFT( t->lrpointers + (n) )
#define    _SET_SPNODE_LEFT(n, v)      SET_SPNODE_LEFT( t->lrpointers + (n), (v) )
#define    _GET_SPNODE_RIGHT(n)        GET_SPNODE_RIGHT( t->lrpointers + (n) )
#define    _SET_SPNODE_RIGHT(n, v)     SET_SPNODE_RIGHT( t->lrpointers + (n), (v) )
#define _GET_SPNODE_BALANCE(n) GET_SPNODE_BALANCE( t->lrpointers + (n) )
#define _SET_SPNODE_BALANCE(n, v) SET_SPNODE_BALANCE( t->lrpointers + (n), (v) )

#define    ITHELEM(t, i)               ( (char *) (t)->members + (t)->elemsize * (i) )
#define    ELEMIDX(t, e)               ( ((e) - (t)->members) / (t)->elemsize )

/*
 * makes definition of tree with methods, name should
 * be unique across all definitions.
 *
 * Methods:
 *   void sptree_NAME_init(sptree_NAME *tree, size_t elemsize, void *array,
 *                         spnode_t array_len, spnode_t array_size,
 *                         int (*compare)(const void *key, const void *elem, void *arg),
 *                         int (*elemcompare)(const void *e1, const void *e2, void *arg),
 *                         void *arg)
 *
 *   void sptree_NAME_replace(sptree_NAME *tree, void *value, void **p_oldvalue)
 *   void sptree_NAME_delete(sptree_NAME *tree, void *value)
 *   void* sptree_NAME_find(sptree_NAME *tree, void *key)
 *
 *   spnode_t sptree_NAME_walk(sptree_NAME *t, void* array, spnode_t limit, spnode_t offset)
 *   void sptree_NAME_walk_cb(sptree_NAME *t, int (*cb)(void* cb_arg, void* elem), void *cb_arg)
 *
 *   sptree_NAME_iterator* sptree_NAME_iterator_init(sptree_NAME *t)
 *   void sptree_NAME_iterator_init_set(sptree_NAME *t, sptree_NAME_iterator **iterator, void *start)
 *   sptree_NAME_iterator* sptree_NAME_iterator_reverse_init(sptree_NAME *t)
 *   void sptree_NAME_iterator_reverse_init_set(sptree_NAME *t, sptree_NAME_iterator **iterator, void *start)
 *   void sptree_NAME_iterator_free(sptree_NAME_iterator *i)
 *
 *   void* sptree_NAME_iterator_next(sptree_NAME_iterator *i)
 *   void* sptree_NAME_iterator_reverse_next(sptree_NAME_iterator *i)
 */

#define SPTREE_AVL_DEF(name, realloc)                                                     \
typedef struct sptree_##name {                                                            \
    void *members;                                                                        \
    sptree_node_pointers *lrpointers;                                                     \
                                                                                          \
    spnode_t nmember;                                                                     \
    spnode_t ntotal;                                                                      \
                                                                                          \
    int (*compare)(const void *key, const void *elem, void *, compare_hint *);            \
    int (*elemcompare)(const void *e1, const void *e2, void *, compare_hint *);           \
    void* arg;                                                                            \
    size_t elemsize;                                                                      \
                                                                                          \
    spnode_t root;                                                                        \
    spnode_t garbage_head;                                                                \
    spnode_t size;                                                                        \
    spnode_t max_size;                                                                    \
    spnode_t max_depth;                                                                   \
    spnode_t min_item;                                                                    \
    spnode_t max_item;                                                                    \
} sptree_##name;                                                                          \
                                                                                          \
static spnode_t                                                                           \
sptree_##name##_mktree(sptree_##name *t, spnode_t depth,                                  \
		      spnode_t start, spnode_t end, int* height) {                        \
    spnode_t    half = ( (end + start) >> 1 ), tmp;                                       \
    int lh = 0, rh = 0;                                                                   \
                                                                                          \
    if (depth > t->max_depth) t->max_depth = depth;                                       \
                                                                                          \
    if ( half == start ||                                                                 \
            ( tmp = sptree_##name##_mktree(t, depth+1, start, half, &lh)) == half )       \
        _SET_SPNODE_LEFT(half, SPNIL);                                                    \
    else                                                                                  \
        _SET_SPNODE_LEFT(half, tmp);                                                      \
    if ( half+1 >= end ||                                                                 \
            ( tmp = sptree_##name##_mktree(t, depth+1, half+1, end, &rh)) == half )       \
        _SET_SPNODE_RIGHT(half, SPNIL);                                                   \
    else                                                                                  \
        _SET_SPNODE_RIGHT(half, tmp);                                                     \
                                                                                          \
    _SET_SPNODE_BALANCE(half, rh - lh);                                                   \
                                                                                          \
	if(height) {                                                                      \
		*height = (lh > rh ? lh : rh) + 1;                                        \
	}                                                                                 \
                                                                                          \
    return half;                                                                          \
}                                                                                         \
                                                                                          \
static inline int                                                                         \
sptree_##name##_height_of_subtree(sptree_##name *t, spnode_t node) {                      \
    if (node == SPNIL)                                                                    \
	    return 0;                                                                     \
	int l = sptree_##name##_height_of_subtree(t, _GET_SPNODE_LEFT(node));             \
	int r = sptree_##name##_height_of_subtree(t, _GET_SPNODE_RIGHT(node));            \
    return 1 + (l > r ? l : r);                                                           \
}                                                                                         \
                                                                                          \
static inline int                                                                         \
sptree_##name##_check_subtree(sptree_##name *t, spnode_t node) {                          \
    if (node == SPNIL)                                                                    \
	    return 0;                                                                     \
	if(_GET_SPNODE_LEFT(node) != SPNIL) {                                             \
		void* l = ITHELEM(t, _GET_SPNODE_LEFT(node));                             \
		void* c = ITHELEM(t, node);                                               \
		compare_hint dummy_hint = {0, 0};                                         \
		if(t->elemcompare(l, c, t->arg, &dummy_hint) >= 0) {                      \
			return 1;                                                         \
		}                                                                         \
	}                                                                                 \
	if(_GET_SPNODE_RIGHT(node) != SPNIL) {                                            \
		void* r = ITHELEM(t, _GET_SPNODE_RIGHT(node));                            \
		void* c = ITHELEM(t, node);                                               \
		compare_hint dummy_hint = {0, 0};                                         \
		if(t->elemcompare(c, r, t->arg, &dummy_hint) >= 0) {                      \
			return 2;                                                         \
		}                                                                         \
	}                                                                                 \
	int lh = sptree_##name##_height_of_subtree(t, _GET_SPNODE_LEFT(node));            \
	int rh = sptree_##name##_height_of_subtree(t, _GET_SPNODE_RIGHT(node));           \
	if(rh - lh != _GET_SPNODE_BALANCE(node)) {                                        \
		return 4;                                                                 \
	}                                                                                 \
	spnode_t l = sptree_##name##_check_subtree(t, _GET_SPNODE_LEFT(node));            \
	spnode_t r = sptree_##name##_check_subtree(t, _GET_SPNODE_RIGHT(node));           \
    return l | r;                                                                         \
}                                                                                         \
                                                                                          \
static inline void                                                                        \
sptree_##name##_init(sptree_##name *t, size_t elemsize, void *m,                          \
                     spnode_t nm, spnode_t nt,                                            \
                     int (*compare)(const void *, const void *, void *, compare_hint *),  \
                     int (*elemcompare)(const void *, const void *, void *, compare_hint *), \
                     void *arg) {                                                         \
    memset(t, 0, sizeof(*t));                                                             \
    t->members = m;                                                                       \
    t->max_size = t->size = t->nmember = nm;                                              \
    t->ntotal = (nt==0) ? nm : nt;                                                        \
    t->compare = compare != NULL ? compare : elemcompare;                                 \
    t->elemcompare = elemcompare != NULL ? elemcompare : compare;                         \
    t->arg = arg;                                                                         \
    t->elemsize = elemsize;                                                               \
    t->garbage_head = t->root = SPNIL;                                                    \
    t->min_item = t->max_item = SPNIL;                                                    \
                                                                                          \
    if (t->ntotal == 0 || t->members == NULL) { /* from scratch */                        \
        if (t->ntotal == 0) {                                                             \
            t->members = NULL;                                                            \
            t->ntotal = 64;                                                               \
        }                                                                                 \
                                                                                          \
        if (t->members == NULL)                                                           \
            t->members = realloc(NULL, elemsize * t->ntotal);                             \
    }                                                                                     \
    t->lrpointers = (sptree_node_pointers *) realloc(NULL,                                \
                                sizeof(sptree_node_pointers) * t->ntotal);                \
                                                                                          \
    if (t->nmember == 1) {                                                                \
        t->root = 0;                                                                      \
        _SET_SPNODE_RIGHT(0, SPNIL);                                                      \
        _SET_SPNODE_LEFT(0, SPNIL);                                                       \
        t->min_item = 0;                                                                  \
        t->max_item = 0;                                                                  \
    } else if (t->nmember > 1)    {                                                       \
        qsort_arg_hint(t->members, t->nmember, elemsize, t->elemcompare, t->arg);         \
        /* create tree */                                                                 \
        t->root = sptree_##name##_mktree(t, 1, 0, t->nmember, 0);                         \
        /*sptree_##name##_check_subtree(t, t->root);*/                                    \
        t->min_item = 0;                                                                  \
        t->max_item = t->nmember - 1;                                                     \
    }                                                                                     \
}                                                                                         \
                                                                                          \
static inline void                                                                        \
sptree_##name##_destroy(sptree_##name *t) {                                               \
        if (t == NULL)    return;                                                         \
    free(t->members);                                                                     \
    free(t->lrpointers);                                                                  \
}                                                                                         \
                                                                                          \
/** Nodes in the garbage list have a loop on their right link. */                         \
static inline bool                                                                        \
sptree_##name##_node_is_deleted(const sptree_##name *t, spnode_t node) {                  \
                                                                                          \
    return _GET_SPNODE_RIGHT(node) == node;                                               \
}                                                                                         \
                                                                                          \
static inline void*                                                                       \
sptree_##name##_first(const sptree_##name *t) {                                           \
    if (t->min_item != SPNIL)                                                             \
	return ITHELEM(t, t->min_item);                                                   \
    return NULL;                                                                          \
}                                                                                         \
                                                                                          \
static inline void*                                                                       \
sptree_##name##_last(const sptree_##name *t) {                                            \
    if (t->max_item != SPNIL)                                                             \
	return ITHELEM(t, t->max_item);                                                   \
    return NULL;                                                                          \
}                                                                                         \
                                                                                          \
static inline void*                                                                       \
sptree_##name##_find(const sptree_##name *t, void *k) {                                   \
    spnode_t    node = t->root;                                                           \
    if (node == SPNIL) {                                                                  \
        return NULL;                                                                      \
    }                                                                                     \
    compare_hint lh = {0, 0}, rh = {0, 0}, mh = {0, 0};                                   \
    int r = t->compare(k, ITHELEM(t, node), t->arg, &mh);                                 \
    if (r > 0) {                                                                          \
        node = _GET_SPNODE_RIGHT(node);                                                   \
        lh = mh;                                                                          \
        (void)t->compare(k, sptree_##name##_last(t), t->arg, &rh);                        \
    } else if (r < 0) {                                                                   \
        node = _GET_SPNODE_LEFT(node);                                                    \
        rh = mh;                                                                          \
        (void)t->compare(k, sptree_##name##_first(t), t->arg, &lh);                       \
    } else {                                                                              \
        return ITHELEM(t, node);                                                          \
    }                                                                                     \
                                                                                          \
    while(node != SPNIL) {                                                                \
	min_hint(&lh, &rh, &mh);                                                          \
	int r = t->compare(k, ITHELEM(t, node), t->arg, &mh);                             \
        if (r > 0) {                                                                      \
            node = _GET_SPNODE_RIGHT(node);                                               \
            lh = mh;                                                                      \
        } else if (r < 0) {                                                               \
            node = _GET_SPNODE_LEFT(node);                                                \
            rh = mh;                                                                      \
        } else {                                                                          \
            return ITHELEM(t, node);                                                      \
        }                                                                                 \
    }                                                                                     \
    return NULL;                                                                          \
}                                                                                         \
                                                                                          \
static inline void*                                                                       \
sptree_##name##_random(const sptree_##name *t, spnode_t rnd) {                            \
    for (spnode_t i = 0; i < t->size; i++, rnd++) {                                       \
        rnd %= t->nmember;                                                                \
        if (!sptree_##name##_node_is_deleted(t, rnd))                                     \
            return ITHELEM(t, rnd);                                                       \
                                                                                          \
    }                                                                                     \
                                                                                          \
    return NULL;                                                                          \
}                                                                                         \
static inline spnode_t                                                                    \
sptree_##name##_get_place(sptree_##name *t) {                                             \
    spnode_t    node;                                                                     \
    if (t->garbage_head != SPNIL) {                                                       \
        node = t->garbage_head;                                                           \
        t->garbage_head = _GET_SPNODE_LEFT(t->garbage_head);                              \
    } else {                                                                              \
        if (t->nmember >= t->ntotal) {                                                    \
            t->ntotal *= 2;                                                               \
            t->members = realloc(t->members, t->ntotal * t->elemsize);                    \
            t->lrpointers = (sptree_node_pointers *) realloc(t->lrpointers,               \
                                    t->ntotal * sizeof(sptree_node_pointers));            \
        }                                                                                 \
                                                                                          \
        node = t->nmember;                                                                \
        t->nmember++;                                                                     \
    }                                                                                     \
    _SET_SPNODE_LEFT(node, SPNIL);                                                        \
    _SET_SPNODE_RIGHT(node, SPNIL);                                                       \
    _SET_SPNODE_BALANCE(node, 0);                                                         \
    return node;                                                                          \
}                                                                                         \
                                                                                          \
static inline bool                                                                        \
sptree_##name##_rotate_left(sptree_##name *t, spnode_t parent, spnode_t *new_parent) {    \
	spnode_t node = _GET_SPNODE_RIGHT(parent);                                        \
	if(_GET_SPNODE_BALANCE(node) > 0) {                                               \
		_SET_SPNODE_BALANCE(parent, 0);                                           \
		_SET_SPNODE_BALANCE(node, 0);                                             \
		_SET_SPNODE_RIGHT(parent, _GET_SPNODE_LEFT(node));                        \
		_SET_SPNODE_LEFT(node, parent);                                           \
		*new_parent = node;                                                       \
		return true;                                                              \
	} else if(_GET_SPNODE_BALANCE(node) == 0) {                                       \
		_SET_SPNODE_BALANCE(parent, 1);                                           \
		_SET_SPNODE_BALANCE(node, -1);                                            \
		_SET_SPNODE_RIGHT(parent, _GET_SPNODE_LEFT(node));                        \
		_SET_SPNODE_LEFT(node, parent);                                           \
		*new_parent = node;                                                       \
		return false;                                                             \
	} else {                                                                          \
		spnode_t l = _GET_SPNODE_LEFT(node);                                      \
		spnode_t ll = _GET_SPNODE_LEFT(l);                                        \
		spnode_t lr = _GET_SPNODE_RIGHT(l);                                       \
		int l_balance = _GET_SPNODE_BALANCE(l);                                   \
		_SET_SPNODE_BALANCE(l, 0);                                                \
		_SET_SPNODE_BALANCE(node, l_balance < 0 ? 1 : 0);                         \
		_SET_SPNODE_BALANCE(parent, l_balance > 0 ? -1 : 0);                      \
		_SET_SPNODE_RIGHT(parent, ll);                                            \
		_SET_SPNODE_LEFT(node, lr);                                               \
		_SET_SPNODE_LEFT(l, parent);                                              \
		_SET_SPNODE_RIGHT(l, node);                                               \
		*new_parent = l;                                                          \
		return true;                                                              \
	}                                                                                 \
}                                                                                         \
                                                                                          \
static inline bool                                                                        \
sptree_##name##_rotate_right(sptree_##name *t, spnode_t parent, spnode_t *new_parent) {   \
	spnode_t node = _GET_SPNODE_LEFT(parent);                                         \
	if(_GET_SPNODE_BALANCE(node) < 0) {                                               \
		_SET_SPNODE_BALANCE(parent, 0);                                           \
		_SET_SPNODE_BALANCE(node, 0);                                             \
		_SET_SPNODE_LEFT(parent, _GET_SPNODE_RIGHT(node));                        \
		_SET_SPNODE_RIGHT(node, parent);                                          \
		*new_parent = node;                                                       \
		return true;                                                              \
	} else if(_GET_SPNODE_BALANCE(node) == 0) {                                       \
		_SET_SPNODE_BALANCE(parent, -1);                                          \
		_SET_SPNODE_BALANCE(node, 1);                                             \
		_SET_SPNODE_LEFT(parent, _GET_SPNODE_RIGHT(node));                        \
		_SET_SPNODE_RIGHT(node, parent);                                          \
		*new_parent = node;                                                       \
		return false;                                                             \
	} else {                                                                          \
		spnode_t r = _GET_SPNODE_RIGHT(node);                                     \
		spnode_t rl = _GET_SPNODE_LEFT(r);                                        \
		spnode_t rr = _GET_SPNODE_RIGHT(r);                                       \
		int r_balance = _GET_SPNODE_BALANCE(r);                                   \
		_SET_SPNODE_BALANCE(r, 0);                                                \
		_SET_SPNODE_BALANCE(node, r_balance > 0 ? -1 : 0);                        \
		_SET_SPNODE_BALANCE(parent, r_balance < 0 ? 1 : 0);                       \
		_SET_SPNODE_LEFT(parent, rr);                                             \
		_SET_SPNODE_RIGHT(node, rl);                                              \
		_SET_SPNODE_RIGHT(r, parent);                                             \
		_SET_SPNODE_LEFT(r, node);                                                \
		*new_parent = r;                                                          \
		return true;                                                              \
	}                                                                                 \
}                                                                                         \
                                                                                          \
static inline void                                                                        \
sptree_##name##_replace(sptree_##name *t, void *v, void **p_old) {                        \
    spnode_t    node, depth = 0;                                                          \
    spnode_t    path[ MAX_DEPTH ];                                                        \
                                                                                          \
    if (t->root == SPNIL) {                                                               \
        _SET_SPNODE_LEFT(0, SPNIL);                                                       \
        _SET_SPNODE_RIGHT(0, SPNIL);                                                      \
        _SET_SPNODE_BALANCE(0, 0);                                                        \
        memcpy(t->members, v, t->elemsize);                                               \
        t->root = 0;                                                                      \
        t->min_item = t->max_item = 0;                                                    \
        t->garbage_head = SPNIL;                                                          \
        t->nmember = 1;                                                                   \
        t->size=1;                                                                        \
        if (p_old)                                                                        \
            *p_old = NULL;                                                                \
        return;                                                                           \
    } else {                                                                              \
	compare_hint lh = {0, 0}, rh = {0, 0}, mh = {0, 0};                               \
        spnode_t    parent = t->root;                                                     \
        int r = t->elemcompare(v, ITHELEM(t, parent), t->arg, &mh);                       \
        if (r==0) {                                                                       \
            if (p_old)                                                                    \
                memcpy(*p_old, ITHELEM(t, parent), t->elemsize);                          \
            memcpy(ITHELEM(t, parent), v, t->elemsize);                                   \
            return;                                                                       \
        }                                                                                 \
        path[depth] = parent;                                                             \
        depth++;                                                                          \
        int right_only = 1;                                                               \
        int left_only = 1;                                                                \
        int need_deeper_search = 1;                                                       \
        if (r>0) {                                                                        \
            if (_GET_SPNODE_RIGHT(parent) == SPNIL) {                                     \
                node = sptree_##name##_get_place(t);                                      \
                memcpy(ITHELEM(t, node), v, t->elemsize);                                 \
                _SET_SPNODE_RIGHT(parent, node);                                          \
                t->max_item = node;                                                       \
                need_deeper_search = 0;                                                   \
            } else {                                                                      \
        	left_only = 0;                                                            \
                parent = _GET_SPNODE_RIGHT(parent);                                       \
                lh = mh;                                                                  \
                (void)t->elemcompare(v, sptree_##name##_last(t), t->arg, &rh);            \
            }                                                                             \
        } else {                                                                          \
            if (_GET_SPNODE_LEFT(parent) == SPNIL) {                                      \
                node = sptree_##name##_get_place(t);                                      \
                memcpy(ITHELEM(t, node), v, t->elemsize);                                 \
                _SET_SPNODE_LEFT(parent, node);                                           \
                t->min_item = node;                                                       \
                need_deeper_search = 0;                                                   \
            } else {                                                                      \
                right_only = 0;                                                           \
                parent = _GET_SPNODE_LEFT(parent);                                        \
                rh = mh;                                                                  \
                (void)t->elemcompare(v, sptree_##name##_first(t), t->arg, &lh);           \
            }                                                                             \
        }                                                                                 \
        if (need_deeper_search) {                                                         \
		for(;;)    {                                                              \
		    min_hint(&lh, &rh, &mh);                                              \
		    int r = t->elemcompare(v, ITHELEM(t, parent), t->arg, &mh);           \
		    if (r==0) {                                                           \
			if (p_old)                                                        \
			    memcpy(*p_old, ITHELEM(t, parent), t->elemsize);              \
			memcpy(ITHELEM(t, parent), v, t->elemsize);                       \
			return;                                                           \
		    }                                                                     \
		    path[depth] = parent;                                                 \
		    depth++;                                                              \
		    if (r>0) {                                                            \
			if (_GET_SPNODE_RIGHT(parent) == SPNIL) {                         \
			    node = sptree_##name##_get_place(t);                          \
			    memcpy(ITHELEM(t, node), v, t->elemsize);                     \
			    _SET_SPNODE_RIGHT(parent, node);                              \
			    if (right_only)                                               \
				    t->max_item = node;                                   \
			    break;                                                        \
			} else {                                                          \
	        	    left_only = 0;                                                \
			    parent = _GET_SPNODE_RIGHT(parent);                           \
			    lh = mh;                                                      \
			}                                                                 \
		    } else {                                                              \
			if (_GET_SPNODE_LEFT(parent) == SPNIL) {                          \
			    node = sptree_##name##_get_place(t);                          \
			    memcpy(ITHELEM(t, node), v, t->elemsize);                     \
			    _SET_SPNODE_LEFT(parent, node);                               \
			    if (left_only)                                                \
				    t->min_item = node;                                   \
			    break;                                                        \
			} else {                                                          \
			    right_only = 0;                                               \
			    parent = _GET_SPNODE_LEFT(parent);                            \
			    rh = mh;                                                      \
			}                                                                 \
		    }                                                                     \
		}                                                                         \
	}                                                                                 \
    }                                                                                     \
    if (p_old)                                                                            \
        *p_old = NULL;                                                                    \
                                                                                          \
    t->size++;                                                                            \
    if ( t->size > t->max_size )                                                          \
        t->max_size = t->size;                                                            \
    if ( depth > t->max_depth )                                                           \
        t->max_depth = depth;                                                             \
                                                                                          \
	path[depth] = node;                                                               \
    while(depth > 0) {                                                                    \
        spnode_t node = path[depth];                                                      \
        spnode_t parent = path[depth - 1];                                                \
        if(_GET_SPNODE_RIGHT(parent) == node) {                                           \
        	if(_GET_SPNODE_BALANCE(parent) < 0) {                                     \
        		_SET_SPNODE_BALANCE(parent, 0);                                   \
        		break;                                                            \
        	} else if(_GET_SPNODE_BALANCE(parent) == 0) {                             \
        		_SET_SPNODE_BALANCE(parent, 1);                                   \
        	} else {                                                                  \
        		bool should_break =                                               \
        			sptree_##name##_rotate_left(t, parent, path + depth - 1); \
        		if(depth > 1) {                                                   \
        			if(_GET_SPNODE_LEFT(path[depth-2]) == parent) {           \
        				_SET_SPNODE_LEFT(path[depth-2], path[depth-1]);   \
        			} else {                                                  \
        				_SET_SPNODE_RIGHT(path[depth-2], path[depth-1]);  \
        			}                                                         \
        		}                                                                 \
        		if(should_break) {                                                \
        			break;                                                    \
        		}                                                                 \
        	}                                                                         \
        } else {                                                                          \
        	if(_GET_SPNODE_BALANCE(parent) > 0) {                                     \
        		_SET_SPNODE_BALANCE(parent, 0);                                   \
        		break;                                                            \
        	} else if(_GET_SPNODE_BALANCE(parent) == 0) {                             \
        		_SET_SPNODE_BALANCE(parent, -1);                                  \
        	} else {                                                                  \
        		bool should_break =                                               \
        			sptree_##name##_rotate_right(t, parent, path + depth - 1);\
        		if(depth > 1) {                                                   \
        			if(_GET_SPNODE_LEFT(path[depth-2]) == parent) {           \
        				_SET_SPNODE_LEFT(path[depth-2], path[depth-1]);   \
        			} else {                                                  \
        				_SET_SPNODE_RIGHT(path[depth-2], path[depth-1]);  \
        			}                                                         \
        		}                                                                 \
        		if(should_break) {                                                \
        			break;                                                    \
        		}                                                                 \
        	}                                                                         \
        }                                                                                 \
        depth--;                                                                          \
    }                                                                                     \
    t->root = path[0];                                                                    \
    /*sptree_##name##_check_subtree(t, t->root);*/                                        \
}                                                                                         \
                                                                                          \
static inline void                                                                        \
sptree_##name##_delete(sptree_##name *t, void *k) {                                       \
    spnode_t    node = t->root;                                                           \
    spnode_t    parent = SPNIL;                                                           \
    spnode_t    depth = 0;                                                                \
    spnode_t    path[MAX_DEPTH];                                                          \
    int            lr = 0;                                                                \
    compare_hint lh = {0, 0}, rh = {0, 0}, mh = {0, 0};                                   \
    int first_round = 1;                                                                  \
    while(node != SPNIL) {                                                                \
        path[depth++] = node;                                                             \
        int r;                                                                            \
        if (first_round) {                                                                \
                first_round = 0;                                                          \
                r = t->elemcompare(k, ITHELEM(t, node), t->arg, &mh);                     \
                if (r > 0) {                                                              \
                        (void)t->elemcompare(k, sptree_##name##_last(t), t->arg, &rh);    \
                } else if (r < 0) {                                                       \
                        (void)t->elemcompare(k, sptree_##name##_first(t), t->arg, &lh);   \
                }                                                                         \
    	} else {                                                                          \
                min_hint(&lh, &rh, &mh);                                                  \
                r = t->elemcompare(k, ITHELEM(t, node), t->arg, &mh);                     \
        }                                                                                 \
        if (r > 0) {                                                                      \
            parent = node;                                                                \
            node = _GET_SPNODE_RIGHT(node);                                               \
            lr = +1;                                                                      \
            lh = mh;                                                                      \
        } else if (r < 0) {                                                               \
            parent = node;                                                                \
            node = _GET_SPNODE_LEFT(node);                                                \
            lr = -1;                                                                      \
            rh = mh;                                                                      \
        } else {/* found */                                                               \
            if (_GET_SPNODE_LEFT(node) == SPNIL && _GET_SPNODE_RIGHT(node) == SPNIL) {    \
                if (t->min_item == node)                                                  \
            	    t->min_item = parent;                                                 \
		if (t->max_item == node)                                                  \
		    t->max_item = parent;                                                 \
            	path[depth-1] = SPNIL;                                                    \
                if ( parent == SPNIL )                                                    \
                    t->root = SPNIL;                                                      \
                else if (lr <0)                                                           \
                    _SET_SPNODE_LEFT(parent, SPNIL);                                      \
                else                                                                      \
                    _SET_SPNODE_RIGHT(parent, SPNIL);                                     \
            } else if (_GET_SPNODE_LEFT(node) == SPNIL) {                                 \
                spnode_t    child = _GET_SPNODE_RIGHT(node);                              \
                if (t->min_item == node) {                                                \
                	spnode_t min_item_search = child;                                 \
                	spnode_t tmp;                                                     \
                	while ((tmp = _GET_SPNODE_LEFT(min_item_search)) != SPNIL)        \
                		min_item_search = tmp;                                    \
                	t->min_item = min_item_search;                                    \
                }                                                                         \
                path[depth-1] = child;                                                    \
                if (parent == SPNIL) t->root = child;                                     \
                else if (lr <0) _SET_SPNODE_LEFT(parent, child);                          \
                else _SET_SPNODE_RIGHT(parent, child);                                    \
            } else if (_GET_SPNODE_RIGHT(node) == SPNIL) {                                \
                spnode_t    child = _GET_SPNODE_LEFT(node);                               \
                if (t->max_item == node) {                                                \
                	spnode_t max_item_search = child;                                 \
                	spnode_t tmp;                                                     \
                	while ((tmp = _GET_SPNODE_RIGHT(max_item_search)) != SPNIL)       \
                		max_item_search = tmp;                                    \
                	t->max_item = max_item_search;                                    \
                }                                                                         \
                path[depth-1] = child;                                                    \
                if (parent == SPNIL) t->root = child;                                     \
                else if (lr <0) _SET_SPNODE_LEFT(parent, child);                          \
                else _SET_SPNODE_RIGHT(parent, child);                                    \
            } else {                                                                      \
                spnode_t todel;                                                           \
                if (_GET_SPNODE_BALANCE(node) >= 0) {                                     \
                    todel = _GET_SPNODE_RIGHT(node);                                      \
            	    path[depth++] = todel;                                                \
                    parent = SPNIL;                                                       \
                    lr = 1;                                                               \
                    for(;;) {                                                             \
                        if ( _GET_SPNODE_LEFT(todel) != SPNIL ) {                         \
                            parent = todel;                                               \
                            todel = _GET_SPNODE_LEFT(todel);                              \
                    	    path[depth++] = todel;                                        \
                            lr = -1;                                                      \
                        } else                                                            \
                            break;                                                        \
                    }                                                                     \
                    memcpy(ITHELEM(t, node), ITHELEM(t, todel), t->elemsize);             \
                    if (parent != SPNIL)                                                  \
                        _SET_SPNODE_LEFT(parent, _GET_SPNODE_RIGHT(todel));               \
                    else                                                                  \
                        _SET_SPNODE_RIGHT(node, _GET_SPNODE_RIGHT(todel));                \
            	} else {                                                                  \
                    todel = _GET_SPNODE_LEFT(node);                                       \
            	    path[depth++] = todel;                                                \
                    parent = SPNIL;                                                       \
                    lr = -1;                                                              \
                    for(;;) {                                                             \
                        if ( _GET_SPNODE_RIGHT(todel) != SPNIL ) {                        \
                            parent = todel;                                               \
                            todel = _GET_SPNODE_RIGHT(todel);                             \
                    	    path[depth++] = todel;                                        \
                            lr = 1;                                                       \
                        } else                                                            \
                            break;                                                        \
                    }                                                                     \
                    memcpy(ITHELEM(t, node), ITHELEM(t, todel), t->elemsize);             \
                    if (parent != SPNIL)                                                  \
                        _SET_SPNODE_RIGHT(parent, _GET_SPNODE_LEFT(todel));               \
                    else                                                                  \
                        _SET_SPNODE_LEFT(node, _GET_SPNODE_LEFT(todel));                  \
                }                                                                         \
                node = todel; /* node to delete */                                        \
            }                                                                             \
                                                                                          \
            _SET_SPNODE_LEFT(node, t->garbage_head);                                      \
            /*                                                                            \
             * Loop back on the right link indicates that the node                        \
             * is in the garbage list.                                                    \
             */                                                                           \
            _SET_SPNODE_RIGHT(node, node);                                                \
            t->garbage_head = node;                                                       \
                                                                                          \
            break;                                                                        \
        }                                                                                 \
    }                                                                                     \
                                                                                          \
    if (node == SPNIL) /* not found */                                                    \
        return;                                                                           \
                                                                                          \
    t->size --;                                                                           \
                                                                                          \
    depth--;                                                                              \
    while(depth > 0) {                                                                    \
        spnode_t node = path[depth];                                                      \
        spnode_t parent = path[depth - 1];                                                \
        if(lr == 1 || (lr == 0 && _GET_SPNODE_RIGHT(parent) == node)) {                   \
        	if(_GET_SPNODE_BALANCE(parent) == 0) {                                    \
        		_SET_SPNODE_BALANCE(parent, -1);                                  \
        		break;                                                            \
        	} else if(_GET_SPNODE_BALANCE(parent) == 1) {                             \
        		_SET_SPNODE_BALANCE(parent, 0);                                   \
        	} else {                                                                  \
        		bool should_break =                                               \
        			!sptree_##name##_rotate_right(t, parent, path + depth - 1);\
        		if(depth > 1) {                                                   \
        			if(_GET_SPNODE_LEFT(path[depth-2]) == parent) {           \
        				_SET_SPNODE_LEFT(path[depth-2], path[depth-1]);   \
        			} else {                                                  \
        				_SET_SPNODE_RIGHT(path[depth-2], path[depth-1]);  \
        			}                                                         \
        		}                                                                 \
        		if(should_break) {                                                \
        			break;                                                    \
        		}                                                                 \
        	}                                                                         \
        } else {                                                                          \
        	if(_GET_SPNODE_BALANCE(parent) == 0) {                                    \
        		_SET_SPNODE_BALANCE(parent, 1);                                   \
        		break;                                                            \
        	} else if(_GET_SPNODE_BALANCE(parent) == -1) {                            \
        		_SET_SPNODE_BALANCE(parent, 0);                                   \
        	} else {                                                                  \
        		bool should_break =                                               \
        			!sptree_##name##_rotate_left(t, parent, path + depth - 1);\
        		if(depth > 1) {                                                   \
        			if(_GET_SPNODE_LEFT(path[depth-2]) == parent) {           \
        				_SET_SPNODE_LEFT(path[depth-2], path[depth-1]);   \
        			} else {                                                  \
        				_SET_SPNODE_RIGHT(path[depth-2], path[depth-1]);  \
        			}                                                         \
        		}                                                                 \
        		if(should_break) {                                                \
        			break;                                                    \
        		}                                                                 \
        	}                                                                         \
        }                                                                                 \
        lr = 0;                                                                           \
        depth--;                                                                          \
    }                                                                                     \
    t->root = path[0];                                                                    \
    /*sptree_##name##_check_subtree(t, t->root);*/                                        \
                                                                                          \
}                                                                                         \
                                                                                          \
static inline spnode_t                                                                    \
sptree_##name##_walk(sptree_##name *t, void* array, spnode_t limit, spnode_t offset) {    \
    int         level = 0;                                                                \
    spnode_t    count= 0,                                                                 \
                node,                                                                     \
                stack[ t->max_depth + 1 ];                                                \
                                                                                          \
    if (t->root == SPNIL) return 0;                                                       \
    stack[0] = t->root;                                                                   \
                                                                                          \
    while( (node = _GET_SPNODE_LEFT( stack[level] )) != SPNIL ) {                         \
        level++;                                                                          \
        stack[level] = node;                                                              \
    }                                                                                     \
                                                                                          \
    while( count < offset + limit && level >= 0 ) {                                       \
                                                                                          \
        if (count >= offset)                                                              \
             memcpy((char *) array + (count-offset) * t->elemsize,                        \
                    ITHELEM(t, stack[level]), t->elemsize);                               \
        count++;                                                                          \
                                                                                          \
        node = _GET_SPNODE_RIGHT( stack[level] );                                         \
        level--;                                                                          \
        while( node != SPNIL ) {                                                          \
            level++;                                                                      \
            stack[level] = node;                                                          \
            node = _GET_SPNODE_LEFT( stack[level] );                                      \
        }                                                                                 \
    }                                                                                     \
                                                                                          \
    return (count > offset) ? count - offset : 0;                                         \
}                                                                                         \
                                                                                          \
static inline void                                                                        \
sptree_##name##_walk_cb(sptree_##name *t, int (*cb)(void*, void*), void *cb_arg ) {       \
    int         level = 0;                                                                \
    spnode_t    node,                                                                     \
                stack[ t->max_depth + 1 ];                                                \
                                                                                          \
    if (t->root == SPNIL) return;                                                         \
    stack[0] = t->root;                                                                   \
                                                                                          \
    while( (node = _GET_SPNODE_LEFT( stack[level] )) != SPNIL ) {                         \
        level++;                                                                          \
        stack[level] = node;                                                              \
    }                                                                                     \
                                                                                          \
    while( level >= 0 ) {                                                                 \
        if ( cb(cb_arg, ITHELEM(t, stack[level])) == 0 )                                  \
             return;                                                                      \
                                                                                          \
        node = _GET_SPNODE_RIGHT( stack[level] );                                         \
        level--;                                                                          \
        while( node != SPNIL ) {                                                          \
            level++;                                                                      \
            stack[level] = node;                                                          \
            node = _GET_SPNODE_LEFT( stack[level] );                                      \
        }                                                                                 \
    }                                                                                     \
}                                                                                         \
                                                                                          \
typedef struct sptree_##name##_iterator {                                                 \
    const sptree_##name        *t;                                                        \
    int                  level;                                                           \
    int                  max_depth;                                                       \
    spnode_t             stack[0];                                                        \
} sptree_##name##_iterator;                                                               \
                                                                                          \
static inline sptree_##name##_iterator *                                                  \
sptree_##name##_iterator_alloc(sptree_##name *t) {                                        \
    sptree_##name##_iterator *i = (sptree_##name##_iterator *)                            \
        realloc(NULL, sizeof(*i) + sizeof(spnode_t) * (t->max_depth + 1));                \
    i->t = t;                                                                             \
    i->level = 0;                                                                         \
    i->stack[0] = t->root;                                                                \
    return i;                                                                             \
}                                                                                         \
                                                                                          \
static inline sptree_##name##_iterator *                                                  \
sptree_##name##_iterator_init(sptree_##name *t) {                                         \
    sptree_##name##_iterator *i;                                                          \
    spnode_t node;                                                                        \
                                                                                          \
    if (t->root == SPNIL) return NULL;                                                    \
    i = sptree_##name##_iterator_alloc(t);                                                \
                                                                                          \
    while( (node = _GET_SPNODE_LEFT( i->stack[i->level] )) != SPNIL ) {                   \
        i->level++;                                                                       \
        i->stack[i->level] = node;                                                        \
    }                                                                                     \
                                                                                          \
    return i;                                                                             \
}                                                                                         \
                                                                                          \
static inline void                                                                        \
sptree_##name##_iterator_init_set(const sptree_##name *t, sptree_##name##_iterator **i,   \
                                  void *k) {                                              \
    spnode_t node;                                                                        \
    int      lastLevelEq = -1, cmp;                                                       \
                                                                                          \
    if ((*i) == NULL || t->max_depth > (*i)->max_depth)                                   \
        *i = (sptree_##name##_iterator *) realloc(*i, sizeof(**i) +                       \
                                       sizeof(spnode_t) * (t->max_depth + 31));           \
                                                                                          \
    (*i)->t = t;                                                                          \
    (*i)->level = -1;                                                                     \
    if (t->root == SPNIL) {                                                               \
            (*i)->max_depth = 0; /* valgrind points out it's used in the check above ^.*/ \
            return;                                                                       \
    }                                                                                     \
                                                                                          \
    (*i)->max_depth = t->max_depth;                                                       \
    (*i)->stack[0] = t->root;                                                             \
                                                                                          \
    node = t->root;                                                                       \
    compare_hint lh = {0, 0}, rh = {0, 0}, mh = {0, 0};                                   \
    if (node != SPNIL) {                                                                  \
        cmp = t->compare(k, ITHELEM(t, node), t->arg, &mh);                               \
                                                                                          \
        (*i)->level++;                                                                    \
        (*i)->stack[(*i)->level] = node;                                                  \
                                                                                          \
        if (cmp > 0) {                                                                    \
            (*i)->level--; /* exclude current node from path, ie "mark as visited" */     \
            node = _GET_SPNODE_RIGHT(node);                                               \
            lh = mh;                                                                      \
            (void)t->compare(k, sptree_##name##_last(t), t->arg, &rh);                    \
        } else if (cmp < 0) {                                                             \
            node = _GET_SPNODE_LEFT(node);                                                \
            rh = mh;                                                                      \
            (void)t->compare(k, sptree_##name##_first(t), t->arg, &lh);                   \
        } else {                                                                          \
            lastLevelEq = (*i)->level;                                                    \
            node = _GET_SPNODE_LEFT(node); /* one way iterator: from left to right */     \
            rh = mh;                                                                      \
            (void)t->compare(k, sptree_##name##_first(t), t->arg, &lh);                   \
        }                                                                                 \
    }                                                                                     \
    while(node != SPNIL) {                                                                \
	min_hint(&lh, &rh, &mh);                                                          \
        cmp = t->compare(k, ITHELEM(t, node), t->arg, &mh);                               \
                                                                                          \
        (*i)->level++;                                                                    \
        (*i)->stack[(*i)->level] = node;                                                  \
                                                                                          \
        if (cmp > 0) {                                                                    \
            (*i)->level--; /* exclude current node from path, ie "mark as visited" */     \
            node = _GET_SPNODE_RIGHT(node);                                               \
            lh = mh;                                                                      \
        } else if (cmp < 0) {                                                             \
            node = _GET_SPNODE_LEFT(node);                                                \
            rh = mh;                                                                      \
        } else {                                                                          \
            lastLevelEq = (*i)->level;                                                    \
            node = _GET_SPNODE_LEFT(node); /* one way iterator: from left to right */     \
            rh = mh;                                                                      \
        }                                                                                 \
    }                                                                                     \
                                                                                          \
    if (lastLevelEq >= 0)                                                                 \
        (*i)->level = lastLevelEq;                                                        \
}                                                                                         \
                                                                                          \
static inline sptree_##name##_iterator *                                                  \
sptree_##name##_iterator_reverse_init(sptree_##name *t) {                                 \
    sptree_##name##_iterator *i;                                                          \
    spnode_t node;                                                                        \
                                                                                          \
    if (t->root == SPNIL) return NULL;                                                    \
    i = sptree_##name##_iterator_alloc(t);                                                \
                                                                                          \
    while( (node = _GET_SPNODE_RIGHT( i->stack[i->level] )) != SPNIL ) {                  \
        i->level++;                                                                       \
        i->stack[i->level] = node;                                                        \
    }                                                                                     \
                                                                                          \
    return i;                                                                             \
}                                                                                         \
                                                                                          \
static inline void                                                                        \
sptree_##name##_iterator_reverse_init_set(const sptree_##name *t,                         \
                                          sptree_##name##_iterator **i, void *k) {        \
    spnode_t node;                                                                        \
    int      lastLevelEq = -1, cmp;                                                       \
                                                                                          \
    if ((*i) == NULL || t->max_depth > (*i)->max_depth)                                   \
        *i = (sptree_##name##_iterator *) realloc(*i, sizeof(**i) +                       \
                                      sizeof(spnode_t) * (t->max_depth + 31));            \
                                                                                          \
    (*i)->t = t;                                                                          \
    (*i)->level = -1;                                                                     \
    if (t->root == SPNIL) {                                                               \
            (*i)->max_depth = 0;                                                          \
            return;                                                                       \
    }                                                                                     \
                                                                                          \
    (*i)->max_depth = t->max_depth;                                                       \
    (*i)->stack[0] = t->root;                                                             \
                                                                                          \
    node = t->root;                                                                       \
    compare_hint lh = {0, 0}, rh = {0, 0}, mh = {0, 0};                                   \
    if (node != SPNIL) {                                                                  \
        cmp = t->compare(k, ITHELEM(t, node), t->arg, &mh);                               \
                                                                                          \
        (*i)->level++;                                                                    \
        (*i)->stack[(*i)->level] = node;                                                  \
                                                                                          \
        if (cmp < 0) {                                                                    \
            (*i)->level--;                                                                \
            node = _GET_SPNODE_LEFT(node);                                                \
            rh = mh;                                                                      \
            (void)t->compare(k, sptree_##name##_first(t), t->arg, &lh);                   \
        } else if (cmp > 0) {                                                             \
            node = _GET_SPNODE_RIGHT(node);                                               \
            lh = mh;                                                                      \
            (void)t->compare(k, sptree_##name##_last(t), t->arg, &rh);                    \
        } else {                                                                          \
            lastLevelEq = (*i)->level;                                                    \
            node = _GET_SPNODE_RIGHT(node);                                               \
            lh = mh;                                                                      \
            (void)t->compare(k, sptree_##name##_last(t), t->arg, &rh);                    \
        }                                                                                 \
    }                                                                                     \
    while(node != SPNIL) {                                                                \
        cmp = t->compare(k, ITHELEM(t, node), t->arg, &mh);                               \
                                                                                          \
        (*i)->level++;                                                                    \
        (*i)->stack[(*i)->level] = node;                                                  \
                                                                                          \
        if (cmp < 0) {                                                                    \
            (*i)->level--;                                                                \
            node = _GET_SPNODE_LEFT(node);                                                \
            rh = mh;                                                                      \
        } else if (cmp > 0) {                                                             \
            node = _GET_SPNODE_RIGHT(node);                                               \
            lh = mh;                                                                      \
        } else {                                                                          \
            lastLevelEq = (*i)->level;                                                    \
            node = _GET_SPNODE_RIGHT(node);                                               \
            lh = mh;                                                                      \
        }                                                                                 \
    }                                                                                     \
                                                                                          \
    if (lastLevelEq >= 0)                                                                 \
        (*i)->level = lastLevelEq;                                                        \
}                                                                                         \
                                                                                          \
static inline void                                                                        \
sptree_##name##_iterator_free(sptree_##name##_iterator *i) {                              \
    if (i == NULL)    return;                                                             \
    free(i);                                                                              \
}                                                                                         \
                                                                                          \
/**                                                                                       \
 * Get the last node on the iterator stack, check                                         \
 * if the node is not deleted.                                                            \
 */                                                                                       \
static inline spnode_t                                                                    \
sptree_##name##_iterator_next_node(sptree_##name##_iterator *i) {                         \
                                                                                          \
    while (i->level >= 0) {                                                               \
        spnode_t return_node = i->stack[i->level--];                                      \
        if (! sptree_##name##_node_is_deleted(i->t, return_node))                         \
            return return_node;                                                           \
    }                                                                                     \
    return SPNIL;                                                                         \
}                                                                                         \
                                                                                          \
static inline void*                                                                       \
sptree_##name##_iterator_next(sptree_##name##_iterator *i) {                              \
                                                                                          \
    if (i == NULL)  return NULL;                                                          \
                                                                                          \
    const sptree_##name *t = i->t;                                                        \
    spnode_t returnNode = sptree_##name##_iterator_next_node(i);                          \
                                                                                          \
    if (returnNode == SPNIL) return NULL;                                                 \
                                                                                          \
    spnode_t node = _GET_SPNODE_RIGHT(returnNode);                                        \
    while (node != SPNIL) {                                                               \
        i->level++;                                                                       \
        i->stack[i->level] = node;                                                        \
        node = _GET_SPNODE_LEFT(i->stack[i->level]);                                      \
    }                                                                                     \
                                                                                          \
    return ITHELEM(t, returnNode);                                                        \
}                                                                                         \
                                                                                          \
static inline void*                                                                       \
sptree_##name##_iterator_reverse_next(sptree_##name##_iterator *i) {                      \
                                                                                          \
    if (i == NULL)  return NULL;                                                          \
                                                                                          \
    const sptree_##name *t = i->t;                                                        \
    spnode_t returnNode = sptree_##name##_iterator_next_node(i);                          \
                                                                                          \
    if (returnNode == SPNIL) return NULL;                                                 \
                                                                                          \
    spnode_t node = _GET_SPNODE_LEFT(returnNode);                                         \
    while (node != SPNIL) {                                                               \
        i->level++;                                                                       \
        i->stack[i->level] = node;                                                        \
        node = _GET_SPNODE_RIGHT(i->stack[i->level]);                                     \
    }                                                                                     \
    return ITHELEM(t, returnNode);                                                        \
}
/*
 * vim: ts=4 sts=4 et
 */
#endif

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */
