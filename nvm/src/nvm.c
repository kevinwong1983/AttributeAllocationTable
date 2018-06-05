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
size_t WriteToFile(int address, void *pPage);
size_t ReadFromFile(int address, void *pPage);

/******************************************************************************/
/*                          PRIVATE DATA DEFINITIONS                          */
/******************************************************************************/
static char Path[PATH_LENGTH];
/******************************************************************************/
/*                      PUBLIC FUNCTION IMPLEMENTATIONS                       */
/******************************************************************************/
void gpNvm_NvmStoragePath(const char *pPath)
{
	strncpy(Path, pPath, strlen(pPath));
}

size_t gpNvm_Write(int address, void *pPage)
{
	return WriteToFile(address, pPage);
}

size_t gpNvm_Read(int address, void *pPage)
{
	return ReadFromFile(address, pPage);
}
/******************************************************************************/
/*                      PRIVATE FUNCTION IMPLEMENTATIONS                      */
/******************************************************************************/
size_t WriteToFile(int address, void *pPage)
{
	if (pPage == NULL)
	{
		return 0;
	}

	FILE *fp = NULL;
	size_t rv = 0;
	char filePath[PATH_LENGTH];

	sprintf(filePath, "%s/%d.bin", Path, address);
	fp = fopen(Path, "w");
	if(fp == NULL)
	{
		fp = freopen(filePath, "w", stderr);
	}

	rv = fwrite(pPage, 1, PAGE_SIZE, fp);
	fclose(fp);

	return rv;
}

size_t ReadFromFile(int address, void *pPage)
{
	if (pPage == NULL)
	{
		return 0;
	}

	FILE *fp = NULL;
	size_t rv = 0;
	char filePath[PATH_LENGTH];

	sprintf(filePath, "%s/%d.bin", Path, address);
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



