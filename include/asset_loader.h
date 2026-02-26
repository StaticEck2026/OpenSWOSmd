#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;
typedef long int32_t;

/* ---- Asset loader region management ---- */
void _AssetLoaderRegisterRegion(uint32_t size, uint8_t *region);
void _AssetLoaderInitAltPathAndBanks(void);

/* ---- Asset precaching ---- */
void _AssetPrecacheCore(void);
void _AssetPrecacheOneCore(const uint8_t *assetName);
void _AssetPrecacheMenuPack(void);
void _AssetPrecacheOneMenu(const uint8_t *assetName);

/* ---- Asset service operations ---- */
int16_t _AssetSvcLoaderOp(void);
int16_t _AssetSvcLoaderOp2(void);
int16_t _AssetSvcLoaderOp3(void);

/* ---- Asset cache operations ---- */
int32_t _AssetCacheFindByName(const uint8_t *name);
int16_t _AssetCacheCopyByName(uint8_t *name, uint8_t *dest, uint32_t *outFileSize);
int16_t _AssetCacheLoadFromAssetSys(const uint8_t *name);
void _AssetCacheStoreFromMemory(uint8_t *name, uint8_t *src, uint32_t size);

/* ---- Asset file loading with retry ---- */
int16_t _PlaySong(uint8_t *name, uint8_t *dest);
int16_t _AssetEnsureReadyWithRetry(uint8_t *name, uint8_t *dest);
int16_t _LoadRNCToBufferNoBlit(uint8_t *name, uint8_t *dest);

#endif
