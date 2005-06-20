/* Classic Ladder Project */
/* Copyright (C) 2001-2003 Marc Le Douarain */
/* mavati@club-internet.fr */
/* http://www.multimania.com/mavati/classicladder */
/* February 2001 */
/* --------------------------- */
/* Alloc/free global variables */
/* --------------------------- */
/* This library is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU Lesser General Public */
/* License as published by the Free Software Foundation; either */
/* version 2.1 of the License, or (at your option) any later version. */

/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU */
/* Lesser General Public License for more details. */

/* You should have received a copy of the GNU Lesser General Public */
/* License along with this library; if not, write to the Free Software */
/* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef MODULE
#include <stdlib.h>
#include <stdio.h>
#endif

#include "rtapi.h"
#include "classicladder.h"
#include "arrays.h"


int					ShmemId;

// Pointers to SHMEM global data.
StrInfosGene				*InfosGene = NULL;
StrRung					*RungArray;
StrTimer				*TimerArray;
StrMonostable				*MonostableArray;
StrArithmExpr				*ArithmExpr;
StrSection				*SectionArray;
#ifdef SEQUENTIAL_SUPPORT
StrSequential				*Sequential;
#endif
int					*VarWordArray;
TYPE_FOR_BOOL_VAR			*VarArray;

#if !defined(MODULE) && defined(GTK_INTERFACE)
/* used for the editor */
StrEditRung				EditDatas;
StrArithmExpr				*EditArithmExpr = NULL;
#endif


/* return TRUE if okay */
#ifdef MODULE
int ClassicLadderAllocAll(int compId, plc_sizeinfo_s *pSizesInfos)
#else
int ClassicLadderAllocAll(int compId)
#endif
{
    unsigned char			*pByte;
    unsigned long			bytes= sizeof(StrInfosGene);
#ifdef MODULE
    int					numBits, numWords;
#else
    plc_sizeinfo_s 			plcSizeInfo, *pSizesInfos=&plcSizeInfo;
#endif

#ifdef MODULE
    // Calculate SHMEM size.
    numBits = pSizesInfos->nbr_bits + pSizesInfos->nbr_phys_inputs + pSizesInfos->nbr_phys_outputs;
    numWords = pSizesInfos->nbr_words;
#ifdef SEQUENTIAL_SUPPORT
    numBits += pSizesInfos->nbr_steps;
    numWords += pSizesInfos->nbr_steps;
#endif
    bytes += pSizesInfos->nbr_rungs * sizeof(StrRung);
    bytes += pSizesInfos->nbr_timers * sizeof(StrTimer);
    bytes += pSizesInfos->nbr_monostables * sizeof(StrMonostable);
    bytes += pSizesInfos->nbr_arithm_expr * sizeof(StrArithmExpr);
    bytes += pSizesInfos->nbr_sections * sizeof(StrSection);
#ifdef SEQUENTIAL_SUPPORT
    bytes += sizeof(StrSequential))){
#endif
    bytes += numWords * sizeof(int);
    bytes += numBits * sizeof(TYPE_FOR_BOOL_VAR);
#endif

    // Allocate SHMEM.
    if((ShmemId = rtapi_shmem_new(CL_SHMEM_KEY, compId, bytes)) < 0){
        rtapi_print("Failed to alloc shared memory !\n");
        return FALSE;
    }

    // Map SHMEM.
    if(rtapi_shmem_getptr(ShmemId, (void **)&InfosGene) < 0){
        rtapi_print("Failed to map shared memory !\n");
        return FALSE;
    }

#ifndef MODULE
    // Check signature written by RT module to make sure we have the
    // right region and RT module is loaded.
    if(InfosGene->Signature != CL_SHMEM_KEY){
        rtapi_print("Shared memory conflict or RT component not loaded!\n");
        return FALSE;
    }

    // Copy size info.
    *pSizesInfos = InfosGene->SizesInfos;
#endif

    // Set global SHMEM pointer.
    pByte = (unsigned char *)InfosGene;
    pByte += sizeof(StrInfosGene);

    RungArray = (StrRung *)pByte;
    pByte += pSizesInfos->nbr_rungs * sizeof(StrRung);

    TimerArray = (StrTimer *)pByte;
    pByte += pSizesInfos->nbr_timers * sizeof(StrTimer);

    MonostableArray = (StrMonostable *)pByte;
    pByte += pSizesInfos->nbr_monostables * sizeof(StrMonostable);

    ArithmExpr = (StrArithmExpr *)pByte;
    pByte += pSizesInfos->nbr_arithm_expr * sizeof(StrArithmExpr);

    SectionArray = (StrSection *)pByte;
    pByte += pSizesInfos->nbr_sections * sizeof(StrSection);

#ifdef SEQUENTIAL_SUPPORT
    Sequential = (StrSequential *)pByte;
    pByte += sizeof(StrSequential);
#endif

    VarWordArray = (int *)pByte;
    pByte += SIZE_VAR_WORD_ARRAY * sizeof(int);

    // Allocate last for alignment reasons.
    VarArray = (TYPE_FOR_BOOL_VAR *)pByte;

#ifndef MODULE
#ifdef GTK_INTERFACE
    EditArithmExpr = (StrArithmExpr *)malloc( NBR_ARITHM_EXPR * sizeof(StrArithmExpr) );
    if (!EditArithmExpr)
    {
        printf("Failed to alloc EditArithmExpr !\n");
        return FALSE;
    }
#endif
#else
    // Initialize SHMEM.
    InfosGene->Signature = CL_SHMEM_KEY;
    InfosGene->SizesInfos = *pSizesInfos;

    InfosGene->LadderState = STATE_LOADING;
    InfosGene->CmdRefreshVarsBits = FALSE;

    InfosGene->BlockWidth = BLOCK_WIDTH_DEF;
    InfosGene->BlockHeight = BLOCK_HEIGHT_DEF;
    InfosGene->PageWidth = 0;
    InfosGene->PageHeight = 0;
    InfosGene->TopRungDisplayed = 0;
    InfosGene->OffsetHiddenTopRungDisplayed = 0;
    InfosGene->OffsetCurrentRungDisplayed = 0;
    InfosGene->VScrollValue = 0;
    InfosGene->HScrollValue = 0;

    InfosGene->DurationOfLastScan = 0;
    InfosGene->CurrentSection = 0;
#endif

    return TRUE;
}

void ClassicLadderFreeAll(int compId)
{
#if !defined(MODULE) && defined(GTK_INTERFACE)
    if (EditArithmExpr)
        free(EditArithmExpr);
#endif
    if (InfosGene)
        rtapi_shmem_delete(ShmemId, compId);
}
