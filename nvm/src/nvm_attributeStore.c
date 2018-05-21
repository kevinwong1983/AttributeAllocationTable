/******************************************************************************/
/*                               INCLUDE FILES                                */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "nvm_attributeStore.h"
#include "nvm_attributeTypes.h"
#include "nvm_attributeAllocationTable.h"
#include "nvm.h"

#include "crc8.h"
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
static bool ExtractTableFromBuffer(UInt8* pBuffer);
static bool IsValid(AAT_format_t* pAttrAllocTableFormat);
static bool ReadAttrAllocTableFromNvm(UInt8* pBuffer, int startingAddress);
static bool ReadBackupAttrAllocTableFromNvm(UInt8* pBuffer);
static bool ReadMainAttrAllocTableFromNvm(UInt8* pBuffer);
static bool WriteAttrAllocTableToNvm(UInt8* pBuffer, int startingAddress);
static bool WriteBackupAttrAllocTableToNvm(UInt8* pBuffer);
static bool WriteMainAttrAllocTableToNvm(UInt8* pBuffer);
static gpNvm_Result GetAttributeAllocTable();
static gpNvm_Result ReadBackupAndRestoreMainAttrAllocTable(UInt8* pBuffer);
/******************************************************************************/
/*                          PRIVATE DATA DEFINITIONS                          */
/******************************************************************************/
static AAT_t* attrAllocTable = NULL;
/******************************************************************************/
/*                      PUBLIC FUNCTION IMPLEMENTATIONS                       */
/******************************************************************************/
gpNvm_Result gpNvm_Init()
{
	attrAllocTable = (AAT_t*) gpNvm_Loc_GetAttrAllocTable();
	return GetAttributeAllocTable();
}

gpNvm_Result gpNvm_GetAttribute(gpNvm_AttrId attrId, UInt8* pLength, UInt8* pValue)
{
	gpNvm_Result rv = OK;

	UInt16 currentAdress = attrAllocTable[attrId].address;
	UInt8 numberOfBytesToBeRead = attrAllocTable[attrId].length;

	int i;
	while (numberOfBytesToBeRead)
	{
		UInt8 page[PAGE_SIZE];
		if (numberOfBytesToBeRead == attrAllocTable[attrId].length) 	// firstPage
		{
			gpNvm_Read(currentAdress, page);
			for (i = attrAllocTable[attrId].offset; (i < PAGE_SIZE) && (i < (attrAllocTable[attrId].offset + attrAllocTable[attrId].length)); i++)
			{
				*pValue++ = page[i];
				numberOfBytesToBeRead --;
			}
		}
		else if (numberOfBytesToBeRead > PAGE_SIZE)						// middle pages
		{
			gpNvm_Read(currentAdress, page);
			for (i = 0; i < PAGE_SIZE; i++)
			{
				*pValue++ = page[i];
				numberOfBytesToBeRead --;
			}
		}
		else															// lastPage
		{
			gpNvm_Read(currentAdress, page);
			int bytesLeft = numberOfBytesToBeRead;
			for (i = 0; i < bytesLeft; i++)
			{
				*pValue++ = page[i];
				numberOfBytesToBeRead --;
			}
		}
		currentAdress ++;
	}

	*pLength = attrAllocTable[attrId].length;

	return rv;
}

gpNvm_Result gpNvm_SetAttribute(gpNvm_AttrId attrId, UInt8 length, UInt8* pValue)
{
	gpNvm_Result rv = OK;

	// calculateNextFreeAdressAndOffsetThatFitsNewAttribute()
	AAT_t* pValidAAT = NULL;
	size_t sizeOfValidAAT = 0;
	gpNvm_Loc_SortAndFilterAllocationTable(attrAllocTable, MAX_ATTRIBUTES, &pValidAAT, &sizeOfValidAAT);

	AAT_t AAT_debug[255];
	memset(AAT_debug,0,sizeof(AAT_debug));
	memcpy(AAT_debug, pValidAAT, sizeOfValidAAT*sizeof(AAT_t));

	UInt16 endOfPreviousAddressPage;
	UInt8 endOfPreviousAddressOffset;
	UInt16 startAdress = 0;
	UInt8 startOffset = 0;

	int i;
	if (sizeOfValidAAT == 0)
	{
		//emptyAAT, thus empty Flash
		endOfPreviousAddressOffset = 0;
		endOfPreviousAddressPage = NVM_OFFSET + AAT_DATA_OFFSET;
	}
	else if (sizeOfValidAAT == 1)
	{
		endOfPreviousAddressOffset = (pValidAAT[0].offset + pValidAAT[0].length % PAGE_SIZE) % PAGE_SIZE;
		endOfPreviousAddressPage = (((pValidAAT[0].address - (NVM_OFFSET + AAT_DATA_OFFSET)) * PAGE_SIZE) + (pValidAAT[0].length / PAGE_SIZE)) + (endOfPreviousAddressOffset/PAGE_SIZE) + NVM_OFFSET + AAT_DATA_OFFSET;
	}
	else
	{
		for (i = 0; i <= (sizeOfValidAAT); i++)
		{
			int endOfCurrentAttribute = ((pValidAAT[i].address - (NVM_OFFSET + AAT_DATA_OFFSET)) * PAGE_SIZE) + pValidAAT[i].offset + pValidAAT[i].length;
			int beginOfNextAttribute = (i>sizeOfValidAAT)?(PAGE_SIZE*NUMBER_OF_PAGES):((pValidAAT[i+1].address - (NVM_OFFSET + AAT_DATA_OFFSET)) * PAGE_SIZE) + pValidAAT[i+1].offset;
			if (beginOfNextAttribute - endOfCurrentAttribute >= length)
			{
				endOfPreviousAddressOffset = (pValidAAT[i].offset + pValidAAT[i].length % PAGE_SIZE) % PAGE_SIZE;
				endOfPreviousAddressPage = (((pValidAAT[i].address - (NVM_OFFSET + AAT_DATA_OFFSET)) * PAGE_SIZE) + (pValidAAT[i].length / PAGE_SIZE)) + (endOfPreviousAddressOffset/PAGE_SIZE)  + NVM_OFFSET + AAT_DATA_OFFSET;;
				break;
			}
		}
	}

	if (endOfPreviousAddressOffset == PAGE_SIZE-1)//on the edge of the page
	{
		startOffset = 0;
		startAdress = endOfPreviousAddressPage + 1;
	}
	else
	{
		startOffset = endOfPreviousAddressOffset;
		startAdress = endOfPreviousAddressPage;
	}

	UInt8 crc = Crc8_getCrc(pValue, length);

	// Write()
	UInt8 currentOffset = startOffset;
	UInt16 currentAdress = startAdress;
	UInt8 numberOfBytesToBeWritten = length;
	while (numberOfBytesToBeWritten)
	{
		UInt8 page[PAGE_SIZE];
		memset(page, 0, sizeof(page));
		if (numberOfBytesToBeWritten == length)
		{
			gpNvm_Read(currentAdress, page);
			for (i = currentOffset; (i < PAGE_SIZE) && (i < (currentOffset + length)); i++)
			{
				page[currentOffset] = *pValue++;
				numberOfBytesToBeWritten --;
			}
			gpNvm_Write(currentAdress, page);
		}
		else if (numberOfBytesToBeWritten > PAGE_SIZE)
		{
			for (i = 0; i < PAGE_SIZE; i++)
			{
				page[i] = *pValue++;
				numberOfBytesToBeWritten --;
			}
			gpNvm_Write(currentAdress, page);
		}
		else
		{
			int bytesLeft = numberOfBytesToBeWritten;
			for (i = 0; i < bytesLeft; i++)
			{
				gpNvm_Read(currentAdress, page);
				page[i] = *pValue++;
				numberOfBytesToBeWritten --;
			}
			gpNvm_Write(currentAdress, page);
		}
		currentAdress ++;
		currentOffset = 0;
	}

	//update()
	attrAllocTable[attrId].address = startAdress;
	attrAllocTable[attrId].offset = startOffset;
	attrAllocTable[attrId].length = length;
	attrAllocTable[attrId].crc = crc;

    WriteBackupAttrAllocTableToNvm((UInt8*) attrAllocTable);
	WriteMainAttrAllocTableToNvm((UInt8*) attrAllocTable);

	return rv;
}

/******************************************************************************/
/*                      PRIVATE FUNCTION IMPLEMENTATIONS                      */
/******************************************************************************/
static gpNvm_Result GetAttributeAllocTable()
{
	gpNvm_Result rv = NOK;

	UInt8 buffer[PAGE_SIZE * AAT_NUMBER_OF_PAGES];
	memset(buffer, 0, sizeof(buffer));

	if (ReadMainAttrAllocTableFromNvm((UInt8*) buffer))
	{
		if (ExtractTableFromBuffer(buffer))
		{
			rv = OK;
		}
		else
		{
			memset(buffer, 0, sizeof(buffer));
			rv = ReadBackupAndRestoreMainAttrAllocTable(buffer);
		}
	}
	else
	{
		memset(buffer, 0, sizeof(buffer));
		rv = ReadBackupAndRestoreMainAttrAllocTable(buffer);
	}

	return rv;
}

static gpNvm_Result ReadBackupAndRestoreMainAttrAllocTable(UInt8* pBuffer)
{
	gpNvm_Result rv = NOK;

	if (ReadBackupAttrAllocTableFromNvm(pBuffer))
	{
		if (ExtractTableFromBuffer(pBuffer))
		{
			WriteMainAttrAllocTableToNvm(pBuffer);
			rv = OK;
		}
	}

	return rv;
}

static bool ReadAttrAllocTableFromNvm(UInt8* pBuffer, int startingAddress)
{
	int totalBytesRead = 0;
	int i;

	for (i = startingAddress; i < (startingAddress + AAT_NUMBER_OF_PAGES); i++)
	{
		totalBytesRead += gpNvm_Read(NVM_OFFSET + i, pBuffer);
		pBuffer += PAGE_SIZE;
	}

	return (totalBytesRead == (PAGE_SIZE * AAT_NUMBER_OF_PAGES));
}

static bool WriteAttrAllocTableToNvm(UInt8* pBuffer, int startingAddress)
{
	int totalBytesRead = 0;
	int i;

	for (i = startingAddress; i < (startingAddress + AAT_NUMBER_OF_PAGES); i++)
	{
		totalBytesRead += gpNvm_Write(NVM_OFFSET + i, pBuffer);
		pBuffer += PAGE_SIZE;
	}

	return (totalBytesRead == (PAGE_SIZE * AAT_NUMBER_OF_PAGES));
}

static bool WriteBackupAttrAllocTableToNvm(UInt8* pBuffer)
{
	return WriteAttrAllocTableToNvm(pBuffer, AAT_BACKUP_FIRST_PAGE);
}

static bool WriteMainAttrAllocTableToNvm(UInt8* pBuffer)
{
	return WriteAttrAllocTableToNvm(pBuffer, AAT_MAIN_FIRST_PAGE);
}

static bool ReadMainAttrAllocTableFromNvm(UInt8* pBuffer)
{
	return ReadAttrAllocTableFromNvm(pBuffer, AAT_MAIN_FIRST_PAGE);
}

static bool ReadBackupAttrAllocTableFromNvm(UInt8* pBuffer)
{
	return ReadAttrAllocTableFromNvm(pBuffer, AAT_BACKUP_FIRST_PAGE);
}

static bool IsValid(AAT_format_t* pAttrAllocTableFormat)
{
	return (pAttrAllocTableFormat->crc == Crc8_getCrc((UInt8*) pAttrAllocTableFormat->attrAllocTable, sizeof(pAttrAllocTableFormat->attrAllocTable)));
}

static bool ExtractTableFromBuffer(UInt8* pBuffer)
{
	bool rv = false;
	AAT_format_t attrAllocTableFormat;

	memcpy(&attrAllocTableFormat, pBuffer, sizeof(attrAllocTableFormat));

	if (IsValid(&attrAllocTableFormat))
	{
		memcpy(attrAllocTable, attrAllocTableFormat.attrAllocTable, sizeof(attrAllocTable));
		rv = true;
	}

	return rv;
}
/******************************************************************************/
/*                                 END OF FILE                                */
/******************************************************************************/

