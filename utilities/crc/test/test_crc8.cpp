#include <stdio.h>
#include <dirent.h>
#include <nvm_attributeStore.h>
#include <nvm_attributeTypes.h>
#include <string.h>
#include <stdlib.h>
#include "gtest/gtest.h"

#include "nvm_attributeTypes.h"
#include "crc8.h"

class crc: public ::testing::Test {
public:
   void SetUp( ) {
   }

   void TearDown( ) {
   }
};

TEST_F(crc, GivenSameDataSet_ReturnSameCrc)
{
	UInt8 data[] = "HelloWorld1234567890";

	UInt8 crc1 = Crc8_getCrc(data, sizeof(data));
	UInt8 crc2 = Crc8_getCrc(data, sizeof(data));
	EXPECT_EQ(crc1, crc2);
}

TEST_F(crc, GivenDifferentDataset_ReturnDifferentCrc)
{
	UInt8 data1[] = "HelloWorld1234567890";
	UInt8 data2[] = "HelloWorld!234567890";

	UInt8 crc1 = Crc8_getCrc(data1, sizeof(data1));
	UInt8 crc2 = Crc8_getCrc(data2, sizeof(data2));
	EXPECT_NE(crc1, crc2);
}

TEST_F(crc, GivenNullAsDataSet_ReturnsZero)
{
	UInt8 crc = Crc8_getCrc(NULL, 20);
	EXPECT_EQ(0, crc);
}
