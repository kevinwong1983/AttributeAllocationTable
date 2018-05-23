/******************************************************************************/
/*                               INCLUDE FILES                                */
/******************************************************************************/
#include "nvm_attributeStore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "nvm.h"

#include "crc8.h"

#include "nvm_attributeTypes.h"
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
static bool ExtractTableFromBuffer(UInt8* pBuffer);
static bool IsValidAttrAllocTable(AAT_format_t* pAttrAllocTableFormat);
static bool IsValidAttrAllocTableEntry(gpNvm_AttrId attrId);
static bool IsValidValue(gpNvm_AttrId attrId, UInt8* pValue);
static bool ReadAttrAllocTableFromNvm(UInt8* pBuffer, int startingAddress);
static bool ReadBackupAttrAllocTableFromNvm(UInt8* pBuffer);
static bool ReadMainAttrAllocTableFromNvm(UInt8* pBuffer);
static bool WriteAttrAllocTableToNvm(UInt8* pBuffer, int startingAddress);
static bool WriteBackupAttrAllocTableToNvm(UInt8* pBuffer);
static bool WriteMainAttrAllocTableToNvm(UInt8* pBuffer);
static gpNvm_Result CalculateNextFreeAdressAndOffsetThatFitsNewAttribute(UInt8 length, UInt16* pAddress, UInt8* pOffset);
static gpNvm_Result GetAndValidateAttributeValue(gpNvm_AttrId attrId, UInt8* pLength, UInt8* pValue);
static gpNvm_Result GetAttributeAllocTable();
static gpNvm_Result GetAttributeValue(gpNvm_AttrId attrId, UInt8 *pValue);
static gpNvm_Result ReadBackupAndRestoreMainAttrAllocTable(UInt8* pBuffer);
static gpNvm_Result SetAttributeValue(UInt8 length, UInt16 startOfCurrentAddress, UInt8 startOfCurrentOffset, UInt8* pValue);
static UInt16 GetAdressWithOffset(UInt16 address);
static UInt16 GetAdressWithoutOffset(UInt16 address);
static void ClearAttributeAllocTableEntry(gpNvm_AttrId attrId);
static void UpdateAttrAllocTable(gpNvm_AttrId attrId, UInt16 address, UInt8 offset, UInt8 length, UInt8 crc);
/******************************************************************************/
/*                          PRIVATE DATA DEFINITIONS                          */
/******************************************************************************/
static AAT_t* AttrAllocTable = NULL;
/******************************************************************************/
/*                      PUBLIC FUNCTION IMPLEMENTATIONS                       */
/******************************************************************************/
gpNvm_Result gpNvm_Init()
{
	AttrAllocTable = (AAT_t*) gpNvm_Loc_GetAttrAllocTable();
	memset(AttrAllocTable, 0, (sizeof(AAT_t)*MAX_ATTRIBUTES));
	return GetAttributeAllocTable();
}

gpNvm_Result gpNvm_GetAttribute(gpNvm_AttrId attrId, UInt8* pLength, UInt8* pValue)
{
	gpNvm_Result rv = NOK;

	if ((pLength == NULL) || (pValue == NULL))
	{
		return rv;
	}

	if (IsValidAttrAllocTableEntry(attrId))
	{
		rv = GetAndValidateAttributeValue(attrId, pLength, pValue);
	}

	return rv;
}

gpNvm_Result gpNvm_SetAttribute(gpNvm_AttrId attrId, UInt8 length, UInt8* pValue)
{
	gpNvm_Result rv = NOK;

	UInt8 offset = 0;
	UInt16 address = 0;

	if (pValue == NULL || length == 0)
	{
		return rv;
	}

	ClearAttributeAllocTableEntry(attrId);
	rv = CalculateNextFreeAdressAndOffsetThatFitsNewAttribute(length, &address, &offset);
	if (rv != OK)
	{
		return rv;
	}

	rv = SetAttributeValue(length, address, offset, pValue);
	if (rv != OK)
	{
		return rv;
	}

	UpdateAttrAllocTable(attrId, address, offset, length, Crc8_getCrc(pValue, length));

	return rv;
}

gpNvm_Result gpNvm_DeleteAttribute(gpNvm_AttrId attrId)
{
	ClearAttributeAllocTableEntry(attrId);

	WriteBackupAttrAllocTableToNvm((UInt8*) AttrAllocTable);
	WriteMainAttrAllocTableToNvm((UInt8*) AttrAllocTable);

	return OK;
}
/******************************************************************************/
/*                      PRIVATE FUNCTION IMPLEMENTATIONS                      */
/******************************************************************************/
static void UpdateAttrAllocTable(gpNvm_AttrId attrId, UInt16 address, UInt8 offset, UInt8 length, UInt8 crc)
{
	AttrAllocTable[attrId].address = address;
	AttrAllocTable[attrId].offset = offset;
	AttrAllocTable[attrId].length = length;
	AttrAllocTable[attrId].crc = crc;

	WriteBackupAttrAllocTableToNvm((UInt8*) AttrAllocTable);
	WriteMainAttrAllocTableToNvm((UInt8*) AttrAllocTable);
}

static bool IsValidAttrAllocTableEntry(gpNvm_AttrId attrId)
{
	return ((AttrAllocTable[attrId].address != 0) && (AttrAllocTable[attrId].length != 0));
}

static bool IsValidValue(gpNvm_AttrId attrId, UInt8* pValue)
{
	return (AttrAllocTable[attrId].crc == Crc8_getCrc(pValue, AttrAllocTable[attrId].length));
}

static gpNvm_Result GetAndValidateAttributeValue(gpNvm_AttrId attrId, UInt8* pLength, UInt8* pValue)
{
	gpNvm_Result rv = NOK;

	if (GetAttributeValue(attrId, pValue) == OK)
	{
		if (IsValidValue(attrId, pValue))
		{
			*pLength = AttrAllocTable[attrId].length;
			rv = OK;
		}
	}

	return rv;
}

static gpNvm_Result GetAttributeValue(gpNvm_AttrId attrId, UInt8 *pValue)
{
	gpNvm_Result rv = OK;
	UInt16 addressToRead = AttrAllocTable[attrId].address;
	UInt8 length = AttrAllocTable[attrId].length;

	int i;
	while (length)
	{
		UInt8 page[PAGE_SIZE];
		if (length == AttrAllocTable[attrId].length) 	// first page
		{
			gpNvm_Read(addressToRead, page);
			for (i = AttrAllocTable[attrId].offset; ((i < PAGE_SIZE) && (i < (AttrAllocTable[attrId].offset + AttrAllocTable[attrId].length))); i++)
			{
				*pValue++ = page[i];
				length--;
			}
		}
		else if (length > PAGE_SIZE)					// middle pages
		{
			gpNvm_Read(addressToRead, page);
			for (i = 0; i < PAGE_SIZE; i++)
			{
				*pValue++ = page[i];
				length--;
			}
		}
		else											// last page
		{
			gpNvm_Read(addressToRead, page);
			int bytesLeft = length;
			for (i = 0; i < bytesLeft; i++)
			{
				*pValue++ = page[i];
				length--;
			}
		}
		addressToRead++;
	}

	return rv;
}

static gpNvm_Result SetAttributeValue(UInt8 length, UInt16 address, UInt8 offset, UInt8* pValue)
{
	gpNvm_Result rv = OK;

	int i;
	UInt8 numberOfBytesToBeWritten = length;

	while (numberOfBytesToBeWritten)
	{
		UInt8 page[PAGE_SIZE];
		memset(page, 0, sizeof(page));
		if (numberOfBytesToBeWritten == length)			// first page
		{
			gpNvm_Read(address, page);
			for (i = offset; (i < PAGE_SIZE) && (i < (offset + length)); i++)
			{
				page[i] = *pValue++;
				numberOfBytesToBeWritten--;
			}
			gpNvm_Write(address, page);
		}
		else if (numberOfBytesToBeWritten > PAGE_SIZE)	// middle pages
		{
			for (i = 0; i < PAGE_SIZE; i++)
			{
				page[i] = *pValue++;
				numberOfBytesToBeWritten--;
			}
			gpNvm_Write(address, page);
		}
		else											// last page
		{
			int bytesLeft = numberOfBytesToBeWritten;
			gpNvm_Read(address, page);
			for (i = 0; i < bytesLeft; i++)
			{
				page[i] = *pValue++;
				numberOfBytesToBeWritten--;
			}
			gpNvm_Write(address, page);
		}
		address++;
		offset = 0;
	}

	return rv;
}

static void CalculateAddressAndOffsetFromAbsEndOfPreviousEntry(int absoluteEndOfPreviousEntry, UInt16* pAddress, UInt8* pOffset)
{
	*pOffset = absoluteEndOfPreviousEntry % PAGE_SIZE;
	*pAddress = GetAdressWithOffset(absoluteEndOfPreviousEntry / PAGE_SIZE);
}

static bool WillFit(int startNext, int endCurrent, UInt8 length)
{
	return ((startNext - endCurrent) >= length);
}

static int CalculateAbsoluteStartOfEntry(int index, AAT_t* pAttrAllocTableEntries)
{
	return (GetAdressWithoutOffset(pAttrAllocTableEntries[index].address) * PAGE_SIZE) + pAttrAllocTableEntries[index].offset;
}

static gpNvm_Result CalculateNextFreeAdressAndOffsetThatFitsNewAttribute(UInt8 length, UInt16* pAddress, UInt8* pOffset)
{
	gpNvm_Result rv = NOK;

	AAT_t* pAttrAllocTableEntries = NULL;
	size_t numberOfAttrAllocTableEntries = 0;
	gpNvm_Loc_SortAndFilterAllocationTable(AttrAllocTable, MAX_ATTRIBUTES, &pAttrAllocTableEntries, &numberOfAttrAllocTableEntries);

	int i;
	if (numberOfAttrAllocTableEntries == 0)					// no entries
	{
		CalculateAddressAndOffsetFromAbsEndOfPreviousEntry(0, pAddress, pOffset);
		rv = OK;
	}
	else if (numberOfAttrAllocTableEntries == 1)			// one entry
	{
		int absStartCurrent = CalculateAbsoluteStartOfEntry(0, pAttrAllocTableEntries);
		int absEndCurrent = absStartCurrent + pAttrAllocTableEntries[0].length;

		CalculateAddressAndOffsetFromAbsEndOfPreviousEntry(absEndCurrent, pAddress, pOffset);
		rv = OK;
	}
	else													// >1 entries
	{
		for (i = 0; i < numberOfAttrAllocTableEntries; i++)
		{
			int absStartCurrent = CalculateAbsoluteStartOfEntry(i, pAttrAllocTableEntries);
			int absEndCurrent = absStartCurrent + pAttrAllocTableEntries[i].length;

			int absStartNext = 0;

			if (i == (numberOfAttrAllocTableEntries - 1))	// last entry
			{
				absStartNext = MAX_STORAGE_CAPACITY;
				if (WillFit(absStartNext, absEndCurrent, length))
				{
					CalculateAddressAndOffsetFromAbsEndOfPreviousEntry(absEndCurrent, pAddress, pOffset);
					rv = OK;
					break;
				}
				else
				{
					rv = FULL;
					break;
				}
			}
			else											// !last entry
			{
				absStartNext = CalculateAbsoluteStartOfEntry(i+1, pAttrAllocTableEntries);
				if (WillFit(absStartNext, absEndCurrent, length))
				{
					CalculateAddressAndOffsetFromAbsEndOfPreviousEntry(absEndCurrent, pAddress, pOffset);
					rv = OK;
					break;
				}
			}
		}
	}

	return rv;
}

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
	AttrAllocTable[attrId].address = 0;
	AttrAllocTable[attrId].offset = 0;
	AttrAllocTable[attrId].length = 0;
	AttrAllocTable[attrId].crc = 0;
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

static bool IsValidAttrAllocTable(AAT_format_t* pAttrAllocTableFormat)
{
	return (pAttrAllocTableFormat->crc == Crc8_getCrc((UInt8*) pAttrAllocTableFormat->attrAllocTable, sizeof(pAttrAllocTableFormat->attrAllocTable)));
}

static bool ExtractTableFromBuffer(UInt8* pBuffer)
{
	bool rv = false;
	AAT_format_t attrAllocTableFormat;

	memcpy(&attrAllocTableFormat, pBuffer, sizeof(attrAllocTableFormat));

	if (IsValidAttrAllocTable(&attrAllocTableFormat))
	{
		memcpy(AttrAllocTable, attrAllocTableFormat.attrAllocTable, sizeof(attrAllocTableFormat.attrAllocTable));
		rv = true;
	}

	return rv;
}
/******************************************************************************/
/*                                 END OF FILE                                */
/******************************************************************************/

