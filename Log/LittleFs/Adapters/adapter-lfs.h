/**
 *******************************************************************************
 * @file    adapter-lfs.h
 * @brief   LittleFS 与 Flash 设备驱动之间的适配层
 *******************************************************************************
 * @attention
 *
 * none
 *
 *******************************************************************************
 * @note
 *
 *  只依赖 IFlashDevice 抽象，不感知具体芯片（W25Qxx）与总线（OSPI）实现。
 *  块大小、块数、页大小等几何参数由设备驱动提供。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2025/8/25
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/
#pragma once




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include "../lfs.h"
#include "flash-intf.h"




/*-------- 2. enum & define ------------------------------------------------------------------------------------------*/

/* 文件系统缓存 / 寿命参数，几何参数（read/prog/block 尺寸）由 IFlashDevice 提供 */
#define LFS_READ_SIZE      64
#define LFS_CACHE_SIZE     256
#define LFS_LOOKAHEAD_SIZE 256
#define LFS_BLOCK_CYCLES   500



/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

/**
 * @brief LittleFS 块设备适配层
 */
class LfsAdapter {
  public:

    static int read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size);

    static int program(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer,
                       lfs_size_t size);

    static int erase(const struct lfs_config* c, lfs_block_t block);

    static int sync(const struct lfs_config* c);

    static void config(IFlashDevice* device);

  private:
    static IFlashDevice* _device;
};


/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/


/*-------- 5. factories ----------------------------------------------------------------------------------------------*/
