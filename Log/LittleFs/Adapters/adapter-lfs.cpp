/**
 *******************************************************************************
 * @file    adapter-lfs.cpp
 * @brief   LittleFS 与 Flash 设备驱动之间的适配层
 *******************************************************************************
 * @attention
 *
 * none
 *
 *******************************************************************************
 * @note
 *
 *  只做地址换算与错误码转换，具体的同步、分页等细节由 IFlashDevice 实现负责。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2025/8/25
 * @version 1.0
 *******************************************************************************
 */


/* ------- define --------------------------------------------------------------------------------------------------*/



/* ------- include ---------------------------------------------------------------------------------------------------*/

#include "adapter-lfs.h"


/* ------- class prototypes-----------------------------------------------------------------------------------------*/



/* ------- macro -----------------------------------------------------------------------------------------------------*/



/* ------- variables -------------------------------------------------------------------------------------------------*/

IFlashDevice* LfsAdapter::_device = nullptr;


/* ------- function implement ----------------------------------------------------------------------------------------*/

int LfsAdapter::read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
    static_cast<void>(c);

    if (_device == nullptr) {
        return LFS_ERR_IO;
    }

    const uint32_t addr = static_cast<uint32_t>(block) * _device->sectorSize() + off;
    return (_device->read(addr, buffer, size) == FlashErrCode::SUCCESS) ? 0 : LFS_ERR_IO;
}

int LfsAdapter::program(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer,
                        lfs_size_t size) {
    static_cast<void>(c);

    if (_device == nullptr) {
        return LFS_ERR_IO;
    }

    const uint32_t addr = static_cast<uint32_t>(block) * _device->sectorSize() + off;
    return (_device->program(addr, buffer, size) == FlashErrCode::SUCCESS) ? 0 : LFS_ERR_IO;
}

int LfsAdapter::erase(const struct lfs_config* c, lfs_block_t block) {
    static_cast<void>(c);

    if (_device == nullptr) {
        return LFS_ERR_IO;
    }

    const uint32_t addr = static_cast<uint32_t>(block) * _device->sectorSize();
    return (_device->eraseSector(addr) == FlashErrCode::SUCCESS) ? 0 : LFS_ERR_IO;
}

int LfsAdapter::sync(const struct lfs_config* c) {
    static_cast<void>(c);

    if (_device == nullptr) {
        return LFS_ERR_IO;
    }

    return (_device->waitUntilIdle() == FlashErrCode::SUCCESS) ? 0 : LFS_ERR_IO;
}

void LfsAdapter::config(IFlashDevice* device) { _device = device; }
