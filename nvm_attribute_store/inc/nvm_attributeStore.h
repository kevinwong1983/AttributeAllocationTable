#ifndef NVM_INC_NVM_ATTRIBUTESTORE_H_
#define NVM_INC_NVM_ATTRIBUTESTORE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "nvm_attributeTypes.h"

#define AAT_MAIN_FIRST_PAGE				0
#define AAT_BACKUP_FIRST_PAGE			20
#define AAT_NUMBER_OF_PAGES				20
#define AAT_DATA_OFFSET 				(AAT_BACKUP_FIRST_PAGE + AAT_NUMBER_OF_PAGES)

#define MAX_ATTRIBUTES					255
#define MAX_STORAGE_CAPACITY	  		(PAGE_SIZE * (NUMBER_OF_PAGES - AAT_DATA_OFFSET))

enum
{
	OK = 0,
	NOK,
	FULL,
};

typedef UInt8 gpNvm_AttrId;

typedef UInt8 gpNvm_Result;

/**
 * Initialize non-volitile memory component
 */
gpNvm_Result gpNvm_Init();

/**
 * Get attribute value from non-volitile memory.
 * @param[in] 	attrId		Attribute id.
 * @param[out] 	pLength		Value length of attribute.
 * @param[out] 	pValue		Pointer to value buffer.
 * @return 		OK
 * 				NOK
 */
gpNvm_Result gpNvm_GetAttribute(gpNvm_AttrId attrId, UInt8* pLength, UInt8* pValue);

/**
 * Set attribute value to non-volitile memory.
 * @param[in] 	attrId		Attribute id.
 * @param[in] 	length		Value length of attribute.
 * @param[out] 	pValue		Pointer to value buffer.
 * @return 		OK
 * 				NOK
 * 				FULL		non-volitile memory full
 */
gpNvm_Result gpNvm_SetAttribute(gpNvm_AttrId attrId, UInt8 length, UInt8* pValue);

/**
 * Delete attribute value to non-volitile memory.
 * @param[in] 	attrId		Attribute id.
 * @param[in] 	length		Value length of attribute.
 * @param[out] 	pValue		Pointer to value buffer.
 * @return 		OK
 * 				NOK
 */
gpNvm_Result gpNvm_DeleteAttribute(gpNvm_AttrId attrId);

#ifdef __cplusplus
}
#endif

#endif /* NVM_INC_NVM_ATTRIBUTESTORE_H_ */
