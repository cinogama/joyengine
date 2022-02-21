#define JE_IMPL
#include "jeecs.hpp"

#include <cstdio>
#include <cstdarg>

#ifdef _WIN32
#   include <Windows.h>
#endif

void je_log(int level, const char* format, ...)
{
    va_list va, vb;
    va_start(va, format);
    va_copy(vb, va);

    size_t total_buffer_sz = vsnprintf(nullptr, 0, format, va);
    va_end(va);

    char* buf = (char*)je_mem_alloc(total_buffer_sz + 1);
    vsnprintf(buf, total_buffer_sz, format, vb);
    va_end(vb);

    FILE* output_place = stdout;

    switch (level)
    {
    case JE_LOG_NORMAL:
    case JE_LOG_INFO:
        break;
    case JE_LOG_WARNING:
        output_place = stderr; break;
    case JE_LOG_ERROR:
        output_place = stderr; break;
    case JE_LOG_FATAL:
    default:
        output_place = stderr; break;
    }

#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    switch (level)
    {
    case JE_LOG_NORMAL:
        SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); fputs("[normal] ", output_place);
        break;
    case JE_LOG_INFO:
        SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | FOREGROUND_BLUE | FOREGROUND_GREEN); fputs("[infor] ", output_place);
        break;
    case JE_LOG_WARNING:
        SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN); fputs("[warni] ", output_place);
        break;
    case JE_LOG_ERROR:
        SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | FOREGROUND_RED); fputs("[error] ", output_place);
        break;
    case JE_LOG_FATAL:
    default:
        SetConsoleTextAttribute(handle, BACKGROUND_INTENSITY | BACKGROUND_RED); fputs("[fatal] ", output_place);
        break;
    }
#else


#endif // _WIN32

    fputs(buf, output_place);

#ifdef _WIN32
    SetConsoleTextAttribute(handle, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    fputs("\n", output_place);
#else
    fputs("\033[2;37;0m\n", output_place);
#endif
    je_mem_free(buf);
}