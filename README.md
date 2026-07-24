# Cake
A single header build system written in C for C/C++ projects.

## Description

The objective of Cake is to provide a build system that can be configured using a commonly understood language. In this case, the very language your project is written in. It provides a shell API, an easy to use stage system, and a built in logging system.

### Notes

* Cake currently only compiles on WIN32. This won't be for long, UNIX support coming soon!
* Cake is a work in progress.

## Getting Started

### Installing

1. Simply clone or copy cake.h to your project folder
2. Create a C file and include the header
3. Define `void configure_build()` and build using your favorite compiler.

### Setting up stages
```c
// cake.c
#include "cake.h"

ck_stage_result stage_scaffold()
{
    // do scaffold work...

    return CK_STAGE_SUCCESS;
}

ck_stage_result stage_build()
{
    // do build work...

    return CK_STAGE_SUCCESS;
}

void configure_build()
{
    ck_stage_handle scaffold = ck_make_stage("scaffold", &stage_scaffold, 0);
    ck_make_stage("build", &stage_build, scaffold);
}
```
The above snippet creates 2 stages and configures them such that stage "build" depends on stage "scaffold".

When running cake with no arguments, it will automatically execute the last stage that was registered. In the case of the snippet above, it would execute "build". Cake also accepts one argument which is a stage name. In the case where an argument is provided, Cake will attempt to find a stage by that name and execute it.
* e.g. `.\cake clean`, `.\cake build_tooling`

#### `ck_make_stage`
* `ck_make_stage` takes the stage name, the stage work, and a list of dependency stages or 0 if no dependencies.
* The "stage work" is a pointer to a function of type `ck_stage_result (*stage_work)(void)`.
* It returns a ck_stage_handle which can be passed into subsequent stages as a dependency.

### Cake Shell Utility
* Cake provides a shell utility in the form of `ck_shell(...)`
* It functions in the same way as `printf` where it accepts a format string and arguments, except that `ck_shell` executes the command.
* It returns a struct `ck_shell_output` containing all output lines resulting from the command. This includes stdout and stderr.

Below is an example of a stage that finds all files in a directory matching the pattern "*.json" and builds the compile_commands.json file for LLVM.
```c
// WIN32 Example
ck_stage_result stage_generate_compile_commands()
{
    ck_shell_output std_out = ck_shell("where /r ./build/bin-obj/ *.json");

    ck_shell("echo [ > compile_commands.json");
    for (int i = 0; i < std_out.count; ++i)
    {
        ck_shell("type %s >> compile_commands.json", std_out.lines[i]);
    }
    ck_shell("echo ] >> compile_commands.json");

    ck_shell_free_output(&std_out);

    return CK_STAGE_SUCCESS;
}
```

### Cake Logging Utility
* Cake provides a logging utility, it includes log levels, and level cutoff for both user and Cake logs.
```c
void configure_build()
{
    ck_set_log_level(CK_LOG_LEVEL_INFO);
    ck_set_internal_log_level(CK_LOG_LEVEL_DEBUG);

    int errorCode = 100;

    CK_FATAL("Fatal log: %d", errorCode);
    CK_ERROR("Error log: %d", errorCode);
    CK_WARN("Warning log: %d", errorCode);
    CK_INFO("Info log: %d", errorCode);
    CK_DEBUG("Debug log: %d", errorCode);
    CK_TRACE("Trace log: %d", errorCode);

    // ...
}
```

### Using multiple "build scripts"
Cake supports multiple build scripts (for subprojects for example). As an example, we'll have a main cake file that calls into a subproject cake file. The below method is convenient because it allows you to continue building cake with a single c file.
```c
// subproject_cake.c
#include "cake.h"

ck_stage_result stage_build_subproject()
{
    // do build_subproject work...

    return CK_STAGE_SUCCESS;
}

ck_stage_result configure_subproject_build()
{
    ck_stage_handle build_subproject = ck_make_stage("build_subproject", &stage_build_subproject, 0);

    return build_subproject;
}
```

```c
// cake.c
#include "cake.h

ck_stage_result stage_build()
{
    // do build work...

    return CK_STAGE_SUCCESS;
}

#include "subproject/subproject_cake.c"

void configure_build()
{
    ck_stage_handle subproject_stages = configure_subproject_build();
    ck_make_stage("build", &stage_build, subproject_stages);
}
```
Then build and run as usual:
```
clang cake.c -o cake && ./cake
```

## License

This project is licensed under the GPL 3.0 License - see the [LICENSE](LICENSE) file for details

## Acknowledgments

* [Tsoding Daily](https://www.youtube.com/watch?v=eRt7vhosgKE&list=PLpM-Dvs8t0Va1sCJpPFjs2lKzv8bw3Lpw) - Nob build system
