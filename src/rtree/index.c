/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2019 The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */
/****************************************************************************
 * MODULE:       R-Tree library
 *
 * AUTHOR(S):    Antonin Guttman - original code
 *               Melinda Green (melinda@superliminal.com) - major clean-up
 *                               and implementation of bounding spheres
 *
 * PURPOSE:      Multidimensional index
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "assert.h"
#include "index.h"
#include "card.h"


// Make a new index, empty.  Consists of a single node.
//
struct Node * Xastir_RTreeNewIndex(void)
{
  struct Node *x;
  x = Xastir_RTreeNewNode();
  x->level = 0; /* leaf */
  return x;
}



// Search in an index tree or subtree for all data retangles that
// overlap the argument rectangle.
// Return the number of qualifying data rects.
//
int Xastir_RTreeSearch(struct Node *N, struct Rect *R, SearchHitCallback shcb, void* cbarg)
{
  struct Node *n = N;
  struct Rect *r = R; // NOTE: Suspected bug was R sent in as Node* and cast to Rect* here. Fix not yet tested.
  int hitCount = 0;
  int i;

  assert(n);
  assert(n->level >= 0);
  assert(r);

  if (n->level > 0) /* this is an internal node in the tree */
  {
    for (i=0; i<Xastir_NODECARD; i++)
      if (n->branch[i].child &&
          Xastir_RTreeOverlap(r,&n->branch[i].rect))
      {
        hitCount += Xastir_RTreeSearch(n->branch[i].child, R, shcb, cbarg);
      }
  }
  else /* this is a leaf node */
  {
    for (i=0; i<Xastir_LEAFCARD; i++)
      if (n->branch[i].child &&
          Xastir_RTreeOverlap(r,&n->branch[i].rect))
      {
        hitCount++;
        if(shcb) // call the user-provided callback
          if( ! shcb(n->branch[i].child, cbarg))
          {
            return hitCount;  // callback wants to terminate search early
          }
      }
  }
  return hitCount;
}



// Inserts a new data rectangle into the index structure.
// Recursively descends tree, propagates splits back up.
// Returns 0 if node was not split.  Old node updated.
// If node was split, returns 1 and sets the pointer pointed to by
// new_node to point to the new node.  Old node updated to become one of two.
// The level argument specifies the number of steps up from the leaf
// level to insert; e.g. a data rectangle goes in at level = 0.
//
static int Xastir_RTreeInsertRect2(struct Rect *r,
                                   void *tid, struct Node *n, struct Node **new_node, int level)
{
  /*
    register struct Rect *r = R;
    register int tid = Tid;
    register struct Node *n = N, **new_node = New_node;
    register int level = Level;
  */

  int i;
  struct Branch b;
  struct Node *n2;

  assert(r && n && new_node);
  assert(level >= 0 && level <= n->level);

  // Still above level for insertion, go down tree recursively
  //
  if (n->level > level)
  {
    i = Xastir_RTreePickBranch(r, n);
    if (!Xastir_RTreeInsertRect2(r, tid, n->branch[i].child, &n2, level))
    {
      // child was not split
      //
      n->branch[i].rect =
        Xastir_RTreeCombineRect(r,&(n->branch[i].rect));
      return 0;
    }
    else    // child was split
    {
      n->branch[i].rect = Xastir_RTreeNodeCover(n->branch[i].child);
      b.child = n2;
      b.rect = Xastir_RTreeNodeCover(n2);
      return Xastir_RTreeAddBranch(&b, n, new_node);
    }
  }

  // Have reached level for insertion. Add rect, split if necessary
  //
  else if (n->level == level)
  {
    b.rect = *r;
    b.child = (struct Node *) tid;
    /* child field of leaves contains tid of data record */
    return Xastir_RTreeAddBranch(&b, n, new_node);
  }
  else
  {
    /* Not supposed to happen */
    assert (FALSE);
    return 0;
  }
}



// Insert a data rectangle into an index structure.
// Xastir_RTreeInsertRect provides for splitting the root;
// returns 1 if root was split, 0 if it was not.
// The level argument specifies the number of steps up from the leaf
// level to insert; e.g. a data rectangle goes in at level = 0.
// Xastir_RTreeInsertRect2 does the recursion.
//
int Xastir_RTreeInsertRect(struct Rect *R, void *Tid, struct Node **Root, int Level)
{
  struct Rect *r = R;
  void *tid = Tid;
  struct Node **root = Root;
  int level = Level;
  int i;
  struct Node *newroot;
  struct Node *newnode;
  struct Branch b;
  int result;

  assert(r && root);
  assert(level >= 0 && level <= (*root)->level);
  for (i=0; i<NUMDIMS; i++)
  {
    assert(r->boundary[i] <= r->boundary[NUMDIMS+i]);
  }

  if (Xastir_RTreeInsertRect2(r, tid, *root, &newnode, level))  /* root split */
  {
    newroot = Xastir_RTreeNewNode();  /* grow a new root, & tree taller */
    newroot->level = (*root)->level + 1;
    b.rect = Xastir_RTreeNodeCover(*root);
    b.child = *root;
    Xastir_RTreeAddBranch(&b, newroot, NULL);
    b.rect = Xastir_RTreeNodeCover(newnode);
    b.child = newnode;
    Xastir_RTreeAddBranch(&b, newroot, NULL);
    *root = newroot;
    result = 1;
  }
  else
  {
    result = 0;
  }

  return result;
}




// Allocate space for a node in the list used in DeletRect to
// store Nodes that are too empty.
//
static struct ListNode * Xastir_RTreeNewListNode(void)
{
  return (struct ListNode *) malloc(sizeof(struct ListNode));
  //return new ListNode;
}


static void Xastir_RTreeFreeListNode(struct ListNode *p)
{
  free(p);
  //delete(p);
}



// Add a node to the reinsertion list.  All its branches will later
// be reinserted into the index structure.
//
static void Xastir_RTreeReInsert(struct Node *n, struct ListNode **ee)
{
  struct ListNode *l;

  l = Xastir_RTreeNewListNode();
  l->node = n;
  l->next = *ee;
  *ee = l;
}


// Delete a rectangle from non-root part of an index structure.
// Called by Xastir_RTreeDeleteRect.  Descends tree recursively,
// merges branches on the way back up.
// Returns 1 if record not found, 0 if success.
//
static int
Xastir_RTreeDeleteRect2(struct Rect *R, void *Tid, struct Node *N, struct ListNode **Ee)
{
  struct Rect *r = R;
  void *tid = Tid;
  struct Node *n = N;
  struct ListNode **ee = Ee;
  int i;

  assert(r && n && ee);
  assert(tid != NULL);
  assert(n->level >= 0);

  if (n->level > 0)  // not a leaf node
  {
    for (i = 0; i < Xastir_NODECARD; i++)
    {
      if (n->branch[i].child && Xastir_RTreeOverlap(r, &(n->branch[i].rect)))
      {
        if (!Xastir_RTreeDeleteRect2(r, tid, n->branch[i].child, ee))
        {
          if (n->branch[i].child->count >= MinNodeFill)
            n->branch[i].rect = Xastir_RTreeNodeCover(
                                  n->branch[i].child);
          else
          {
            // not enough entries in child,
            // eliminate child node
            //
            Xastir_RTreeReInsert(n->branch[i].child, ee);
            Xastir_RTreeDisconnectBranch(n, i);
          }
          return 0;
        }
      }
    }
    return 1;
  }
  else  // a leaf node
  {
    for (i = 0; i < Xastir_LEAFCARD; i++)
    {
      if (n->branch[i].child &&
          n->branch[i].child == (struct Node *) tid)
      {
        Xastir_RTreeDisconnectBranch(n, i);
        return 0;
      }
    }
    return 1;
  }
}



// Delete a data rectangle from an index structure.
// Pass in a pointer to a Rect, the tid of the record, ptr to ptr to root node.
// Returns 1 if record not found, 0 if success.
// Xastir_RTreeDeleteRect provides for eliminating the root.
//
int Xastir_RTreeDeleteRect(struct Rect *R, void *Tid, struct Node**Nn)
{
  struct Rect *r = R;
  void *tid = Tid;
  struct Node **nn = Nn;
  int i;
  struct Node *tmp_nptr=NULL; // Original superliminal.com
  // source did not initialize.
  // Code analysis says shouldn't
  // matter, but let's initialize
  // to shut up GCC
  struct ListNode *reInsertList = NULL;
  struct ListNode *e;

  assert(r && nn);
  assert(*nn);
  assert(tid != NULL);

  if (!Xastir_RTreeDeleteRect2(r, tid, *nn, &reInsertList))
  {
    /* found and deleted a data item */

    /* reinsert any branches from eliminated nodes */
    while (reInsertList)
    {
      tmp_nptr = reInsertList->node;
      for (i = 0; i < MAXKIDS(tmp_nptr); i++)
      {
        if (tmp_nptr->branch[i].child)
        {
          Xastir_RTreeInsertRect(
            &(tmp_nptr->branch[i].rect),
            tmp_nptr->branch[i].child,
            nn,
            tmp_nptr->level);
        }
      }
      e = reInsertList;
      reInsertList = reInsertList->next;
      Xastir_RTreeFreeNode(e->node);
      Xastir_RTreeFreeListNode(e);
    }

    /* check for redundant root (not leaf, 1 child) and eliminate
     */
    if ((*nn)->count == 1 && (*nn)->level > 0)
    {
      for (i = 0; i < Xastir_NODECARD; i++)
      {
        tmp_nptr = (*nn)->branch[i].child;
        if(tmp_nptr)
        {
          break;
        }
      }
      assert(tmp_nptr);
      Xastir_RTreeFreeNode(*nn);
      *nn = tmp_nptr;
    }
    return 0;
  }
  else
  {
    return 1;
  }
}
