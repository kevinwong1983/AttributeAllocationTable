/******************************************************************************/
/*                               INCLUDE FILES                                */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nvm.h"

#include "nvm_loc_attributeAllocationTable.h"
/******************************************************************************/
/*                            CONSTANT DEFINITIONS                            */
/******************************************************************************/

/******************************************************************************/
/*                              MACRO DEFINITIONS                             */
/******************************************************************************/

/******************************************************************************/
/*                              TYPE DEFINITIONS                              */
/******************************************************************************/

/******************************************************************************/
/*                          PUBLIC DATA DEFINITIONS                           */
/******************************************************************************/

/******************************************************************************/
/*                        PRIVATE FUNCTION PROTOTYPES                         */
/******************************************************************************/
static int GetNumberOfValidAdresses(size_t sIn, AAT_t* pIn);
static int Partition (AAT_t* pArr, int low, int high);
static void FilterOutInvalidAdresses(AAT_t* pIn, size_t sIn, AAT_t** ppOut, size_t* sOut);
static void FilterValidAddresses(size_t sIn, AAT_t* pIn, AAT_t* pSortedAndFilteredTable);
static void QuickSort(AAT_t pArr[], int low, int high);
static void Swap(AAT_t* pA, AAT_t* pB);
/******************************************************************************/
/*                          PRIVATE DATA DEFINITIONS                          */
/******************************************************************************/
static AAT_t attrAllocTable[MAX_ATTRIBUTES];
/******************************************************************************/
/*                      PUBLIC FUNCTION IMPLEMENTATIONS                       */
/******************************************************************************/
AAT_t* gpNvm_Loc_GetAttrAllocTable()
{
	return attrAllocTable;
}

void gpNvm_Loc_SortAndFilterAllocationTable(AAT_t* pIn, size_t sizeIn, AAT_t** ppOut, size_t* pSizeOut)
{
	if (pIn == NULL)
	{
		*pSizeOut = 0;
		return;
	}

	if (sizeIn == 0)
	{
		*pSizeOut = 0;
		return;
	}

	AAT_t* pSortedTable = (AAT_t*) malloc(sizeIn*sizeof(AAT_t));
	memcpy(pSortedTable, pIn, sizeIn*sizeof(AAT_t));
	QuickSort(pSortedTable, 0, sizeIn-1);

	AAT_t* pSortedTableAndFiltered = NULL;
	FilterOutInvalidAdresses(pSortedTable, sizeIn, &pSortedTableAndFiltered, pSizeOut);
	free(pSortedTable);

	*ppOut = pSortedTableAndFiltered;
}

/******************************************************************************/
/*                      PRIVATE FUNCTION IMPLEMENTATIONS                      */
/******************************************************************************/
static void FilterOutInvalidAdresses(AAT_t* pIn, size_t sIn, AAT_t** ppOut, size_t* sOut)
{
	int numberOfValidAddresses = GetNumberOfValidAdresses(sIn, pIn);

	AAT_t* pSortedAndFilteredTable = (AAT_t*) malloc(numberOfValidAddresses*sizeof(AAT_t));
	memset(pSortedAndFilteredTable, 0, numberOfValidAddresses*sizeof(AAT_t));

	FilterValidAddresses(sIn, pIn, pSortedAndFilteredTable);

	*ppOut = pSortedAndFilteredTable;
	*sOut = numberOfValidAddresses;
}

static void FilterValidAddresses(size_t sIn, AAT_t* pIn, AAT_t* pSortedAndFilteredTable)
{
	int i = 0;
	int j = 0;
	for (i = 0; i < sIn; i++)
	{
		if (((pIn[i].address * PAGE_SIZE) + pIn[i].offset) > 0)
		{
			pSortedAndFilteredTable[j] = pIn[i];
			j++;
		}
	}
}

static int GetNumberOfValidAdresses(size_t sIn, AAT_t* pIn)
{
	int numberOfValidAddresses = 0;

	int i;
	for (i = 0; i < sIn; i++)
	{
		if (((pIn[i].address * PAGE_SIZE) + pIn[i].offset) > 0)
		{
			numberOfValidAddresses++;
		}
	}

	return numberOfValidAddresses;
}

static void Swap(AAT_t* pA, AAT_t* pB)
{
	AAT_t t = *pA;
    *pA = *pB;
    *pB = t;
}

static int Partition(AAT_t* pArr, int low, int high)
{
	AAT_t pivot = pArr[high];
    int i = (low - 1);

    for (int j = low; j <= high- 1; j++)
    {
        if (((pArr[j].address * PAGE_SIZE) + pArr[j].offset) <= ((pivot.address * PAGE_SIZE)+ pivot.offset))
        {
            i++;
            Swap(&pArr[i], &pArr[j]);
        }
    }
    Swap(&pArr[i + 1], &pArr[high]);
    return (i + 1);
}

static void QuickSort(AAT_t* pArr, int low, int high)
{
    if (low < high)
    {
        int pi = Partition(pArr, low, high);

        QuickSort(pArr, low, pi - 1);
        QuickSort(pArr, pi + 1, high);
    }
}
/******************************************************************************/
/*                                 END OF FILE                                */
/******************************************************************************/



