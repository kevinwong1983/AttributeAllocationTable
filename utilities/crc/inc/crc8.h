#ifndef UTILITIES_CRC_INC_CRC8_H_
#define UTILITIES_CRC_INC_CRC8_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "nvm_attributeTypes.h"

UInt8 Crc8_getCrc(UInt8 const *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UTILITIES_CRC_INC_CRC8_H_ */
