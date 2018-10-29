#include "s3.h"
#include "debugCodes.h"

#include <pxr/base/tf/diagnosticLite.h>
#include <pxr/base/tf/fileUtils.h>
#include <pxr/base/tf/pathUtils.h>

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsV2Request.h>
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

    std::string get_env_var(const std::string& server_name, const std::string& env_var, const std::string& default_value) {
        const auto env_first = getenv((server_name + "_" + env_var).c_str());
        if (env_first != nullptr) {
            return env_first;
        }
        const auto env_second = getenv(env_var.c_str());
        if (env_second != nullptr) {
            return env_second;
        }
        return default_value;
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
    Aws::S3::S3Client* s3_client;  

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
        TF_DEBUG(S3_DBG).Msg("S3: client setup \n");
        Aws::InitAPI(options);

        Aws::Client::ClientConfiguration config;
        // TODO: set executor to a PooledThreadExecutor
        config.scheme = Aws::Http::SchemeMapper::FromString("http");
        config.proxyHost = get_env_var("", PROXY_HOST_ENV_VAR, "").c_str();
        config.proxyPort = atoi(get_env_var("", PROXY_PORT_ENV_VAR, "").c_str());
        config.connectTimeoutMs = 3000;
        config.requestTimeoutMs = 3000;
        s3_client = Aws::New<Aws::S3::S3Client>("s3resolver", config);
    }

    S3::~S3() {
        TF_DEBUG(S3_DBG).Msg("S3: client teardown \n");
        Aws::Delete(s3_client);
        Aws::ShutdownAPI(options);        
    }

    // checks if a path exists with a ListObjectsV2 (bucket b, prefix p)
    // better could be a HeadRequest (bucket b, key k)
    std::string S3::resolve_name(const std::string& path) {
        TF_DEBUG(S3_DBG).Msg("S3: resolve_name %s\n", path.c_str());
        
        
        Aws::String bucket_name = get_bucket_name(path).c_str();
        Aws::String object_name = get_object_name(path).c_str();
        TF_DEBUG(S3_DBG).Msg("S3: resolve_name bucket: %s and object: %s\n", bucket_name.c_str(), object_name.c_str());
        
        // Aws::S3::Model::ListObjectsV2Request objects_request;
        // objects_request.WithBucket(bucket_name);
        // objects_request.WithPrefix(object_name);

        Aws::S3::Model::HeadObjectRequest head_request;
        head_request.WithBucket(bucket_name);
        head_request.WithKey(object_name);

        // auto list_objects_outcome = s3_client.ListObjectsV2(objects_request);
        auto head_object_outcome = s3_client->HeadObject(head_request);

        if (head_object_outcome.IsSuccess())
        {
            TF_DEBUG(S3_DBG).Msg("S3: resolve_name OK\n");
            // TODO: prepend working directory from environment variable
            std::string cache_path = get_env_var("", CACHE_PATH_ENV_VAR, "/tmp");
            return TfNormPath(cache_path + "/" + path.substr(3));
        }
        else
        {
            TF_DEBUG(S3_DBG).Msg("S3: resolve_name NOK\n");
            std::cout << "HeadObjects error: " <<
                head_object_outcome.GetError().GetExceptionName() << " " <<
                head_object_outcome.GetError().GetMessage() << std::endl;
            return std::string();
        };
    }

    bool S3::fetch_asset(const std::string& path, const std::string& localPath) {
        TF_DEBUG(S3_DBG).Msg("S3: fetch_asset %s\n", path.c_str());
                
        Aws::S3::Model::GetObjectRequest object_request;
        Aws::String bucket_name = get_bucket_name(path).c_str();
        Aws::String object_name = get_object_name(path).c_str();
        TF_DEBUG(S3_DBG).Msg("S3: fetch_object %s from bucket %s\n", object_name.c_str(), bucket_name.c_str());
        object_request.WithBucket(bucket_name).WithKey(object_name);

        auto get_object_outcome = s3_client->GetObject(object_request);        

        if (get_object_outcome.IsSuccess())
        {
            TF_DEBUG(S3_DBG).Msg("S3: fetch_object get object success\n");
            // prepare directory
            const std::string& bucket_path = localPath.substr(0, localPath.find_last_of('/'));
            if (!TfIsDir(bucket_path)) {
                bool isSuccess = TfMakeDirs(bucket_path);
                if (! isSuccess) {
                    TF_DEBUG(S3_DBG).Msg("S3: fetch_object failed to create bucket directory\n");
                }
            }
            
            Aws::OFStream local_file;
            local_file.open(localPath, std::ios::out | std::ios::binary);
            local_file << get_object_outcome.GetResult().GetBody().rdbuf();
            TF_DEBUG(S3_DBG).Msg("S3: fetch_object \n");
            return true;
        }
        else
        {
            std::cout << "GetObject error: " <<
                get_object_outcome.GetError().GetExceptionName() << " " <<
                get_object_outcome.GetError().GetMessage() << std::endl;
            return false;
        }
        
    }    

    bool S3::matches_schema(const std::string& path) {
        //TF_DEBUG(S3_DBG).Msg("S3: matches_schema %s\n", path.c_str());
        //constexpr auto schema_length_short = cexpr_strlen(usd_s3::S3_SUFFIX);
        //return path.compare(path.length()-3, schema_length_short, ".s3") == 0;
        constexpr auto schema_length_short = cexpr_strlen(usd_s3::S3_PREFIX);
        return path.compare(0, schema_length_short, "s3:") == 0;
    }

    std::string S3::get_bucket_name(const std::string& path) {
        return path.substr(3, path.find_first_of('/') - 3);
    }

    std::string S3::get_object_name(const std::string& path) {
        return path.substr(path.find_first_of('/') + 1);
    }

    
}