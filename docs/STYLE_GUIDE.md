# TPBench C Code Style Guide

This document defines the coding style conventions for the TPBench framework.
Follow these guidelines for consistency across all `tpb-*` source files.

## 1. Function Naming Conventions

### 1.1. Public Functions
**Public APIs for TPBench core functionality**
- Functions in `src/include/*` headers and implemented in `src/corelib/*`: use `tpb_` prefix. E.g., `tpb_printf()`, `tpb_register_kernel()`
- Functions in `src/corelib/<module>/`: use `tpb_<module>_<verb>_<noun>` pattern. Omit the filename; use module name. E.g., `tpb_raf_read_record()` in `src/corelib/rafdb/tpb-raf-record.c`, not `tpb_record_read_record()`.

**Public APIs for the kernel-specific behaviors**
- For normal single kernel: `tpb_k_<do>_<something>`. E.g. `tpb_k_write_task()`.
- Kernels with special backends (e.g., MPI, OpenMP) require special wrappers beyond single-core APIs. Use `tpb_<prefix>k_<do>_<something>` where `<prefix>` identifies the backend. For example: `tpb_mpik_write_task()` handles MPI-based multi-core task and capsule record writing.

### 1.2. Module Internal Functions
- Single-file static functions: prefix with `_sf_`. Use minimal names for simple ops, descriptive names otherwise. E.g., `_sf_min()`, `_sf_hex_to_bin()`, `_sf_search_headers()`.
- Cross-file module functions: prefix with `_tpb_`. Use verb-first pattern `_tpb_<verb>_<noun>[_<detail>]`. E.g., `_tpb_get_workspace_path()`, `_tpb_init_corelib()`.

### 1.3. Polymorphism
- `<function>_<impl_tag>` for polymorphsm. E.g. `tpb_write_u64`, `tpb_write_i64`.

## 2. Function

### 2.1. Declaration
- Declare intra-file interfaces in header Files (*.h)
- Declare prototypes of all local static functions at the beginning of the file, in the alphabet ascending order.
- If exceeds the line limit, use one-line-per-parameter style and align to the first parameter.
- No space after the function's name.

**Examples**

```c
/* Local Function Prototypes */
static void init_cliout(void);
static int format_sigfig(double val, int sigfig, char *buf, size_t len);
```

### 2.2. Function Implementation
- Put the function name on its own line, after the return type.
- If parameters exceed line limit, wrap with one parameter per line, aligned to the first.

```c
<return_type>
<function_name>(<parameters>)
{
    <body>
}
```

### 2.3. Function Documentation
**General Rules**
- All public functions require Doxygen documentation.
- Document structure: 1) What it does; 2) Inputs; 3) Outputs/Effects; 4) Edge cases (if applicable).

**Public APIs (in headers)**
- Place Doxygen comments before the prototype.
- Include `@brief`, `@param`, `@return` (if applicable).
- Before implementation in .c files, repeat the `@brief` line only.

Example in header:
```c
/**
 * @brief Parse command-line arguments into internal structures.
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, negative error code on failure
 */
int tpb_parse_args(int argc, char **argv);
```

Example in .c file:
```c
/**
 * @brief Parse command-line arguments into internal structures.
 */
int
tpb_parse_args(int argc, char **argv)
{
    if (!argc || !argv) {
        return -TPB_EINVAL;
    }
    /* ... */
}
```

**Static Functions**
- Use plain `/* */` block comments before the prototype.
- No repetition needed before implementation.

```c
/*
 * Initialize the CLI output formatter.
 * Sets up default formatting options from CLI and environment.
 */
static void init_cliout(void);
```

### 2.4. Function Calls
**Basic format:**
```c
result = function_name(arg1, arg2, arg3);
```

**Wrapped format** (exceeds 85 columns):
```c
result = long_function_name(first_argument,
                            second_argument,
                            third_argument,
                            fourth_argument);
```

## 3. Struct Member Documentation

### Format
Use Doxygen's `/**<` inline style after each member:

```c
struct tpb_timer {
    uint64_t start;     /**< Start timestamp */
    uint64_t end;       /**< End timestamp */
    double resolution;  /**< Resolution in seconds */
    int type;           /**< Timer type ID */
};
```

### Guidelines
- Place `/**<` comment immediately after the member
- Provide brief, descriptive explanations
- Align comments across members

```c
struct example {
    int count;  /**< Item count */
    char *name; /**< Item name */
    double value;  /**< Numeric value */
};
```

## 4. Comment Styles

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

## 5. Line Length

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

## 6. Alignment of Block Macros and Multi-line Comments

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

## 7. File Headers

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




## 8. Control Structures

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

## 9. Include Guards

All header files must have include guards:

```c
#ifndef TPB_FILENAME_H
#define TPB_FILENAME_H

/* content */

#endif /* TPB_FILENAME_H */
```

## 10. Indentation

- Use 4 spaces for indentation
- Do NOT use tabs
- Align continuation lines appropriately

## 11. Section Comments

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
