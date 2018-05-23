#ifndef NVM_INC_NVM_ATTRIBUTEALLOCATIONTABLE_H_
#define NVM_INC_NVM_ATTRIBUTEALLOCATIONTABLE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "../../nvm_attribute_store/inc/nvm_attributeTypes.h"
#include "../../nvm_attribute_store/inc/nvm_attributeStore.h"

typedef struct
{
	UInt16 address;
	UInt8 offset;
	UInt8 length;
	UInt8 crc;
}__attribute__((packed, aligned(1))) AAT_t;

typedef struct
{
	AAT_t attrAllocTable[MAX_ATTRIBUTES];
	UInt8 crc;
} AAT_format_t;

AAT_t* gpNvm_Loc_GetAttrAllocTable();

void gpNvm_Loc_SortAndFilterAllocationTable(AAT_t* pIn, size_t sIn, AAT_t** ppOut, size_t* sOut);

#ifdef __cplusplus
}
#endif


#endif /* NVM_INC_NVM_ATTRIBUTEALLOCATIONTABLE_H_ */
