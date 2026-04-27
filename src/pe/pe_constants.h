/*
 * src/pe/pe_constants.h
 *
 * Self-contained PE constants used by the parser. We do NOT pull in
 * <windows.h> here because the parser is meant to be portable and
 * fuzzable on Linux/macOS without an SDK. Windows-specific consumers
 * still get full IMAGE_* names through their own includes.
 *
 * All field offsets and sizes were cross-checked against:
 *  - Microsoft "PE Format" docs (winmd revision 2024-02)
 *  - <winnt.h> from Windows 10 SDK 10.0.22621
 *  - MinGW-w64 headers (winnt.h)
 */

#ifndef WRAITH_PE_CONSTANTS_H
#define WRAITH_PE_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DOS / NT magic */
#define WRAITH_PE_DOS_SIGNATURE  0x5A4D  /* 'MZ' */
#define WRAITH_PE_NT_SIGNATURE  0x00004550  /* 'PE\0\0' */
#define WRAITH_PE_OPT_MAGIC_PE32  0x010B
#define WRAITH_PE_OPT_MAGIC_PE32PLUS  0x020B

/* IMAGE_FILE_MACHINE_* (FileHeader.Machine) */
#define WRAITH_PE_MACHINE_AMD64  0x8664
#define WRAITH_PE_MACHINE_I386  0x014c
#define WRAITH_PE_MACHINE_ARM64  0xAA64

/* IMAGE_FILE_HEADER.Characteristics */
#define WRAITH_PE_FILE_DLL  0x2000
#define WRAITH_PE_FILE_EXECUTABLE  0x0002

/* IMAGE_DIRECTORY_ENTRY_* indices into OptionalHeader.DataDirectory[] */
#define WRAITH_PE_DIR_EXPORT  0
#define WRAITH_PE_DIR_IMPORT  1
#define WRAITH_PE_DIR_RESOURCE  2
#define WRAITH_PE_DIR_EXCEPTION  3
#define WRAITH_PE_DIR_SECURITY  4
#define WRAITH_PE_DIR_BASERELOC  5
#define WRAITH_PE_DIR_DEBUG  6
#define WRAITH_PE_DIR_TLS  9
#define WRAITH_PE_DIR_LOAD_CONFIG  10
#define WRAITH_PE_DIR_BOUND_IMPORT  11
#define WRAITH_PE_DIR_IAT  12
#define WRAITH_PE_DIR_DELAY_IMPORT  13
#define WRAITH_PE_DIR_COM_DESCRIPTOR  14
#define WRAITH_PE_DIR_COUNT  16

/* Section characteristics (subset relevant to the loader) */
#define WRAITH_PE_SCN_CNT_CODE  0x00000020
#define WRAITH_PE_SCN_CNT_INITIALIZED_DATA  0x00000040
#define WRAITH_PE_SCN_CNT_UNINITIALIZED  0x00000080
#define WRAITH_PE_SCN_MEM_DISCARDABLE  0x02000000
#define WRAITH_PE_SCN_MEM_NOT_CACHED  0x04000000
#define WRAITH_PE_SCN_MEM_NOT_PAGED  0x08000000
#define WRAITH_PE_SCN_MEM_SHARED  0x10000000
#define WRAITH_PE_SCN_MEM_EXECUTE  0x20000000
#define WRAITH_PE_SCN_MEM_READ  0x40000000
#define WRAITH_PE_SCN_MEM_WRITE  0x80000000

/* Relocation types we accept (x64 only). HIGHLOW is x86 - rejected. */
#define WRAITH_PE_REL_ABSOLUTE  0
#define WRAITH_PE_REL_HIGHLOW  3  /* x86 only - rejected */
#define WRAITH_PE_REL_DIR64  10  /* x64 - the only one we honor */

/* IMAGE_SIZEOF_SHORT_NAME */
#define WRAITH_PE_SECTION_NAME_LEN  8

/* IMAGE_BASE_RELOCATION header size (matches sizeof(IMAGE_BASE_RELOCATION)) */
#define WRAITH_PE_RELOC_HDR_SIZE  8

#pragma pack(push, 1)

typedef struct wr_pe_dos_header {
  uint16_t e_magic;  /* 'MZ' */
  uint16_t e_cblp;
  uint16_t e_cp;
  uint16_t e_crlc;
  uint16_t e_cparhdr;
  uint16_t e_minalloc;
  uint16_t e_maxalloc;
  uint16_t e_ss;
  uint16_t e_sp;
  uint16_t e_csum;
  uint16_t e_ip;
  uint16_t e_cs;
  uint16_t e_lfarlc;
  uint16_t e_ovno;
  uint16_t e_res[4];
  uint16_t e_oemid;
  uint16_t e_oeminfo;
  uint16_t e_res2[10];
  uint32_t e_lfanew;  /* file offset to NT headers */
} wr_pe_dos_header;

typedef struct wr_pe_file_header {
  uint16_t Machine;
  uint16_t NumberOfSections;
  uint32_t TimeDateStamp;
  uint32_t PointerToSymbolTable;
  uint32_t NumberOfSymbols;
  uint16_t SizeOfOptionalHeader;
  uint16_t Characteristics;
} wr_pe_file_header;

typedef struct wr_pe_data_directory {
  uint32_t VirtualAddress;
  uint32_t Size;
} wr_pe_data_directory;

/* PE32+ (64-bit) optional header. We don't model PE32 (x86) - x64 only. */
typedef struct wr_pe_optional_header64 {
  uint16_t Magic;
  uint8_t  MajorLinkerVersion;
  uint8_t  MinorLinkerVersion;
  uint32_t SizeOfCode;
  uint32_t SizeOfInitializedData;
  uint32_t SizeOfUninitializedData;
  uint32_t AddressOfEntryPoint;
  uint32_t BaseOfCode;
  uint64_t ImageBase;
  uint32_t SectionAlignment;
  uint32_t FileAlignment;
  uint16_t MajorOperatingSystemVersion;
  uint16_t MinorOperatingSystemVersion;
  uint16_t MajorImageVersion;
  uint16_t MinorImageVersion;
  uint16_t MajorSubsystemVersion;
  uint16_t MinorSubsystemVersion;
  uint32_t Win32VersionValue;
  uint32_t SizeOfImage;
  uint32_t SizeOfHeaders;
  uint32_t CheckSum;
  uint16_t Subsystem;
  uint16_t DllCharacteristics;
  uint64_t SizeOfStackReserve;
  uint64_t SizeOfStackCommit;
  uint64_t SizeOfHeapReserve;
  uint64_t SizeOfHeapCommit;
  uint32_t LoaderFlags;
  uint32_t NumberOfRvaAndSizes;
  wr_pe_data_directory DataDirectory[WRAITH_PE_DIR_COUNT];
} wr_pe_optional_header64;

typedef struct wr_pe_nt_headers64 {
  uint32_t  Signature;
  wr_pe_file_header  FileHeader;
  wr_pe_optional_header64  OptionalHeader;
} wr_pe_nt_headers64;

typedef struct wr_pe_section_header {
  uint8_t  Name[WRAITH_PE_SECTION_NAME_LEN];
  uint32_t VirtualSize;
  uint32_t VirtualAddress;
  uint32_t SizeOfRawData;
  uint32_t PointerToRawData;
  uint32_t PointerToRelocations;
  uint32_t PointerToLinenumbers;
  uint16_t NumberOfRelocations;
  uint16_t NumberOfLinenumbers;
  uint32_t Characteristics;
} wr_pe_section_header;

typedef struct wr_pe_base_relocation {
  uint32_t VirtualAddress;
  uint32_t SizeOfBlock;
  /* uint16_t TypeOffset[]; */
} wr_pe_base_relocation;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_PE_CONSTANTS_H */
