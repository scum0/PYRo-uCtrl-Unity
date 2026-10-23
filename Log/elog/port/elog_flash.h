/**
 * @file elog_flash.h
 * @brief 日志 Flash 后端（追加式日志，基于 IFlashDevice 抽象）
 *
 * 使用方式：
 *   1. 在 elog_cfg.h 中定义 ELOG_OUTPUT_BACKEND_FLASH；
 *   2. 应用层在挂载/初始化 Flash 设备后，调用 elog_port_flash_set_device()
 *      把设备注入进来；
 *   3. 之后所有日志会以追加方式写入 Flash 的保留扇区。
 *
 * 日志区位于设备末尾的 ELOG_FLASH_SECTOR_NUM 个扇区，采用固定长度记录 + 环形覆盖：
 * 写满后擦除最早的扇区继续写，保证掉电时最多丢失一个扇区。
 */

#ifndef __ELOG_FLASH_H__
#define __ELOG_FLASH_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief 注入 Flash 设备（C++ 的 IFlashDevice*，以 void* 传递避免 C/C++ 头文件耦合）
 * @param device IFlashDevice 指针，传 NULL 表示关闭 Flash 日志
 */
void elog_port_flash_set_device(void *device);

/**
 * @brief Flash 日志后端初始化：扫描已有记录，定位写入位置
 * @return 0 成功，非 0 失败（设备未注入或设备无效）
 */
int elog_port_flash_init(void);

/**
 * @brief 追加一条日志记录
 * @param log  日志内容（已组装完成的完整字符串）
 * @param size 长度
 * @return 0 成功，非 0 失败
 */
int elog_port_flash_append(const char *log, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __ELOG_FLASH_H__ */
