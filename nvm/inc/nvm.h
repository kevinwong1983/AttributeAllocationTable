#ifndef NVM_INC_NVM_H_
#define NVM_INC_NVM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define PAGE_SIZE				64		// bytes per non-volatile memory page

#define NUMBER_OF_PAGES			0x400

#define NVM_OFFSET				0x1000	// offset address to the non-volatile memory

size_t gpNvm_Write(int address, void *pData);

size_t gpNvm_Read(int address, void *pData);

#ifdef __cplusplus
}
#endif

#endif /* NVM_INC_NVM_H_ */
