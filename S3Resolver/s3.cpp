#include "s3.h"
#include "debugCodes.h"

#include <pxr/base/tf/diagnosticLite.h>

PXR_NAMESPACE_USING_DIRECTIVE

// -------------------------------------------------------------------------------
// If you want to print out a stacktrace everywhere S3_WARN is called, set this
// to a value > 0 - it will print out this number of stacktrace entries
#define USD_S3_DEBUG_STACKTRACE_SIZE 0

#if USD_S3_DEBUG_STACKTRACE_SIZE > 0

#include <execinfo.h>

#define S3_WARN \
    { \
        void* backtrace_array[USD_S3_DEBUG_STACKTRACE_SIZE]; \
        size_t stack_size = backtrace(backtrace_array, USD_S3_DEBUG_STACKTRACE_SIZE); \
        TF_WARN("\n\n====================================\n"); \
        TF_WARN("Stacktrace:\n"); \
        backtrace_symbols_fd(backtrace_array, stack_size, STDERR_FILENO); \
    } \
    TF_WARN

#else // STACKTRACE_SIZE

#define S3_WARN TF_WARN

#endif // STACKTRACE_SIZE

// -------------------------------------------------------------------------------

// If you want to control the number of seconds an idle connection is kept alive
// for, set this to something other than zero

#define SESSION_WAIT_TIMEOUT 0

#if SESSION_WAIT_TIMEOUT > 0

#define _USD_S3_SIMPLE_QUOTE(ARG) #ARG
#define _USD_S3_EXPAND_AND_QUOTE(ARG) _SIMPLE_QUOTE(ARG)
#define SET_SESSION_WAIT_TIMEOUT_QUERY ( "SET SESSION wait_timeout=" _USD_S3_EXPAND_AND_QUOTE( SESSION_WAIT_TIMEOUT ) )
#define SET_SESSION_WAIT_TIMEOUT_QUERY_STRLEN ( sizeof(SET_SESSION_WAIT_TIMEOUT_QUERY) - 1 )


#endif // SESSION_WAIT_TIMEOUT

// -------------------------------------------------------------------------------
