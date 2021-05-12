//------------------------------------------------------------------------------
// LAGraph_BreadthFirstSearch:  breadth-first search
//------------------------------------------------------------------------------

// LAGraph, (c) 2021 by The LAGraph Contributors, All Rights Reserved.
// SPDX-License-Identifier: BSD-2-Clause

//------------------------------------------------------------------------------

// References:
//
// Carl Yang, Aydin Buluc, and John D. Owens. 2018. Implementing Push-Pull
// Efficiently in GraphBLAS. In Proceedings of the 47th International
// Conference on Parallel Processing (ICPP 2018). ACM, New York, NY, USA,
// Article 89, 11 pages. DOI: https://doi.org/10.1145/3225058.3225122
//
// Scott Beamer, Krste Asanovic and David A. Patterson, The GAP Benchmark
// Suite, http://arxiv.org/abs/1508.03619, 2015.  http://gap.cs.berkeley.edu/

#define LAGRAPH_FREE_WORK   \
{                           \
    GrB_free (&frontier);   \
    GrB_free (&index_ramp); \
}

#define LAGRAPH_FREE_ALL    \
{                           \
    LAGRAPH_FREE_WORK ;     \
    GrB_free (&l_parent);   \
    GrB_free (&l_level);    \
}

#include "LG_internal.h"

//****************************************************************************
int LAGraph_BreadthFirstSearch_vanilla
(
    GrB_Vector    *level,
    GrB_Vector    *parent,
    LAGraph_Graph  G,
    GrB_Index      src,
    bool           pushpull,
    char          *msg
)
{
    //--------------------------------------------------------------------------
    // check inputs
    //--------------------------------------------------------------------------

    LG_CLEAR_MSG ;
    GrB_Vector frontier = NULL;     // the current frontier
    GrB_Vector index_ramp = NULL;   // used in assigning parent ids
    GrB_Vector l_parent = NULL;     // parent vector
    GrB_Vector l_level = NULL;      // level vector

    bool compute_level  = (level != NULL);
    bool compute_parent = (parent != NULL);
    if (compute_level ) (*level ) = NULL;
    if (compute_parent) (*parent) = NULL;

    LG_CHECK (LAGraph_CheckGraph (G, msg), -101, "graph is invalid") ;

    if (!(compute_level || compute_parent))
    {
        // nothing to do
        return (0) ;
    }

    //--------------------------------------------------------------------------
    // get the problem size and properties
    //--------------------------------------------------------------------------
    GrB_Matrix A = G->A ;

    GrB_Index n;
    GrB_TRY( GrB_Matrix_nrows (&n, A) );
    LG_CHECK( src >= n, -102, "src is out of range") ;

    GrB_Matrix AT ;
    GrB_Vector Degree = G->rowdegree ;
    LAGraph_Kind kind = G->kind ;

    if (kind == LAGRAPH_ADJACENCY_UNDIRECTED ||
       (kind == LAGRAPH_ADJACENCY_DIRECTED &&
        G->A_pattern_is_symmetric == LAGRAPH_TRUE))
    {
        // AT and A have the same pattern and can be used in both directions
        AT = G->A ;
    }
    else
    {
        // AT = A' is different from A
        AT = G->AT ;
    }

    // determine the semiring type
    GrB_Type     int_type  = (n > INT32_MAX) ? GrB_INT64 : GrB_INT32 ;
    //GrB_BinaryOp plus_op   = (n > INT32_MAX) ? GrB_PLUS_INT64 : GrB_PLUS_INT32;
    GrB_BinaryOp second_op = (n > INT32_MAX) ? GrB_SECOND_INT64 : GrB_SECOND_INT32;
    GrB_Semiring semiring  = NULL;

    if (compute_parent)
    {
        // create the parent vector.  l_parent(i) is the parent id of node i
        GrB_TRY (GrB_Vector_new(&l_parent, int_type, n)) ;

        semiring = (n > INT32_MAX) ?
            GrB_MIN_FIRST_SEMIRING_INT64 : GrB_MIN_FIRST_SEMIRING_INT32;

        // create a sparse integer vector frontier, and set frontier(src) = src
        GrB_TRY (GrB_Vector_new(&frontier, int_type, n)) ;
        GrB_TRY (GrB_Vector_setElement(frontier, src, src)) ;

        // create an index ramp (TODO: replace with apply(i) in GraphBLAS 2.0
        GrB_Index *idx = (GrB_Index *)LAGraph_Malloc(n, sizeof(GrB_Index));
        for (GrB_Index ix = 0; ix < n; ++ix) idx[ix] = ix;
        GrB_TRY( GrB_Vector_new(&index_ramp, int_type, n) );
        GrB_TRY( GrB_Vector_build(index_ramp, idx, idx, n, GrB_FIRST_UINT64) );
        LAGraph_Free((void **) &idx);
    }
    else
    {
        // only the level is needed
        semiring = GrB_LOR_LAND_SEMIRING_BOOL;

        // create a sparse boolean vector frontier, and set frontier(src) = true
        GrB_TRY (GrB_Vector_new(&frontier, GrB_BOOL, n)) ;
        GrB_TRY (GrB_Vector_setElement(frontier, true, src)) ;
    }

    if (compute_level)
    {
        // create the level vector. v(i) is the level of node i
        // v (src) = 0 denotes the source node
        GrB_TRY (GrB_Vector_new(&l_level, int_type, n)) ;
        //GrB_TRY (GrB_Vector_setElement(l_level, 0, src)) ;
    }

    //--------------------------------------------------------------------------
    // BFS traversal and label the nodes
    //--------------------------------------------------------------------------
    GrB_Index nq = 1 ;          // number of nodes in the current level
    GrB_Index last_nq = 0 ;
    GrB_Index current_level = 0;
    GrB_Index nvals = 1;

    // {!mask} is the set of unvisited nodes
    GrB_Vector mask = (compute_parent) ? l_parent : l_level ;

    // parent BFS
    do
    {
        if (compute_level)
        {
            // assign levels: l_level<s(frontier)> = current_level
            GrB_TRY( GrB_assign(l_level, frontier, GrB_NULL,
                                current_level, GrB_ALL, n, GrB_DESC_S) );
            ++current_level;
        }

        if (compute_parent)
        {
            // frontier(i) currently contains the parent id of node i in tree.
            // l_parent<s(frontier)> = frontier
            GrB_TRY( GrB_assign(l_parent, frontier, GrB_NULL,
                                frontier, GrB_ALL, n, GrB_DESC_S) );

            // convert all stored values in frontier to their indices
            // TODO: replace with apply(i) in GraphBLAS 2.0
            GrB_TRY( GrB_eWiseMult(frontier, GrB_NULL, GrB_NULL, GrB_FIRST_UINT64,
                                   index_ramp, frontier, GrB_NULL) );
        }

        // frontier = kth level of the BFS
        // mask is l_parent if computing parent, l_level if computing just level
        GrB_TRY( GrB_vxm(frontier, mask, GrB_NULL, semiring,
                         frontier, A, GrB_DESC_RSC) );

        // done if frontier is empty
        GrB_TRY( GrB_Vector_nvals(&nvals, frontier) );
    } while (nvals > 0);

    //--------------------------------------------------------------------------
    // free workspace and return result
    //--------------------------------------------------------------------------

    if (compute_parent) (*parent) = l_parent ;
    if (compute_level ) (*level ) = l_level ;
    LAGRAPH_FREE_WORK ;
    return (0) ;
}