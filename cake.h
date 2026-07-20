#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vadefs.h>
#include <wchar.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "shellapi.h"
#else

#endif

extern void configure_build();

typedef int (*stage_work)(void);

typedef int ck_stage_handle;
typedef void* ck_handle;

typedef struct ck_stage
{
    char* name;
    stage_work work;
    ck_stage_handle* dependencies;
    int depCount;
} ck_stage;

typedef struct
{
    unsigned int count;
    char** files;
    void* data;
} ck_file_group;

typedef struct
{
    char** lines;
    int count;
    int isValid;
} ck_shell_output;

typedef struct
{
    void* shell;
    ck_shell_output std_out;
    ck_shell_output std_err;
} ck_shell_info;

typedef enum
{
    CK_LOG_LEVEL_FATAL,
    CK_LOG_LEVEL_ERROR,
    CK_LOG_LEVEL_WARNING,
    CK_LOG_LEVEL_INFO,
    CK_LOG_LEVEL_DEBUG,
    CK_LOG_LEVEL_TRACE,
} ck_log_level;

typedef struct
{
    ck_stage* stages;
    int stageCount;
    ck_log_level logLevel;
    ck_log_level internalLogLevel;
} ck_state;

static ck_state _internal_ck_state =
{
    .logLevel = CK_LOG_LEVEL_TRACE,
    .internalLogLevel = CK_LOG_LEVEL_INFO
};

// ==================================== LOG API ========================================

static const char* level_strings[] = {"[FATAL]", "[ERROR]", "[WARNING]", "[INFO]", "[DEBUG]", "[TRACE]" };
static const char* level_color[] = {"\033[1;97m\033[101m", "\033[1;31m", "\033[1;33m", "\033[0;97m", "\033[0;92m", "\033[0;34m"};
static const char* color_reset = "\033[0m";

#define CK_FATAL(format, ...) _internal_ck_log(0, CK_LOG_LEVEL_FATAL, format, ##__VA_ARGS__)
#define CK_ERROR(format, ...) _internal_ck_log(0, CK_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define CK_WARN(format, ...)  _internal_ck_log(0, CK_LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define CK_INFO(format, ...)  _internal_ck_log(0, CK_LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define CK_DEBUG(format, ...) _internal_ck_log(0, CK_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define CK_TRACE(format, ...) _internal_ck_log(0, CK_LOG_LEVEL_TRACE, format, ##__VA_ARGS__)

#define _CK_FATAL(format, ...) _internal_ck_log(1, CK_LOG_LEVEL_FATAL, format, ##__VA_ARGS__)
#define _CK_ERROR(format, ...) _internal_ck_log(1, CK_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define _CK_WARN(format, ...)  _internal_ck_log(1, CK_LOG_LEVEL_WARNING, format, ##__VA_ARGS__)
#define _CK_INFO(format, ...)  _internal_ck_log(1, CK_LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define _CK_DEBUG(format, ...) _internal_ck_log(1, CK_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define _CK_TRACE(format, ...) _internal_ck_log(1, CK_LOG_LEVEL_TRACE, format, ##__VA_ARGS__)

void _internal_ck_log(int internal, ck_log_level level, char* format, ...)
{
    if (internal && level > _internal_ck_state.internalLogLevel) { return; }
    if (!internal && level > _internal_ck_state.logLevel) { return; }

    va_list ptr;

    char out_message[32000] = "";

    va_start(ptr, format);
    vsnprintf(out_message, 32000, format, ptr);
    va_end(ptr);

    char formatted_message[32000] = "";

    if (internal)
    {
        sprintf(formatted_message, "%s[CAKE] %s%s\n", level_color[level], out_message, color_reset);
    }
    else
    {
        sprintf(formatted_message, "%s%s %s%s\n", level_color[level], level_strings[level], out_message, color_reset);
    }

    printf("%s", formatted_message);
}

void ck_set_log_level(ck_log_level level)
{
    _internal_ck_state.logLevel = level;
}

void ck_set_internal_log_level(ck_log_level level)
{
    _internal_ck_state.internalLogLevel = level;
}

// ==================================== LOG API ========================================

#define ck_make_stage(name, work, ...) _internal_ck_make_stage(name, work, __VA_ARGS__, 0)
ck_stage_handle _internal_ck_make_stage(char* stage_name, stage_work work, ...)
{
    int argCount = 0;
    va_list args;

    va_start(args, work);
    while (va_arg(args, const ck_stage_handle) != 0)
    {
        argCount++;
    }
    va_end(args);

    ck_state* state = &_internal_ck_state;
    state->stageCount++;
    state->stages = realloc(state->stages, sizeof(ck_stage) * state->stageCount);

    ck_stage* stage = &state->stages[state->stageCount - 1];
    stage->work = work;
    stage->depCount = argCount;
    if (argCount > 0) { stage->dependencies = malloc(sizeof(ck_stage_handle) * argCount); }

    int nameLength = strlen(stage_name);
    stage->name = malloc((nameLength + 1) * sizeof(char));
    strncpy_s(stage->name, nameLength + 1, stage_name, nameLength + 1);

    va_start(args, work);
    for (int i = 0; i < argCount; ++i)
    {
        const ck_stage_handle depHandle = va_arg(args, const ck_stage_handle);
        stage->dependencies[i] = depHandle;
    }
    va_end(args);

    return state->stageCount;
}

void _internal_ck_execute_stage(ck_stage* stage, int indentLevel)
{
    ck_state* state = &_internal_ck_state;

    for (int i = 0; i < stage->depCount; ++i)
    {
        ck_stage* dependencyStage = &state->stages[stage->dependencies[i] - 1];
        _internal_ck_execute_stage(dependencyStage, indentLevel + 1);
    }

    _CK_INFO("Executing Stage: %s", stage->name);
    stage->work();
}

int main(int argc, char** argv)
{
    _CK_INFO("---------- Starting Cake ----------");

    configure_build();

    ck_state* state = &_internal_ck_state;

    if (argc > 1)
    {
        for (int i = 0; i < state->stageCount; ++i)
        {
            ck_stage stage = state->stages[i];
            if (strcmp(argv[1], stage.name) == 0)
            {
                _internal_ck_execute_stage(&stage, 0);
                break;
            }
        }
    }
    else
    {
        _internal_ck_execute_stage(&_internal_ck_state.stages[_internal_ck_state.stageCount - 1], 0);
    }

    _CK_INFO("------------ Finished -------------\n");

    return 0;
}

// ===================================== SHELL API =====================================

void _internal_ck_shell_output(ck_shell_output* output, ck_handle handle)
{
#ifdef WIN32
    int capacity = 10;
    output->lines = malloc(sizeof(char*) * capacity);
    output->count = 0;

    DWORD chunkSize = 4096;
    DWORD bytesRead;
    char readBuffer[chunkSize];

    while(1)
    {
        DWORD result = ReadFile(handle, readBuffer, chunkSize, &bytesRead, NULL);
        if (!result || bytesRead == 0)
        {
            // done reading
            break;
        }

        DWORD lastLine = 0;
        for (DWORD i = 0; i < bytesRead; ++i)
        {
            if (readBuffer[i] == '\n')
            {
                DWORD carriageReturnPadding = 0;
                if (readBuffer[i-1] == '\r')
                {
                    carriageReturnPadding = 1;
                }
                output->lines[output->count] = malloc(i - lastLine + 1);
                memcpy(output->lines[output->count], readBuffer + lastLine, i - lastLine + 1);
                output->lines[output->count][i - lastLine - carriageReturnPadding] = '\0';
                output->count++;

                if (output->count >= capacity)
                {
                    capacity *= 2;
                    output->lines = realloc(output->lines, sizeof(char*) * capacity);
                }

                lastLine = i + 1;
            }
        }
    }

    output->isValid = 1;
#else
#endif
}

#define ck_shell(command, ...) _internal_ck_shell(command, __VA_ARGS__, 0)
ck_shell_output _internal_ck_shell(char* command, ...)
{
#ifdef WIN32
    ck_shell_output result =
    {
        .isValid = 0
    };

    char inCommand[32000];
    va_list args;
    va_start(args, command);
    vsnprintf(inCommand, sizeof(inCommand), command, args);
    va_end(args);

    char formattedCommand[32000];
    sprintf(formattedCommand, "cmd.exe /c %s", inCommand);

    _CK_TRACE("%s", formattedCommand);

    HANDLE std_out_read = INVALID_HANDLE_VALUE;
    HANDLE std_out_write = INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES saAttr =
    {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
        .lpSecurityDescriptor = NULL,
    };

    if (!CreatePipe(&std_out_read, &std_out_write, &saAttr, 0))
    {
        _CK_ERROR("[Cake] Failed to create anonymous pipe when executing ck_shell with: \"%s\"", inCommand);
        _CK_ERROR("[Cake] WIN32 Error Code: %d", GetLastError());
        return result;
    }

    STARTUPINFO si =
    {
        .cb = sizeof(STARTUPINFO),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdOutput = std_out_write,
        .hStdError = std_out_write,
        .hStdInput = GetStdHandle(STD_INPUT_HANDLE),
    };

    PROCESS_INFORMATION pi = {0};

    if (!CreateProcess
    (
        NULL,
        formattedCommand,
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    ))
    {
        _CK_ERROR("[Cake] Failed to create process for command: \"%s\"", inCommand);
        _CK_ERROR("[Cake] WIN32 Error Code: %d", GetLastError());
        return result;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(std_out_write);

    _internal_ck_shell_output(&result, std_out_read);

    CloseHandle(std_out_read);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return result;
#else
#endif
}

void ck_shell_free_output(ck_shell_output* output)
{
#ifdef WIN32
    for (int i = 0; i < output->count; ++i)
    {
        free(output->lines[i]);
    }
    free(output->lines);
#else
#endif
}

// ===================================== SHELL API =====================================

#define ck_make_path(format, ...) _internal_ck_make_path(format, ##__VA_ARGS__)
char* _internal_ck_make_path(char* format, ...)
{
#ifdef WIN32
    char pathBuffer[32000] = "";
    
    va_list args;
    va_start(args, format);
    vsnprintf(pathBuffer, sizeof(pathBuffer), format, args);
    va_end(args);

    int pathLength = strlen(pathBuffer);
    for (int i = 0; i < pathLength; ++i)
    {
        if (pathBuffer[i] == '/') { pathBuffer[i] = '\\'; }
    }

    char* path = malloc(pathLength + 1);
    memcpy(path, pathBuffer, pathLength);
    path[pathLength] = '\0';

    return path;
#else
#endif
}
