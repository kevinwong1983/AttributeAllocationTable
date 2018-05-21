#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include "gtest/gtest.h"

#include "nvm_attributeAllocationTable.h"
#include "nvm_attributeStore.h"
#include "nvm_attributeTypes.h"
#include "nvm.h"

#include "nvm_stub.h"

class AAT: public ::testing::Test {
public:
   void SetUp( ) {
   }

   void TearDown( ) {
   }

private:
};

bool SortByAddress(const AAT_t &lhs, const AAT_t &rhs)
{
	return lhs.address < rhs.address;
}

TEST_F(AAT, GivenUnsortedAAT_WhenSort_ThenRetunsSortedTable)
{
	AAT_t attrAllocTable[10] = {{55,0,0,0},{3,0,0,0},{8,0,0,0},{1,0,0,0},{13,0,0,0},{17,0,0,0},{10,0,0,0},{11,0,0,0},{123,0,0,0},{32,0,0,0}};
	size_t sizeIn = 10;
	AAT_t* pAttrAllocTableOut = NULL;
	size_t sizeOut = 0;

	gpNvm_Loc_SortAndFilterAllocationTable(attrAllocTable, sizeIn, &pAttrAllocTableOut, &sizeOut);

	std::sort(&attrAllocTable[0], &attrAllocTable[sizeIn], SortByAddress);
	EXPECT_TRUE(0 == memcmp(attrAllocTable, pAttrAllocTableOut, sizeof(attrAllocTable)));
	EXPECT_EQ(sizeIn, sizeOut);

	free(pAttrAllocTableOut);
}

TEST_F(AAT, GivenLargeUnsortedAAT_WhenSort_ThenRetunsSortedTable)
{
	size_t sizeIn = 1000;
	AAT_t* pAttrAllocTable =(AAT_t*) malloc(sizeIn*sizeof(AAT_t));
	memset(pAttrAllocTable, 0, sizeIn*sizeof(AAT_t));
	int i;
	for (i=0; i<(int)sizeIn; i++)
	{
		pAttrAllocTable[i].address = random()%123;
		if (pAttrAllocTable[i].address == 0)
		{
			pAttrAllocTable[i].address++;
		}
	}

	AAT_t* pAttrAllocTableOut = NULL;
	size_t sizeOut = 0;

	gpNvm_Loc_SortAndFilterAllocationTable(pAttrAllocTable, sizeIn, &pAttrAllocTableOut, &sizeOut);

	std::sort(&pAttrAllocTable[0], &pAttrAllocTable[sizeIn], SortByAddress);

	EXPECT_TRUE(0 == memcmp(pAttrAllocTable, pAttrAllocTableOut, sizeIn*sizeof(AAT_t)));
	EXPECT_EQ(sizeIn, sizeOut);

	free(pAttrAllocTable);
	free(pAttrAllocTableOut);
}

TEST_F(AAT, GivenUnsortedAATWithAttrIdZero_WhenSort_ThenRetunsSortedTableWithAttrIdZeroFilteredOut)
{
	AAT_t attrAllocTable[10] = {{0,0,0,0},{0,0,0,0},{8,0,0,0},{1,0,0,0},{13,0,0,0},{17,0,0,0},{10,0,0,0},{0,0,0,0},{123,0,0,0},{32,0,0,0}};
	size_t sizeIn = 10;

	AAT_t expecetedAttrAllocTable[7] = {{1,0,0,0},{8,0,0,0},{10,0,0,0},{13,0,0,0},{17,0,0,0},{32,0,0,0},{123,0,0,0}};
	size_t expecetedSizeIn = 7;

	AAT_t* pAttrAllocTableOut = NULL;
	size_t sizeOut = 0;

	gpNvm_Loc_SortAndFilterAllocationTable(attrAllocTable, sizeIn, &pAttrAllocTableOut, &sizeOut);

	EXPECT_TRUE(0 == memcmp(expecetedAttrAllocTable, pAttrAllocTableOut, sizeof(expecetedAttrAllocTable)));
	EXPECT_EQ(expecetedSizeIn, sizeOut);

	free(pAttrAllocTableOut);
}

TEST_F(AAT, GivenUnsortedAATWithOffset_WhenSort_ThenRetunsSortedTable)
{
	AAT_t attrAllocTable[5] = {{1,2,0,0},{1,1,0,0},{0,1,0,0},{1,0,0,0},{0,0,0,0}};
	size_t sizeIn = 5;

	AAT_t expecetedAttrAllocTable[4] = {{0,1,0,0},{1,0,0,0},{1,1,0,0},{1,2,0,0}};
	size_t expecetedSizeIn = 4;

	AAT_t* pAttrAllocTableOut = NULL;
	size_t sizeOut = 0;

	gpNvm_Loc_SortAndFilterAllocationTable(attrAllocTable, sizeIn, &pAttrAllocTableOut, &sizeOut);

	EXPECT_TRUE(0 == memcmp(expecetedAttrAllocTable, pAttrAllocTableOut, sizeof(expecetedAttrAllocTable)));
	EXPECT_EQ(expecetedSizeIn, sizeOut);

	free(pAttrAllocTableOut);
}

TEST_F(AAT, GivenATWithOnlyAttrIdZero_WhenSort_ThenRetunsZeroSizeTable)
{
	AAT_t attrAllocTable[10] = {{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}};
	size_t sizeIn = 10;

	size_t expecetedSizeIn = 0;

	AAT_t* pAttrAllocTableOut = NULL;
	size_t sizeOut = 0;

	gpNvm_Loc_SortAndFilterAllocationTable(attrAllocTable, sizeIn, &pAttrAllocTableOut, &sizeOut);
	EXPECT_EQ(expecetedSizeIn, sizeOut);

	free(pAttrAllocTableOut);
}

TEST_F(AAT, GivenNullPointerAAT_WhenSort_ThenRetunsZeroSizeTable)
{
	size_t expecetedSizeIn = 0;
	AAT_t* pAttrAllocTableOut = NULL;
	size_t sizeOut = 321;

	gpNvm_Loc_SortAndFilterAllocationTable(NULL, 123, &pAttrAllocTableOut, &sizeOut);
	EXPECT_EQ(expecetedSizeIn, sizeOut);

	free(pAttrAllocTableOut);
}

TEST_F(AAT, GivenSizeZeroInput_WhenSort_ThenRetunsZeroSizeTable)
{
	size_t expecetedSizeIn = 0;
	AAT_t* pAttrAllocTableOut = NULL;
	size_t sizeOut = 321;

	gpNvm_Loc_SortAndFilterAllocationTable(NULL, 0, &pAttrAllocTableOut, &sizeOut);
	EXPECT_EQ(expecetedSizeIn, sizeOut);

	free(pAttrAllocTableOut);
}
