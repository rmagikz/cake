#pragma once

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <vadefs.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "shellapi.h"
#else

#endif

extern void configure_build();

typedef int (*stage_work)(void);

typedef int ck_handle;

typedef struct ck_stage
{
    stage_work work;
    ck_handle* dependencies;
    int depCount;
} ck_stage;

typedef struct
{
    ck_stage stage;
    char* name;
} ck_stage_name_pair;

// ===================================== INTERNAL =====================================

typedef struct
{
    ck_stage_name_pair* stageNamePair;
    int pairCount;
} ck_state;

static ck_state _internal_ck_state = {};

#define ck_make_stage(name, work, ...) _impl_ck_make_stage(name, work, __VA_ARGS__, 0)
ck_handle _impl_ck_make_stage(char* stage_name, stage_work work, ...)
{
    int argCount = 0;
    va_list args;

    va_start(args, work);
    while (va_arg(args, const ck_handle) != 0)
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
    if (argCount > 0) { pair->stage.dependencies = malloc(sizeof(ck_handle) * argCount); }

    int nameLength = strlen(stage_name);
    pair->name = malloc((nameLength + 1) * sizeof(char));
    strncpy_s(pair->name, nameLength + 1, stage_name, nameLength + 1);

    va_start(args, work);
    for (int i = 0; i < argCount; ++i)
    {
        const ck_handle depHandle = va_arg(args, const ck_handle);
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

// ===================================== INTERNAL =====================================

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
    int pathLength = strlen(path);
    char* doubleNullPath = malloc(pathLength + 2);
    strncpy_s(doubleNullPath, pathLength + 1, path, pathLength + 1);
    doubleNullPath[pathLength] = '\0';
    doubleNullPath[pathLength + 1] = '\0';

    SHFILEOPSTRUCTA fileOp = 
    {
        .wFunc = FO_DELETE,
        .pFrom = doubleNullPath,
        .fFlags = FOF_NO_UI | FOF_NOCONFIRMATION,
    };

    return !SHFileOperationA(&fileOp);
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

void* ck_create_file(char* path)
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

BOOL ck_write_file(void* handle, char* data, unsigned long dataSize)
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

BOOL ck_append_file(void* handle, char* data, unsigned long dataSize)
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
