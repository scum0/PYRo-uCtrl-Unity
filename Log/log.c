/**
 * @file log.c
 * @brief 日志抽象适配层实现，基于EasyLogger
 * @note 调用顺序：LogHandle.Ctrl.Init()
 *       Init() 内部已完成初始化并开启输出，无需再手动调用 EnableOutput()
 */
#include "log.h"

#include <stdarg.h>
#include <stdio.h>

static void Init();
static void EnableOutput();
static void DisableOutput();

static void output_vargs(uint8_t level, const char *format, va_list args);

static void OutputA(const char *format, ...);
static void OutputE(const char *format, ...);
static void OutputW(const char *format, ...);
static void OutputI(const char *format, ...);
static void OutputD(const char *format, ...);
static void OutputV(const char *format, ...);
static void OutputRaw(const char *format, ...);

//TBD : Add the function to store log in flash
LogHandle_t LogHandle =
{
    .Ctrl.Init          = Init,
    .Ctrl.EnableOutput  = EnableOutput,
    .Ctrl.DisableOutput = DisableOutput,
    .O.A                = OutputA,
    .O.E                = OutputE,
    .O.W                = OutputW,
    .O.I                = OutputI,
    .O.D                = OutputD,
    .O.V                = OutputV,
    .O.Raw              = OutputRaw,
};

/**
 * @brief 日志初始化：elog底层初始化、配置各等级日志格式、开启终端彩色并开启输出
 * @note 唯一的初始化入口，调用后日志即可正常输出
 */
static void Init()
{
    elog_init();

    /* set EasyLogger log format */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TIME | ELOG_FMT_T_INFO | ELOG_FMT_FUNC | ELOG_FMT_LINE | ELOG_FMT_DIR);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TIME | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TIME | ELOG_FMT_T_INFO);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_LVL | ELOG_FMT_TIME | ELOG_FMT_T_INFO | ELOG_FMT_LINE | ELOG_FMT_DIR);
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_LVL | ELOG_FMT_TIME | ELOG_FMT_T_INFO);

    /* Enable color 开启终端彩色日志 */
    elog_set_text_color_enabled(true);

    /* 初始化完成后直接开启输出 */
    elog_start();
}

/**
 * @brief 开启日志输出
 */
static void EnableOutput()
{
    elog_start();
}

/**
 * @brief 关闭日志输出
 */
static void DisableOutput()
{
    elog_stop();
}

/**
 * @brief 各等级日志输出封装，统一使用终端0与默认tag
 * @note elog_output 为可变参函数，无法用 va_list 转发，
 *       故此处先用 vsnprintf 将参数格式化为字符串，再以 "%s" 交给 elog_output
 */
static void output_vargs(uint8_t level, const char *format, va_list args)
{
    char buf[ELOG_LINE_BUF_SIZE];

    vsnprintf(buf, sizeof(buf), format, args);

    switch (level)
    {
    case ELOG_LVL_ASSERT:
        elog_a(DEFAULT_TERMINAL, DEFAULT_TAG, "%s", buf);
        break;
    case ELOG_LVL_ERROR:
        elog_e(DEFAULT_TERMINAL, DEFAULT_TAG, "%s", buf);
        break;
    case ELOG_LVL_WARN:
        elog_w(DEFAULT_TERMINAL, DEFAULT_TAG, "%s", buf);
        break;
    case ELOG_LVL_INFO:
        elog_i(DEFAULT_TERMINAL, DEFAULT_TAG, "%s", buf);
        break;
    case ELOG_LVL_DEBUG:
        elog_d(DEFAULT_TERMINAL, DEFAULT_TAG, "%s", buf);
        break;
    case ELOG_LVL_VERBOSE:
        elog_v(DEFAULT_TERMINAL, DEFAULT_TAG, "%s", buf);
        break;
    default:
        break;
    }
}

static void OutputA(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    output_vargs(ELOG_LVL_ASSERT, format, args);
    va_end(args);
}

static void OutputE(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    output_vargs(ELOG_LVL_ERROR, format, args);
    va_end(args);
}

static void OutputW(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    output_vargs(ELOG_LVL_WARN, format, args);
    va_end(args);
}

static void OutputI(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    output_vargs(ELOG_LVL_INFO, format, args);
    va_end(args);
}

static void OutputD(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    output_vargs(ELOG_LVL_DEBUG, format, args);
    va_end(args);
}

static void OutputV(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    output_vargs(ELOG_LVL_VERBOSE, format, args);
    va_end(args);
}

/**
 * @brief 原始日志输出，不带等级/时间等前缀
 */
static void OutputRaw(const char *format, ...)
{
    char buf[ELOG_LINE_BUF_SIZE];
    va_list args;

    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    elog_raw(DEFAULT_TERMINAL, "%s", buf);
}
