#ifndef QORVO_NVM_INC_NVM_ATTRIBUTETYPES_H_
#define QORVO_NVM_INC_NVM_ATTRIBUTETYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char            Int8;

typedef unsigned char 		   UInt8;

typedef signed short           Int16;

typedef unsigned short         UInt16;

typedef signed int             Int32;

typedef unsigned int           UInt32;

typedef struct {
	UInt8 id;
	UInt32 options;
	UInt8 length;
	UInt8 data[20];
} gpTestData_t;

#ifdef __cplusplus
}
#endif

#endif /* QORVO_NVM_INC_NVM_ATTRIBUTETYPES_H_ */
