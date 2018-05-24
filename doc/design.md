/**
@startuml FunctionalRequirment

note as N1
<b>Functional requirement NVM component should:
* be able to backup and restore values
corresponding to a unique attribute identifier.
* support different datatypes (UInt8, UInt32, Other structs...)
* support arrays of the datatypes.
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
	resulting in NVM size of 64bytesx1024pages = 64Kbytes = 65536bytes.
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
	is one byte. Thus it is possible to store 64 attributes
	of each one byte in one page.
	* <b>maximum storage space allocation per attribute
	An Attribute value length is limited to a UInt8 value .Thus
	a length of 255. Also the Attribute Id is limited to a UInt8.
	Thus Attribute Id between 0 and 255 are supported.
	We can have a max of 255 attribute of 255 bytes each =
	65025 bytes.
	* A 8 bytes CRC implementation is
	used from https://users.ece.cmu.edu/~koopman/roses/dsn04/
	koopman04_crc_poly_embedded.pdf sourcecode can be found
	here: https://stackoverflow.com/questions/15169387/definitive-crc-for-c
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
	* To keep an administration on data that is stored 
	in the nvm, we make use of a so called Attribute
	Allocation Table. This Table is loosely based on
	a very light weight File Allocation Table (FAT) 
	that is used to administrate files on a hard drive. 
	We use the AAT to administrate attributes that are
	written to the nvm.
	* The AAT is stored in pages 1-20 of
	the NVM. This table defines
	the memory format/layout of the rest
	of the NVM. It consist of a array of
	Attribute Allocation elements and a CRC
	over the whole Table.
	* Data integrity of the AAT is very important.
	<b>If AAT is corrupted in anyway, all the data in
	NVM is lost. In order to prevent this,redundancy
	is added. An exact copy
	of the AAT is stored in page 21 and 40 of the NVM.
	We will call this <b>"backupAAT"
	The backupAAT is synced to the main AAT and
	only used when main AAT is corrupted.
	* The AAT tables are overhead of the storage
	capacity. We still have 1024 - 40 = 984 pages for storage.
	984 pages x 64bytes = 62976 bytes. This mean that when
	we want to write the max supported bytes of 65025,
	the NVM will get full.
end note

note top of AAT-element
	<b>Each element of the AAT represent data on NVM
	that corresponds to a attribute Identifier. This Id is
	the index of the AAT.
end note
@enduml

@startuml FutureImprovements

note as N1
	<b>Future Improvements
	* The init, getter and setter will be Mutex locked to prevent
	access to the same data at the same time by different
	threads. This will prevent data corruption.
	* At this moment the nvm component is tested on a
	semi unit-test/integration test level. In the future
	these levels should be split up into a unit-test level tests
	(Sported by gtest and gmock) and integration level tests
	using a underlying file structure to emulate non-volatile
	memory storage.
	* After after many Sets, Get and Deletes the attributes will be
	fragmented over the nvm. It is then possible that the nvm is
	full while there is space between the attributes. To cope with this
	an de-fegmentation action needs to be preformed. Free space is filled
	with attributes that can fit in them.   
end note

@enduml

@startuml gpNvm_Init
note left of gpAttrStore
	Initializing the attribute storage component by reading 
	in the AAT that is stored in nmv. An backup AAT is available
	in case the main AAT is currupted.
end note

database gpNvm
participant CrcUtil
participant gpAttrStore
participant Application

Application -> gpAttrStore : gpNvm_Init()
loop for each AAT_NvmAdress in which AAT is stored
	gpAttrStore <-> gpNvm : Read(AAT_NvmAdress, Buffer[..]))
end
gpAttrStore -> gpAttrStore: extract AAT from Buffer

gpAttrStore <-> CrcUtil : getCrc(AAT, sizeof(AAT))
alt CrcUtil OK
	gpAttrStore -> Application : return OK
else
 	note left of gpAttrStore
	Here NVM has been corrupted
	and read in redundancy backup AAT.
	end note
	loop for each AAT_Backup_NvmAdress of where AAT_Backup is stored
		gpAttrStore <-> gpNvm : Read(AAT_Backup_NvmAdress, Buffer[..]))
	end
	gpAttrStore -> gpAttrStore: extract AAT from Buffer
	gpAttrStore <-> CrcUtil : getCrc(AAT, sizeof(AAT))
	alt CrcUtil OK
		loop for each AAT_NvmAdress in which AAT is stored
			gpAttrStore -> gpNvm : Write(AAT_NvmAdress, Buffer[..])
		end
		note left of gpAttrStore
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
note left of gpAttrStore
	Gets the attribute data stored in nvm with a 
	attribute id.
end note

database gpNvm
participant CrcUtil
participant gpAttrStore
participant Application

Application -> gpAttrStore : gpNvm_GetAttribute(id, &length, &value);
 	note right of gpAttrStore
		The attribute Id is used as an index of the 
		AAT.  
	end note
	gpAttrStore -> gpAttrStore : AAT_t entry = AttrAllocTable[id]
	group ReadAttribute(startAdress, &pValue, &pLength)
		gpAttrStore -> gpAttrStore: address = entry.startAdress
		alt address == firstPage
			gpAttrStore -> gpAttrStore: Read(address, Buffer)
			gpAttrStore -> gpAttrStore: copyFromBufferToValuePointer(startOffset, entry.length || PAGE_SIZE, pValue)
		else address == middle
			gpAttrStore -> gpAttrStore:  Read(address, Buffer)
			gpAttrStore -> gpAttrStore: copyFromBufferToValuePointer(0, PAGE_SIZE, pValue)
		else address == endAdress
			gpAttrStore -> gpAttrStore: Read(address, Buffer))
			gpAttrStore -> gpAttrStore: copyFromBufferToValuePointer(0, bytes_left, pValue)
		end
	end
	gpAttrStore <-> CrcUtil: crc = getCrc(&pValue, length))
	alt crc == entry.crc
		gpAttrStore -> Application: OK
	else 
		gpAttrStore -> Application: NOK
	end
@enduml

@startuml gpNvm_SetAttribute
note left of gpAttrStore
	Sets the attribute data in nvm with a 
	attribute id and a length.
end note

database gpNvm
participant CrcUtil
participant gpAttrStore
participant Application

Application -> gpAttrStore : gpNvm_SetAttribute(id, length, &value);
	note right of gpAttrStore
		loop through AAT to find first
		available address that fits the
		length of data you want to store.
	end note
	group calculateNextFreeAdressAndOffsetThatFitsNewAttribute()
		gpAttrStore -> gpAttrStore: sortedAAT = sortByAdress(AAT)
		note right: from lowest to highest address
		loop foreach element in sortedAAT
			alt element.next.startOfAttributeInBytes - element.startOfAttributeInBytes >= length
				note left of gpAttrStore
					startOfAttributeInBytes = (address x pagesize) + offset
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
		alt address == firstPage
			gpAttrStore -> gpAttrStore: address = startAdress
			gpAttrStore -> gpAttrStore: Read(address, Buffer))
			gpAttrStore -> gpAttrStore: WriteBuffer(startOffset, Buffer, pValue, PAGE)
			gpAttrStore -> gpAttrStore: Write((startAdress, Buffer))
		else address == middle
			gpAttrStore -> gpAttrStore: WriteBuffer(address, Buffer)
		else address == endAdress
			gpAttrStore -> gpAttrStore: Read(address, Buffer))
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
*/