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
#include "assert.h"
#include "index.h"
#include "card.h"
#include "split_l.h"


/*-----------------------------------------------------------------------------
  | Load branch buffer with branches from full node plus the extra branch.
  -----------------------------------------------------------------------------*/
static void Xastir_RTreeGetBranches(struct Node *N, struct Branch *B)
{
  register struct Node *n = N;
  register struct Branch *b = B;
  register int i;

  assert(n);
  assert(b);

  /* load the branch buffer */
  for (i=0; i<MAXKIDS(n); i++)
  {
    assert(n->branch[i].child);  /* every entry should be full */
    Xastir_BranchBuf[i] = n->branch[i];
  }
  Xastir_BranchBuf[MAXKIDS(n)] = *b;
  Xastir_BranchCount = MAXKIDS(n) + 1;

  /* calculate rect containing all in the set */
  Xastir_CoverSplit = Xastir_BranchBuf[0].rect;
  for (i=1; i<MAXKIDS(n)+1; i++)
  {
    Xastir_CoverSplit = Xastir_RTreeCombineRect(&Xastir_CoverSplit, &Xastir_BranchBuf[i].rect);
  }

  Xastir_RTreeInitNode(n);
}



/*-----------------------------------------------------------------------------
  | Initialize a PartitionVars structure.
  -----------------------------------------------------------------------------*/
static void Xastir_RTreeInitPVars(struct PartitionVars *P, int maxrects, int minfill)
{
  register struct PartitionVars *p = P;
  register int i;
  assert(p);

  p->count[0] = p->count[1] = 0;
  p->total = maxrects;
  p->minfill = minfill;
  for (i=0; i<maxrects; i++)
  {
    p->taken[i] = FALSE;
    p->partition[i] = -1;
  }
}



/*-----------------------------------------------------------------------------
  | Put a branch in one of the groups.
  -----------------------------------------------------------------------------*/
static void Xastir_RTreeClassify(int i, int group, struct PartitionVars *p)
{
  assert(p);
  assert(!p->taken[i]);

  p->partition[i] = group;
  p->taken[i] = TRUE;

  if (p->count[group] == 0)
  {
    p->cover[group] = Xastir_BranchBuf[i].rect;
  }
  else
    p->cover[group] = Xastir_RTreeCombineRect(&Xastir_BranchBuf[i].rect,
                      &p->cover[group]);
  p->area[group] = Xastir_RTreeRectSphericalVolume(&p->cover[group]);
  p->count[group]++;
}



/*-----------------------------------------------------------------------------
  | Pick two rects from set to be the first elements of the two groups.
  | Pick the two that are separated most along any dimension, or overlap least.
  | Distance for separation or overlap is measured modulo the width of the
  | space covered by the entire set along that dimension.
  -----------------------------------------------------------------------------*/
static void Xastir_RTreePickSeeds(struct PartitionVars *P)
{
  register struct PartitionVars *p = P;
  register int i, dim, high;
  register struct Rect *r, *rlow, *rhigh;
  // Original superliminal.com implementation had no initializers here.
  // They are not strictly necessary, as the variables are initialized
  // in the first iteration of the first for loop, but GCC complains
  // anyway.  Initializers added to keep it happy.
  register float w, separation, bestSep=0.0;
  RectReal width[NUMDIMS];
  int leastUpper[NUMDIMS], greatestLower[NUMDIMS];
  int seed0=0, seed1=0;
  assert(p);

  for (dim=0; dim<NUMDIMS; dim++)
  {
    high = dim + NUMDIMS;

    /* find the rectangles farthest out in each direction
     * along this dimens */
    greatestLower[dim] = leastUpper[dim] = 0;
    for (i=1; i<Xastir_NODECARD+1; i++)
    {
      r = &Xastir_BranchBuf[i].rect;
      if (r->boundary[dim] >
          Xastir_BranchBuf[greatestLower[dim]].rect.boundary[dim])
      {
        greatestLower[dim] = i;
      }
      if (r->boundary[high] <
          Xastir_BranchBuf[leastUpper[dim]].rect.boundary[high])
      {
        leastUpper[dim] = i;
      }
    }

    /* find width of the whole collection along this dimension */
    width[dim] = Xastir_CoverSplit.boundary[high] -
                 Xastir_CoverSplit.boundary[dim];
  }

  /* pick the best separation dimension and the two seed rects */
  for (dim=0; dim<NUMDIMS; dim++)
  {
    high = dim + NUMDIMS;

    /* divisor for normalizing by width */
    assert(width[dim] >= 0);
    if (width[dim] == 0)
    {
      w = (RectReal)1;
    }
    else
    {
      w = width[dim];
    }

    rlow = &Xastir_BranchBuf[leastUpper[dim]].rect;
    rhigh = &Xastir_BranchBuf[greatestLower[dim]].rect;
    if (dim == 0)
    {
      seed0 = leastUpper[0];
      seed1 = greatestLower[0];
      separation = bestSep =
                     (rhigh->boundary[0] -
                      rlow->boundary[NUMDIMS]) / w;
    }
    else
    {
      separation =
        (rhigh->boundary[dim] -
         rlow->boundary[dim+NUMDIMS]) / w;
      if (separation > bestSep)
      {
        seed0 = leastUpper[dim];
        seed1 = greatestLower[dim];
        bestSep = separation;
      }
    }
  }

  if (seed0 != seed1)
  {
    Xastir_RTreeClassify(seed0, 0, p);
    Xastir_RTreeClassify(seed1, 1, p);
  }
}



/*-----------------------------------------------------------------------------
  | Put each rect that is not already in a group into a group.
  | Process one rect at a time, using the following hierarchy of criteria.
  | In case of a tie, go to the next test.
  | 1) If one group already has the max number of elements that will allow
  | the minimum fill for the other group, put r in the other.
  | 2) Put r in the group whose cover will expand less.  This automatically
  | takes care of the case where one group cover contains r.
  | 3) Put r in the group whose cover will be smaller.  This takes care of the
  | case where r is contained in both covers.
  | 4) Put r in the group with fewer elements.
  | 5) Put in group 1 (arbitrary).
  |
  | Also update the covers for both groups.
  -----------------------------------------------------------------------------*/
static void Xastir_RTreePigeonhole(struct PartitionVars *P)
{
  register struct PartitionVars *p = P;
  struct Rect newCover[2];
  register int i, group;
  RectReal newArea[2], increase[2];

  for (i=0; i<Xastir_NODECARD+1; i++)
  {
    if (!p->taken[i])
    {
      /* if one group too full, put rect in the other */
      if (p->count[0] >= p->total - p->minfill)
      {
        Xastir_RTreeClassify(i, 1, p);
        continue;
      }
      else if (p->count[1] >= p->total - p->minfill)
      {
        Xastir_RTreeClassify(i, 0, p);
        continue;
      }

      /* find areas of the two groups' old and new covers */
      for (group=0; group<2; group++)
      {
        if (p->count[group]>0)
          newCover[group] = Xastir_RTreeCombineRect(
                              &Xastir_BranchBuf[i].rect,
                              &p->cover[group]);
        else
        {
          newCover[group] = Xastir_BranchBuf[i].rect;
        }
        newArea[group] = Xastir_RTreeRectSphericalVolume(
                           &newCover[group]);
        increase[group] = newArea[group]-p->area[group];
      }

      /* put rect in group whose cover will expand less */
      if (increase[0] < increase[1])
      {
        Xastir_RTreeClassify(i, 0, p);
      }
      else if (increase[1] < increase[0])
      {
        Xastir_RTreeClassify(i, 1, p);
      }

      /* put rect in group that will have a smaller cover */
      else if (p->area[0] < p->area[1])
      {
        Xastir_RTreeClassify(i, 0, p);
      }
      else if (p->area[1] < p->area[0])
      {
        Xastir_RTreeClassify(i, 1, p);
      }

      /* put rect in group with fewer elements */
      else if (p->count[0] < p->count[1])
      {
        Xastir_RTreeClassify(i, 0, p);
      }
      else
      {
        Xastir_RTreeClassify(i, 1, p);
      }
    }
  }
  assert(p->count[0] + p->count[1] == Xastir_NODECARD + 1);
}



/*-----------------------------------------------------------------------------
  | Method 0 for finding a partition:
  | First find two seeds, one for each group, well separated.
  | Then put other rects in whichever group will be smallest after addition.
  -----------------------------------------------------------------------------*/
static void Xastir_RTreeMethodZero(struct PartitionVars *p, int minfill)
{
  Xastir_RTreeInitPVars(p, Xastir_BranchCount, minfill);
  Xastir_RTreePickSeeds(p);
  Xastir_RTreePigeonhole(p);
}




/*-----------------------------------------------------------------------------
  | Copy branches from the buffer into two nodes according to the partition.
  -----------------------------------------------------------------------------*/
static void Xastir_RTreeLoadNodes(struct Node *N, struct Node *Q,
                                  struct PartitionVars *P)
{
  register struct Node *n = N, *q = Q;
  register struct PartitionVars *p = P;
  register int i;
  assert(n);
  assert(q);
  assert(p);

  for (i=0; i<Xastir_NODECARD+1; i++)
  {
    if (p->partition[i] == 0)
    {
      Xastir_RTreeAddBranch(&Xastir_BranchBuf[i], n, NULL);
    }
    else if (p->partition[i] == 1)
    {
      Xastir_RTreeAddBranch(&Xastir_BranchBuf[i], q, NULL);
    }
    else
    {
      assert(FALSE);
    }
  }
}



/*-----------------------------------------------------------------------------
  | Split a node.
  | Divides the nodes branches and the extra one between two nodes.
  | Old node is one of the new ones, and one really new one is created.
  -----------------------------------------------------------------------------*/
void Xastir_RTreeSplitNode(struct Node *n, struct Branch *b, struct Node **nn)
{
  register struct PartitionVars *p;
  register int level;
  // This variable is declared, assigned a value, then never used.
  // Newer GCCs warn about that.  Shut them up.
  // RectReal area;

  assert(n);
  assert(b);

  /* load all the branches into a buffer, initialize old node */
  level = n->level;
  Xastir_RTreeGetBranches(n, b);

  /* find partition */
  p = &Xastir_Partitions[0];

  /* Note: can't use MINFILL(n) below since n was cleared by GetBranches() */
  Xastir_RTreeMethodZero(p, level>0 ? MinNodeFill : MinLeafFill);

  /* record how good the split was for statistics */
  // This variable is declared, assigned a value, then never used.
  // Newer GCCs warn about that.  Shut them up.
  // area = p->area[0] + p->area[1];

  /* put branches from buffer in 2 nodes according to chosen partition */
  *nn = Xastir_RTreeNewNode();
  (*nn)->level = n->level = level;
  Xastir_RTreeLoadNodes(n, *nn, p);
  assert(n->count + (*nn)->count == Xastir_NODECARD+1);
}



/*-----------------------------------------------------------------------------
  | Print out data for a partition from PartitionVars struct.
  -----------------------------------------------------------------------------*/

// This is not used at the moment, and because it's declared static gcc
// warns us it's not used.  Commented out to shut gcc up
#if 0
static void Xastir_RTreePrintPVars(struct PartitionVars *p)
{
  int i;
  assert(p);

  printf("\npartition:\n");
  for (i=0; i<Xastir_NODECARD+1; i++)
  {
    printf("%3d\t", i);
  }
  printf("\n");
  for (i=0; i<Xastir_NODECARD+1; i++)
  {
    if (p->taken[i])
    {
      printf("  t\t");
    }
    else
    {
      printf("\t");
    }
  }
  printf("\n");
  for (i=0; i<Xastir_NODECARD+1; i++)
  {
    printf("%3d\t", p->partition[i]);
  }
  printf("\n");

  printf("count[0] = %d  area = %f\n", p->count[0], p->area[0]);
  printf("count[1] = %d  area = %f\n", p->count[1], p->area[1]);
  printf("total area = %f  effectiveness = %3.2f\n",
         p->area[0] + p->area[1],
         Xastir_RTreeRectSphericalVolume(&Xastir_CoverSplit)/(p->area[0]+p->area[1]));

  printf("cover[0]:\n");
  Xastir_RTreePrintRect(&p->cover[0], 0);

  printf("cover[1]:\n");
  Xastir_RTreePrintRect(&p->cover[1], 0);
}
#endif // shut up GCC
