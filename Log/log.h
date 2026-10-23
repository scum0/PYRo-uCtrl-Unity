#ifndef LOG_H
#define LOG_H

#include "SEGGER_RTT.h"
#include "elog.h"

typedef struct LogHandle
{
    struct Control
    {
        void(*Init)(void);
        void(*EnableOutput)(void);
        void(*DisableOutput)(void);
    }Ctrl;
    struct Output
    {
        void(*A)(const char *format, ...);
        void(*E)(const char *format, ...);
        void(*W)(const char *format, ...);
        void(*I)(const char *format, ...);
        void(*D)(const char *format, ...);
        void(*V)(const char *format, ...);
        void(*Raw)(const char *format, ...);
    }O;

    
}LogHandle_t;

extern LogHandle_t LogHandle;
#endif
