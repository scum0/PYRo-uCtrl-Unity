# PYRo 日志系统说明

本文档说明 PYRo 工程的日志架构、驱动层日志内容，以及通过宏控制日志的方法。

## 一、整体架构

日志采用「库层门面 + 应用层注入」的分层设计，依赖方向单向：

```
应用层  App/pyro_init_thread.cpp ── pyro_log_to_elog 适配器（唯一 elog 消费者）
CubeMX  main.c ── LogHandle.Ctrl.Init()（初始化 elog）
                                        │ (注入)
                                        ▼
库层门面  PYRo/Core/Def/pyro_log.h/.c  ◄── PYRO_LOGI/E/W/D/V 宏
              ▲
              │ 库内模块只依赖此门面
   ┌──────┬───┴───┬────────┬─────────┬──────┐
 UART   CAN    Task    CRC算法   FSM   超电
驱动   驱动    层       层      层    组件
```

- **库内模块**（驱动、任务、算法）不直接依赖 EasyLogger，只依赖门面 `pyro_log.h`。
- **应用层**是唯一认识 elog 的地方，通过适配器把库内日志转发给 elog。
- 换日志实现、整体关日志，都只影响门面这一处。

## 二、日志 API 与宏

### 输出宏

定义在 `PYRo/Core/Def/pyro_log.h`：

```c
PYRO_LOGA(tag, fmt, ...)   // ASSERT
PYRO_LOGE(tag, fmt, ...)   // ERROR
PYRO_LOGW(tag, fmt, ...)   // WARN
PYRO_LOGI(tag, fmt, ...)   // INFO
PYRO_LOGD(tag, fmt, ...)   // DEBUG
PYRO_LOGV(tag, fmt, ...)   // VERBOSE
```

用法：第一个参数是 tag，后面是 printf 风格格式串。

```c
PYRO_LOGI("UART", "%s init success", name);
PYRO_LOGE("CAN", "%s send_msg failed, id=0x%03lX", can_name, id);
```

### 控制宏

| 宏 | 位置 | 作用 |
|---|---|---|
| `PYRO_LOG_ENABLE` | `pyro_log.h`，默认 1 | 编译期总开关。为 0 时所有 `PYRO_LOGx` 展开成 `((void)0)`，日志代码完全裁掉，零开销 |
| `pyro_log_set_handler(fn)` | 应用层调用 | 运行期注入日志实现。传 NULL 则静默丢弃 |

### 关闭日志的三种方式

1. 改 `pyro_log.h`：`#define PYRO_LOG_ENABLE 0`
2. 编译选项：`-DPYRO_LOG_ENABLE=0`
3. CMake：`target_compile_definitions(... PYRO_LOG_ENABLE=0)`

### 注入位置

`App/pyro_init_thread.cpp` 的 `pyro_log_to_elog()` 适配器，在 `pyro_init_thread` 开头调用 `pyro_log_set_handler()` 注册。必须在任何驱动打日志之前完成。

## 三、各模块日志内容

### UART（tag `"UART"`）

| 位置 | 等级 | 内容 |
|---|---|---|
| `report_pending_error()` | ERROR | `%s enable rx dma failed`（延迟上报） |
| `report_pending_error()` | ERROR | `%s rx error, HAL error code=0x%08lX`（延迟上报） |
| `reset()` | INFO | `%s reconfig, baud=%lu, len=%s, stop=%s` |
| `reset()` | ERROR | `%s disable rx dma failed` |
| `reset()` | ERROR | `%s HAL_UART_DeInit failed` |
| `reset()` | ERROR | `%s HAL_UART_Init failed` |
| `reset()` | INFO | `%s init success` |

接收故障和错误中断采用**延迟上报**：ISR 里只记标志到 `state.pending_error_code` / `state.pending_rx_enable_fail`，由任务上下文的 `write()` / `reset()` / `disable_rx_dma()` 调用 `report_pending_error()` 补打。

### CAN（tag `"CAN"`）

| 位置 | 等级 | 内容 |
|---|---|---|
| `recover_bus_off()` | WARN | `%s Bus-Off detected, try auto-recover` |
| `recover_bus_off()` | ERROR | `%s auto-recover start failed` |
| `recover_bus_off()` | ERROR | `%s auto-recover activate notify failed` |
| `recover_bus_off()` | INFO | `%s auto-recover Bus-Off success` |
| `init()` | ERROR | `%s config filter fail` |
| `init()` | ERROR | `%s config global filter fail` |
| `init()` | ERROR | `%s config fifo watermark fail` |
| `init()` | INFO | `%s init ok` |
| `start()` | ERROR | `%s start failed` |
| `start()` | ERROR | `%s activate notification failed` |
| `start()` | INFO | `%s start ok` |
| `send_msg()` | ERROR | `send_msg invalid param, id=0x%03lX` |
| `send_msg()` | WARN | `%s send_msg skipped, bus not ready` |
| `send_msg()` | WARN | `%s send_msg drop, tx fifo full, id=0x%03lX`（两处） |
| `send_msg()` | ERROR | `%s send_msg failed, id=0x%03lX, hal_status=%u` |

### 超电（tag `"SUPERCAP"`）

| 位置 | 等级 | 内容 |
|---|---|---|
| `run_loop_impl()` | INFO | `online`（上线时打一次） |
| `run_loop_impl()` | WARN | `rx crc check failed, frame dropped` |
| `run_loop_impl()` | WARN | `offline, no feedback for 120ms` |

### Task 层（tag `"TASK"`，`pyro_task.cpp`）

| 等级 | 内容 |
|---|---|
| WARN | `task[%s] already start` |
| INFO | `stop() called, delete task: %s` |
| INFO | `SUSPEND task: %s` |
| INFO | `RESUME task: %s` |
| ERROR | `task[%s] init() fail, ret=%d` |
| INFO | `CREATE task[%s] stack=%u words, prio=%u` |
| ERROR | `CREATE FAIL task[%s], xTaskCreate return fail(heap insufficient)` |
| WARN | `task[%s] run_loop exit, will delete` |
| ERROR | `monitor table full! increase MONITOR_MAX_TASK` |

### 系统巡检（tag `"SYS_MON"`，`sys_task_monitor.cpp`）

| 等级 | 内容 |
|---|---|
| INFO | `%-6s remain_stack_min: %1lu word` |
| WARN | `[STACK WARN] %s stack remain only %lu word !` |

### 其他层（非驱动，但走同一门面）

- **CRC 算法**（tag `"CRC"`）：4 处 WARN，CRC16 / CRC8 校验失败。
- **FSM**（tag `"FSM"`）：1 处 INFO，状态切换。

## 四、tag 一览

`UART`、`CAN`、`SUPERCAP`、`TASK`、`SYS_MON`、`CRC`、`FSM`

## 五、关键设计点

1. **ISR 安全**：中断上下文的日志（UART 错误中断、rx dma 启动失败）不直接打，而是记标志 + 任务上下文延迟上报，因为 elog 的互斥锁在 ISR 里不安全。
2. **临界区外打日志**：CAN `send_msg` 里 FIFO 满 / 失败的日志都放在 `taskEXIT_CRITICAL()` 之后。
3. **零开销开关**：`PYRO_LOG_ENABLE=0` 时日志代码不编译进固件。
4. **两套开关配合**：`PYRO_LOG_ENABLE`（编译期裁代码）和 elog 的 `ELOG_OUTPUT_LVL`（运行期过滤等级）是独立的，可叠加使用。

## 六、日志输出后端

后端选择在 `PYRo/Log/elog/port/elog_cfg.h` 中通过宏切换：

```c
#define ELOG_OUTPUT_BACKEND_RTT      // 默认走 SEGGER RTT
//#define ELOG_OUTPUT_BACKEND_UART   // 需要串口输出时打开
```

串口后端使用 DMA 非阻塞发送，相关配置：

```c
#define ELOG_UART_HANDLE          huart1   // 日志串口（huart1/huart7/huart10，huart5 无 TX DMA）
#define ELOG_UART_TX_BUF_NUM      4        // DMA 发送缓冲区数量
#define ELOG_UART_TX_WAIT_MS      20       // 等待空闲缓冲区超时(ms)
```

## 七、当前覆盖情况

**已有日志**：UART、CAN、超电、Task、SYS_MON、CRC、FSM。

**仍无日志**（后续可补范围）：电机（DJI / DM）、BMI088、裁判系统、遥控器、WS2812、图像、INS 等。
