/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2004  The Xastir Group
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
 *               Daniel Green (green@superliminal.com) - major clean-up
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
//static int nnodes_alloced=0;
//static long int bytes_malloced=0;

// Initialize one branch cell in a node.
//
static void RTreeInitBranch(struct Branch *b)
{
	RTreeInitRect(&(b->rect));
	b->child = NULL;
}



// Initialize a Node structure.
//
void RTreeInitNode(struct Node *N)
{
	register struct Node *n = N;
	register int i;
	n->count = 0;
	n->level = -1;
	for (i = 0; i < MAXCARD; i++)
		RTreeInitBranch(&(n->branch[i]));
}



// Make a new node and initialize to have all branch cells empty.
//
struct Node * RTreeNewNode()
{
	register struct Node *n;

	//n = new Node;
	n = (struct Node*)malloc(sizeof(struct Node));
	assert(n);
        //        nnodes_alloced++;
        //        bytes_malloced+= sizeof(struct Node);
        //        fprintf(stderr,"   Currently %d nodes (%ld bytes) in all rtrees\n",nnodes_alloced, bytes_malloced);
	RTreeInitNode(n);
	return n;
}


void RTreeFreeNode(struct Node *p)
{
	assert(p);
	//delete p;
        //        nnodes_alloced--;
        //        bytes_malloced-= sizeof(struct Node);
	free(p);
}



static void RTreePrintBranch(struct Branch *b, int depth)
{
	RTreePrintRect(&(b->rect), depth);
	RTreePrintNode(b->child, depth);
}


extern void RTreeTabIn(int depth)
{
	int i;
	for(i=0; i<depth; i++)
		putchar('\t');
}


// Print out the data in a node.
//
void RTreePrintNode(struct Node *n, int depth)
{
	int i;
	assert(n);

	RTreeTabIn(depth);
	printf("node");
	if (n->level == 0)
		printf(" LEAF");
	else if (n->level > 0)
		printf(" NONLEAF");
	else
		printf(" TYPE=?");
        // Original superliminal.com implementation had no cast before 
        // n, gcc gripes about "int format, pointer arg"
        printf("  level=%d  count=%d  address=%lx\n", n->level, n->count, (unsigned long) n);

	for (i=0; i<n->count; i++)
	{
		if(n->level == 0) {
                    //			RTreeTabIn(depth);
                    //			printf("\t%d: data = %d\n", i, n->branch[i].child);
		}
		else {
			RTreeTabIn(depth);
			printf("branch %d\n", i);
			RTreePrintBranch(&n->branch[i], depth+1);
		}
	}
}



// Find the smallest rectangle that includes all rectangles in
// branches of a node.
//
struct Rect RTreeNodeCover(struct Node *N)
{
	register struct Node *n = N;
	register int i, first_time=1;
	struct Rect r;
	assert(n);

	RTreeInitRect(&r);
	for (i = 0; i < MAXKIDS(n); i++)
		if (n->branch[i].child)
		{
			if (first_time)
			{
				r = n->branch[i].rect;
				first_time = 0;
			}
			else
				r = RTreeCombineRect(&r, &(n->branch[i].rect));
		}
	return r;
}



// Pick a branch.  Pick the one that will need the smallest increase
// in area to accomodate the new rectangle.  This will result in the
// least total area for the covering rectangles in the current node.
// In case of a tie, pick the one which was smaller before, to get
// the best resolution when searching.
//
int RTreePickBranch(struct Rect *R, struct Node *N)
{
	register struct Rect *r = R;
	register struct Node *n = N;
	register struct Rect *rr;
	register int i, first_time=1;
        // Although it is impossible for bestArea and best to be used
        // unininitialized the way the code is structured, gcc complains 
        // about possible uninitialized usage.  Let's keep it happy.
        // Original superliminal.com had no initializers here.
	RectReal increase, bestIncr=(RectReal)-1, area, bestArea=0.0;
	int best=0;
	struct Rect tmp_rect;
	assert(r && n);

	for (i=0; i<MAXKIDS(n); i++)
	{
		if (n->branch[i].child)
		{
			rr = &n->branch[i].rect;
			area = RTreeRectSphericalVolume(rr);
			tmp_rect = RTreeCombineRect(r, rr);
			increase = RTreeRectSphericalVolume(&tmp_rect) - area;
			if (increase < bestIncr || first_time)
			{
				best = i;
				bestArea = area;
				bestIncr = increase;
				first_time = 0;
			}
			else if (increase == bestIncr && area < bestArea)
			{
				best = i;
				bestArea = area;
				bestIncr = increase;
			}
		}
	}
	return best;
}



// Add a branch to a node.  Split the node if necessary.
// Returns 0 if node not split.  Old node updated.
// Returns 1 if node split, sets *new_node to address of new node.
// Old node updated, becomes one of two.
//
int RTreeAddBranch(struct Branch *B, struct Node *N, struct Node **New_node)
{
	register struct Branch *b = B;
	register struct Node *n = N;
	register struct Node **new_node = New_node;
	register int i;

	assert(b);
	assert(n);

	if (n->count < MAXKIDS(n))  /* split won't be necessary */
	{
		for (i = 0; i < MAXKIDS(n); i++)  /* find empty branch */
		{
			if (n->branch[i].child == NULL)
			{
				n->branch[i] = *b;
				n->count++;
				break;
			}
		}
		return 0;
	}
	else
	{
		assert(new_node);
		RTreeSplitNode(n, b, new_node);
		return 1;
	}
}



// Disconnect a dependent node.
//
void RTreeDisconnectBranch(struct Node *n, int i)
{
	assert(n && i>=0 && i<MAXKIDS(n));
	assert(n->branch[i].child);

	RTreeInitBranch(&(n->branch[i]));
	n->count--;
}

// Destroy (free) node recursively. 
void RTreeDestroyNode (struct Node *n)
{
    int i;
    fprintf(stderr,"  Freeing node %lx\n",(unsigned long int) n);
    if (n->level > 0) {  //it is not leaf -> destroy childs 
        for ( i = 0; i < NODECARD; i++) {
            if ( n->branch[i].child ) {
                RTreeDestroyNode ( n->branch[i].child );
            }
        }
    }
    
    //
    RTreeFreeNode( n );
}
