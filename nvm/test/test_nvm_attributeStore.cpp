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

TEST_F(storage, GivenEmptyAat_WhenSetAttributeOfOneByte_ThenAdministrationInTableShouldReferenceToAttribute)
{
	SetupMainAttrAllocTabletInNvm(false);
	SetupBackupAttrAllocTabletInNvm(false);
	EXPECT_EQ(OK, gpNvm_Init());

	UInt8 data_to_set = 'a';
	UInt8 length_to_set  = 1;
	UInt8 attributeId = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(attributeId, length_to_set, &data_to_set));
	ExpectAttributeAllocInfo(attributeId, 0, 0, length_to_set, Crc8_getCrc(&data_to_set, length_to_set));
}

TEST_F(storage, GivenEmptyAat_WhenSetAndGetAttributeOfOneByte_ThenValueShouldBeTheSame)
{
	SetupMainAttrAllocTabletInNvm(false);
	SetupBackupAttrAllocTabletInNvm(false);
	EXPECT_EQ(OK, gpNvm_Init());

	UInt8 data_to_set = 'a';
	UInt8 length_to_set  = 1;

	UInt8 data_to_get = 0;
	UInt8 length_to_get = 0;

	UInt8 attributeId = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(attributeId, length_to_set, &data_to_set));
	EXPECT_EQ(OK, gpNvm_GetAttribute(attributeId, &length_to_get, &data_to_get));

	EXPECT_EQ(data_to_set, data_to_get);
	ExpectAttributeAllocInfo(attributeId, 0, 0, length_to_set, Crc8_getCrc(&data_to_set, length_to_set));
}

TEST_F(storage, GivenNonEmptyAat_WhenSetAndGetAttributeOfOneByte_ThenValueShouldBeTheSame)
{
	SetupMainAttrAllocTabletInNvm(false);
	SetupBackupAttrAllocTabletInNvm(false);
	EXPECT_EQ(OK, gpNvm_Init());

	UInt8 data_to_set1 = 'a';
	UInt8 data_to_set2 = 'b';
	UInt8 data_to_set3 = 'c';
	UInt8 data_to_set4 = 'd';

	UInt8 length_to_set  = 1;

	UInt8 data_to_get = 0;
	UInt8 length_to_get = 0;

	EXPECT_EQ(OK, gpNvm_SetAttribute(1, length_to_set, &data_to_set1));
	EXPECT_EQ(OK, gpNvm_SetAttribute(2, length_to_set, &data_to_set2));
	EXPECT_EQ(OK, gpNvm_SetAttribute(3, length_to_set, &data_to_set3));
	EXPECT_EQ(OK, gpNvm_SetAttribute(4, length_to_set, &data_to_set4));

    data_to_get = 0;
    length_to_get = 0;
	EXPECT_EQ(OK, gpNvm_GetAttribute(1, &length_to_get, &data_to_get));
	EXPECT_EQ(1, length_to_get);
	EXPECT_EQ(data_to_set1, data_to_get);

	data_to_get = 0;
	length_to_get = 0;
	EXPECT_EQ(OK, gpNvm_GetAttribute(2, &length_to_get, &data_to_get));
	EXPECT_EQ(1, length_to_get);
	EXPECT_EQ(data_to_set2, data_to_get);

    data_to_get = 0;
    length_to_get = 0;
	EXPECT_EQ(OK, gpNvm_GetAttribute(3, &length_to_get, &data_to_get));
	EXPECT_EQ(1, length_to_get);
	EXPECT_EQ(data_to_set3, data_to_get);

	data_to_get = 0;
	length_to_get = 0;
	EXPECT_EQ(OK, gpNvm_GetAttribute(4, &length_to_get, &data_to_get));
	EXPECT_EQ(1, length_to_get);
	EXPECT_EQ(data_to_set4, data_to_get);

	ExpectAttributeAllocInfo(1, 0, 0, length_to_set, Crc8_getCrc(&data_to_set1, length_to_set));
	ExpectAttributeAllocInfo(2, 0, 1, length_to_set, Crc8_getCrc(&data_to_set2, length_to_set));
	ExpectAttributeAllocInfo(3, 0, 2, length_to_set, Crc8_getCrc(&data_to_set3, length_to_set));
	ExpectAttributeAllocInfo(4, 0, 3, length_to_set, Crc8_getCrc(&data_to_set4, length_to_set));
}

TEST_F(storage, GivenNonEmptyAat_WhenSetAndGetAttributeOfByteArray_ThenValueShouldBeTheSame)
{
	SetupMainAttrAllocTabletInNvm(false);
	SetupBackupAttrAllocTabletInNvm(false);
	EXPECT_EQ(OK, gpNvm_Init());

	UInt8 data_to_set[64] = 'a';
	UInt8 length_to_set  = 1;

	UInt8 data_to_get = 0;
	UInt8 length_to_get = 0;

	UInt8 attributeId = 1;

	EXPECT_EQ(OK, gpNvm_SetAttribute(attributeId, length_to_set, &data_to_set));
	EXPECT_EQ(OK, gpNvm_GetAttribute(attributeId, &length_to_get, &data_to_get));

	EXPECT_EQ(data_to_set, data_to_get);
	ExpectAttributeAllocInfo(attributeId, 0, 0, length_to_set, Crc8_getCrc(&data_to_set, length_to_set));
}

