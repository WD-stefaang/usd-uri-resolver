#include "s3.h"
#include "debugCodes.h"

#include <pxr/base/tf/diagnosticLite.h>

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <fstream>

#include <iostream>
#include <fstream>
#include <time.h>

#include <aws/core/utils/logging/LogMacros.h>

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

namespace {
    constexpr double INVALID_TIME = std::numeric_limits<double>::lowest();

    using mutex_scoped_lock = std::lock_guard<std::mutex>;

    // Otherwise clang static analyser will throw errors.
    template <size_t len> constexpr size_t
    cexpr_strlen(const char (&)[len]) {
        return len - 1;
    }

    // std::string generate_name(const std::string& base, const std::string& extension, char* buffer) {
    //     std::tmpnam(buffer);
    //     std::string ret(buffer);
    //     const auto last_slash = ret.find_last_of('/');
    //     if (last_slash == std::string::npos) {
    //         return base + ret + extension;
    //     } else {
    //         return base + ret.substr(last_slash + 1) + extension;
    //     }
    // }
}

namespace usd_s3 {
    Aws::SDKOptions options;

    enum CacheState {
            CACHE_MISSING,
            CACHE_NEEDS_FETCHING,
            CACHE_FETCHED
        };

        struct Cache {
            CacheState state;
            std::string local_path;
            double timestamp;
};

    S3::S3() {
        //TF_DEBUG(S3_DBG).Msg("S3: MODULE CREATED %s\n");
        S3_WARN("HELLOWORLD");        
        Aws::InitAPI(options);
    }

    S3::~S3() {
        clear();
    }
    void S3::clear() {
        Aws::ShutdownAPI(options);
        // thread init
        // delete connections
        // connections.clear
    }

    std::string S3::resolve_name(const std::string& path) {
        TF_DEBUG(S3_DBG).Msg("S3: resolve_name %s\n", path.c_str());
        Aws::Client::ClientConfiguration config;
        config.scheme = Aws::Http::SchemeMapper::FromString("http");
        config.proxyHost = "10.249.64.116";
        config.proxyPort = 80;

        Aws::S3::S3Client s3_client(config);

        Aws::S3::Model::ListObjectsRequest objects_request;
        Aws::String bucket_name = get_bucket_name(path).c_str();
        TF_DEBUG(S3_DBG).Msg("S3: resolve_name stripped: %s\n", bucket_name.c_str());
        objects_request.WithBucket(bucket_name);

        auto list_objects_outcome = s3_client.ListObjects(objects_request);

        if (list_objects_outcome.IsSuccess())
        {
            TF_DEBUG(S3_DBG).Msg("S3: resolve_name OK\n");
            Aws::Vector<Aws::S3::Model::Object> object_list =
                list_objects_outcome.GetResult().GetContents();

            for (auto const &s3_object : object_list)
            {
                std::cout << "* " << s3_object.GetKey() << std::endl;
            }
        }
        else
        {
            TF_DEBUG(S3_DBG).Msg("S3: resolve_name NOK\n");
            std::cout << "ListObjects error: " <<
                list_objects_outcome.GetError().GetExceptionName() << " " <<
                list_objects_outcome.GetError().GetMessage() << std::endl;
        };
        return "./defaultLayer.usd";
    }

    bool S3::fetch_asset(const std::string& path) {
        TF_DEBUG(S3_DBG).Msg("S3: fetch_asset %s\n", path.c_str());
        return false;
    }    

    bool S3::matches_schema(const std::string& path) {
        //TF_DEBUG(S3_DBG).Msg("S3: matches_schema %s\n", path.c_str());
        constexpr auto schema_length_short = cexpr_strlen(usd_s3::S3_SUFFIX);
        return path.compare(path.length()-3, schema_length_short, ".s3") == 0;
    }

    std::string S3::get_bucket_name(const std::string& path) {
        return path.substr(0, path.length()-3);
    }
}
