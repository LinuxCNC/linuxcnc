/** This file, 'avltree.c', implements AVL balanced binary trees.
    Binary trees allow fast searching, insertion, and deletion of
    data by maintaining the data in a sorted form and doing binary
    searches.  Ordinary binary trees can get out of balance, with
    some branches much deeper than others.  If data is added in
    sorted or partially sorted order, the tree can get completely
    out of balance and turn into a slow linear search.  AVL trees
    avoid this by dynamically re-balancing the tree if adding or
    deleting data causes it to become distorted.

   See avltree.h for full documentation.
*/

/**
* Author:    John Kasunich
*            <jmkasunich AT users DOT sourceforge DOT net>
* License:   GPL Version 2
* System:    Linux
*
* Copyright (c) 1994 All rights reserved.
* Revised and released under GPL 2005
*
* Last change:
* $Revision$
* $Author$
* $Date$
*
*/

#include <stddef.h>		/* defines NULL */
//#include <stdlib.h>
#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/slab.h>		/* replaces malloc.h in recent kernels */
/* redefine malloc calls to use the kernel equivalents */
#define malloc(size) (kmalloc((size),GFP_ATOMIC))
#define free(object) kfree(object)
#else
#include <malloc.h>		/* malloc(), free() */
#endif
#include "avltree.h"

#define MAX(a,b) ( (a) > (b) ? (a) : (b) )

/*****************************/
/* Internal Data Structures  */
/*****************************/

/* depth of an AVL tree is a maximum of 1.44*log2(n) */
/* so a depth of 47 can handle 2^32 nodes */
#define TREE_DEPTH 47

/* struct used to store one tree entry */

typedef struct avltree_node {
    struct avltree_node *lptr;	/* left hand sub-tree */
    struct avltree_node *rptr;	/* right hand sub-tree */
    void *data;			/* pointer to node data */
    char lsiz;			/* depth of left sub-tree */
    char rsiz;			/* depth of right sub-tree */
} avltree_node;

/* struct used to record a path thru the tree */

typedef struct avltree_stack_node {
    struct avltree_node  *node;	/* pointer to corresponding node */
    enum { TREE_NONE,
           TREE_LEFT,
           TREE_RIGHT
         } source;		/* how this level was reached */
} avltree_stack_node;

/*********************************/
/* Internal Function Prototypes  */
/*********************************/

static void balance_node ( avltree_node **rootptr );

/*************************/
/* Public Function Code  */
/*************************/

avltree *avltree_create(int(*cmp_func)(void *data1, void *data2))
{
    avltree *tptr;

    if ( cmp_func == NULL ) {
	return ( NULL );
    }
    tptr = (avltree *)( malloc ( sizeof ( avltree ) ) );
    if ( tptr == NULL ) {
	return ( NULL );
    }
    tptr->root = NULL;
    tptr->cmp_func = cmp_func;
    return ( tptr );
}

void avltree_destroy(avltree *tree)
{
    avltree_node *nptr;
    avltree_stack_node stack[TREE_DEPTH+1];
    avltree_stack_node *sp;

    /* check argument */
    if ( tree == NULL ) {
	return;
    }
    /* initialize stack and point to root of tree */
    sp = stack;
    nptr = sp->node = tree->root;
    sp->source = TREE_NONE;
    /* loop till entire tree has been covered */
    while ( nptr != NULL ) {
	if ( nptr->lptr != NULL ) {
	    /* move down into left sub-tree */
	    sp++;
	    nptr = sp->node = nptr->lptr;
	    sp->source = TREE_LEFT;
	} else if ( nptr->rptr != NULL ) {
	    /* move down into right sub-tree */
	    sp++;
	    nptr = sp->node = nptr->rptr;
	    sp->source = TREE_RIGHT;
	} else {
	    /* free the node */
	    free ( nptr );
	    /* move up one level */
	    if ( sp->source == TREE_LEFT ) {
		sp--;
		nptr = sp->node;
		nptr->lptr = NULL;
	    } else if ( sp->source == TREE_RIGHT ) {
		sp--;
		nptr = sp->node;
		nptr->rptr = NULL;
	    } else {
		nptr = NULL;
	    }
	}
    }
    tree->root = NULL;
    free ( tree );
}

void *avltree_insert(avltree *tree, void *data)
{
    avltree_node *nptr;
    avltree_node **stack[TREE_DEPTH+1];
    avltree_node ***sp;
    int cmp;

    /* check arguments */
    if ( ( tree == NULL ) || ( data == NULL ) ) {
	return ( NULL );
    }
    /* point at root of tree, initialize stack */
    sp = stack;
    *(sp++) = &(tree->root);
    nptr = tree->root;
    /* traverse tree looking for existing data */
    while ( 1 ) {
	if ( nptr == NULL ) {
	    /* reached tip of branch, no match */
	    break;
	}
	cmp = tree->cmp_func ( data, nptr->data );
	if ( cmp > 0 ) {
	    /* follow right hand branch */
	    *(sp++) = &(nptr->rptr);
	    nptr = nptr->rptr;
	} else if ( cmp < 0 ) {
	    /* follow left hand branch */
	    *(sp++) = &(nptr->lptr);
	    nptr = nptr->lptr;
	} else {
	    /* data is already in the tree, fail */
	    return ( nptr->data );
	}
    }
    /* get new node struct */
    nptr = (avltree_node *) malloc(sizeof(avltree_node));
    if ( nptr == NULL ) {
	/* failed to allocate a node structure */
	return ( NULL );
    }
    /* initialize fields of the new structure */
    nptr->lptr = NULL;
    nptr->rptr = NULL;
    nptr->lsiz = 0;
    nptr->rsiz = 0;
    nptr->data = data;
    /* append new node to tip of branch */
    **(--sp) = nptr;
    /* traverse back up to root of tree */
    while ( sp != stack ) {
	nptr = **(--sp);
	/* compute left sub-tree size */
	if ( nptr->lptr == NULL ) {
	    nptr->lsiz = 0;
	} else {
	    nptr->lsiz = MAX( nptr->lptr->lsiz, nptr->lptr->rsiz ) + 1;
	}
	/* compute right sub-tree size */
	if ( nptr->rptr == NULL ) {
	    nptr->rsiz = 0;
	} else {
	    nptr->rsiz = MAX( nptr->rptr->lsiz, nptr->rptr->rsiz ) + 1;
	}
	/* balance sub-tree if needed */
	/* FIXME - a test for ( abs(unbalance) > 1 ) here could reduce call overhead */
	balance_node ( *sp );
    }
    return ( data );
}

void *avltree_delete(avltree *tree, void *data)
{
    int      cmp;
    avltree_node *nptr;
    avltree_node *bptr;
    avltree_node *optr;
    avltree_node **stack[TREE_DEPTH+1];
    avltree_node ***sp;

    /* check arguments */
    if ( ( tree == NULL ) || ( data == NULL ) ) {
	return ( NULL );
    }
    /* point at root of tree, initialize stack */
    sp = stack;
    *(sp++) = &(tree->root);
    nptr = tree->root;
    /* traverse tree looking for data */
    while ( 1 ) {
	if ( nptr == NULL ) {
	    /* reached end of branch with no match */
	    return ( NULL );
	}
	cmp = tree->cmp_func ( data, nptr->data );
	if ( cmp > 0 ) {
	    /* follow right hand branch */
	    *(sp++) = &(nptr->rptr);
	    nptr = nptr->rptr;
	} else if ( cmp < 0 ) {
	    /* follow left hand branch */
	    *(sp++) = &(nptr->lptr);
	    nptr = nptr->lptr;
	} else {
	    /* found a match, break out of loop */
	    break;
	}
    }
    /* save pointer to matched node */
    optr = nptr;
    /* unlink node from the tree */
    if ( nptr->lsiz < nptr->rsiz ) {
	/* right sub-tree is deeper, make it the new root */
	**(sp-1) = nptr->rptr;
	if ( nptr->lptr != NULL ) {
	    /* point to previous left sub-tree */
	    bptr = nptr->lptr;
	    /* and right subtree */
	    nptr = nptr->rptr;
	    /* traverse left side of right subtree */
	    while ( nptr->lptr != NULL ) {
		*(sp++) = &(nptr->lptr);
		nptr = nptr->lptr;
	    }
	    /* attach left sub-tree to left side of right sub-tree */
	    nptr->lptr = bptr;
	}
    }  else {
	/* left sub-tree is deeper, make it the new root */
	**(sp-1) = nptr->lptr;
	if ( nptr->rptr != NULL ) {
	    /* point to previous right sub-tree */
	    bptr = nptr->rptr;
	    /* and left sub-tree */
	    nptr = nptr->lptr;
	    /* traverse right side of left sub-tree */
	    while ( nptr->rptr != NULL ) {
		*(sp++) = &(nptr->rptr);
		nptr = nptr->rptr;
	    }
	    /* attach right sub-tree to right side of left sub-tree */
	    nptr->rptr = bptr;
	}
    }
    /* free deleted tree node */
    free ( optr );
    /* FIXME - don't know what this does, figure it out and comment it */
    if ( **(sp-1) == NULL ) {
	sp--;
    }
    /* traverse back up to root of tree */
    while ( sp != stack ) {
	nptr = **(--sp);
	/* compute left sub-tree size */
	if ( nptr->lptr == NULL ) {
	    nptr->lsiz = 0;
	} else {
	    nptr->lsiz = MAX( nptr->lptr->lsiz, nptr->lptr->rsiz ) + 1;
	}
	/* compute right sub-tree size */
	if ( nptr->rptr == NULL ) {
	    nptr->rsiz = 0;
	} else {
	    nptr->rsiz = MAX( nptr->rptr->lsiz, nptr->rptr->rsiz ) + 1;
	}
	/* balance sub-tree if needed */
	/* FIXME - a test for ( abs(unbalance) > 1 ) here could reduce call overhead */
	balance_node ( *sp );
    }
    return ( optr->data );
}

void *avltree_find(avltree *tree, void *data)
{
    avltree_node  *nptr;
    int cmp;

    /* check arguments */
    if ( ( tree == NULL ) || ( data == NULL ) ) {
	return ( NULL );
    }
    /* point to root of tree */
    nptr = tree->root;
    /* loop thru the tree */
    while ( nptr != NULL ) {
	/* test node against supplied data */
	cmp = tree->cmp_func(data, nptr->data);
	if ( cmp > 0 ) {
	    /* too low, try right sub-tree */
	    nptr = nptr->rptr;
	} else if ( cmp < 0 ) {
	    /* too high, try left sub-tree */
	    nptr = nptr->lptr;
	} else {
	    /* found a match */
	    return ( nptr->data );
	}
    }
    /* reached end of branch, no match */
    return ( NULL );
}

void *avltree_first(avltree *tree)
{
    avltree_node *nptr;

    /* check argument */
    if ( tree == NULL ) {
	return ( NULL );
    }
    nptr = tree->root;
    if ( nptr == NULL ) {
	/* list is empty */
	return ( NULL );
    }
    /* find first item */
    while ( nptr->lptr != NULL ) {
	nptr = nptr->lptr;
    }
    return ( nptr );
}

void *avltree_last(avltree *tree)
{
    avltree_node *nptr;

    /* check argument */
    if ( tree == NULL ) {
	return ( NULL );
    }
    nptr = tree->root;
    if ( nptr == NULL ) {
	/* list is empty */
	return ( NULL );
    }
    /* find last item */
    while ( nptr->rptr != NULL ) {
	nptr = nptr->rptr;
    }
    return ( nptr );
}

void *avltree_next(avltree *tree, void *data)
{
    avltree_node  *nptr;
    avltree_stack_node stack[TREE_DEPTH+1];
    avltree_stack_node *sp;
    int cmp;

    /* check arguments */
    if ( ( tree == NULL ) || ( data == NULL ) ) {
	return ( NULL );
    }
    /* initialize stack, point to root of tree */
    sp = stack;
    sp->node = tree->root;
    sp->source = TREE_NONE;
    /* traverse tree, looking for a match and
       storing the search path in stack[] */
    while ( sp->node != NULL ) {
	nptr = sp->node;
	/* test for match */
	cmp = tree->cmp_func ( data, nptr->data );
	if ( cmp > 0 ) {
	    /* too low, try right sub-tree */
	    sp++;
	    sp->node = nptr->rptr;
	    sp->source = TREE_RIGHT;
	}
	else if ( cmp < 0 ) {
	    /* too high, try left sub-tree */
	    sp++;
	    sp->node = nptr->lptr;
	    sp->source = TREE_LEFT;
	} else {
	    /* match */
	    break;
	}
    }
    /* at this point, we've either found a match, or
       are at the end of a branch - next step is to
       find the next item */
    nptr = sp->node;
    if ( ( nptr != NULL ) && ( nptr->rptr != NULL ) ) {
	/* next item is below and to the right */
	/* move one step down and right */
	nptr = nptr->rptr;
	/* move down and left as far as possible */
	while ( nptr->lptr != NULL ) {
	    nptr = nptr->lptr;
	}
	/* done */
	return ( nptr->data );
    } else {
	/* next item is up and to the right */
	/* move up until we can go right */
	while ( sp->source == TREE_RIGHT ) {
	    sp--;
	}
	if ( sp->source == TREE_LEFT ) {
	    /* take one step right */
	    sp--;
	    /* done */
	    return ( sp->node->data );
	} else {
	    /* reached root, there is no 'next item' */
	    return ( NULL );
	}
    }
}

void  *avltree_prev(avltree *tree, void *data)
{
    avltree_node  *nptr;
    avltree_stack_node stack[TREE_DEPTH+1];
    avltree_stack_node *sp;
    int cmp;

    /* check arguments */
    if ( ( tree == NULL ) || ( data == NULL ) ) {
	return ( NULL );
    }
    /* initialize stack, point to root of tree */
    sp = stack;
    sp->node = tree->root;
    sp->source = TREE_NONE;
    /* traverse tree, looking for a match and
       storing the search path in stack[] */
    while ( sp->node != NULL ) {
	nptr = sp->node;
	/* test for match */
	cmp = tree->cmp_func ( data, nptr->data );
	if ( cmp > 0 ) {
	    /* too low, try right sub-tree */
	    sp++;
	    sp->node = nptr->rptr;
	    sp->source = TREE_RIGHT;
	}
	else if ( cmp < 0 ) {
	    /* too high, try left sub-tree */
	    sp++;
	    sp->node = nptr->lptr;
	    sp->source = TREE_LEFT;
	} else {
	    /* match */
	    break;
	}
    }
    /* at this point, we've either found a match, or
       are at the end of a branch - next step is to
       find the previous item */
    nptr = sp->node;
    if ( ( nptr != NULL ) && ( nptr->lptr != NULL ) ) {
	/* previous item is below and to the left */
	/* move one step down and left */
	nptr = nptr->lptr;
	/* move down and right as far as possible */
	while ( nptr->rptr != NULL ) {
	    nptr = nptr->rptr;
	}
	/* done */
	return ( nptr->data );
    } else {
	/* previous item is up and to the left */
	/* move up until we can go left */
	while ( sp->source == TREE_LEFT ) {
	    sp--;
	}
	if ( sp->source == TREE_RIGHT ) {
	    /* take one step left */
	    sp--;
	    /* done */
	    return ( sp->node->data );
	} else {
	    /* reached root, there is no 'next item' */
	    return ( NULL );
	}
    }
}

/***************************/
/* Internal Function Code  */
/***************************/

/* balance_node() balances sub-tree.  Note two levels of indirection on
   the 'rootptr' argument - that is because the root of the subtree may
   be changed as part of the balancing process.  'rootptr' must point to
   the pointer by which the rest of the tree accesses the sub-tree to be
   balanced, and that pointer will be changed if neccessary.
*/

/* Warning - much pointer manipulation here - to understand this code
   it is best to draw some pictures of sample trees, and walk thru it.
   I find it just about impossible to understand without drawing the
   trees.  Comments aren't really that much help - you have to be able
   to see the data structures.
*/

static void balance_node ( avltree_node **rootptr )
{
    int unbal;
    avltree_node *nptr1;
    avltree_node *nptr2;
    avltree_node *nptr3;

    while ( 1 )
	{
	nptr1 = *rootptr;
	unbal = nptr1->lsiz - nptr1->rsiz;
	if ( unbal > 1 ) {
	    /* left heavy */
	    nptr2 = nptr1->lptr;
	    unbal = nptr2->lsiz - nptr2->rsiz;
	    if ( unbal > 0 ) {
		/* outside heavy */
		nptr1->lptr = nptr2->rptr;
		nptr2->rptr = nptr1;
		*rootptr = nptr2;
		if ( nptr1->lptr == NULL ) {
		    nptr1->lsiz = 0;
		} else {
		    nptr1->lsiz = MAX( nptr1->lptr->lsiz, nptr1->lptr->rsiz ) + 1;
		}
		nptr2->rsiz = MAX( nptr2->rptr->lsiz, nptr2->rptr->rsiz ) + 1;
	    } else {
		/* inside heavy */
		nptr3 = nptr2->rptr;
		nptr1->lptr = nptr3->rptr;
		nptr2->rptr = nptr3->lptr;
		nptr3->lptr = nptr2;
		nptr3->rptr = nptr1;
		*rootptr = nptr3;
		if ( nptr1->lptr == NULL ) {
		    nptr1->lsiz = 0;
		} else {
		    nptr1->lsiz = MAX( nptr1->lptr->lsiz, nptr1->lptr->rsiz ) + 1;
		}
		if ( nptr2->rptr == NULL ) {
		    nptr2->rsiz = 0;
		} else {
		    nptr2->rsiz = MAX( nptr2->rptr->lsiz, nptr2->rptr->rsiz ) + 1;
		}
		nptr3->lsiz = MAX( nptr3->lptr->lsiz, nptr3->lptr->rsiz ) + 1;
		nptr3->rsiz = MAX( nptr3->rptr->lsiz, nptr3->rptr->rsiz ) + 1;
	    }
	} else if ( unbal < -1 ) {
	    /* right heavy */
	    nptr2 = nptr1->rptr;
	    unbal = nptr2->rsiz - nptr2->lsiz;
	    if ( unbal > 0 ) {
		/* outside heavy */
		nptr1->rptr = nptr2->lptr;
		nptr2->lptr = nptr1;
		*rootptr = nptr2;
		if ( nptr1->rptr == NULL ) {
		    nptr1->rsiz = 0;
		} else {
		    nptr1->rsiz = MAX( nptr1->rptr->rsiz, nptr1->rptr->lsiz ) + 1;
		}
		nptr2->lsiz = MAX( nptr2->lptr->rsiz, nptr2->lptr->lsiz ) + 1;
	    } else {
		/* inside heavy */
		nptr3 = nptr2->lptr;
		nptr1->rptr = nptr3->lptr;
		nptr2->lptr = nptr3->rptr;
		nptr3->rptr = nptr2;
		nptr3->lptr = nptr1;
		*rootptr = nptr3;
		if ( nptr1->rptr == NULL ) {
		    nptr1->rsiz = 0;
		} else {
		    nptr1->rsiz = MAX( nptr1->rptr->rsiz, nptr1->rptr->lsiz ) + 1;
		}
		if ( nptr2->lptr == NULL ) {
		    nptr2->lsiz = 0;
		} else {
		    nptr2->lsiz = MAX( nptr2->lptr->rsiz, nptr2->lptr->lsiz ) + 1;
		}
		nptr3->rsiz = MAX( nptr3->rptr->rsiz, nptr3->rptr->lsiz ) + 1;
		nptr3->lsiz = MAX( nptr3->lptr->rsiz, nptr3->lptr->lsiz ) + 1;
	    }
	} else {
	    return;
	}
    }
}

/*****************/
/* Testing Code  */
/*****************/

#define BUILD_TEST

#ifndef __KERNEL__
#ifdef BUILD_TEST

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/** The remaining code is not part of the avltree library, instead it
    makes a simple command line program that can be used to test the
    library.

    This code uses standard C strings as data items.  That means that
    the compare function is strcmp(), and that data items can be
    printed out using printf.
*/

void print_node ( avltree_node *node, int indent )
{
    int n;

    /* check argument */
    if ( node == NULL ) {
	return;
    }
    if ( node->lptr != NULL ) {
	/* move down into left sub-tree */
	print_node ( node->lptr, indent + 2 );
    }
    printf ( "%08X: %08X %2d  %08X %2d ", (int) node, (int) node->lptr, node->lsiz, (int) node->rptr, node->rsiz );
    for ( n = 0 ; n < indent ; n++ ) {
	putchar ( ' ' );
    }
    printf ( "'%s'\n", (char *)(node->data) );
    if ( node->rptr != NULL ) {
	/* move down into right sub-tree */
	print_node ( node->rptr, indent + 2 );\
    }
}

void print_tree ( avltree *tree )
{
    printf ( "==========================\n" );
    if ( tree->root != NULL ) {
	print_node ( tree->root, 2 );
    } else {
	printf ( "empty tree\n" );
    }
    printf ( "==========================\n" );
}

int main ( int argc, char *argv[] )
{
    avltree *tree;
    char *buf, *rv;


    tree = avltree_create ( (int(*)(void *, void *))strcmp );
    if ( tree == NULL ) {
	printf ( "Failed to create tree\n" );
	return 1;
    }
    print_tree ( tree );

    buf = malloc(80);
    while ( 1 ) {
        rv = fgets( buf, 75, stdin );
	if ( rv == NULL ) {
	    break;
	}
	rv = strchr ( buf, '\n' );
	if ( rv != NULL ) {
	    *rv = '\0';
	}
//	if ( *buf == '\0' ) {
//	    break;
//	}
	if ( buf[0] != '-' ) {
	    rv = avltree_insert ( tree, buf );
	    if ( rv == buf ) {
		printf ( "Inserted %s\n", rv );
	    } else if ( rv == NULL ) {
		printf ( "No RAM\n" );
	    } else {
		printf ( "Duplicate entry\n" );
	    }
	} else {
	    rv = avltree_delete ( tree, buf+1 );
	    if ( rv == NULL ) {
		printf ( "No match found\n" );
	    } else {
		printf ( "Deleted %s\n", rv );
	    }
	}
	print_tree ( tree );
	buf = malloc(80);
    }
    print_tree ( tree );
    avltree_destroy ( tree );
    return 0;
}

#endif /* BUILD_TEST */
#endif /* not __KERNEL__ */
