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
    CK_LOG_LEVEL_INTERNAL,
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
} ck_state;

static ck_state _internal_ck_state =
{
    .logLevel = CK_LOG_LEVEL_INFO
};

// ==================================== LOG API ========================================

static const char* level_strings[] = {"[CAKE]", "[FATAL]", "[ERROR]", "[WARNING]", "[INFO]", "[DEBUG]", "[TRACE]" };
static const char* level_color[] = {"\033[0m", "\033[1;97m\033[101m", "\033[1;31m", "\033[1;33m", "\033[0;97m", "\033[0;92m", "\033[0;34m"};
static const char* color_reset = "\033[0m";

#define CK_FATAL(format, ...) ck_log(CK_LOG_LEVEL_FATAL, format, ##__VA_ARGS__)

#define CK_ERROR(format, ...) ck_log(CK_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

#define CK_WARN(format, ...) ck_log(CK_LOG_LEVEL_WARNING, format, ##__VA_ARGS__)

#define CK_INFO(format, ...) ck_log(CK_LOG_LEVEL_INFO, format, ##__VA_ARGS__)

#define CK_DEBUG(format, ...) ck_log(CK_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

#define CK_TRACE(format, ...) ck_log(CK_LOG_LEVEL_TRACE, format, ##__VA_ARGS__)

#define CK_INTERNAL(format, ...) ck_log(CK_LOG_LEVEL_INTERNAL, format, ##__VA_ARGS__)

void ck_log(ck_log_level level, char* format, ...)
{
    if (level > _internal_ck_state.logLevel) { return; }

    va_list ptr;

    char out_message[32000] = "";

    va_start(ptr, format);
    vsnprintf(out_message, 32000, format, ptr);
    va_end(ptr);

    char formatted_message[32000] = "";
    sprintf(formatted_message, "%s%s %s%s\n", level_color[level], level_strings[level], out_message, color_reset);
    printf("%s", formatted_message);
}

void ck_set_log_level(ck_log_level level)
{
    if (level == 0)
    {
        CK_FATAL("Setting Cake log level to 0 is not permitted.");
        return;
    }
    _internal_ck_state.logLevel = level;
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

    CK_INTERNAL("Executing Stage: %s", stage->name);
    stage->work();
}

int main(int argc, char** argv)
{
    CK_INTERNAL("---------- Starting Cake ----------\n");

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

    return 0;
}

// ===================================== SHELL API =====================================

void _internal_ck_shell_output(ck_shell_output* output, ck_handle handle)
{
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
}

#define ck_shell(command, ...) _internal_ck_shell(command, __VA_ARGS__, 0)
ck_shell_output _internal_ck_shell(char* command, ...)
{
#ifdef WIN32
    ck_shell_output result =
    {
        .isValid = 0
    };

    char commandBuffer[1024];
    va_list args;
    va_start(args, command);
    vsnprintf(commandBuffer, sizeof(commandBuffer), command, args);
    va_end(args);

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
        CK_ERROR("Cake: Failed to create anonymous pipe when executing ck_shell with: \"%s\"", commandBuffer);
        CK_ERROR("Cake: WIN32 Error Code: %d", GetLastError());
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
        commandBuffer,
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
        CK_ERROR("Cake: Failed to create process for command: \"%s\"", commandBuffer);
        CK_ERROR("Cake: WIN32 Error Code: %d", GetLastError());
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
