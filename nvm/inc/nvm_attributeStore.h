/**
@startuml FunctionalRequirment

note as N1
<b>Functional requirement NVM component should:
* be able to backup and restore values
corresponding to a unique attribute identifier.
* support different datatypes (UInt8, UInt32, Other structs...)
* support arrays of the datatypes (in assignment
it only states UInt8, however I have implemented
as for all datatypes)
* discovers corruption in the underlying storage.
* recover from corruption in the underlying
storage.
end note

@enduml

@startuml DesignAndAssumption
rectangle memory[

<b>NVM layout
----
page0 AAT(page 1of2)
----
page1 AAT(page 2of2)
----
page2 AAT-backup(page 1of2)
----
page3 AAT-backup(page 2of2)
----
page4
----
page5
====
...
====
page n-1 data
----
page n data
]

note top of memory
<b>Assumptions of the NVM:
* The non-volatile memory consist of n pages
of 64 bytes each. For now lets say n=1024
resulting in NVM size of 64bytesx1024pages=64Kbytes.
* NVM address starts with in offset of
0x1000. Thus the pages are accessible
using address 0x1000 till 0x1400.
* reading NVM address 0x1000, reads the first
NVM page of 64 bytes
* reading NVM address 0x1399, reads the last
NVM page of 64 bytes
end note

note right of memory
<b>Design decisions
Taken the Assumptions of NVM into account:
* Each value can be stored, dependent on the
length, in one (length<64 bytes) or
more (length>64) pages in NVM.
* <b>minimum storage space allocation per attribute
Each value associated to a unique attribute id
is store on a separate page. This has the disadvantage
that a value of length 1 byte will allocate 64 bytes.
However had the advantage that it saves a whole lot
of complexity. In the future this can be optimized.
* <b>maximum storage space allocation per attribute
An Attribute value length is limited to a Uint8 value thus 255
However for future changes the nvm component is designed
so that can be as big  as the NVM minus
the AAT tables e.g. almost an uint8 array of 64Kbytes.
 However your NVM is then full and the attribute needs
 to be deleted before you can store new attribute values.
* A 8 bytes CRC implementation is
used from https://users.ece.cmu.edu/~koopman/roses/dsn04/
koopman04_crc_poly_embedded.pdf sourcecode can be found
here: https://stackoverflow.com/questions/15169387/definitive-crc-for-c
* The init, getter and setter will be Mutex locked to prevent
access to the same data at the same time by different
threads. This will prevent data corruption.
For this implementation a stub will be used.
end note

together structs{
    class format_AAT_pages{
      AAT[255]
      CRC
    }

    class AAT-element{
      address:2bytes = address of page
      offset:1bytes = offset of page
      length:1bytes = size in bytes of data stored
      CRC:1byte
    }
}

note top of format_AAT_pages
<b>The Attribute Allocation Table (AAT)
is stored in pages 1-20 of
the NVM. This table defines
the memory format/layout of the rest
of the NVM. It consist of a array of
Attribute Allocation elements and a CRC
over the whole Table.

Data integrity of the AAT is very important.
<b>If AAT is corrupted in anyway, all the data in
NVM is lost. In order to prevent this redundancy
is added. An exact copy
of the AAT is stored in page 21 and 40 of the NVM.
We will call this <b>"backupAAT"
The backupAAT is synced to the main AAT and
only used when main AAT is corrupted.
end note

note top of AAT-element
<b>Each element of the AAT array represent data on NVM
that corresponds to a attribute Identifier. This Id is
the index of the AAT.
@enduml

@startuml gpNvm_Init

database gpNvm
participant CrcUtil
participant gpAttrStore
participant Application
participant MutexUtil

Application -> gpAttrStore : gpNvm_Init()
loop for each AAT_NvmAdress in which AAT is stored
	gpAttrStore <-> gpNvm : Read(AAT_NvmAdress, Buffer[..]))
end
gpAttrStore -> gpAttrStore: extract AAT from Buffer

gpAttrStore -> CrcUtil : getCrc(AAT, sizeof(AAT))
alt CrcUtil OK
	gpAttrStore -> Application : return OK
else
 	note right of gpAttrStore
	Here NVM has been corrupted
	and read in redundancy backup AAT.
	end note
	loop for each AAT_Backup_NvmAdress of where AAT_Backup is stored
		gpAttrStore <-> gpNvm : Read(AAT_Backup_NvmAdress, Buffer[..]))
	end
	gpAttrStore -> gpAttrStore: extract AAT from Buffer
	gpAttrStore -> CrcUtil : getCrc(AAT, sizeof(AAT))
	alt CrcUtil OK
		loop for each AAT_NvmAdress in which AAT is stored
			gpAttrStore -> gpNvm : Write(AAT_NvmAdress, Buffer[..])
		end
		note right of gpAttrStore
		Also store correct AAT in
		AAT_NvmAdress, overwriting the
		corrupted AAT
		end note
		gpAttrStore -> Application : return OK
	else
		gpAttrStore -> Application : return NOK
	    note right of gpAttrStore
		Here we NVM has corrupted
		and redundancy has failed.
		end note
	end
end
@enduml

@startuml gpNvm_GetAttribute

database gpNvm
participant CrcUtil
participant gpAttrStore
participant Application
participant MutexUtil

Application -> gpAttrStore : gpNvm_GetAttribute(id, &length, &value);
	loop foreach element in AAT
	 	note right of gpAttrStore
		loop through all elements of AAT
		until it finds element with same
		attribute Id
		end note
		alt element.attrId == id
			gpAttrStore <-> gpNvm: Read(element.adress, &value, element.size);
			gpAttrStore <-> gpAttrStore: &length = element.size
			gpAttrStore -> CrcUtil : getCrc(&value, element.size);
			alt CrcUtil OK
				gpAttrStore -> Application : return OK
			else
				gpAttrStore -> Application : return NOK
			end
		end
	end
@enduml

@startuml gpNvm_SetAttribute

database gpNvm
participant CrcUtil
participant gpAttrStore
participant Application
participant MutexUtil

Application -> gpAttrStore : gpNvm_SetAttribute(id, length, &value);
	note right of gpAttrStore
		loop through AAT to find first
		available adress that fits the
		lenght of data you want to store.
	end note
	group calculateNextFreeAdressAndOffsetThatFitsNewAttribute()
		gpAttrStore -> gpAttrStore: sortedAAT = sortByAdress(AAT)
		note right: from lowest to highst adress
		loop foreach element in sortedAAT
			alt element.next.startOfAttributeInBytes - element.startOfAttributeInBytes >= length
				note left of gpAttrStore
					startOfAttributeInBytes = (adress x pagesize) + offset
				end note
				gpAttrStore -> gpAttrStore: element
			end
		end
		gpAttrStore -> gpAttrStore: startAdress = getEndAdress(element.adress, element.offset);
		gpAttrStore -> gpAttrStore: startOffset = getEndOffset(element.adress, element.offset);
	end

	group Write(startAdress, startOffset, &value, length)
		note left of gpAttrStore
			need to be carefull not to erase
			data from other attribute in the first
			and last page
		end note
		alt adress == firstPage
			gpAttrStore -> gpAttrStore: adress = startAdress
			gpAttrStore -> gpAttrStore: Read(adress, Buffer))
			gpAttrStore -> gpAttrStore: WriteBuffer(startOffset, Buffer, pValue, PAGE)
			gpAttrStore -> gpAttrStore: Write((startAdress, Buffer))
		else adress == middle
			gpAttrStore -> gpAttrStore: WriteBuffer(adress, Buffer)
		else adress endAdress
			gpAttrStore -> gpAttrStore: Read(adress, Buffer))
			gpAttrStore -> gpAttrStore: WriteBuffer(0, Buffer, pValue, PAGE)
			gpAttrStore -> gpAttrStore: Write((startAdress, Buffer))
		end
	end

	group update Attribute Allocation Table
		gpAttrStore -> CrcUtil : CRC = getCrc(&value, length);
		gpAttrStore -> gpAttrStore : AAT[id].address = startAdress
		gpAttrStore -> gpAttrStore : AAT[id].size = length
		gpAttrStore -> gpAttrStore : AAT[id].crc = CRC
		gpAttrStore -> gpNvm: Write(AAT_NvmAdress, AAT, sizeof(AAT))
		gpAttrStore -> gpNvm: Write(AAT_backup, AAT, sizeof(AAT))
	end

	note left of gpAttrStore
		update the Attribute Allocation
		Table with the new entry
	end note
	gpAttrStore -> Application : Return OK

@enduml

@enduml
*/

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
};

typedef UInt8 gpNvm_AttrId;

typedef UInt8 gpNvm_Result;

gpNvm_Result gpNvm_Init();

gpNvm_Result gpNvm_GetAttribute(gpNvm_AttrId attrId, UInt8* pLength, UInt8* pValue);

gpNvm_Result gpNvm_SetAttribute(gpNvm_AttrId attrId, UInt8 length, UInt8* pValue);

#ifdef __cplusplus
}
#endif

#endif /* QORVO_NVM_INC_NVM_ATTRIBUTESTORE_H_ */
