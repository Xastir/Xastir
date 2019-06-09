

/* Copyright (C) 2002, 2004 Christopher Clark <firstname.lastname@cl.cam.ac.uk> */
// Portions Copyright (C) 2000-2019 The Xastir Group


#ifndef __HASHTABLE_ITR_CWC22__
#define __HASHTABLE_ITR_CWC22__
#include "hashtable.h"
#include "hashtable_private.h" /* needed to enable inlining */

/*****************************************************************************/
/* This struct is only concrete here to allow the inlining of two of the
 * accessor functions. */
struct hashtable_itr
{
  struct hashtable *h;
  struct entry *e;
  struct entry *parent;
  unsigned int index;
};


/*****************************************************************************/
/* hashtable_iterator
 */

struct hashtable_itr *
hashtable_iterator(struct hashtable *h);

#if 0
// BZZZZT!  it is very, very wrong to be inlining this this way.
// If one calls hashtable_iterator on a hash table from which everything
// has been deleted, the iterator has a null for i->e.
// It is not good to require the caller to check the internals of the iterator
// structure just to be sure there are no null pointers inside.
// For whatever reason, these are defined again in the hashtable_iterator.c
// file, not inlined.  I have modified the ones in hashtable_iterator so they
// actually check for nulls and don't try to dereference them.
/*****************************************************************************/
/* hashtable_iterator_key
 * - return the value of the (key,value) pair at the current position */

extern inline void *
hashtable_iterator_key(struct hashtable_itr *i)
{
  return i->e->k;
}

/*****************************************************************************/
/* value - return the value of the (key,value) pair at the current position */

extern inline void *
hashtable_iterator_value(struct hashtable_itr *i)
{
  return i->e->v;
}
#else
// SO instead of inlining, just declare.  No need to be "extern"
// The ones in the .c file check their arguments and return nulls if they
// can't comply with the request.  Much nicer for the calling routine to
// check a return value than to monkey with the internals of the struct.
void * hashtable_iterator_key(struct hashtable_itr *i);
void * hashtable_iterator_value(struct hashtable_itr *i);
#endif

/*****************************************************************************/
/* advance - advance the iterator to the next element
 *           returns zero if advanced to end of table */

int
hashtable_iterator_advance(struct hashtable_itr *itr);

/*****************************************************************************/
/* remove - remove current element and advance the iterator to the next element
 *          NB: if you need the value to free it, read it before
 *          removing. ie: beware memory leaks!
 *          returns zero if advanced to end of table */

int
hashtable_iterator_remove(struct hashtable_itr *itr);

/*****************************************************************************/
/* search - overwrite the supplied iterator, to point to the entry
 *          matching the supplied key.
            h points to the hashtable to be searched.
 *          returns zero if not found. */
int
hashtable_iterator_search(struct hashtable_itr *itr,
                          struct hashtable *h, void *k);

#define DEFINE_HASHTABLE_ITERATOR_SEARCH(fnname, keytype) \
int fnname (struct hashtable_itr *i, struct hashtable *h, keytype *k) \
{ \
    return (hashtable_iterator_search(i,h,k)); \
}



#endif /* __HASHTABLE_ITR_CWC22__*/

/*
 * Copyright (C) 2002, 2004 Christopher Clark <firstname.lastname@cl.cam.ac.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * */


