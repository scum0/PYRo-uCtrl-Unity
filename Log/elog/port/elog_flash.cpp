/**
 * @file elog_flash.cpp
 * @brief 日志 Flash 后端实现（追加式日志，基于 IFlashDevice 抽象）
 */

#include "elog_flash.h"

#include "elog_cfg.h"
#include "flash-intf.h"

#include <cstring>

/*---------------------------------------------------------------------------*/
/* 记录格式：每条日志占一个固定长度槽位，便于掉电后逐条扫描恢复 */
/*   [0..1] magic  : 0x5A 0xA5，标识一个有效槽位 */
/*   [2]    len    : 有效数据长度（不含结尾 '\0'） */
/*   [3]    crc    : 数据区校验（简单累加和） */
/*   [4..]  data   : 日志内容 */
/*---------------------------------------------------------------------------*/

#define ELOG_FLASH_REC_MAGIC0  0x5AU
#define ELOG_FLASH_REC_MAGIC1  0xA5U
#define ELOG_FLASH_REC_HDR     4U
#define ELOG_FLASH_REC_DATA    (ELOG_FLASH_RECORD_SIZE - ELOG_FLASH_REC_HDR)

/* 未注入设备时为 nullptr，日志静默丢弃 */
static IFlashDevice *s_flash = nullptr;

/*
 * MDMA 传输缓冲区。
 * 这些缓冲区会作为 MDMA 的源/目标（经 IFlashDevice -> OSPI），
 * 必须放在 DMA/MDMA 可访问的 RAM 区，不能放栈或 DTCMRAM。
 * .dma_heap 段在链接脚本中映射到 RAM_D2。
 */
__attribute__((section(".dma_heap"))) static uint8_t s_rec_hdr_buf[ELOG_FLASH_REC_HDR];
__attribute__((section(".dma_heap"))) static uint8_t s_rec_buf[ELOG_FLASH_RECORD_SIZE];

/* 日志区几何信息（init 时计算） */
static uint32_t s_region_start = 0;   /* 日志区起始地址 */
static uint32_t s_sector_size  = 0;   /* 单个扇区大小 */
static uint32_t s_slot_count   = 0;   /* 日志区总槽位数 */
static uint32_t s_cursor       = 0;   /* 当前写入槽位索引 */
static bool     s_ready        = false;

/*---------------------------------------------------------------------------*/
/* 内部工具 */
/*---------------------------------------------------------------------------*/

/** 简单的数据校验和 */
static uint8_t elog_flash_checksum(const uint8_t *data, uint32_t len) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) {
        sum = static_cast<uint8_t>(sum + data[i]);
    }
    return sum;
}

/** 槽位索引 -> 绝对地址 */
static uint32_t elog_flash_slot_addr(uint32_t slot) {
    return s_region_start + slot * ELOG_FLASH_RECORD_SIZE;
}

/** 判断一个槽位是否为空（全 0xFF） */
static bool elog_flash_slot_is_empty(uint32_t slot) {
    if (s_flash->read(elog_flash_slot_addr(slot), s_rec_hdr_buf, sizeof(s_rec_hdr_buf)) !=
        FlashErrCode::SUCCESS) {
        return false;
    }
    for (uint8_t b : s_rec_hdr_buf) {
        if (b != 0xFFU) {
            return false;
        }
    }
    return true;
}

/** 擦除某个槽位所属的扇区 */
static bool elog_flash_erase_slot_sector(uint32_t slot) {
    const uint32_t addr = elog_flash_slot_addr(slot);
    return s_flash->eraseSector(addr) == FlashErrCode::SUCCESS;
}

/*---------------------------------------------------------------------------*/
/* 对外实现 */
/*---------------------------------------------------------------------------*/

void elog_port_flash_set_device(void *device) {
    s_flash = static_cast<IFlashDevice *>(device);
    s_ready = false;
}

int elog_port_flash_init(void) {
    if (s_flash == nullptr) {
        return -1;
    }

    s_sector_size = s_flash->sectorSize();
    if (s_sector_size == 0 || ELOG_FLASH_RECORD_SIZE == 0) {
        return -1;
    }

    /* 日志区取设备末尾的 ELOG_FLASH_SECTOR_NUM 个扇区 */
    const uint32_t total_sectors = s_flash->sectorCount();
    if (total_sectors <= ELOG_FLASH_SECTOR_NUM) {
        return -1;
    }
    s_region_start = (total_sectors - ELOG_FLASH_SECTOR_NUM) * s_sector_size;
    s_slot_count   = (ELOG_FLASH_SECTOR_NUM * s_sector_size) / ELOG_FLASH_RECORD_SIZE;
    if (s_slot_count == 0) {
        return -1;
    }

    /* 扫描已有记录，定位第一个空槽位作为写入游标 */
    s_cursor = s_slot_count; /* 默认：全部写满 */
    for (uint32_t i = 0; i < s_slot_count; ++i) {
        if (elog_flash_slot_is_empty(i)) {
            s_cursor = i;
            break;
        }
    }

    /* 全部写满：从头开始（擦除第一个扇区） */
    if (s_cursor >= s_slot_count) {
        if (!elog_flash_erase_slot_sector(0)) {
            return -1;
        }
        s_cursor = 0;
    }

    s_ready = true;
    return 0;
}

int elog_port_flash_append(const char *log, size_t size) {
    if (!s_ready || s_flash == nullptr || log == nullptr || size == 0) {
        return -1;
    }

    /* 截断到单条记录可容纳的长度 */
    if (size > ELOG_FLASH_REC_DATA) {
        size = ELOG_FLASH_REC_DATA;
    }

    /* 走到扇区边界时，先擦除下一个扇区（环形覆盖） */
    const uint32_t slots_per_sector = s_sector_size / ELOG_FLASH_RECORD_SIZE;
    if (slots_per_sector != 0 && (s_cursor % slots_per_sector) == 0) {
        if (!elog_flash_erase_slot_sector(s_cursor)) {
            return -1;
        }
    }

    memset(s_rec_buf, 0xFF, sizeof(s_rec_buf));
    s_rec_buf[0] = ELOG_FLASH_REC_MAGIC0;
    s_rec_buf[1] = ELOG_FLASH_REC_MAGIC1;
    s_rec_buf[2] = static_cast<uint8_t>(size);
    memcpy(&s_rec_buf[ELOG_FLASH_REC_HDR], log, size);
    s_rec_buf[3] = elog_flash_checksum(&s_rec_buf[ELOG_FLASH_REC_HDR], static_cast<uint32_t>(size));

    if (s_flash->program(elog_flash_slot_addr(s_cursor), s_rec_buf, sizeof(s_rec_buf)) !=
        FlashErrCode::SUCCESS) {
        return -1;
    }

    /* 推进游标，环形回绕 */
    s_cursor = (s_cursor + 1) % s_slot_count;
    return 0;
}
