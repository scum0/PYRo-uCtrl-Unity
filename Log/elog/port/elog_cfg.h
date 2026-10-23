/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015-2016, Armink, <armink.ztl@gmail.com>
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
 * Function: It is the configure head file for this library.
 * Created on: 2015-07-30
 */

#ifndef _ELOG_CFG_H_
#define _ELOG_CFG_H_
/*---------------------------------------------------------------------------*/
/* enable log output. */
#define ELOG_OUTPUT_ENABLE
/* setting static output log level. range: from ELOG_LVL_ASSERT to ELOG_LVL_VERBOSE */
#define ELOG_OUTPUT_LVL                          ELOG_LVL_VERBOSE
/* enable assert check */
#define ELOG_ASSERT_ENABLE
/* buffer size for every line's log */
#define ELOG_LINE_BUF_SIZE                       256
/* output line number max length */
#define ELOG_LINE_NUM_MAX_LEN                    10
/* output filter's tag max length */
#define ELOG_FILTER_TAG_MAX_LEN                  30
/* output filter's keyword max length */
#define ELOG_FILTER_KW_MAX_LEN                   32
/* output filter's tag level max num */
#define ELOG_FILTER_TAG_LVL_MAX_NUM              5
/* output newline sign */
#define ELOG_NEWLINE_SIGN                        "\r\n"
/*---------------------------------------------------------------------------*/
/* enable log color */
#define ELOG_COLOR_ENABLE
/* change the some level logs to not default color if you want */
#define ELOG_COLOR_ASSERT                        (F_MAGENTA B_NULL S_NORMAL)
#define ELOG_COLOR_ERROR                         (F_RED B_NULL S_NORMAL)
#define ELOG_COLOR_WARN                          (F_YELLOW B_NULL S_NORMAL)
#define ELOG_COLOR_INFO                          (F_CYAN B_NULL S_NORMAL)
#define ELOG_COLOR_DEBUG                         (F_GREEN B_NULL S_NORMAL)
#define ELOG_COLOR_VERBOSE                       (F_BLUE B_NULL S_NORMAL)
/*---------------------------------------------------------------------------*/
/* enable log fmt */
/* comment it if you don't want to output them at all */
#define ELOG_FMT_USING_FUNC
#define ELOG_FMT_USING_DIR
#define ELOG_FMT_USING_LINE
/*---------------------------------------------------------------------------*/
/* enable asynchronous output mode */
//#define ELOG_ASYNC_OUTPUT_ENABLE					
/* the highest output level for async mode, other level will sync output */
#define ELOG_ASYNC_OUTPUT_LVL                    ELOG_LVL_ASSERT
/* buffer size for asynchronous output mode */
#define ELOG_ASYNC_OUTPUT_BUF_SIZE               (ELOG_LINE_BUF_SIZE * 10)
/* each asynchronous output's log which must end with newline sign */
#define ELOG_ASYNC_LINE_OUTPUT
/* asynchronous output mode using POSIX pthread implementation */
//#define ELOG_ASYNC_OUTPUT_USING_PTHREAD
/*---------------------------------------------------------------------------*/
/* enable buffered output mode */
//#define ELOG_BUF_OUTPUT_ENABLE
/* buffer size for buffered output mode */
#define ELOG_BUF_OUTPUT_BUF_SIZE                 (ELOG_LINE_BUF_SIZE * 1)
/* enable auto word wrap */
#define ELOG_AUTO_WORD_WRAP
#define ELOG_CLEAR_LAST_BUFFER
/*---------------------------------------------------------------------------*/
#define DEFAULT_TAG      "log"
#define DEFAULT_TERMINAL  0
/*---------------------------------------------------------------------------*/
/* log output backend selection */
/* 三选一：RTT / UART / FLASH，只能定义一个 */
/* ELOG_OUTPUT_BACKEND_RTT   : 通过 SEGGER RTT 输出 */
/* ELOG_OUTPUT_BACKEND_UART  : 通过硬件串口（DMA）输出 */
/* ELOG_OUTPUT_BACKEND_FLASH : 写入外部 Flash（追加式日志） */
#define ELOG_OUTPUT_BACKEND_RTT
//#define ELOG_OUTPUT_BACKEND_UART
//#define ELOG_OUTPUT_BACKEND_FLASH

/* UART backend configuration (only used when ELOG_OUTPUT_BACKEND_UART is defined) */
/* UART handle used for log output, e.g. huart1 / huart5 / huart7 / huart10 */
#define ELOG_UART_HANDLE          huart1
/* number of TX buffers used for DMA log output (ring buffer depth) */
#define ELOG_UART_TX_BUF_NUM      4
/* max time (ms) to wait for a free TX buffer before dropping the log */
#define ELOG_UART_TX_WAIT_MS      20

/* Flash backend configuration (only used when ELOG_OUTPUT_BACKEND_FLASH is defined) */
/* 日志占用的 Flash 扇区数量（从设备末尾往前数） */
#define ELOG_FLASH_SECTOR_NUM     8
/* 每条日志记录的最大长度（含结尾 '\0'） */
#define ELOG_FLASH_RECORD_SIZE    256
/*---------------------------------------------------------------------------*/
#endif /* _ELOG_CFG_H_ */

#define  ENABLE_FSM_STATE_LOG
