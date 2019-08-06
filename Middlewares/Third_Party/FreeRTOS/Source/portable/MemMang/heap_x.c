/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*
 * A sample implementation of pvPortMalloc() and vPortFree() that combines
 * (coalescences) adjacent memory blocks as they are freed, and in so doing
 * limits memory fragmentation.
 *
 * See heap_1.c, heap_2.c and heap_3.c for alternative implementations, and the
 * memory management pages of http://www.FreeRTOS.org for more information.
 */
#include <stdlib.h>
#include <string.h>

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"

#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#if( configSUPPORT_DYNAMIC_ALLOCATION == 0 )
	#error This file must not be used if configSUPPORT_DYNAMIC_ALLOCATION is 0
#endif

/* Block sizes must not get too small. */
#define heapMINIMUM_BLOCK_SIZE	( ( size_t ) ( xHeapStructSize << 1 ) )

/* Assumes 8bit bytes! */
#define heapBITS_PER_BYTE		( ( size_t ) 8 )

/* Allocate the memory for the heap. */
#if( configAPPLICATION_ALLOCATED_HEAP == 1 )
	/* The application writer has already defined the array used for the RTOS
	heap - probably so it can be placed in a special segment or address. */
	extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#else
	static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
#endif /* configAPPLICATION_ALLOCATED_HEAP */

/* Define the linked list structure.  This is used to link free blocks in order
of their memory address. */
typedef struct A_BLOCK_LINK
{
	struct A_BLOCK_LINK *pxNextFreeBlock;	/*<< The next free block in the list. */
	size_t xBlockSize;						/*<< The size of the free block. */
} BlockLink_t;

/*-----------------------------------------------------------*/

/*
 * Inserts a block of memory that is being freed into the correct position in
 * the list of free memory blocks.  The block being freed will be merged with
 * the block in front it and/or the block behind it if the memory blocks are
 * adjacent to each other.
 */
static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert );

/*
 * Called automatically to setup the required heap structures the first time
 * pvPortMalloc() is called.
 */
static void prvHeapInit( void );

/*-----------------------------------------------------------*/

/* The size of the structure placed at the beginning of each allocated memory
block must by correctly byte aligned. */
static const size_t xHeapStructSize	= ( sizeof( BlockLink_t ) + ( ( size_t ) ( portBYTE_ALIGNMENT - 1 ) ) ) & ~( ( size_t ) portBYTE_ALIGNMENT_MASK );

/* Create a couple of list links to mark the start and end of the list. */
static BlockLink_t xStart, *pxEnd = NULL;

/* Keeps track of the number of free bytes remaining, but says nothing about
fragmentation. */
static size_t xFreeBytesRemaining = 0U;
static size_t xMinimumEverFreeBytesRemaining = 0U;

/* Gets set to the top bit of an size_t type.  When this bit in the xBlockSize
member of an BlockLink_t structure is set then the block belongs to the
application.  When the bit is free the block is still part of the free heap
space. */
static size_t xBlockAllocatedBit = 0;

/*-----------------------------------------------------------*/

void *pvPortMalloc( size_t xWantedSize )
{
BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
void *pvReturn = NULL;

	vTaskSuspendAll();
	{
		/* If this is the first call to malloc then the heap will require
		initialisation to setup the list of free blocks. */
		if( pxEnd == NULL )
		{
			prvHeapInit();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		/* Check the requested block size is not so large that the top bit is
		set.  The top bit of the block size member of the BlockLink_t structure
		is used to determine who owns the block - the application or the
		kernel, so it must be free. */
		if( ( xWantedSize & xBlockAllocatedBit ) == 0 )
		{
			/* The wanted size is increased so it can contain a BlockLink_t
			structure in addition to the requested amount of bytes. */
			if( xWantedSize > 0 )
			{
				xWantedSize += xHeapStructSize;

				/* Ensure that blocks are always aligned to the required number
				of bytes. */
				if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
				{
					/* Byte alignment required. */
					xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
					configASSERT( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) == 0 );
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) )
			{
				/* Traverse the list from the start	(lowest address) block until
				one	of adequate size is found. */
				pxPreviousBlock = &xStart;
				pxBlock = xStart.pxNextFreeBlock;
				while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) )
				{
					pxPreviousBlock = pxBlock;
					pxBlock = pxBlock->pxNextFreeBlock;
				}

				/* If the end marker was reached then a block of adequate size
				was	not found. */
				if( pxBlock != pxEnd )
				{
					/* Return the memory space pointed to - jumping over the
					BlockLink_t structure at its start. */
					pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );

					/* This block is being returned for use so must be taken out
					of the list of free blocks. */
					pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

					/* If the block is larger than required it can be split into
					two. */
					if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE )
					{
						/* This block is to be split into two.  Create a new
						block following the number of bytes requested. The void
						cast is used to prevent byte alignment warnings from the
						compiler. */
						pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
						configASSERT( ( ( ( size_t ) pxNewBlockLink ) & portBYTE_ALIGNMENT_MASK ) == 0 );

						/* Calculate the sizes of two blocks split from the
						single block. */
						pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
						pxBlock->xBlockSize = xWantedSize;

						/* Insert the new block into the list of free blocks. */
						prvInsertBlockIntoFreeList( pxNewBlockLink );
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					xFreeBytesRemaining -= pxBlock->xBlockSize;

					if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining )
					{
						xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
					}
					else
					{
						mtCOVERAGE_TEST_MARKER();
					}

					/* The block is being returned - it is allocated and owned
					by the application and has no "next" block. */
					pxBlock->xBlockSize |= xBlockAllocatedBit;
					pxBlock->pxNextFreeBlock = NULL;

#if configMALLOC_FILL
					memset ( ((uint8_t*)pxBlock)+sizeof(BlockLink_t), 0xdd, xHeapStructSize-sizeof(BlockLink_t) );
					memset ( ((uint8_t*)pxBlock)+xHeapStructSize, 0xcd, (pxBlock->xBlockSize&~xBlockAllocatedBit) - xHeapStructSize );
#endif
				}
				else
				{
					mtCOVERAGE_TEST_MARKER();
				}
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}

		traceMALLOC( pvReturn, xWantedSize );
	}
	( void ) xTaskResumeAll();

	#if( configUSE_MALLOC_FAILED_HOOK == 1 )
	{
		if( pvReturn == NULL )
		{
			extern void vApplicationMallocFailedHook( void );
			vApplicationMallocFailedHook();
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
	#endif

	configASSERT( ( ( ( size_t ) pvReturn ) & ( size_t ) portBYTE_ALIGNMENT_MASK ) == 0 );
	return pvReturn;
}
/*-----------------------------------------------------------*/

void vPortFree( void *pv )
{
uint8_t *puc = ( uint8_t * ) pv;
BlockLink_t *pxLink;

	if( pv != NULL )
	{
		/* The memory being freed will have an BlockLink_t structure immediately
		before it. */
		puc -= xHeapStructSize;

		/* This casting is to keep the compiler from issuing warnings. */
		pxLink = ( void * ) puc;

		/* Check the block is actually allocated. */
		configASSERT( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 );
		configASSERT( pxLink->pxNextFreeBlock == NULL );

		if( ( pxLink->xBlockSize & xBlockAllocatedBit ) != 0 )
		{
			if( pxLink->pxNextFreeBlock == NULL )
			{
				/* The block is being returned to the heap - it is no longer
				allocated. */
				pxLink->xBlockSize &= ~xBlockAllocatedBit;

				vTaskSuspendAll();
				{
					/* Add this block to the list of free blocks. */
					xFreeBytesRemaining += pxLink->xBlockSize;
					traceFREE( pv, pxLink->xBlockSize );
					prvInsertBlockIntoFreeList( ( ( BlockLink_t * ) pxLink ) );
				}
				( void ) xTaskResumeAll();
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}
		}
		else
		{
			mtCOVERAGE_TEST_MARKER();
		}
	}
}
/*-----------------------------------------------------------*/



void* pvPortRealloc( void* pvOrig, size_t xWantedSize )
{
	if ( 0 == xWantedSize )	/* resize to 0 is effectively free() */
	{
		vPortFree( pvOrig );
		return NULL;
	}
	else if ( NULL == pvOrig )	/* NULL original block is effectively malloc() */
	{
		return pvPortMalloc( xWantedSize );
	}
	else	/* otherwise we are resizing a valid block, trying to keep it in place */
	{
uint8_t* pbyScratch;
BlockLink_t* pxLinkThis;
void* pvNewBlock;

		vTaskSuspendAll();

		/* get to the BlockLink_t header that precedes this block */
		pbyScratch = ((uint8_t*)pvOrig);
		pbyScratch -= xHeapStructSize;
		pxLinkThis = (void*) pbyScratch;	/* (cast avoids warnings) */

		/* Verify that the block is actually allocated. */
		configASSERT( ( pxLinkThis->xBlockSize & xBlockAllocatedBit ) != 0 );
		configASSERT( pxLinkThis->pxNextFreeBlock == NULL );

		if( ( pxLinkThis->xBlockSize & xBlockAllocatedBit ) != 0 )
		{
size_t nThisBlockSize = (pxLinkThis->xBlockSize & ~xBlockAllocatedBit);
size_t nNewBlockSize = xWantedSize;

			/* The wanted size is increased so it can contain a BlockLink_t
			structure in addition to the requested amount of bytes. */
			nNewBlockSize += xHeapStructSize;

			/* Ensure that blocks are always aligned to the required number
			of bytes. */
			if( ( nNewBlockSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )
			{
				/* Byte alignment required. */
				nNewBlockSize += ( portBYTE_ALIGNMENT - ( nNewBlockSize & portBYTE_ALIGNMENT_MASK ) );
				configASSERT( ( nNewBlockSize & portBYTE_ALIGNMENT_MASK ) == 0 );
			}
			else
			{
				mtCOVERAGE_TEST_MARKER();
			}

			/* staying the same size, which can happen for small size changes */
			if ( nNewBlockSize == nThisBlockSize )
			{
				pvNewBlock = pvOrig;	/* we have not moved */
			}
			else if ( nNewBlockSize < nThisBlockSize )	/* getting smaller */
			{
BlockLink_t* pxLinkNext;
				/* see if we should split it */

				/* next (new) block */
				pbyScratch += nNewBlockSize;
				pxLinkNext = (void*) pbyScratch;	/* (cast avoids warnings) */

				/* adjust our size, and set next block size */
				pxLinkThis->xBlockSize = nNewBlockSize | xBlockAllocatedBit;
				pxLinkNext->xBlockSize = nThisBlockSize - nNewBlockSize;

				/* add next block to free list */
				prvInsertBlockIntoFreeList ( pxLinkNext );
				
				/* update to show much we have released to the free pool */
				xFreeBytesRemaining += nThisBlockSize - nNewBlockSize;

				pvNewBlock = pvOrig;	/* we have not moved */
			}
			else	/* getting bigger */
			{
BlockLink_t* pxLinkNext;
size_t nExtraSize;

				/* get to next memory block */
				pbyScratch += nThisBlockSize;
				pxLinkNext = (void*) pbyScratch;	/* (cast avoids warnings) */

				/* amount to remove from next block, if possible */
				nExtraSize = nNewBlockSize - nThisBlockSize;
				/* see if it's free and if there is enough in it */
				if ( ( ( pxLinkNext->xBlockSize & xBlockAllocatedBit ) == 0 ) &&
					( pxLinkNext->xBlockSize >= nExtraSize ) )
				{
BlockLink_t* pxIterator;
BlockLink_t* pxLinkNewNext;

					/* first, take the free block out of the free list */
					for( pxIterator = &xStart; pxIterator->pxNextFreeBlock != pxEnd; pxIterator = pxIterator->pxNextFreeBlock )
					{
						if ( pxIterator->pxNextFreeBlock == pxLinkNext )
						{
							pxIterator->pxNextFreeBlock = pxLinkNext->pxNextFreeBlock;
							break;
						}
					}

					/* split that block, if there is any left */
					pbyScratch += nExtraSize;
					pxLinkNewNext = (void*) pbyScratch;	/*(cast avoids warnings) */

					if ( pxLinkNewNext != pxEnd )
					{
						/* adjust that size and re-return it to the free pool to
						  update pointers correctly
						*/
						pxLinkNewNext->xBlockSize = pxLinkNext->xBlockSize - nExtraSize;
						prvInsertBlockIntoFreeList ( pxLinkNewNext );
					}

					/* update our size */
					pxLinkThis->xBlockSize = nNewBlockSize | xBlockAllocatedBit;

					/* update to show much we have taken from the free pool */
					xFreeBytesRemaining -= nExtraSize;

#if configMALLOC_FILL
					memset ( ((uint8_t*)pxLinkThis)+sizeof(BlockLink_t), 0xdd, xHeapStructSize-sizeof(BlockLink_t) );
					memset ( ((uint8_t*)pxLinkThis)+nThisBlockSize, 0xcd, nExtraSize );
#endif
					pvNewBlock = pvOrig;	/* we have not moved */
				}
				else
				{
					/* cannot use stuff in next block; because of block merging
					  on free, we know that there is only contiguous space in
					  this block alone, therefore we must simply make a new
					  allocation, copy the existing contents, and free this
					*/
					pvNewBlock = pvPortMalloc ( xWantedSize );	/* we moved to a new spot */
					if ( NULL != pvNewBlock )
					{
						memcpy ( pvNewBlock, pvOrig, nThisBlockSize - xHeapStructSize );
						vPortFree ( pvOrig );
					}
				}
			}
		}
		else	/* horror; this block does not appear to be allocated */
		{
			pvNewBlock = NULL;
			mtCOVERAGE_TEST_MARKER();
		}

		( void ) xTaskResumeAll();

		return pvNewBlock;
	}
}


/*-----------------------------------------------------------*/


/* heap corruption symptoms:
  pointer unaligned as per portBYTE_ALIGNMENT_MASK -- invalid; unknown
  pointer outside of established bounds of ucHeap -- corrupt; could hardfault
  pointer moving backwards -- corrupt; could infinite loop
  size marking alloc/free incorrectly -- corrupt; unknown
*/
static int inspectHeapCorruption ( BlockLink_t* pxLinkThis, BlockLink_t* pxLinkLast, int bAllocated )
{
	if ( 0 != ( (uintptr_t)pxLinkThis & portBYTE_ALIGNMENT_MASK ) )
		return 1;	/* symptom 1:  block pointer unaligned */
	if ( (uint8_t*)pxLinkThis < ucHeap || (uint8_t*)pxLinkThis >= ucHeap + configTOTAL_HEAP_SIZE )
		return 2;	/* symptom 2:  block pointer out-of-bounds */
	if ( pxLinkThis <= pxLinkLast )
		return 3;	/* symptom 3:  linked list moving backwards */
	if ( ( bAllocated && ! ( (uintptr_t)pxLinkThis & xBlockAllocatedBit ) ) ||
			( ! bAllocated && ( (uintptr_t)pxLinkThis & xBlockAllocatedBit ) ) )
		return 4;	/* symptom 4: unexpected allocation state */
	return 0;	/* no heap corruption detected */
}


typedef int (*CBK_HEAPWALK) ( void* pblk, uint32_t nBlkSize, int bIsFree, void* pinst );

int vPortHeapWalk ( CBK_HEAPWALK pfnWalk, void* pinst )
{
	vTaskSuspendAll();
	
	/* In the pathological case where we heapwalk before we have init'ed the
	heap, ensure that the heap is init'ed. */
	if( pxEnd == NULL )
	{
		prvHeapInit();
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	BlockLink_t* pxLinkFree;
	BlockLink_t* pxLinkThis;
	BlockLink_t* pxLinkLast;
	int nInspectResult;

	/* the free list is traversed as a linked list, the allocated blocks are
	  traversed as contiguous blocks between the linked free ones. */

	/* init block pointers */
	pxLinkFree = xStart.pxNextFreeBlock;
	pxLinkThis = (BlockLink_t*) ( ucHeap + ( portBYTE_ALIGNMENT - ( (size_t)ucHeap & portBYTE_ALIGNMENT_MASK ) ) );
	pxLinkLast = NULL;

	/* inspect first block for sanity before proceeding */
	nInspectResult = inspectHeapCorruption ( pxLinkFree, pxLinkLast, 0 );
	if ( 0 != nInspectResult )
		return nInspectResult;

	/* do the heap walk until done */
	while ( pxLinkThis != pxEnd )
	{
		/* advance through alloc'ed blocks less than the next free'ed block */
		while ( pxLinkThis < pxLinkFree )
		{
			/* inspect this block for sanity */
			nInspectResult = inspectHeapCorruption ( pxLinkThis, pxLinkLast, 1 );
			if ( 0 != nInspectResult )
				return nInspectResult;
			pxLinkLast = pxLinkThis;

			/* emit data; alloc'ed block */
			pfnWalk ( pxLinkThis, (pxLinkThis->xBlockSize&~xBlockAllocatedBit), 1, pinst );
			
			pxLinkThis = (BlockLink_t*) (((uint8_t*)pxLinkThis) + (pxLinkThis->xBlockSize&~xBlockAllocatedBit));
		}

		/* now were are doing a single interceding free block */
		pxLinkThis = pxLinkFree;

		/* inspect this block for sanity */
		nInspectResult = inspectHeapCorruption ( pxLinkThis, pxLinkLast, 0 );
		if ( 0 != nInspectResult )
			return nInspectResult;
		pxLinkLast = pxLinkThis;

		/* emit data; free'ed block */
		pfnWalk ( pxLinkThis, pxLinkThis->xBlockSize, 0, pinst );

		/* advance ptr to next (alloc'ed) block, and next free block */
		pxLinkThis = (BlockLink_t*) (((uint8_t*)pxLinkThis) + (pxLinkThis->xBlockSize&~xBlockAllocatedBit));
		pxLinkFree = pxLinkFree->pxNextFreeBlock;
	}

	( void ) xTaskResumeAll();

	/* finished; no detected heap errors */
	return 0;
}


/*-----------------------------------------------------------*/
size_t xPortGetFreeHeapSize( void )
{
	return xFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

size_t xPortGetMinimumEverFreeHeapSize( void )
{
	return xMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

void vPortInitialiseBlocks( void )
{
	/* This just exists to keep the linker quiet. */
}
/*-----------------------------------------------------------*/

static void prvHeapInit( void )
{
BlockLink_t *pxFirstFreeBlock;
uint8_t *pucAlignedHeap;
size_t uxAddress;
size_t xTotalHeapSize = configTOTAL_HEAP_SIZE;

	/* Ensure the heap starts on a correctly aligned boundary. */
	uxAddress = ( size_t ) ucHeap;

	if( ( uxAddress & portBYTE_ALIGNMENT_MASK ) != 0 )
	{
		uxAddress += ( portBYTE_ALIGNMENT - 1 );
		uxAddress &= ~( ( size_t ) portBYTE_ALIGNMENT_MASK );
		xTotalHeapSize -= uxAddress - ( size_t ) ucHeap;
	}

	pucAlignedHeap = ( uint8_t * ) uxAddress;

	/* xStart is used to hold a pointer to the first item in the list of free
	blocks.  The void cast is used to prevent compiler warnings. */
	xStart.pxNextFreeBlock = ( void * ) pucAlignedHeap;
	xStart.xBlockSize = ( size_t ) 0;

	/* pxEnd is used to mark the end of the list of free blocks and is inserted
	at the end of the heap space. */
	uxAddress = ( ( size_t ) pucAlignedHeap ) + xTotalHeapSize;
	uxAddress -= xHeapStructSize;
	uxAddress &= ~( ( size_t ) portBYTE_ALIGNMENT_MASK );
	pxEnd = ( void * ) uxAddress;
	pxEnd->xBlockSize = 0;
	pxEnd->pxNextFreeBlock = NULL;

	/* To start with there is a single free block that is sized to take up the
	entire heap space, minus the space taken by pxEnd. */
	pxFirstFreeBlock = ( void * ) pucAlignedHeap;
	pxFirstFreeBlock->xBlockSize = uxAddress - ( size_t ) pxFirstFreeBlock;
	pxFirstFreeBlock->pxNextFreeBlock = pxEnd;

#if configMALLOC_FILL
	/* fill in dead data at beginning and end of heap, due to misalignment */
	memset ( ucHeap, 0xdd, pucAlignedHeap - ucHeap );
	memset ( ((uint8_t*)pxEnd)+sizeof(BlockLink_t), 0xdd, configTOTAL_HEAP_SIZE - ( ((uint8_t*)pxEnd)+sizeof(BlockLink_t) - ucHeap ) );
	/* fill in dead data at end of heap management header, due to alignment */
	memset ( ((uint8_t*)pxFirstFreeBlock)+sizeof(BlockLink_t), 0xdd, xHeapStructSize-sizeof(BlockLink_t) );
	/* fill in free data area with distinctive pattern to make the areas which have never been allocated plainly visible */
	memset ( ((uint8_t*)pxFirstFreeBlock)+xHeapStructSize, 0xed, (pxFirstFreeBlock->xBlockSize&~xBlockAllocatedBit) - xHeapStructSize );
#endif
	/* Only one block exists - and it covers the entire usable heap space. */
	xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
	xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;

	/* Work out the position of the top bit in a size_t variable. */
	xBlockAllocatedBit = ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * heapBITS_PER_BYTE ) - 1 );
}
/*-----------------------------------------------------------*/

static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert )
{
BlockLink_t *pxIterator;
uint8_t *puc;

	/* Iterate through the list until a block is found that has a higher address
	than the block being inserted. */
	for( pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock )
	{
		/* Nothing to do here, just iterate to the right position. */
	}

	/* Do the block being inserted, and the block it is being inserted after
	make a contiguous block of memory? */
	puc = ( uint8_t * ) pxIterator;
	if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert )
	{
		pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
		pxBlockToInsert = pxIterator;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

	/* Do the block being inserted, and the block it is being inserted before
	make a contiguous block of memory? */
	puc = ( uint8_t * ) pxBlockToInsert;
	if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock )
	{
		if( pxIterator->pxNextFreeBlock != pxEnd )
		{
			/* Form one big block from the two blocks. */
			pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
			pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
		}
		else
		{
			pxBlockToInsert->pxNextFreeBlock = pxEnd;
		}
	}
	else
	{
		pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
	}

	/* If the block being inserted plugged a gap, so was merged with the block
	before and the block after, then it's pxNextFreeBlock pointer will have
	already been set, and should not be set here as that would make it point
	to itself. */
	if( pxIterator != pxBlockToInsert )
	{
		pxIterator->pxNextFreeBlock = pxBlockToInsert;
	}
	else
	{
		mtCOVERAGE_TEST_MARKER();
	}

#if configMALLOC_FILL
	memset ( ((uint8_t*)pxBlockToInsert)+sizeof(BlockLink_t), 0xdd, xHeapStructSize-sizeof(BlockLink_t) );
	memset ( ((uint8_t*)pxBlockToInsert)+xHeapStructSize, 0xfd, pxBlockToInsert->xBlockSize - xHeapStructSize );
#endif
}

