#ifndef TPB_IMPL_H
#define TPB_IMPL_H

// === Timers ===
#include "timers/timers.h"

// === Kernels ===
#include "kernels/kernels.h"

// === Errors ===
// Error macro for translating error messages into tpbench error
#define __error_lt(evar, eno, err_name) if((evar) < (eno)) {    \
    return (err_name);  \
}

#define __error_gt(evar, eno, err_name) if((evar) > (eno)) {    \
    return (err_name);  \
}

#define __error_eq(evar, eno, err_name) if((evar) == (eno)) {   \
    return (err_name);  \
}

#define __error_ne(evar, eno, err_name) if((evar) != (eno)) {   \
    return (err_name);  \
}

#define __error_exit(evar) if((evar)) {exit(1);}

#define __error_fun(err, msg)   if((err)) { \
err = tpb_printf(err, 1, 1, msg);   \
__error_exit(err);                  \
}
// ERROR CODE

#define TPB_NO_ERROR    0
// 1 - 50, tpbench general error
#define GRP_ARG_ERROR   1
#define KERN_ARG_ERROR  2
#define KERN_NE         3
#define GRP_NE          4
#define SYNTAX_ERROR    5
#define FILE_OPEN_FAIL  6
#define MALLOC_FAIL     7
#define ARGS_MISS       8
#define MKDIR_ERROR     9
#define RES_INIT_FAIL   10

// 51-100, MPI error
#define MPI_INIT_FAIL   51

// WARNING
#define VERIFY_FAIL     101
#define OVER_OPTMIZE    102
#define DEFAULT_DIR     103
#define CORE_NOT_BIND   104

#define DHLINE "================================================================================\n"
#define HLINE  "--------------------------------------------------------------------------------\n"

#endif