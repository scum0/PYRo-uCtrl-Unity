/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 */

#include "elog.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "main.h"

/* 后端互斥检查：RTT / UART / FLASH 必须且只能定义一个 */
#if (defined(ELOG_OUTPUT_BACKEND_RTT) + defined(ELOG_OUTPUT_BACKEND_UART) + defined(ELOG_OUTPUT_BACKEND_FLASH)) != 1
#error "Exactly one of ELOG_OUTPUT_BACKEND_RTT / _UART / _FLASH must be defined in elog_cfg.h"
#endif

#if defined(ELOG_OUTPUT_BACKEND_UART)
#include "usart.h"
#elif defined(ELOG_OUTPUT_BACKEND_RTT)
#include "SEGGER_RTT.h"
#elif defined(ELOG_OUTPUT_BACKEND_FLASH)
#include "pyro_crashlog.h"
#else
#error "Please select a log output backend in elog_cfg.h (RTT / UART / FLASH)"
#endif

static SemaphoreHandle_t LogMutexSemaphore = NULL;

#if defined(ELOG_OUTPUT_BACKEND_UART)
/*
 * DMA 发送缓冲区环形池
 * HAL_UART_Transmit_DMA 是异步的，源缓冲区必须保持有效直到发送完成，
 * 而 elog 的 log_buf 是复用的单缓冲区，因此这里用一组缓冲区轮转，
 * 每个缓冲区发送完成后由发送完成中断释放。
 */
typedef struct
{
    uint8_t buf[ELOG_LINE_BUF_SIZE];
    volatile uint8_t busy;
} elog_uart_tx_buf_t;

static elog_uart_tx_buf_t s_tx_bufs[ELOG_UART_TX_BUF_NUM];
static volatile uint8_t s_tx_buf_idx = 0;
/* 空闲缓冲区计数信号量，发送完成中断中归还 */
static SemaphoreHandle_t s_tx_free_sem = NULL;

/**
 * @brief 从环形池中取一个空闲缓冲区，等待时间由 ELOG_UART_TX_WAIT_MS 决定
 * @return 空闲缓冲区指针，超时返回 NULL
 */
static elog_uart_tx_buf_t *elog_uart_tx_buf_acquire(void)
{
    if (s_tx_free_sem == NULL)
    {
        return NULL;
    }

    /* 等待一个空闲缓冲区；在调度器启动前不阻塞 */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        if (xSemaphoreTake(s_tx_free_sem, 0) != pdTRUE)
        {
            return NULL;
        }
    }
    else if (xSemaphoreTake(s_tx_free_sem,
                            pdMS_TO_TICKS(ELOG_UART_TX_WAIT_MS)) != pdTRUE)
    {
        return NULL;
    }

    /* 环形查找一个非 busy 的缓冲区 */
    for (uint8_t i = 0; i < ELOG_UART_TX_BUF_NUM; ++i)
    {
        const uint8_t idx = (s_tx_buf_idx + i) % ELOG_UART_TX_BUF_NUM;
        if (!s_tx_bufs[idx].busy)
        {
            s_tx_bufs[idx].busy = 1;
            s_tx_buf_idx        = (idx + 1) % ELOG_UART_TX_BUF_NUM;
            return &s_tx_bufs[idx];
        }
    }

    /* 理论上不会发生：信号量计数与 busy 标志不一致，归还计数避免泄漏 */
    xSemaphoreGive(s_tx_free_sem);
    return NULL;
}

/**
 * @brief 发送完成中断中调用，释放对应缓冲区
 * @note 由 HAL_UART_TxCpltCallback 转发调用，运行在中断上下文
 */
void elog_port_uart_tx_cplt(UART_HandleTypeDef *huart)
{
    if (huart != &ELOG_UART_HANDLE)
    {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (uint8_t i = 0; i < ELOG_UART_TX_BUF_NUM; ++i)
    {
        if (s_tx_bufs[i].busy)
        {
            s_tx_bufs[i].busy = 0;
            if (s_tx_free_sem != NULL)
            {
                xSemaphoreGiveFromISR(s_tx_free_sem, &xHigherPriorityTaskWoken);
            }
            break;
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#endif /* ELOG_OUTPUT_BACKEND_UART */

/* forward declaration, used by elog_port_init before its definition */
void elog_port_output(unsigned char terminal, const char *log, size_t size);

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void)
{
    ElogErrCode result = ELOG_NO_ERR;

    /* 创建日志输出互斥锁，防止多任务同时打印造成日志乱码穿插 */
    LogMutexSemaphore = xSemaphoreCreateMutex();
    if (LogMutexSemaphore == NULL)
    {
        elog_port_output(DEFAULT_TERMINAL, "elog sem create fail\r\n",
                         sizeof("elog sem create fail\r\n") - 1);
        result = ELOG_NO_ERR;
    }

#if defined(ELOG_OUTPUT_BACKEND_UART)
    /* DMA 发送缓冲区空闲计数信号量，初值为缓冲区数量 */
    s_tx_free_sem = xSemaphoreCreateCounting(ELOG_UART_TX_BUF_NUM, ELOG_UART_TX_BUF_NUM);
    if (s_tx_free_sem == NULL)
    {
        elog_port_output(DEFAULT_TERMINAL, "elog tx sem create fail\r\n",
                         sizeof("elog tx sem create fail\r\n") - 1);
        result = ELOG_NO_ERR;
    }
#endif

    return result;
}

/**
 * EasyLogger port deinitialize
 *
 */
void elog_port_deinit(void)
{
    /* add your code here */
}

/**
 * output log port interface
 *
 * @param terminal log terminal number
 * @param log output of log,已经组装完成的完整日志字符串
 * @param size log size
 */
void elog_port_output(unsigned char terminal, const char *log, size_t size)
{
#if defined(ELOG_OUTPUT_BACKEND_UART)
    /* 串口输出：使用 DMA 非阻塞发送，terminal 参数在串口后端下忽略 */
    (void)terminal;

    if (size == 0 || size > ELOG_LINE_BUF_SIZE)
    {
        return;
    }

    elog_uart_tx_buf_t *tx = elog_uart_tx_buf_acquire();
    if (tx == NULL)
    {
        /* 无空闲缓冲区，丢弃本条日志，避免阻塞业务 */
        return;
    }

    memcpy(tx->buf, log, size);

    if (HAL_UART_Transmit_DMA(&ELOG_UART_HANDLE, tx->buf, (uint16_t)size) != HAL_OK)
    {
        /* 启动失败，立即归还缓冲区 */
        tx->busy = 0;
        if (s_tx_free_sem != NULL)
        {
            xSemaphoreGive(s_tx_free_sem);
        }
    }
#elif defined(ELOG_OUTPUT_BACKEND_RTT)
    SEGGER_RTT_Write(0, log, size);
#elif defined(ELOG_OUTPUT_BACKEND_FLASH)
    /*
     * Flash 后端：日志先写入 RAM 环形缓冲（满了丢弃最旧的），不直接写 Flash。
     * 只有发生 HardFault 时，才把环形缓冲 + 异常现场写入 .noinit，
     * 复位后由应用层落盘到 Flash 文件。
     * terminal 参数在 Flash 后端下忽略。
     */
    (void)terminal;
    pyro_crashlog_push(log, size);
#endif
}

/**
 * output lock
 * @note elog输出日志前调用，拿互斥锁保证单次完整日志不会被其他任务打断
 */
void elog_port_output_lock(void)
{
    if (NULL != LogMutexSemaphore)
    {
        xSemaphoreTake(LogMutexSemaphore, portMAX_DELAY);
    }
}

/**
 * output unlock
 * @note elog输出日志完成后调用，释放互斥锁
 */
void elog_port_output_unlock(void)
{
    if (NULL != LogMutexSemaphore)
    {
        xSemaphoreGive(LogMutexSemaphore);
    }
}

/**
 * get current time interface
 *
 * @return current time string pointer，基于HAL_GetTick毫秒时钟
 * @note 使用整数格式化（秒.毫秒），避免依赖 newlib-nano 的浮点 printf 支持
 */
static char time[20];
const char *elog_port_get_time(void)
{
    const uint32_t ms = HAL_GetTick();
    sprintf(time, "%lu.%03lus", (unsigned long)(ms / 1000U),
            (unsigned long)(ms % 1000U));
    return time;
}

/**
 * get current thread name interface
 *
 * @return current thread name string pointer
 */
const char *elog_port_get_t_info(void)
{
#if (INCLUDE_xTaskGetSchedulerState  == 1 )
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
#endif
        return pcTaskGetName(xTaskGetCurrentTaskHandle());
#if (INCLUDE_xTaskGetSchedulerState  == 1 )
    }
#endif
    return "";
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void)
{
    /* add your code here */
    return "";
}
