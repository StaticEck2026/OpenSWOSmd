/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef FILE_IO_H
#define FILE_IO_H

#include "menu_system.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int16_t;

/* ---- File I/O functions ---- */
int16_t _LoadFileEx(uint8_t *filename, uint8_t *dest, uint32_t *outFileSize);
int16_t _WriteFile(uint8_t *filename, uint8_t *src, uint32_t size);
void _ShowFsErrorByCode(uint16_t errCode);
int16_t _LoadCareerFile(uint8_t *filename);
int16_t _SaveCareerFile(uint8_t *filename);
int16_t _LoadPresetFile(uint8_t *filename);
int16_t _SavePresetCompetitionFile(uint8_t *filename);
int16_t _LoadDIYFile(uint8_t *filename);
int16_t _SaveDIYFile(uint8_t *filename);
int16_t _FindFilesExtensionPresent(uint8_t *ext);
int16_t _ReadTacticsFile(uint8_t *filename);
int16_t _WriteTacticsFile(uint8_t *filename, uint8_t *buffer);
int16_t _LoadHilFile(uint8_t *filename);
int16_t _DoSaveHighlights(uint8_t *filename);
void _DiskMenuRet(void);
void _ShowInsertDisk2Dialog(uint8_t *header);
int16_t _Asset_PromptInsertSaveDiskDf0(uint8_t *title);
void _FormatSaveDiskMenu(void);
void _DeleteFileMenu(void);
void _ExitDisk(void);
int16_t _SelectFileToSaveDialog(uint32_t filterExt, uint8_t *defaultFilename, uint8_t *menuTitleStr);
int16_t _GetFilenameAndExtension(uint32_t preferredExt, uint8_t *defaultFilename, uint8_t *menuTitleStr);

/* ---- File Selection Helpers (Chunk 140) ---- */
void _SelectFilenameToSaveAndExit(void);
void _AddExtension(uint8_t *filename);
void _RepositionSelectFileEntries(void);

/* ---- SelectFilesInputSaveFilenameOnSelect (Chunk 142) ---- */
void _SelectFilesInputSaveFilenameOnSelect(MenuEntry *entry);

/* --- Chunk 218: SelectFilesInit, SelectFilesOnReturn --- */
void _SelectFilesInit(void);
void _SelectFilesOnReturn(void);

#endif
