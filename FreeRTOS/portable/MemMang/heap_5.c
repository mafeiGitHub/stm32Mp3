// heap_5.c - multiple memory blocks explicitly declared by app main()
/*{{{  includes*/
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
/*}}}*/

// Block sizes must not get too small
#define heapMINIMUM_BLOCK_SIZE  ((size_t)(uxHeapStructSize << 1))
#define heapBITS_PER_BYTE       ((size_t)8)

// free blocks linked in order of memory address
typedef struct A_BLOCK_LINK {
	struct A_BLOCK_LINK* pxNextFreeBlock; // The next free block in the list
	size_t xBlockSize;                    // The size of the free block
	} BlockLink_t;

// correctly byte align structure at beginning of allocated memory block
static const uint32_t uxHeapStructSize  = (sizeof(BlockLink_t) + (portBYTE_ALIGNMENT-1)) & ~portBYTE_ALIGNMENT_MASK;

static BlockLink_t xStart;
static BlockLink_t* pxEnd = NULL;

// Gets set to the top bit of an size_t type.  When this bit in the xBlockSize
// member of an BlockLink_t structure is set then the block belongs to the application
// When the bit is free the block is still part of the free heap space.
static size_t xBlockAllocatedBit = 0;

static size_t xFreeBytesRemaining = 0;
static size_t xMinimumEverFreeBytesRemaining = 0;

size_t xPortGetFreeHeapSize() { return xFreeBytesRemaining; }
size_t xPortGetMinimumEverFreeHeapSize() { return xMinimumEverFreeBytesRemaining; }

/*{{{*/
static void prvInsertBlockIntoFreeList (BlockLink_t* pxBlockToInsert) {
// Inserts a block of memory that is being freed into the correct position in
// the list of free memory blocks.  The block being freed will be merged with
// the block in front it and/or the block behind it if the memory blocks are adjacent to each other

	// Iterate through the list until a block is found that has a higher address than the block being inserted
	BlockLink_t* pxIterator;
	for (pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock ) {}

	// Do the block being inserted, and the block it is being inserted after make a contiguous block of memory?
	uint8_t* puc = (uint8_t*)pxIterator;
	if ((puc + pxIterator->xBlockSize) == (uint8_t*)pxBlockToInsert) {
		pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
		pxBlockToInsert = pxIterator;
		}
	else
		mtCOVERAGE_TEST_MARKER();

	// Do the block being inserted, and the block it is being inserted before make a contiguous block of memory?
	puc = (uint8_t*)pxBlockToInsert;
	if ((puc + pxBlockToInsert->xBlockSize ) == (uint8_t*)pxIterator->pxNextFreeBlock) {
		if (pxIterator->pxNextFreeBlock != pxEnd) {
			// Form one big block from the two blocks
			pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
			pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
			}
		else
			pxBlockToInsert->pxNextFreeBlock = pxEnd;
		}
	else
		pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;

	// If the block being inserted plugged a gab, so was merged with the block
	// before and the block after, then it's pxNextFreeBlock pointer will have
	// already been set, and should not be set here as that would make it point to itself
	if (pxIterator != pxBlockToInsert)
		pxIterator->pxNextFreeBlock = pxBlockToInsert;
	else
		mtCOVERAGE_TEST_MARKER();
	}
/*}}}*/
/*{{{*/
void* pvPortMalloc (size_t xWantedSize) {

	BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
	void* pvReturn = NULL;

	// The heap must be initialised before the first call to prvPortMalloc()
	configASSERT (pxEnd);

	vTaskSuspendAll();
		{
		// Check the requested block size is not so large that the top bit is set
		// The top bit of the block size member of the BlockLink_t structure
		// is used to determine who owns the block - the application or the kernel, so it must be free
		if ((xWantedSize & xBlockAllocatedBit) == 0) {
			// The wanted size is increased so it can contain a BlockLink_t structure in addition to the requested amount of bytes
			if (xWantedSize > 0) {
				xWantedSize += uxHeapStructSize;

				// Ensure that blocks are always aligned to the required number of bytes
				if ((xWantedSize & portBYTE_ALIGNMENT_MASK) != 0x00) // Byte alignment required
					xWantedSize += (portBYTE_ALIGNMENT - (xWantedSize & portBYTE_ALIGNMENT_MASK));
				else
					mtCOVERAGE_TEST_MARKER();
				}
			else
				mtCOVERAGE_TEST_MARKER();

			if ((xWantedSize > 0) && (xWantedSize <= xFreeBytesRemaining)) {
				// Traverse the list from the start (lowest address) block until one of adequate size is found
				pxPreviousBlock = &xStart;
				pxBlock = xStart.pxNextFreeBlock;
				while ((pxBlock->xBlockSize < xWantedSize) && (pxBlock->pxNextFreeBlock != NULL)) {
					pxPreviousBlock = pxBlock;
					pxBlock = pxBlock->pxNextFreeBlock;
					}

				// If the end marker was reached then a block of adequate size was not found
				if (pxBlock != pxEnd) {
					// Return the memory space pointed to - jumping over the BlockLink_t structure at its start
					pvReturn = (void*)(((uint8_t*)pxPreviousBlock->pxNextFreeBlock) + uxHeapStructSize);

					// This block is being returned for use so must be taken out of the list of free blocks
					pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

					// If the block is larger than required it can be split into two
					if ((pxBlock->xBlockSize - xWantedSize) > heapMINIMUM_BLOCK_SIZE) {
						// This block is to be split into two.  
						// Create a new block following the number of bytes requested. 
						// The void cast is used to prevent byte alignment warnings from the compiler
						pxNewBlockLink = (void*)(((uint8_t*)pxBlock) + xWantedSize);

						// Calculate the sizes of two blocks split from the single block
						pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
						pxBlock->xBlockSize = xWantedSize;

						// Insert the new block into the list of free blocks
						prvInsertBlockIntoFreeList (pxNewBlockLink);
						}
					else
						mtCOVERAGE_TEST_MARKER();

					xFreeBytesRemaining -= pxBlock->xBlockSize;
					if (xFreeBytesRemaining < xMinimumEverFreeBytesRemaining)
						xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
					else
						mtCOVERAGE_TEST_MARKER();

					// The block is being returned - it is allocated and owned by the application and has no "next" block
					pxBlock->xBlockSize |= xBlockAllocatedBit;
					pxBlock->pxNextFreeBlock = NULL;
					}
				else
					mtCOVERAGE_TEST_MARKER();
				}
			else
				mtCOVERAGE_TEST_MARKER();
			}
		else
			mtCOVERAGE_TEST_MARKER();

		traceMALLOC( pvReturn, xWantedSize );
		}
	xTaskResumeAll();

	#if (configUSE_MALLOC_FAILED_HOOK == 1)
		if (pvReturn == NULL) {
			extern void vApplicationMallocFailedHook(void);
			vApplicationMallocFailedHook();
			}
		else
			mtCOVERAGE_TEST_MARKER();
	#endif

	return pvReturn;
	}
/*}}}*/
/*{{{*/
void vPortFree (void* pv) {

	BlockLink_t* pxLink;
	uint8_t* puc = (uint8_t*)pv;
	if (pv != NULL) {
		// memory being freed will have an BlockLink_t structure immediately before it
		puc -= uxHeapStructSize;

		// casting is to keep the compiler from issuing warnings
		pxLink = (void*)puc;

		// Check the block is actually allocated
		configASSERT ((pxLink->xBlockSize & xBlockAllocatedBit) != 0);
		configASSERT (pxLink->pxNextFreeBlock == NULL);

		if ((pxLink->xBlockSize & xBlockAllocatedBit) != 0) {
			if (pxLink->pxNextFreeBlock == NULL ) {
				// The block is being returned to the heap - it is no longer allocated
				pxLink->xBlockSize &= ~xBlockAllocatedBit;

				vTaskSuspendAll();
					{
					// Add this block to the list of free blocks
					xFreeBytesRemaining += pxLink->xBlockSize;
					traceFREE( pv, pxLink->xBlockSize );
					prvInsertBlockIntoFreeList (((BlockLink_t*)pxLink));
					}

				xTaskResumeAll();
				}
			else
				mtCOVERAGE_TEST_MARKER();
			}
		else
			mtCOVERAGE_TEST_MARKER();
		}
	}
/*}}}*/

/*{{{*/
void vPortDefineHeapRegions (const HeapRegion_t* const pxHeapRegions) {

	BlockLink_t *pxFirstFreeBlockInRegion = NULL, *pxPreviousFreeBlock;
	uint8_t *pucAlignedHeap;
	size_t xTotalRegionSize, xTotalHeapSize = 0;
	BaseType_t xDefinedRegions = 0;
	uint32_t ulAddress;

	// Can only call once! */
	configASSERT (pxEnd == NULL);

	const HeapRegion_t* pxHeapRegion = &(pxHeapRegions[ xDefinedRegions]);
	while (pxHeapRegion->xSizeInBytes > 0) {
		xTotalRegionSize = pxHeapRegion->xSizeInBytes;

		// Ensure the heap region starts on a correctly aligned boundary
		ulAddress = (uint32_t)pxHeapRegion->pucStartAddress;
		if ((ulAddress & portBYTE_ALIGNMENT_MASK) != 0) {
			ulAddress += (portBYTE_ALIGNMENT - 1);
			ulAddress &= ~portBYTE_ALIGNMENT_MASK;
			// Adjust the size for the bytes lost to alignment
			xTotalRegionSize -= ulAddress - (uint32_t)pxHeapRegion->pucStartAddress;
			}

		pucAlignedHeap = (uint8_t*)ulAddress;

		// Set xStart if it has not already been set
		if (xDefinedRegions == 0) {
			// xStart is used to hold a pointer to the first item in the list of
			// free blocks.  The void cast is used to prevent compiler warnings
			xStart.pxNextFreeBlock = (BlockLink_t*)pucAlignedHeap;
			xStart.xBlockSize = (size_t)0;
			}
		else {
			// Should only get here if one region has already been added to the heap
			configASSERT (pxEnd != NULL);
			// Check blocks are passed in with increasing start addresses
			configASSERT (ulAddress > (uint32_t) pxEnd);
			}

		// Remember the location of the end marker in the previous region, if any
		pxPreviousFreeBlock = pxEnd;

		// pxEnd is used to mark the end of the list of free blocks and is inserted at the end of the region space
		ulAddress = ((uint32_t) pucAlignedHeap) + xTotalRegionSize;
		ulAddress -= uxHeapStructSize;
		ulAddress &= ~portBYTE_ALIGNMENT_MASK;
		pxEnd = (BlockLink_t*) ulAddress;
		pxEnd->xBlockSize = 0;
		pxEnd->pxNextFreeBlock = NULL;

		// To start with there is a single free block in this region that is
		// sized to take up the entire heap region minus the space taken by the free block structure
		pxFirstFreeBlockInRegion = (BlockLink_t*) pucAlignedHeap;
		pxFirstFreeBlockInRegion->xBlockSize = ulAddress - (uint32_t)pxFirstFreeBlockInRegion;
		pxFirstFreeBlockInRegion->pxNextFreeBlock = pxEnd;

		// If this is not the first region that makes up the entire heap space then link the previous region to this region
		if (pxPreviousFreeBlock != NULL)
			pxPreviousFreeBlock->pxNextFreeBlock = pxFirstFreeBlockInRegion;

		xTotalHeapSize += pxFirstFreeBlockInRegion->xBlockSize;

		// Move onto the next HeapRegion_t structure
		xDefinedRegions++;
		pxHeapRegion = &(pxHeapRegions[xDefinedRegions]);
		}

	xMinimumEverFreeBytesRemaining = xTotalHeapSize;
	xFreeBytesRemaining = xTotalHeapSize;

	// Check something was actually defined before it is accessed
	configASSERT (xTotalHeapSize);

	// Work out the position of the top bit in a size_t variable
	xBlockAllocatedBit = ((size_t)1) << ((sizeof(size_t) * heapBITS_PER_BYTE) - 1);
	}
/*}}}*/
