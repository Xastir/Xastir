

/* Copyright (C) 2002, 2004 Christopher Clark  <firstname.lastname@cl.cam.ac.uk> */
/* Portions Copyright (C) 2000-2019 The Xastir Group */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdlib.h> /* defines NULL */
#include <stdio.h>
//#include <string.h>
//#include <math.h>

#include "hashtable.h"
#include "hashtable_private.h"
#include "hashtable_itr.h"

// Must be last include file
#include "leak_detection.h" /* defines GC_MALLOC/GC_FREE */





/*****************************************************************************/
/* hashtable_iterator    - iterator constructor */

struct hashtable_itr *
hashtable_iterator(struct hashtable *h)
{
  unsigned int i, tablelength;
  struct hashtable_itr *itr = (struct hashtable_itr *)
                              malloc(sizeof(struct hashtable_itr));
  if (NULL == itr)
  {
    return NULL;
  }
  itr->h = h;
  itr->e = NULL;
  itr->parent = NULL;
  tablelength = h->tablelength;
  itr->index = tablelength;
  if (0 == h->entrycount)
  {
    return itr;
  }

  for (i = 0; i < tablelength; i++)
  {
    if (NULL != h->table[i])
    {
      itr->e = h->table[i];
      itr->index = i;
      break;
    }
  }
  return itr;
}

/*****************************************************************************/
/* key      - return the key of the (key,value) pair at the current position */
/* value    - return the value of the (key,value) pair at the current position */

void *
hashtable_iterator_key(struct hashtable_itr *i)
{
  if (!i)
  {
    return NULL;
  }
  if (i->e)
  {
    return i->e->k;
  }
  else
  {
    return NULL;
  }
}

void *
hashtable_iterator_value(struct hashtable_itr *i)
{
  if (!i)
  {
    return NULL;
  }
  if (i->e)
  {
    return i->e->v;
  }
  else
  {
    return NULL;
  }

}

/*****************************************************************************/
/* advance - advance the iterator to the next element
 *           returns zero if advanced to end of table */

int
hashtable_iterator_advance(struct hashtable_itr *itr)
{
  unsigned int j,tablelength;
  struct entry **table;
  struct entry *next;
  if (NULL == itr->e)
  {
    return 0;  /* stupidity check */
  }

  next = itr->e->next;
  if (NULL != next)
  {
    itr->parent = itr->e;
    itr->e = next;
    return -1;
  }
  tablelength = itr->h->tablelength;
  itr->parent = NULL;
  if (tablelength <= (j = ++(itr->index)))
  {
    itr->e = NULL;
    return 0;
  }
  table = itr->h->table;
  while (NULL == (next = table[j]))
  {
    if (++j >= tablelength)
    {
      itr->index = tablelength;
      itr->e = NULL;
      return 0;
    }
  }
  itr->index = j;
  itr->e = next;
  return -1;
}

/*****************************************************************************/
/* remove - remove the entry at the current iterator position
 *          and advance the iterator, if there is a successive
 *          element.
 *          If you want the value, read it before you remove:
 *          beware memory leaks if you don't.
 *          Returns zero if end of iteration. */

int
hashtable_iterator_remove(struct hashtable_itr *itr)
{
  struct entry *remember_e, *remember_parent;
  int ret;

  /* Do the removal */
  if (NULL == (itr->parent))
  {
    /* element is head of a chain */
    itr->h->table[itr->index] = itr->e->next;
  }
  else
  {
    /* element is mid-chain */
    itr->parent->next = itr->e->next;
  }
  /* itr->e is now outside the hashtable */
  remember_e = itr->e;
  itr->h->entrycount--;
  freekey(remember_e->k);

  /* Advance the iterator, correcting the parent */
  remember_parent = itr->parent;
  ret = hashtable_iterator_advance(itr);
  if (itr->parent == remember_e)
  {
    itr->parent = remember_parent;
  }
  free(remember_e);
  return ret;
}

/*****************************************************************************/
int /* returns zero if not found */
hashtable_iterator_search(struct hashtable_itr *itr,
                          struct hashtable *h, void *k)
{
  struct entry *e, *parent;
  unsigned int hashvalue, index;

  hashvalue = hash(h,k);
  index = indexFor(h->tablelength,hashvalue);

  e = h->table[index];
  parent = NULL;
  while (NULL != e)
  {
    /* Check hash value to short circuit heavier comparison */
    if ((hashvalue == e->h) && (h->eqfn(k, e->k)))
    {
      itr->index = index;
      itr->e = e;
      itr->parent = parent;
      itr->h = h;
      return -1;
    }
    parent = e;
    e = e->next;
  }
  return 0;
}


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


