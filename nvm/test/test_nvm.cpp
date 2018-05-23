#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include "gtest/gtest.h"

#include "nvm.h"
#include "nvm_stub.h"
#include "../../nvm_attribute_store/inc/nvm_attributeStore.h"
#include "../../nvm_attribute_store/inc/nvm_attributeTypes.h"

class nvm: public ::testing::Test {
public:
   void SetUp( ) {
		gpNvm_NvmStoragePath(testDataPath);
   }

   void TearDown( ) {
	   cleanUpTestData(testDataPath);
   }

   void cleanUpTestData(const char *path)
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
private:
   const char *testDataPath = "./test_data";
};

TEST_F(nvm, GivenNoWrite_WhenRead_ThenReturnZero)
{
	char data_to_read[PAGE_SIZE];
	memset(data_to_read, 0, sizeof(data_to_read));
	EXPECT_EQ(0, gpNvm_Read(1, &data_to_read));
}

TEST_F(nvm, WhenWriteNullPointerData_ThenReturnZero)
{
	EXPECT_EQ(0, gpNvm_Write(1, NULL));
}

TEST_F(nvm, GivenWritenDataToAdress_WhenReadFromAdress_ThenDataIsTheSameAndReturnIsPageSize)
{
	char data_to_write[PAGE_SIZE] = {1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4};
	char data_to_read[PAGE_SIZE];
	memset(data_to_read, 0, sizeof(data_to_read));
	EXPECT_EQ(PAGE_SIZE, gpNvm_Write(1, &data_to_write));
	EXPECT_EQ(PAGE_SIZE, gpNvm_Read(1, &data_to_read));

	EXPECT_TRUE(0 == memcmp(data_to_write, data_to_read, PAGE_SIZE));
}

TEST_F(nvm, GivenWritenDataToAdress_WhenReadMultripleTimesFromAdress_ThenDataIsTheSameAndReturnIsPageSize)
{
	char data_to_write[PAGE_SIZE] = {1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4,5,6,7,8,9,'a',1,2,3,4};
	char data_to_read[PAGE_SIZE];
	EXPECT_EQ(PAGE_SIZE, gpNvm_Write(1, &data_to_write));

	memset(data_to_read, 0, sizeof(data_to_read));
	EXPECT_EQ(PAGE_SIZE, gpNvm_Read(1, &data_to_read));
	EXPECT_TRUE(0 == memcmp(data_to_write, data_to_read, PAGE_SIZE));

	memset(data_to_read, 0, sizeof(data_to_read));
	EXPECT_EQ(PAGE_SIZE, gpNvm_Read(1, &data_to_read));
	EXPECT_TRUE(0 == memcmp(data_to_write, data_to_read, PAGE_SIZE));

	memset(data_to_read, 0, sizeof(data_to_read));
	EXPECT_EQ(PAGE_SIZE, gpNvm_Read(1, &data_to_read));
	EXPECT_TRUE(0 == memcmp(data_to_write, data_to_read, PAGE_SIZE));
}

