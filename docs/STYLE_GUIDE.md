# TPBench C Code Style Guide

This document defines the coding style conventions for the TPBench framework.
Follow these guidelines for consistency across all `tpb-*` source files.

## 1. Function Naming Conventions

### Public Functions
- Use the `tpb_` prefix for all public functions exposed in header files.
- Example: `tpb_parse_args()`, `tpb_register_kernel()`

### Static (Local) Functions
- Use `<verb>_<noun>` naming pattern without the `tpb_` prefix.
- Example: `init_cliout()`, `format_sigfig()`, `check_dtype_support()`

## 2. Function Declarations

### Implementation Files (*.c)
- Declare prototypes of all local static functions at the beginning of the file.
- Group them under a "Local Function Prototypes" section comment.

```c
/* Local Function Prototypes */
static void init_cliout(void);
static int format_sigfig(double val, int sigfig, char *buf, size_t len);
```

### Header Files (*.h)
- Declare all public function prototypes with Doxygen documentation.
- Use `/** @brief */` style for public API documentation.

```c
/**
 * @brief Parse command line arguments.
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, error code on failure
 */
int tpb_parse_args(int argc, char **argv);
```

## 3. Function Documentation

### Location
- Place detailed comments **before** the function prototype declaration.
- Keep implementation files clean with minimal inline documentation.

### Format for Static Functions
Use plain `/* */` block comments (not Doxygen):

```c
/*
 * Initialize the CLI output formatter.
 * Sets up default formatting options for console output.
 */
static void init_cliout(void);
```

### Format for Public Functions (in headers)

Per-item execute following rules:

1. Use Doxygen-compatible comments, param lists and return 
2. clearly describe the behavior of the function: Run/Register/Set/Convert/Parse/Format/Clean ... on what kinds/names/types/... of structures/files/objects/... for what reasons. Input what, output what a return what.

```c
/**
 * @brief Register a new benchmark kernel and set its name/note/function pointers, appending to the global kernel_all lists. Input the char* name and functions, return the error code.
 * @param name Kernel name (max 32 characters)
 * @param funcs Pointer to kernel function table
 * @return 0 on success, negative error code on failure
 */
int tpb_register_kernel(const char *name, tpb_k_func *funcs);
```

## 4. Struct Member Documentation

### Format
For struct members, use Doxygen's inline member documentation style with `/**<` placed after the member:

```c
struct tpb_timer {
    uint64_t start;     /**< Start time value */
    uint64_t end;       /**< End time value */
    double resolution;  /**< Timer resolution in seconds */
    int type;           /**< Timer type identifier */
};
```

### Guidelines
- Place the `/**<` comment immediately after the member declaration
- Use a single space between the member and the comment
- Provide a brief but descriptive explanation of the member's purpose
- Align comments when documenting multiple members in a struct

```c
/* Correct - aligned comments */
struct example {
    int count;      /**< Number of items */
    char *name;     /**< Item name string */
    double value;   /**< Numeric value */
};
```

## 5. Comment Styles

### Block Comments
Use `/* */` for code block comments, even for single-line comments:

```c
/* Initialize the timer subsystem */
init_timer();

/* This is a multi-line comment explaining
   complex logic in the following code block */
```

### Inline Comments
Use `//` for line/variable comments:

```c
int count = 0;  // number of iterations
```

### Long Inline Comments
If an inline comment would exceed column 85, place it on the line before:

```c
/* This variable tracks the cumulative error across all iterations */
double cumulative_error;
```

### Prohibited Styles
- Do NOT use `===` or `---` splitter lines in comments
- Do NOT use decorative comment borders

```c
/* Wrong - don't do this */
/* ============================================== */
/* Section Header                                  */
/* ============================================== */

/* Correct */
/* Section Header */
```

## 6. Line Length

### General Rule
Keep lines within 85 columns.

### Exceptions
The following may exceed 85 columns:
- Contiguous macro bit operations
- Meaningful calculations
- String literals

```c
/* These lines may exceed 85 columns */
#define TPB_UATTR_MASK  ((TPB_UNIT_T)0xFFFF000000000000)
#define TPB_UNAME_FLOPS (((TPB_UNIT_T)0x0000002000000000) | TPB_UKIND_VOLPTIME)
```

## 7. Alignment of Block Macros and Multi-line Comments

### Rule
Always align the start of key definitions of block macros and multi-line consecutive comments to the next tab stop after the longest column of the line block.

### Block Macros
For blocks of related `#define` statements, align the macro names (or values) to the next tab stop after the longest line in the block:

```c
/* Correct - aligned to next tab after longest line */
#define TPB_UATTR_MASK          ((TPB_UNIT_T)0xFFFF000000000000)
#define TPB_UATTR_CAST_MASK     ((TPB_UNIT_T)(1ULL << 48))
#define TPB_UATTR_CAST_Y        ((TPB_UNIT_T)(1ULL << 48))
#define TPB_UATTR_SHAPE_MASK    ((TPB_UNIT_T)(7ULL << 49))
```

### Multi-line Consecutive Comments
For consecutive multi-line comments, align the comment text to the next tab stop after the longest comment marker:

```c
/* Correct - aligned to next tab after longest marker */
/* Initialize timer subsystem */
/* Set default timeout value */
/* Configure callback handlers */
```

## 8. File Headers

### Format
Each file should have a minimal header with filename and description:

For implementation files (*.c):
```c
/*
 * tpb-driver.c
 * Main entry of benchmarking kernels.
 */
```

For header files (*.h) - include Doxygen tags:
```c
/**
 * @file tpb-driver.h
 * @brief Public API for the TPBench driver.
 */

#ifndef TPB_DRIVER_H
#define TPB_DRIVER_H

/* ... content ... */

#endif /* TPB_DRIVER_H */
```

### Prohibited
- Do NOT include license text in file headers
- Do NOT include author/date information
- Do NOT include old comment history

## 9. Function Implementations

### Format
```c
<return_type>
<function_name>(<parameters>)
{
    <body>
}
```

### Example
```c
int
tpb_register_kernel(const char *name, tpb_k_func *funcs)
{
    if (!name || !funcs) {
        return -TPB_EINVAL;
    }
    /* ... */
}
```

## 10. Function Calls

### Basic Format
```c
result = function_name(arg1, arg2, arg3);
```

### Argument Wrapping
When a function call exceeds column 85, wrap arguments and align to the first argument:

```c
result = long_function_name(first_argument, second_argument,
                            third_argument, fourth_argument);
```

### Long Argument Names
Only use one argument per line when each argument has a long name:

```c
result = configure_kernel(kernel_configuration_struct,
                          memory_allocation_params,
                          performance_measurement_options);
```

### Prohibited
Do NOT use single-line braces without arguments:

```c
/* Wrong */
function_name(
);

/* Correct */
function_name();
```

## 11. Control Structures

### Format
Use space between keyword and parenthesis, space before opening brace:

```c
if (condition) {
    /* body */
} else {
    /* alternative */
}

for (int i = 0; i < n; i++) {
    /* body */
}

while (condition) {
    /* body */
}
```

### Brace Placement
- Opening brace on the same line as the statement
- Closing brace on its own line
- `else` on the same line as the closing brace

## 12. Include Guards

All header files must have include guards:

```c
#ifndef TPB_FILENAME_H
#define TPB_FILENAME_H

/* content */

#endif /* TPB_FILENAME_H */
```

## 13. Indentation

- Use 4 spaces for indentation
- Do NOT use tabs
- Align continuation lines appropriately

## 14. Section Comments

Use simple `/* Section Name */` comments to delineate code sections:

```c
/* Local Function Prototypes */

/* Public API */

/* Utility Functions */
```

---

## Quick Reference

| Element | Style |
|---------|-------|
| Public functions | `tpb_<name>()` |
| Static functions | `<verb>_<noun>()` |
| Block comments | `/* comment */` |
| Inline comments | `// comment` |
| Public API docs | `/** @brief ... */` |
| Static func docs | `/* description */` |
| File header (.c) | `/* filename\n * description */` |
| File header (.h) | `/** @file ... @brief ... */` |
| Line limit | 85 columns (exceptions for macros) |
| Indentation | 4 spaces |
