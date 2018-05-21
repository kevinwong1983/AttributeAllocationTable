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
static UInt16 GetAdressWithOffset(UInt16 address);
static UInt16 GetAdressWithoutOffset(UInt16 address);
static void ClearAttributeAllocTableEntry(gpNvm_AttrId attrId);
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
	memset(attrAllocTable, 0, (sizeof(AAT_t)*MAX_ATTRIBUTES));
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
	UInt8 startOfCurrentOffset = 0;
	UInt16 startOfCurrentAddress = 0;

	ClearAttributeAllocTableEntry(attrId);

	// calculateNextFreeAdressAndOffsetThatFitsNewAttribute()
	{
		AAT_t* pValidAAT = NULL;
		size_t sizeOfValidAAT = 0;
		gpNvm_Loc_SortAndFilterAllocationTable(attrAllocTable, MAX_ATTRIBUTES, &pValidAAT, &sizeOfValidAAT);

		AAT_t AAT_debug[255];
		memset(AAT_debug,0,sizeof(AAT_debug));
		memcpy(AAT_debug, pValidAAT, sizeOfValidAAT*sizeof(AAT_t));

		UInt16 endOfPreviousAddressPage;
		UInt8 endOfPreviousAddressOffset;

		int i;
		if (sizeOfValidAAT == 0)
		{
			endOfPreviousAddressOffset = 0;
			endOfPreviousAddressPage = GetAdressWithOffset(0);
		}
		else if (sizeOfValidAAT == 1)
		{
			int abs_startCurrent = (GetAdressWithoutOffset(pValidAAT[0].address) * PAGE_SIZE) + pValidAAT[0].offset;
			int abs_endCurrent = abs_startCurrent + pValidAAT[0].length;
			endOfPreviousAddressOffset = abs_endCurrent % PAGE_SIZE;
			endOfPreviousAddressPage = GetAdressWithOffset(abs_endCurrent / PAGE_SIZE);
		}
		else
		{
			for (i = 0; i < sizeOfValidAAT; i++)
			{
				int abs_startCurrent = (GetAdressWithoutOffset(pValidAAT[i].address) * PAGE_SIZE) + pValidAAT[i].offset;
				int abs_endCurrent = abs_startCurrent + pValidAAT[i].length;

				int abs_startNext = 0;

				if (i == sizeOfValidAAT-1)
				{
					abs_startNext = PAGE_SIZE * (NUMBER_OF_PAGES-AAT_DATA_OFFSET);
					if (abs_startNext - abs_endCurrent >= length)
					{
						endOfPreviousAddressOffset = abs_endCurrent % PAGE_SIZE;
						endOfPreviousAddressPage =  GetAdressWithOffset(abs_endCurrent / PAGE_SIZE);
						break;
					}
					else
					{
						return FULL;
					}
				}
				else
				{
					abs_startNext = (GetAdressWithoutOffset(pValidAAT[i+1].address) * PAGE_SIZE) + pValidAAT[i+1].offset;
					if (abs_startNext - abs_endCurrent >= length)
					{
						endOfPreviousAddressOffset = abs_endCurrent % PAGE_SIZE;
						endOfPreviousAddressPage =  GetAdressWithOffset(abs_endCurrent / PAGE_SIZE);
						break;
					}
				}
			}
		}

		startOfCurrentOffset = endOfPreviousAddressOffset;
		startOfCurrentAddress = endOfPreviousAddressPage;
	}

	// calculate CRC
	UInt8 crc = Crc8_getCrc(pValue, length);

	// Write()
	{
		int i;
		UInt8 currentOffset = startOfCurrentOffset;
		UInt16 currentAdress = startOfCurrentAddress;
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
					page[i] = *pValue++;
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
	}

	//update()
	{
		attrAllocTable[attrId].address = startOfCurrentAddress;
		attrAllocTable[attrId].offset = startOfCurrentOffset;
		attrAllocTable[attrId].length = length;
		attrAllocTable[attrId].crc = crc;

		WriteBackupAttrAllocTableToNvm((UInt8*) attrAllocTable);
		WriteMainAttrAllocTableToNvm((UInt8*) attrAllocTable);
	}

	return rv;
}

gpNvm_Result gpNvm_DeleteAttribute(gpNvm_AttrId attrId)
{
	ClearAttributeAllocTableEntry(attrId);

	return OK;
}
/******************************************************************************/
/*                      PRIVATE FUNCTION IMPLEMENTATIONS                      */
/******************************************************************************/
static UInt16 GetAdressWithoutOffset(UInt16 address)
{
	return (address - (NVM_OFFSET + AAT_DATA_OFFSET));
}

static UInt16 GetAdressWithOffset(UInt16 address)
{
	return (address + NVM_OFFSET + AAT_DATA_OFFSET);
}

static void ClearAttributeAllocTableEntry(gpNvm_AttrId attrId)
{
	attrAllocTable[attrId].address = 0;
	attrAllocTable[attrId].offset = 0;
	attrAllocTable[attrId].length = 0;
	attrAllocTable[attrId].crc = 0;
}

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

