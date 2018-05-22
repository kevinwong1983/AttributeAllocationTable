#ifndef QORVO_NVM_INC_NVM_ATTRIBUTESTORE_H_
#define QORVO_NVM_INC_NVM_ATTRIBUTESTORE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "nvm_attributeTypes.h"

#define MAX_ATTRIBUTES					255
#define AAT_MAIN_FIRST_PAGE				0
#define AAT_BACKUP_FIRST_PAGE			20
#define AAT_NUMBER_OF_PAGES				20
#define AAT_DATA_OFFSET 				(AAT_BACKUP_FIRST_PAGE + AAT_NUMBER_OF_PAGES)

enum
{
	OK = 0,
	NOK,
	FULL,
};

typedef UInt8 gpNvm_AttrId;

typedef UInt8 gpNvm_Result;

gpNvm_Result gpNvm_Init();

gpNvm_Result gpNvm_GetAttribute(gpNvm_AttrId attrId, UInt8* pLength, UInt8* pValue);

gpNvm_Result gpNvm_SetAttribute(gpNvm_AttrId attrId, UInt8 length, UInt8* pValue);

gpNvm_Result gpNvm_DeleteAttribute(gpNvm_AttrId attrId);

#ifdef __cplusplus
}
#endif

#endif /* QORVO_NVM_INC_NVM_ATTRIBUTESTORE_H_ */
