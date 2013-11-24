
#include "pzsp_defs.h"

pzgstrf_threadarg_t *
pzgstrf_thread_init(SuperMatrix *A, SuperMatrix *L, SuperMatrix *U,
		    superlumt_options_t *options, 
		    pxgstrf_shared_t *pxgstrf_shared,
		    Gstat_t *Gstat, int *info)
{
/*
 * -- SuperLU MT routine (version 2.0) --
 * Lawrence Berkeley National Lab, Univ. of California Berkeley,
 * and Xerox Palo Alto Research Center.
 * September 10, 2007
 *
 *
 * Purpose
 * =======
 *
 * pzgstrf_thread_init() initializes the parallel data structures
 * for the multithreaded routine pzgstrf_thread().
 *
 * Arguments
 * =========
 *
 * A        (input) SuperMatrix*
 *	    Original matrix A, permutated by columns, of dimension
 *          (A->nrow, A->ncol). The type of A can be:
 *          Stype = NCP; Dtype = _D; Mtype = GE.
 *
 * L        (input) SuperMatrix*
 *          If options->refact = YES, then use the existing
 *          storage in L to perform LU factorization;
 *          Otherwise, L is not accessed. L has types: 
 *          Stype = SCP, Dtype = _D, Mtype = TRLU.
 *
 * U        (input) SuperMatrix*
 *          If options->refact = YES, then use the existing
 *          storage in U to perform LU factorization;
 *          Otherwise, U is not accessed. U has types:
 *          Stype = NCP, Dtype = _D, Mtype = TRU.
 *
 * options (input) superlumt_options_t*
 *          The structure contains the parameters to control how the
 *          factorization is performed;
 *          See superlumt_options_t structure defined in slu_mt_util.h.
 *
 * pxgstrf_shared (output) pxgstrf_shared_t*
 *          The structure contains the shared task queue and the 
 *          synchronization variables for parallel factorization.
 *          See pxgstrf_shared_t structure defined in pdsp_defs.h.
 *
 * Gstat    (output) Gstat_t*
 *          Record all the statistics about the factorization; 
 *          See Gstat_t structure defined in slu_mt_util.h.
 *
 * info     (output) int*
 *          = 0: successful exit
 *          > 0: if options->lwork = -1, info returns the estimated
 *               amount of memory (in bytes) required;
 *               Otherwise, it returns the number of bytes allocated when
 *               memory allocation failure occurred, plus A->ncol.
 *
 */
    static GlobalLU_t Glu; /* persistent to support repeated factors. */
    pzgstrf_threadarg_t *pzgstrf_threadarg;
    register int n, i, nprocs;
    NCPformat *Astore;
    int  *perm_c;
    int  *perm_r;
    int  *inv_perm_c; /* inverse of perm_c */
    int  *inv_perm_r; /* inverse of perm_r */
    int	 *xprune;  /* points to locations in subscript vector lsub[*].
			For column i, xprune[i] denotes the point where 
			structural pruning begins.
			I.e. only xlsub[i],..,xprune[i]-1 need to be
			traversed for symbolic factorization.     */
    int  *ispruned;/* flag to indicate whether column j is pruned */
    int   nzlumax;
    pxgstrf_relax_t *pxgstrf_relax;
    
    nprocs     = options->nprocs;
    perm_c     = options->perm_c;
    perm_r     = options->perm_r;
    n          = A->ncol;
    Astore     = A->Store;
    inv_perm_r = (int *) intMalloc(n);
    inv_perm_c = (int *) intMalloc(n);
    xprune     = (int *) intMalloc(n);
    ispruned   = (int *) intCalloc(n);
    
    /* Pack shared data objects to each process. */
    pxgstrf_shared->inv_perm_r   = inv_perm_r;
    pxgstrf_shared->inv_perm_c   = inv_perm_c;
    pxgstrf_shared->xprune       = xprune;
    pxgstrf_shared->ispruned     = ispruned;
    pxgstrf_shared->A            = A;
    pxgstrf_shared->Glu          = &Glu;
    pxgstrf_shared->Gstat        = Gstat;
    pxgstrf_shared->info         = info;

    if ( options->usepr ) {
	/* Compute the inverse of perm_r */
	for (i = 0; i < n; ++i) inv_perm_r[perm_r[i]] = i;
    }
    for (i = 0; i < n; ++i) inv_perm_c[perm_c[i]] = i;

    /* Initialization. */
    Glu.nsuper = -1;
    Glu.nextl  = 0;
    Glu.nextu  = 0;
    Glu.nextlu = 0;
    ifill(perm_r, n, EMPTY);

    /* Identify relaxed supernodes at the bottom of the etree. */
    pxgstrf_relax = (pxgstrf_relax_t *)
        SUPERLU_MALLOC( (size_t) (n+2) * sizeof(pxgstrf_relax_t) );
    if ( options->SymmetricMode == YES ) {
        heap_relax_snode(n, options, pxgstrf_relax);
    } else {
        pxgstrf_relax_snode(n, options, pxgstrf_relax);
    }        
    
    /* Initialize mutex variables, task queue, determine panels. */
    ParallelInit(n, pxgstrf_relax, options, pxgstrf_shared);
    
    /* Set up memory image in lusup[*]. */
    nzlumax = zPresetMap(n, A, pxgstrf_relax, options, &Glu);
    if ( options->refact == NO ) Glu.nzlumax = nzlumax;
    
    SUPERLU_FREE (pxgstrf_relax);

    /* Allocate global storage common to all the factor routines */
    *info = pzgstrf_MemInit(n, Astore->nnz, options, L, U, &Glu);
    if ( *info ) return NULL;

    /* Prepare arguments to all threads. */
    pzgstrf_threadarg = (pzgstrf_threadarg_t *) 
        SUPERLU_MALLOC(nprocs * sizeof(pzgstrf_threadarg_t));
    for (i = 0; i < nprocs; ++i) {
        pzgstrf_threadarg[i].pnum = i;
        pzgstrf_threadarg[i].info = 0;
	pzgstrf_threadarg[i].superlumt_options = options;
	pzgstrf_threadarg[i].pxgstrf_shared = pxgstrf_shared;
    }

#if ( DEBUGlevel==1 )
    printf("** pzgstrf_thread_init() called\n");
#endif

    return (pzgstrf_threadarg);
}
