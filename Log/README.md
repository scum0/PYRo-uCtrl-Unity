# PYRo 日志框架

本文档说明 PYRo 工程当前日志框架的整体结构、使用方法与重要接口。

## 一、整体架构

日志框架采用「库层门面 + 应用层注入 + 多后端输出」的分层设计，依赖方向单向：

```
库内模块（驱动 / 任务 / 算法 / 组件）
        │  只依赖门面宏 PYRO_LOGx
        ▼
门面层  PYRo/Core/Def/pyro_log.h / pyro_log.c
        │  运行期回调 pyro_log_fn
        ▼
适配层  App/pyro_init_thread.cpp :: pyro_log_to_elog()
        │  库内等级 -> EasyLogger 等级
        ▼
实现层  PYRo/Log/elog/  (EasyLogger)
        │  格式化、过滤、彩色、加锁
        ▼
后端层  elog_port.c
        ├── RTT   (SEGGER RTT，默认)
        ├── UART  (HAL + DMA 非阻塞)
        └── FLASH (RAM 环形缓冲 + HardFault 落 .noinit + 复位后落 /crash.log)
```

分层职责：

| 层 | 位置 | 职责 |
|---|---|---|
| 门面层 | `PYRo/Core/Def/pyro_log.h/.c` | 提供与具体日志库无关的 `PYRO_LOGx` 宏与回调注入接口 |
| 适配层 | `App/pyro_init_thread.cpp` | 把门面回调转发给 EasyLogger，是唯一认识 elog 的应用代码 |
| 实现层 | `PYRo/Log/elog/` | EasyLogger 本体：等级过滤、tag 过滤、格式化、彩色、互斥 |
| 后端层 | `PYRo/Log/elog/port/elog_port.c` | 具体输出通道：RTT / UART / FLASH |
| 崩溃日志 | `PYRo/Core/Debug/pyro_crashlog.*` | RAM 环形缓冲 + HardFault 现场快照 |

设计要点：

1. 库内模块不直接依赖 EasyLogger，只依赖 `pyro_log.h`，换日志实现只改适配层一处。
2. 未注入回调时日志静默丢弃，库可以脱离日志系统独立编译运行。
3. `PYRO_LOG_ENABLE=0` 时所有 `PYRO_LOGx` 展开为 `((void)0)`，日志代码不进入固件，零开销。
4. 门面层与 elog 层各有一套开关：门面是编译期裁代码，elog 是运行期过滤等级，可叠加。

## 二、快速上手

### 1. 初始化（应用层，仅一次）

在 CubeMX 生成的 `main.c` 中，`SystemClock_Config()` 之后、外设初始化之前调用：

```c
#include "log.h"

LogHandle.Ctrl.Init();   /* 内部完成 elog_init + 格式配置 + elog_start，无需再手动开启 */
```

`LogHandle.Ctrl.Init()` 内部依次完成：

1. `elog_init()`：初始化 EasyLogger 与端口（创建互斥锁、UART 后端的 DMA 缓冲区信号量）。
2. `elog_set_fmt(...)`：为各等级配置输出格式（等级、时间、任务名、函数、行号、文件名）。
3. `elog_set_text_color_enabled(true)`：开启终端彩色。
4. `elog_start()`：开启输出并打印 EasyLogger 版本号。

### 2. 注入库内日志回调（应用层，仅一次）

在 `App/pyro_init_thread.cpp` 的 `pyro_init_thread()` 开头调用：

```c
pyro_log_set_handler(pyro_log_to_elog);
```

必须放在任何驱动打日志之前，否则早期日志会被静默丢弃。

### 3. 在库内模块打日志

```c
#include "pyro_log.h"

PYRO_LOGI("UART", "%s init success", name);
PYRO_LOGE("CAN", "%s send_msg failed, id=0x%03lX", can_name, id);
```

### 4. 在应用层直接打日志

应用层可直接使用 `LogHandle` 或 elog 宏：

```c
LogHandle.O.I("app boot ok, tick=%lu", (unsigned long)HAL_GetTick());
LogHandle.O.E("open for write failed: %s", path);
LogHandle.O.Raw("raw line without prefix\r\n");
```

## 三、重要接口

### 3.1 门面层 `pyro_log.h`

输出宏（第一个参数是 tag，其后是 printf 风格格式串）：

| 宏 | 等级 | 说明 |
|---|---|---|
| `PYRO_LOGA(tag, fmt, ...)` | ASSERT | 断言级 |
| `PYRO_LOGE(tag, fmt, ...)` | ERROR | 错误 |
| `PYRO_LOGW(tag, fmt, ...)` | WARN | 警告 |
| `PYRO_LOGI(tag, fmt, ...)` | INFO | 常规信息 |
| `PYRO_LOGD(tag, fmt, ...)` | DEBUG | 调试 |
| `PYRO_LOGV(tag, fmt, ...)` | VERBOSE | 最详细 |
| `PYRO_LOG(level, tag, fmt, ...)` | 任意 | 通用入口，等级为 `pyro_log_level_t` |

控制接口：

| 接口 | 说明 |
|---|---|
| `void pyro_log_set_handler(pyro_log_fn fn)` | 注册日志回调；传 `NULL` 关闭库内日志输出 |
| `void pyro_log_output(pyro_log_level_t level, const char *tag, const char *format, ...)` | 统一输出入口，一般由宏调用，不直接使用 |

等级枚举 `pyro_log_level_t` 与 EasyLogger 一一对应：

```c
PYRO_LOG_LVL_ASSERT  = 0
PYRO_LOG_LVL_ERROR   = 1
PYRO_LOG_LVL_WARN    = 2
PYRO_LOG_LVL_INFO    = 3
PYRO_LOG_LVL_DEBUG   = 4
PYRO_LOG_LVL_VERBOSE = 5
```

回调原型：

```c
typedef void (*pyro_log_fn)(pyro_log_level_t level, const char *tag,
                            const char *format, va_list args);
```

### 3.2 适配层 `pyro_log_to_elog()`

位于 `App/pyro_init_thread.cpp`，把库内回调转成 EasyLogger 调用：

```c
static void pyro_log_to_elog(pyro_log_level_t level, const char *tag,
                             const char *format, va_list args)
{
    char buf[ELOG_LINE_BUF_SIZE];
    vsnprintf(buf, sizeof(buf), format, args);
    elog_output(DEFAULT_TERMINAL, (uint8_t)level, tag, NULL, NULL, 0, "%s", buf);
}
```

注意：`elog_output` 是可变参函数，无法直接转发 `va_list`，因此先 `vsnprintf` 成字符串再以 `"%s"` 传入。

### 3.3 应用层句柄 `LogHandle`

定义于 `PYRo/Log/log.h`，是应用层使用的统一门面：

```c
typedef struct LogHandle
{
    struct Control {
        void (*Init)(void);            /* 初始化并开启输出 */
        void (*EnableOutput)(void);    /* 开启输出 */
        void (*DisableOutput)(void);   /* 关闭输出 */
    } Ctrl;
    struct Output {
        void (*A)(const char *format, ...);   /* ASSERT */
        void (*E)(const char *format, ...);   /* ERROR  */
        void (*W)(const char *format, ...);   /* WARN   */
        void (*I)(const char *format, ...);   /* INFO   */
        void (*D)(const char *format, ...);   /* DEBUG  */
        void (*V)(const char *format, ...);   /* VERBOSE*/
        void (*Raw)(const char *format, ...); /* 无前缀原始输出 */
    } O;
} LogHandle_t;

extern LogHandle_t LogHandle;
```

### 3.4 EasyLogger 常用接口 `elog.h`

| 接口 | 说明 |
|---|---|
| `ElogErrCode elog_init(void)` | 初始化 EasyLogger 与端口 |
| `void elog_start(void)` | 开启输出 |
| `void elog_stop(void)` | 关闭输出 |
| `void elog_set_output_enabled(bool enabled)` | 运行期开关输出 |
| `bool elog_get_output_enabled(void)` | 查询输出是否开启 |
| `void elog_set_fmt(uint8_t level, size_t set)` | 设置某等级的格式位掩码 |
| `void elog_set_filter_lvl(uint8_t level)` | 设置全局等级过滤 |
| `void elog_set_filter_tag(const char *tag)` | 设置 tag 过滤 |
| `void elog_set_filter_kw(const char *keyword)` | 设置关键字过滤 |
| `void elog_set_filter_tag_lvl(const char *tag, uint8_t level)` | 为指定 tag 单独设置等级 |
| `void elog_set_text_color_enabled(bool enabled)` | 彩色输出开关 |
| `void elog_output(...)` | 通用输出入口（带等级、tag、文件、函数、行号） |
| `void elog_raw_output(unsigned char terminal, const char *format, ...)` | 无前缀原始输出 |
| `void elog_hexdump(...)` | 十六进制转储 |

格式位掩码 `ElogFmtIndex`：

```c
ELOG_FMT_LVL    等级
ELOG_FMT_TAG    tag
ELOG_FMT_TIME   时间
ELOG_FMT_P_INFO 进程信息
ELOG_FMT_T_INFO 任务/线程名
ELOG_FMT_DIR    文件名
ELOG_FMT_FUNC   函数名
ELOG_FMT_LINE   行号
ELOG_FMT_ALL    全部
```

当前各等级格式（`log.c` 中配置）：

| 等级 | 格式 |
|---|---|
| ASSERT | 全部（等级 + tag + 时间 + 任务 + 文件 + 函数 + 行号） |
| ERROR | 等级 + 时间 + 任务 + 函数 + 行号 + 文件 |
| WARN | 等级 + 时间 + 任务 |
| INFO | 等级 + 时间 + 任务 |
| DEBUG | 等级 + 时间 + 任务 + 行号 + 文件 |
| VERBOSE | 等级 + 时间 + 任务 |

### 3.5 端口接口 `elog_port.c`（移植层）

EasyLogger 通过以下弱符号接口与平台对接，本工程已实现：

| 接口 | 说明 |
|---|---|
| `ElogErrCode elog_port_init(void)` | 创建日志互斥锁；UART 后端额外创建 DMA 缓冲区信号量 |
| `void elog_port_output(unsigned char terminal, const char *log, size_t size)` | 后端输出：RTT / UART / FLASH |
| `void elog_port_output_lock(void)` | 输出前加锁，保证单条日志不被其他任务打断 |
| `void elog_port_output_unlock(void)` | 输出后解锁 |
| `const char *elog_port_get_time(void)` | 返回时间字符串，基于 `HAL_GetTick()`，格式 `秒.毫秒s` |
| `const char *elog_port_get_t_info(void)` | 返回当前 FreeRTOS 任务名 |
| `const char *elog_port_get_p_info(void)` | 返回进程名，本工程返回空串 |

### 3.6 崩溃日志接口 `pyro_crashlog.h`

用于 HardFault 场景：平时日志写入 RAM 环形缓冲，HardFault 时把缓冲与异常现场写入 `.noinit`，复位后由文件系统落盘到 `/crash.log`。

| 接口 | 说明 |
|---|---|
| `void pyro_crashlog_push(const char *log, size_t size)` | 向 RAM 环形缓冲追加一条日志，满了覆盖最旧 |
| `void pyro_crashlog_save_fault(const pyro_crashlog_fault_t *fault)` | HardFault 时保存环形缓冲与异常现场，仅内存操作，异常上下文安全 |
| `int pyro_crashlog_has_pending(void)` | 是否有待落盘记录 |
| `const pyro_crashlog_record_t *pyro_crashlog_get_pending(void)` | 获取待落盘记录，无则返回 `NULL` |
| `void pyro_crashlog_clear_pending(void)` | 落盘完成后清除标志 |

关键参数：

| 宏 | 值 | 说明 |
|---|---|---|
| `PYRO_CRASHLOG_LINE_MAX` | 256 | 单条日志最大长度 |
| `PYRO_CRASHLOG_LINE_NUM` | 32 | 环形缓冲条数 |
| `PYRO_CRASHLOG_MAGIC` | `0x43524C47` | 有效记录魔数 |

### 3.7 Flash 日志后端接口 `elog_flash.h`

当 `ELOG_OUTPUT_BACKEND_FLASH` 开启时使用，把日志以固定长度记录追加写入外部 Flash 保留扇区，环形覆盖。

| 接口 | 说明 |
|---|---|
| `void elog_port_flash_set_device(void *device)` | 注入 `IFlashDevice*`，传 `NULL` 关闭 |
| `int elog_port_flash_init(void)` | 扫描已有记录并定位写入游标 |
| `int elog_port_flash_append(const char *log, size_t size)` | 追加一条日志记录 |

## 四、配置项

### 4.1 门面层开关

| 宏 | 位置 | 默认 | 作用 |
|---|---|---|---|
| `PYRO_LOG_ENABLE` | `pyro_log.h` | 1 | 编译期总开关，为 0 时库内日志代码完全裁掉 |

关闭库内日志的三种方式：

```c
/* 1. 修改 pyro_log.h */
#define PYRO_LOG_ENABLE 0

/* 2. 编译选项 */
-DPYRO_LOG_ENABLE=0

/* 3. CMake */
target_compile_definitions(<target> PRIVATE PYRO_LOG_ENABLE=0)
```

### 4.2 EasyLogger 配置 `elog_cfg.h`

| 宏 | 当前值 | 说明 |
|---|---|---|
| `ELOG_OUTPUT_ENABLE` | 已定义 | 总输出开关 |
| `ELOG_OUTPUT_LVL` | `ELOG_LVL_VERBOSE` | 静态输出等级上限 |
| `ELOG_LINE_BUF_SIZE` | 256 | 单行日志缓冲区大小 |
| `ELOG_NEWLINE_SIGN` | `"\r\n"` | 换行符 |
| `ELOG_COLOR_ENABLE` | 已定义 | 彩色输出 |
| `ELOG_FMT_USING_FUNC/DIR/LINE` | 已定义 | 输出函数名 / 文件名 / 行号 |
| `ELOG_ASYNC_OUTPUT_ENABLE` | 未定义 | 异步输出（当前关闭） |
| `ELOG_BUF_OUTPUT_ENABLE` | 未定义 | 缓冲输出（当前关闭） |
| `DEFAULT_TAG` | `"log"` | 默认 tag |
| `DEFAULT_TERMINAL` | 0 | 默认终端号 |

### 4.3 后端选择 `elog_cfg.h`

三选一，必须且只能定义一个，否则 `elog_port.c` 编译报错：

```c
#define ELOG_OUTPUT_BACKEND_RTT      /* 默认：SEGGER RTT */
//#define ELOG_OUTPUT_BACKEND_UART   /* 硬件串口 + DMA */
//#define ELOG_OUTPUT_BACKEND_FLASH  /* 外部 Flash 追加式日志 */
```

UART 后端配置：

| 宏 | 默认 | 说明 |
|---|---|---|
| `ELOG_UART_HANDLE` | `huart1` | 日志串口（huart5 无 TX DMA） |
| `ELOG_UART_TX_BUF_NUM` | 4 | DMA 发送缓冲区数量 |
| `ELOG_UART_TX_WAIT_MS` | 20 | 等待空闲缓冲区超时（ms），超时丢弃该条日志 |

UART 后端使用 DMA 非阻塞发送：`HAL_UART_Transmit_DMA` 异步，源缓冲区必须保持有效，因此用一组缓冲区轮转，由发送完成中断 `elog_port_uart_tx_cplt()` 归还。该函数需在 `HAL_UART_TxCpltCallback` 中转发调用。

Flash 后端配置：

| 宏 | 默认 | 说明 |
|---|---|---|
| `ELOG_FLASH_SECTOR_NUM` | 8 | 日志占用设备末尾的扇区数 |
| `ELOG_FLASH_RECORD_SIZE` | 256 | 单条记录长度（含结尾 `'\0'`） |

## 五、tag 约定

库内日志统一使用大写模块名作为 tag，便于过滤：

`UART`、`CAN`、`SUPERCAP`、`TASK`、`SYS_MON`、`CRC`、`FSM`、`FLASH`

新增模块时沿用「大写模块名」约定，不要使用文件名或临时字符串。

## 六、注意事项

1. **初始化顺序**：`LogHandle.Ctrl.Init()` 必须在 `SystemClock_Config()` 之后、任何日志调用之前；`pyro_log_set_handler()` 必须在驱动初始化之前。
2. **ISR 安全**：elog 的互斥锁在中断上下文不安全。中断里不要直接打日志，应记录标志，由任务上下文延迟上报（参考 UART 驱动的 `report_pending_error()`）。
3. **临界区外打日志**：不要在 `taskENTER_CRITICAL()` 内调用日志接口，容易造成长时间关中断（参考 CAN `send_msg`）。
4. **单行长度**：超过 `ELOG_LINE_BUF_SIZE`（256）会被截断，长内容请自行分段。
5. **格式化开销**：日志在调用点即完成 `vsnprintf`，即使该等级被运行期过滤，格式化开销仍然存在；对高频路径建议用 `PYRO_LOG_ENABLE` 或等级宏在编译期裁掉。
6. **两套开关独立**：`PYRO_LOG_ENABLE`（编译期裁代码）与 `ELOG_OUTPUT_LVL`（运行期过滤）互不影响，可叠加使用。
7. **崩溃日志**：`ELOG_OUTPUT_BACKEND_FLASH` 下日志只进 RAM 环形缓冲，不直接写 Flash；只有 HardFault 才落 `.noinit`，复位后由 `flash_fs_t::init()` 调用 `flush_crash_log()` 写入 `/crash.log`。
