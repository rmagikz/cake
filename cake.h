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
    stage_work work;
    ck_stage_handle* dependencies;
    int depCount;
} ck_stage;

typedef struct
{
    ck_stage stage;
    char* name;
} ck_stage_name_pair;

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
} ck_shell_output;

typedef struct
{
    void* shell;
    ck_shell_output std_out;
    ck_shell_output std_err;
} ck_shell_info;

typedef struct
{
    ck_stage_name_pair* stageNamePair;
    int pairCount;
} ck_state;

static ck_state _internal_ck_state = {};

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
    state->pairCount++;
    state->stageNamePair = realloc(state->stageNamePair, sizeof(ck_stage_name_pair) * state->pairCount);

    ck_stage_name_pair* pair = &state->stageNamePair[state->pairCount - 1];
    pair->stage.work = work;
    pair->stage.depCount = argCount;
    if (argCount > 0) { pair->stage.dependencies = malloc(sizeof(ck_stage_handle) * argCount); }

    int nameLength = strlen(stage_name);
    pair->name = malloc((nameLength + 1) * sizeof(char));
    strncpy_s(pair->name, nameLength + 1, stage_name, nameLength + 1);

    va_start(args, work);
    for (int i = 0; i < argCount; ++i)
    {
        const ck_stage_handle depHandle = va_arg(args, const ck_stage_handle);
        pair->stage.dependencies[i] = depHandle;
    }
    va_end(args);

    return state->pairCount;

}

void _internal_ck_execute_stage(ck_stage* stage)
{
    ck_state* state = &_internal_ck_state;

    for (int i = 0; i < stage->depCount; ++i)
    {
        ck_stage* dependencyStage = &state->stageNamePair[stage->dependencies[i] - 1].stage;
        _internal_ck_execute_stage(dependencyStage);
    }

    stage->work();
}

int main(int argc, char** argv)
{
    configure_build();

    ck_state* state = &_internal_ck_state;

    if (argc > 1)
    {
        for (int i = 0; i < state->pairCount; ++i)
        {
            ck_stage_name_pair pair = state->stageNamePair[i];
            if (strcmp(argv[1], pair.name) == 0)
            {
                _internal_ck_execute_stage(&pair.stage);
                break;
            }
        }
    }
    else
    {
        _internal_ck_execute_stage(&_internal_ck_state.stageNamePair[_internal_ck_state.pairCount-1].stage);
    }

    return 0;
}

#define CHUNK_SIZE 4096
void _internal_ck_shell_output(ck_shell_output* output, ck_handle handle)
{
    output->lines = malloc(sizeof(char*) * 10);
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
                lastLine = i + 1;
            }
        }
    }
}

#define ck_shell(command, ...) _internal_ck_shell(0, command, __VA_ARGS__, 0)
#define ck_shell_out(std_out, command, ...) _internal_ck_shell(std_out, command, __VA_ARGS__, 0)
void _internal_ck_shell(ck_shell_output* std_out, char* command, ...)
{
#ifdef WIN32
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

    if (std_out != 0 && !CreatePipe(&std_out_read, &std_out_write, &saAttr, 0))
    {
        // handle errors.
        return;
    }

    STARTUPINFO si =
    {
        .cb = sizeof(STARTUPINFO),
        .dwFlags = STARTF_USESTDHANDLES,
        .hStdOutput = std_out == 0 ? GetStdHandle(STD_OUTPUT_HANDLE) : std_out_write,
        .hStdError = std_out == 0 ? GetStdHandle(STD_ERROR_HANDLE) : std_out_write,
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
        // handle errors.
        return;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    if (std_out != 0)
    {
        CloseHandle(std_out_write);
        _internal_ck_shell_output(std_out, std_out_read);
        CloseHandle(std_out_read);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
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

BOOL ck_mkdir(char* path)
{
#ifdef _WIN32
    return CreateDirectory(path, 0);
#else

#endif
}

BOOL ck_rmdir(char* path)
{
#ifdef _WIN32
    char command[1024];
    sprintf(command, "cmd.exe /c rmdir /s /q \"%s\"", path);
    return system(command) == 0;
#else
#endif
}

BOOL ck_rmfile(char* path)
{
#ifdef _WIN32
    return DeleteFile(path);
#else
#endif
}

ck_handle ck_create_file(char* path)
{
#ifdef _WIN32
    HANDLE file = CreateFile
    (
        path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    
    return file;
#else
#endif
}

ck_handle ck_open_file(char* path)
{
#ifdef _WIN32
    HANDLE file = CreateFile
    (
        path,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    
    return file;
#else
#endif
}

BOOL ck_close_file(ck_handle handle)
{
    CloseHandle(handle);
    return 1;
}

BOOL ck_write_file(ck_handle handle, char* data, unsigned long dataSize)
{
#ifdef _WIN32
    DWORD dataWritten = 0;
    BOOL result = WriteFile(handle, data, dataSize, &dataWritten, NULL);

    if (result && dataSize == dataWritten)
    {
        return 1;
    }

    return 0;
#else
#endif
}

BOOL ck_append_file(ck_handle handle, char* data, unsigned long dataSize)
{
#ifdef _WIN32
    SetFilePointer(handle, 0, NULL, FILE_END);

    DWORD dataWritten = 0;
    BOOL result = WriteFile(handle, data, dataSize, &dataWritten, NULL);

    if (result && dataSize == dataWritten)
    {
        return 1;
    }

    return 0;
#else
#endif
}

ck_file_group ck_match_files(char* pattern)
{
    ck_file_group result = {};
    WIN32_FIND_DATA findFileData;

    // figure out length of the base path.
    unsigned int basepathSize = 0;
    for (int i = strlen(pattern); i >= 0; --i)
    {
        if (pattern[i] == '/')
        {
            basepathSize = i + 1;
            break;
        }
    }

    // Find the total count of files.
    HANDLE hFind = FindFirstFileA(pattern, &findFileData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            result.count++;
        }
        while (FindNextFile(hFind, &findFileData) != 0);
    }
    else
    {
        // handle errors
        __builtin_trap();
    }
    FindClose(hFind);

    // Find and store filenames
    hFind = FindFirstFileA(pattern, &findFileData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        char** filenames = malloc(sizeof(char*) * result.count);
        result.count = 0;

        do
        {
            unsigned int filenameSize = strlen(findFileData.cFileName);
            unsigned long fullFilenameSize = filenameSize + basepathSize + 1;

            char* fullFilename = malloc(fullFilenameSize);

            memcpy(fullFilename, pattern, basepathSize);
            memcpy(fullFilename + basepathSize, findFileData.cFileName, filenameSize);

            // add null terminator
            *(char*)(fullFilename + basepathSize + filenameSize + 1) = '\0';

            filenames[result.count] = fullFilename;
            result.count++;
        }
        while (FindNextFile(hFind, &findFileData) != 0);

        result.files = filenames;
        result.data = hFind;
    }
    else
    {
        // handle errors
        __builtin_trap();
    }
    FindClose(hFind);

    return result;
}

void ck_free_matched_files(ck_file_group* fileGroup)
{
    for (unsigned int i = 0; i < fileGroup->count; ++i)
    {
        free(fileGroup->files[i]);
    }

    free(fileGroup->files);
}
