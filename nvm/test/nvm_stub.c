/******************************************************************************/
/*                               INCLUDE FILES                                */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nvm.h"
#include "nvm_stub.h"
/******************************************************************************/
/*                            CONSTANT DEFINITIONS                            */
/******************************************************************************/

/******************************************************************************/
/*                              MACRO DEFINITIONS                             */
/******************************************************************************/
#define PATH_LENGTH				256
/******************************************************************************/
/*                              TYPE DEFINITIONS                              */
/******************************************************************************/

/******************************************************************************/
/*                          PUBLIC DATA DEFINITIONS                           */
/******************************************************************************/

/******************************************************************************/
/*                        PRIVATE FUNCTION PROTOTYPES                         */
/******************************************************************************/
size_t WriteToFile(int adress, void *pPage);
size_t ReadFromFile(int adress, void *pPage);

/******************************************************************************/
/*                          PRIVATE DATA DEFINITIONS                          */
/******************************************************************************/
static char Path[PATH_LENGTH];
static size_t NvmPageSize = PAGE_SIZE;
static int NumberOfWriteCalled;
static int NumberOfReadCalled;
/******************************************************************************/
/*                      PUBLIC FUNCTION IMPLEMENTATIONS                       */
/******************************************************************************/
void gpNvm_Stub_SetTestDataPath(const char *pPath)
{
	strncpy(Path, pPath, strlen(pPath));
}

void gpNvm_Stub_SetNvmPageSize(size_t size)
{
	NvmPageSize = size;
}

void gpNvm_Stub_ClearNumberOfWriteCalled()
{
	NumberOfWriteCalled = 0;
}

void gpNvm_Stub_ClearNumberOfReadCalled()
{
	NumberOfReadCalled = 0;
}

int gpNvm_Stub_GetNumberOfWriteCalled()
{
	return NumberOfWriteCalled;
}

int gpNvm_Stub_GetNumberOfReadCalled()
{
	return NumberOfReadCalled;
}

size_t gpNvm_Write(int adress, void *pPage)
{
	NumberOfWriteCalled++;

	return WriteToFile(adress, pPage);
}

size_t gpNvm_Read(int adress, void *pPage)
{
	NumberOfReadCalled++;

	return ReadFromFile(adress, pPage);
}
/******************************************************************************/
/*                      PRIVATE FUNCTION IMPLEMENTATIONS                      */
/******************************************************************************/
size_t WriteToFile(int adress, void *pPage)
{
	if (pPage == NULL)
	{
		return 0;
	}

	FILE *fp = NULL;
	size_t rv = 0;
	char filePath[PATH_LENGTH];

	sprintf(filePath, "%s/%d.bin", Path, adress);
	fp = fopen(Path, "w");
	if(fp == NULL)
	{
		fp = freopen(filePath, "w", stderr);
	}

	rv = fwrite(pPage, 1, PAGE_SIZE, fp);
	fclose(fp);

	return rv;
}

size_t ReadFromFile(int adress, void *pPage)
{
	if (pPage == NULL)
	{
		return 0;
	}

	FILE *fp = NULL;
	size_t rv = 0;
	char filePath[PATH_LENGTH];

	sprintf(filePath, "%s/%d.bin", Path, adress);
	fp = fopen(filePath, "r");
	if(fp == NULL)
	{
		return 0;
	}

	rv = fread(pPage, 1, PAGE_SIZE, fp);
	fclose(fp);

	return rv;
}
/******************************************************************************/
/*                                 END OF FILE                                */
/******************************************************************************/



