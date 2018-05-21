#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include "gtest/gtest.h"

#include "nvm_attributeAllocationTable.h"
#include "nvm_attributeStore.h"
#include "nvm_attributeTypes.h"
#include "nvm.h"
#include "crc8.h"

#include "nvm_stub.h"

class storage: public ::testing::Test {
public:
	AAT_t DebugAat[MAX_ATTRIBUTES];

	void SetUp( ) {
	    gpNvm_Stub_SetTestDataPath(testDataPath);
	    CleanUpTestData(testDataPath);
	}

	void TearDown( ) {
	   CleanUpTestData(testDataPath);
	}

	void CleanUpTestData(const char *path)
	{
		DIR *folder = opendir(path);
		struct dirent *next_file;
		char filepath[256];

		while ((next_file = readdir(folder)) != NULL )
		{
			sprintf(filepath, "%s/%s", path, next_file->d_name);
			remove(filepath);
		}

		closedir(folder);
	}

	void SetupAttrAllocTabletInNvm(AAT_format_t attrAllocTableFormat, int startingAdressPage)
	{
		int i;
		UInt8* pAAT = (UInt8*) (&attrAllocTableFormat);
		for (i = startingAdressPage; i < (startingAdressPage + AAT_NUMBER_OF_PAGES); i++)
		{
			UInt8 buffer[PAGE_SIZE];
			memset(buffer, 0, sizeof(buffer));
			memcpy(buffer, pAAT, PAGE_SIZE);
			pAAT += PAGE_SIZE;
			gpNvm_Write(NVM_OFFSET+i, buffer);
		}
	}

    AAT_format_t SetupMainAttrAllocTabletInNvm(bool corrupted)
	{
    	AAT_format_t attrAllocTableFormat;
    	memset(&attrAllocTableFormat, 0, sizeof(attrAllocTableFormat));
    	attrAllocTableFormat.crc = Crc8_getCrc((UInt8*) attrAllocTableFormat.attrAllocTable, sizeof(attrAllocTableFormat.attrAllocTable)) + (corrupted?1:0);

    	SetupAttrAllocTabletInNvm(attrAllocTableFormat, AAT_MAIN_FIRST_PAGE);
    	return attrAllocTableFormat;
	}

    AAT_format_t SetupBackupAttrAllocTabletInNvm(bool corrupted)
	{
    	AAT_format_t backupAttrAllocTableFormat;
    	memset(&backupAttrAllocTableFormat, 0, sizeof(backupAttrAllocTableFormat));
    	backupAttrAllocTableFormat.crc = Crc8_getCrc((UInt8*) backupAttrAllocTableFormat.attrAllocTable, sizeof(backupAttrAllocTableFormat.attrAllocTable)) + (corrupted?1:0);

    	SetupAttrAllocTabletInNvm(backupAttrAllocTableFormat, AAT_BACKUP_FIRST_PAGE);
    	return backupAttrAllocTableFormat;
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

    static bool ReadMainAttrAllocTableFromNvm(UInt8* pBuffer)
    {
    	return ReadAttrAllocTableFromNvm(pBuffer, AAT_MAIN_FIRST_PAGE);
    }

    void getAttrAllocTable()
	{
    	AAT_t* pDebugAat = gpNvm_Loc_GetAttrAllocTable();
    	memcpy(DebugAat, pDebugAat, sizeof(DebugAat));
	}

    AAT_t getAttributeInfo(UInt8 AttributeId)
	{
    	getAttrAllocTable();
    	return DebugAat[AttributeId];
	}

    UInt16 getAdressWithOffset(UInt16 address)
    {
    	return (address + NVM_OFFSET + AAT_DATA_OFFSET);
    }

    void ExpectAttributeAllocInfo(UInt16 attributeId, UInt16 address, UInt8 offset, UInt8 length, UInt8 crc)
    {
    	AAT_t AttributeInfo = getAttributeInfo(attributeId);
		EXPECT_EQ(getAdressWithOffset(address), AttributeInfo.address);
		EXPECT_EQ(offset, AttributeInfo.offset);
		EXPECT_EQ(length, AttributeInfo.length);
		EXPECT_EQ(crc, AttributeInfo.crc);
    }

	void ExpectDataAndLengthOfAttributeId(UInt16 attributeId, UInt8 length, UInt8* pData_to_set)
	{
		UInt8 data_to_get[255];
		memset(data_to_get, 0, 255);
		UInt8 length_to_get = 0;

		EXPECT_EQ(OK, gpNvm_GetAttribute(attributeId, &length_to_get, data_to_get));
		EXPECT_EQ(length, length_to_get);
		EXPECT_TRUE(0 == memcmp(pData_to_set, data_to_get, length));
	}

    gpNvm_Result SetupEmptyAat()
    {
    	SetupMainAttrAllocTabletInNvm(false);
    	SetupBackupAttrAllocTabletInNvm(false);
    	return gpNvm_Init();
    }

private:

	const char *testDataPath = "./nvm/test/test_data";
};

TEST_F(storage, GivenNoAttributeAllocTables_whenInit_ThenRetunsNOK)
{
	EXPECT_EQ(NOK, gpNvm_Init());
}

TEST_F(storage, GivenValidMainAttributeAllocTable_whenInit_ThenRetunsOK)
{
	SetupMainAttrAllocTabletInNvm(false);

	EXPECT_EQ(OK, gpNvm_Init());
}

TEST_F(storage, GivenNoMainAndValidBackupAttributeAllocTable_whenInit_ThenRetunsOKAndMainTableRestored)
{
	AAT_format_t backupAttrAllocTableFormat = SetupBackupAttrAllocTabletInNvm(false);
	AAT_format_t mainAttrAllocTableFormat;
	memset(&mainAttrAllocTableFormat, 0, sizeof(mainAttrAllocTableFormat));

	EXPECT_EQ(OK, gpNvm_Init());
	EXPECT_EQ(true, ReadMainAttrAllocTableFromNvm((UInt8*) &mainAttrAllocTableFormat));
	EXPECT_TRUE(0 == memcmp(&backupAttrAllocTableFormat, &mainAttrAllocTableFormat, sizeof(backupAttrAllocTableFormat)));
}

TEST_F(storage, GivenNoMainAndCorruptedBackupAttributeAllocTable_whenInit_ThenRetunsNOK)
{
	SetupBackupAttrAllocTabletInNvm(true);

	EXPECT_EQ(NOK, gpNvm_Init());
}

TEST_F(storage, GivenCorruptedMainNoBackupAttributeAllocTable_whenInit_ThenRetunsNOK)
{
	SetupMainAttrAllocTabletInNvm(true);

	EXPECT_EQ(NOK, gpNvm_Init());
}

TEST_F(storage, GivenCorruptedMainAndValidBackupAttributeAllocTable_whenInit_ThenRetunsOKAndMainTableRestored)
{
	SetupMainAttrAllocTabletInNvm(true);
	AAT_format_t backupAttrAllocTableFormat = SetupBackupAttrAllocTabletInNvm(false);

	AAT_format_t mainAttrAllocTableFormat;
	memset(&mainAttrAllocTableFormat, 0, sizeof(mainAttrAllocTableFormat));

	EXPECT_EQ(OK, gpNvm_Init());
	EXPECT_EQ(true, ReadMainAttrAllocTableFromNvm((UInt8*) &mainAttrAllocTableFormat));
	EXPECT_TRUE(0 == memcmp(&backupAttrAllocTableFormat, &mainAttrAllocTableFormat, sizeof(backupAttrAllocTableFormat)));
}

TEST_F(storage, GivenCorruptedMainAndCorruptedBackupAttributeAllocTable_whenInit_ThenRetunsNOK)
{
	SetupMainAttrAllocTabletInNvm(true);
	SetupBackupAttrAllocTabletInNvm(true);

	EXPECT_EQ(NOK, gpNvm_Init());
}

TEST_F(storage, GivenEmptyAat_WhenGetAttributeThatDoesNotExist_ThenShouldReturnNok)
{
	EXPECT_EQ(OK, SetupEmptyAat());

	UInt8 data_to_get[255];
	memset(data_to_get, 0, 255);
	UInt8 length_to_get = 0;

	gpNvm_AttrId unknownAttributeId = 42;
	EXPECT_EQ(NOK, gpNvm_GetAttribute(unknownAttributeId, &length_to_get, data_to_get));
}

TEST_F(storage, GivenEmptyAat_WhenSetAttributeOfOneByte_ThenAdministrationInTableShouldReferenceToAttribute)
{
	EXPECT_EQ(OK, SetupEmptyAat());

	UInt8 data_to_set = 'a';
	UInt8 length_to_set  = 1;
	UInt8 attributeId = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(attributeId, length_to_set, &data_to_set));
	ExpectAttributeAllocInfo(attributeId, 0, 0, length_to_set, Crc8_getCrc(&data_to_set, length_to_set));
}

TEST_F(storage, GivenEmptyAat_WhenSetAndGetAttributeOfOneByte_ThenValueShouldBeTheSame)
{
	EXPECT_EQ(OK, SetupEmptyAat());

	UInt8 data_to_set = 'a';
	UInt8 length_to_set  = 1;

	UInt8 attributeId = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(attributeId, length_to_set, &data_to_set));
	ExpectDataAndLengthOfAttributeId(attributeId, length_to_set, &data_to_set);
	ExpectAttributeAllocInfo(attributeId, 0, 0, length_to_set, Crc8_getCrc(&data_to_set, length_to_set));
}

TEST_F(storage, GivenAttributeSetInNvm_WhenReuingAttribbuteId_ThenOldDataIsOverwritten)
{
	// given attribute set in Nvm
	EXPECT_EQ(OK, SetupEmptyAat());
	UInt8 attributeId = 1;
	UInt8 data_to_set1 = 'a';
	UInt8 length_to_set1  = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(attributeId, length_to_set1, &data_to_set1));
	ExpectDataAndLengthOfAttributeId(attributeId, length_to_set1, &data_to_set1);
	ExpectAttributeAllocInfo(attributeId, 0, 0, length_to_set1, Crc8_getCrc(&data_to_set1, length_to_set1));

	// when reusing attribute Id to set new data
	UInt8 data_to_set2 = 'b';
	UInt8 length_to_set2  = 1;
	EXPECT_EQ(OK, gpNvm_SetAttribute(attributeId, length_to_set2, &data_to_set2));

	// then old data is overwritten with new data.
	ExpectDataAndLengthOfAttributeId(attributeId, length_to_set2, &data_to_set2);
	ExpectAttributeAllocInfo(attributeId, 0, 0, length_to_set2, Crc8_getCrc(&data_to_set2, length_to_set2));
}

TEST_F(storage, GivenNonEmptyAat_WhenSetAndGetAttributeOfOneByte_ThenValueShouldBeTheSame)
{
	EXPECT_EQ(OK, SetupEmptyAat());

	UInt8 data_to_set1 = 'a';
	UInt8 data_to_set2 = 'b';
	UInt8 data_to_set3 = 'c';
	UInt8 data_to_set4 = 'd';

	UInt8 length_to_set  = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(1, length_to_set, &data_to_set1));
	EXPECT_EQ(OK, gpNvm_SetAttribute(2, length_to_set, &data_to_set2));
	EXPECT_EQ(OK, gpNvm_SetAttribute(3, length_to_set, &data_to_set3));
	EXPECT_EQ(OK, gpNvm_SetAttribute(4, length_to_set, &data_to_set4));

	int i = 10;
	while (i > 0)
	{
		ExpectDataAndLengthOfAttributeId(1, length_to_set, &data_to_set1);
		ExpectDataAndLengthOfAttributeId(2, length_to_set, &data_to_set2);
		ExpectDataAndLengthOfAttributeId(3, length_to_set, &data_to_set3);
		ExpectDataAndLengthOfAttributeId(4, length_to_set, &data_to_set4);
		i--;
	}

	ExpectAttributeAllocInfo(1, 0, 0, length_to_set, Crc8_getCrc(&data_to_set1, length_to_set));
	ExpectAttributeAllocInfo(2, 0, 1, length_to_set, Crc8_getCrc(&data_to_set2, length_to_set));
	ExpectAttributeAllocInfo(3, 0, 2, length_to_set, Crc8_getCrc(&data_to_set3, length_to_set));
	ExpectAttributeAllocInfo(4, 0, 3, length_to_set, Crc8_getCrc(&data_to_set4, length_to_set));
}

TEST_F(storage, GivenAttributeOfOneByteDeleted_WhenSetNewAttributeOfOneByte_ThenNewAttributeWillBeWrittenInDeletedAddress)
{
	// Given attribute of one byte deleted
	EXPECT_EQ(OK, SetupEmptyAat());

	UInt8 data_to_set1 = 'a';
	UInt8 data_to_set2 = 'b';
	UInt8 data_to_set3 = 'c';
	UInt8 data_to_set4 = 'd';
	UInt8 data_to_set5 = 'e';

	UInt8 length_to_set  = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(1, length_to_set, &data_to_set1));
	EXPECT_EQ(OK, gpNvm_SetAttribute(2, length_to_set, &data_to_set2));
	EXPECT_EQ(OK, gpNvm_SetAttribute(3, length_to_set, &data_to_set3));
	EXPECT_EQ(OK, gpNvm_SetAttribute(4, length_to_set, &data_to_set4));

	ExpectDataAndLengthOfAttributeId(1, length_to_set, &data_to_set1);
	ExpectDataAndLengthOfAttributeId(2, length_to_set, &data_to_set2);
	ExpectDataAndLengthOfAttributeId(3, length_to_set, &data_to_set3);
	ExpectDataAndLengthOfAttributeId(4, length_to_set, &data_to_set4);

	ExpectAttributeAllocInfo(1, 0, 0, length_to_set, Crc8_getCrc(&data_to_set1, length_to_set));
	ExpectAttributeAllocInfo(2, 0, 1, length_to_set, Crc8_getCrc(&data_to_set2, length_to_set));
	ExpectAttributeAllocInfo(3, 0, 2, length_to_set, Crc8_getCrc(&data_to_set3, length_to_set));
	ExpectAttributeAllocInfo(4, 0, 3, length_to_set, Crc8_getCrc(&data_to_set4, length_to_set));

	gpNvm_DeleteAttribute(3);

	// When set new attribute of one byte
	EXPECT_EQ(OK, gpNvm_SetAttribute(5, length_to_set, &data_to_set5));

	// Then new attribute will be written in deleted address
	ExpectDataAndLengthOfAttributeId(5, length_to_set, &data_to_set5);
	ExpectAttributeAllocInfo(5, 0, 2, length_to_set, Crc8_getCrc(&data_to_set5, length_to_set));
}

TEST_F(storage, GivenNonEmptyAat_WhenSetAndGetAttributeOfOneByteArray_ThenValueShouldBeTheSame)
{
	EXPECT_EQ(OK, SetupEmptyAat());

	UInt8 data_to_set[PAGE_SIZE] = "1234567890abcdefghijklmnopqrstuvwxyz1234567890abcdefghijklmnopq";
	UInt8 length_to_set  = PAGE_SIZE;
	UInt8 attributeId = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(attributeId, length_to_set, data_to_set));

	ExpectDataAndLengthOfAttributeId(attributeId, length_to_set,  data_to_set);
	ExpectAttributeAllocInfo(attributeId, 0, 0, length_to_set, Crc8_getCrc(data_to_set, length_to_set));
}

TEST_F(storage, GivenNonEmptyAat_WhenSetAndGetAttributeOfMultipleByteArrayOfPageSize_ThenValuesShouldBeTheSame)
{
	// given non empty AAT
	EXPECT_EQ(OK, SetupEmptyAat());
	UInt8 data_to_set1[PAGE_SIZE] = "1234567890abcdefghijklmnopqrstuvwxyz1234567890abcdefghijklmnopq";
	UInt8 length_to_set1  = PAGE_SIZE;

	EXPECT_EQ(OK, gpNvm_SetAttribute(1, length_to_set1, data_to_set1));
	ExpectDataAndLengthOfAttributeId(1, length_to_set1,  data_to_set1);
	ExpectAttributeAllocInfo(1, 0, 0, length_to_set1, Crc8_getCrc(data_to_set1, length_to_set1));

	// when set array of page size on attributeId 2
	UInt8 data_to_set2[PAGE_SIZE] = "!@#$%^&*()abcdefghijklmnopqrstuvwxyz!@#$%^&*()abcdefghijklmnopq";
	UInt8 length_to_set2  = PAGE_SIZE;
	EXPECT_EQ(OK, gpNvm_SetAttribute(2, length_to_set2, data_to_set2));

	// then a get on attributeId 2 should give set data
	ExpectDataAndLengthOfAttributeId(2, length_to_set2,  data_to_set2);

	// then a get on attributeId 2 should give set data
	ExpectAttributeAllocInfo(2, 1, 0, length_to_set2, Crc8_getCrc(data_to_set2, length_to_set2));
}

TEST_F(storage, GivenNonEmptyAat_WhenSetAndGetAttributeOfMultipleByteArrayOfLagerThenSize_ThenValuesShouldBeTheSame)
{
	EXPECT_EQ(OK, SetupEmptyAat());
	UInt8 data_to_set[255] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345601234";
	UInt8 length_to_set = 255;

	EXPECT_EQ(OK, gpNvm_SetAttribute(1, length_to_set, data_to_set));
	ExpectDataAndLengthOfAttributeId(1, length_to_set,  data_to_set);
	ExpectAttributeAllocInfo(1, 0, 0, length_to_set, Crc8_getCrc(data_to_set, length_to_set));

	EXPECT_EQ(OK, gpNvm_SetAttribute(2, length_to_set, data_to_set));
	ExpectDataAndLengthOfAttributeId(2, length_to_set,  data_to_set);
	ExpectAttributeAllocInfo(2, 3, 63, length_to_set, Crc8_getCrc(data_to_set, length_to_set));

	EXPECT_EQ(OK, gpNvm_SetAttribute(3, length_to_set, data_to_set));
	ExpectDataAndLengthOfAttributeId(3, length_to_set,  data_to_set);
	ExpectAttributeAllocInfo(3, 7, 62, length_to_set, Crc8_getCrc(data_to_set, length_to_set));

	EXPECT_EQ(OK, gpNvm_SetAttribute(4, length_to_set, data_to_set));
	ExpectDataAndLengthOfAttributeId(4, length_to_set,  data_to_set);
	ExpectAttributeAllocInfo(4, 11, 61, length_to_set, Crc8_getCrc(data_to_set, length_to_set));
}

TEST_F(storage, GivenGivenFullNvm_WhenSetNewDataThatWillNotFit_ThenNvmFullErrorIsReturned)
{
	// Given given full nvm
	EXPECT_EQ(OK, SetupEmptyAat());
	UInt8 data_to_set[255] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345601234";
	UInt8 length_to_set = 255;

	EXPECT_EQ(length_to_set, sizeof(data_to_set));
	int i;
	for (i = 0; i< 246; i++) //capacity of nvm = 64*(1024-40) = 62976 number of bytes; 62976/255 = 246.9; 246 time 255 byte write will still fit;
	{
		EXPECT_EQ(OK, gpNvm_SetAttribute(i, length_to_set, data_to_set));
		ExpectDataAndLengthOfAttributeId(i, length_to_set,  data_to_set);
	}

	// When set new data that will not fit
	// Then nvm full error is returned
	EXPECT_EQ(FULL, gpNvm_SetAttribute(i, length_to_set, data_to_set));	 //247th of 255 byte write nvm is full
}



