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

    // get an environment variable
    // std::string get_env_var(const std::string& server_name, const std::string& env_var, const std::string& default_value) {
    //     const auto env_first = getenv((server_name + "_" + env_var).c_str());
    //     if (env_first != nullptr) {
    //         return env_first;
    //     }
    std::string get_env_var(const std::string& env_var, const std::string& default_value) {
        const auto env_var_value = getenv(env_var.c_str());
        return (env_var_value != nullptr) ? env_var_value : default_value;
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

    const std::string get_bucket_name(const std::string& path) {
        // TODO: refactor with S3_PREFIX
        return path.substr(3, path.find_first_of('/') - 3);
    }

    const std::string get_object_name(const std::string& path) {
        // TODO: refactor with S3_PREFIX
        return path.substr(path.find_first_of('/') + 1);
    }

    std::string fill_cache_data(const std::string& path, Cache& cache) {
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

            // TODO set cache_dir in S3 constructor
            const std::string cache_dir = get_env_var(CACHE_PATH_ENV_VAR, "/tmp");
            const std::string cache_path = TfNormPath(cache_path + "/" + path.substr(3));
            double date_modified = head_object_outcome.GetResult().GetLastModified().UnderlyingTimestamp().time_since_epoch().count();
            TF_DEBUG(S3_DBG).Msg("S3: resolve_name OK %.0f\n", date_modified);
            // store date modified in cache
            cache.state = CACHE_NEEDS_FETCHING;
            cache.timestamp = 1;
            cache.local_path = cache_path;
            return cache_path;
        }
        else
        {
            TF_DEBUG(S3_DBG).Msg("S3: resolve_name NOK\n");
            cache.timestamp = INVALID_TIME;
            std::cout << "HeadObjects error: " <<
                head_object_outcome.GetError().GetExceptionName() << " " <<
                head_object_outcome.GetError().GetMessage() << std::endl;
            return std::string();
        };
    }

    bool fetch_object(const std::string& path, Cache& cache) {
        Aws::S3::Model::GetObjectRequest object_request;
        Aws::String bucket_name = get_bucket_name(path).c_str();
        Aws::String object_name = get_object_name(path).c_str();
        TF_DEBUG(S3_DBG).Msg("S3: fetch_asset %s from bucket %s\n", object_name.c_str(), bucket_name.c_str());
        object_request.WithBucket(bucket_name).WithKey(object_name);

        auto get_object_outcome = s3_client->GetObject(object_request);

        if (get_object_outcome.IsSuccess())
        {
            TF_DEBUG(S3_DBG).Msg("S3: fetch_asset %s success\n", path.c_str());
            // prepare directory
            const std::string& bucket_path = cache.local_path.substr(0, cache.local_path.find_last_of('/'));
            if (!TfIsDir(bucket_path)) {
                bool isSuccess = TfMakeDirs(bucket_path);
                if (! isSuccess) {
                    TF_DEBUG(S3_DBG).Msg("S3: fetch_asset failed to create bucket directory\n");
                    return false;
                }
            }

            Aws::OFStream local_file;
            local_file.open(cache.local_path, std::ios::out | std::ios::binary);
            local_file << get_object_outcome.GetResult().GetBody().rdbuf();
            cache.timestamp = get_object_outcome.GetResult().GetLastModified().UnderlyingTimestamp().time_since_epoch().count();
            TF_DEBUG(S3_DBG).Msg("S3: fetch_object OK %.0f\n", cache.timestamp);
            cache.state = CACHE_FETCHED;
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

    std::map<std::string, Cache> cached_requests;

    S3::S3() {
        //TF_DEBUG(S3_DBG).Msg("S3: MODULE CREATED %s\n");
        TF_DEBUG(S3_DBG).Msg("S3: client setup \n");
        Aws::InitAPI(options);

        Aws::Client::ClientConfiguration config;
        // TODO: set executor to a PooledThreadExecutor
        config.scheme = Aws::Http::SchemeMapper::FromString("http");
        config.proxyHost = get_env_var(PROXY_HOST_ENV_VAR, "").c_str();
        config.proxyPort = atoi(get_env_var(PROXY_PORT_ENV_VAR, "80").c_str());
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

        const auto cached_result = cached_requests.find(path);
        if (cached_result != cached_requests.end()) {
            if (cached_result->second.state != CACHE_MISSING) {
                TF_DEBUG(S3_DBG).Msg("S3: resolve_name - use cached result for %s\n", path.c_str());
                return cached_result->second.local_path;
            }
            TF_DEBUG(S3_DBG).Msg("S3: resolve_name - refresh cached result for %s\n", path.c_str());
            return fill_cache_data(path, cached_result->second);
        } else {
            Cache cache{
                CACHE_MISSING,
                ""
            };
            TF_DEBUG(S3_DBG).Msg("S3: resolve_name - no cache for %s\n", path.c_str());
            std::string result = fill_cache_data(path, cache);
            cached_requests.insert(std::make_pair(path, cache));
            return result;
        }

    }

    bool S3::fetch_asset(const std::string& path, const std::string& localPath) {
        TF_DEBUG(S3_DBG).Msg("S3: fetch_asset %s\n", path.c_str());

        if (s3_client == nullptr) {
            TF_DEBUG(S3_DBG).Msg("S3: fetch_asset - abort due to s3_client nullptr\n");
            return false;
        }

        const auto cached_result = cached_requests.find(path);
        if (cached_result == cached_requests.end()) {
            S3_WARN("[S3Resolver] %s was not resolved before fetching!", path.c_str());
            return false;
        }

        if (cached_result->second.state != CACHE_NEEDS_FETCHING) {
            // ensure cache state is up to date
            // there is no guarantee that get_timestamp was called prior to fetch
            Cache cache{CACHE_MISSING, ""};
            fill_cache_data(path, cache);
            if (cache.timestamp == INVALID_TIME) {
                cached_result->second.state = CACHE_MISSING;
            } else if (cache.timestamp > cached_result->second.timestamp) {
                TF_DEBUG(S3_DBG).Msg("S3: fetch_asset - local path data is out of date\n");
                cached_result->second.state = CACHE_NEEDS_FETCHING;
            }
        }

        if (cached_result->second.state == CACHE_MISSING) {
            TF_DEBUG(S3_DBG).Msg("S3: fetch_asset - Asset not found, no fetch\n");
            return false;
        }

        if (cached_result->second.state == CACHE_NEEDS_FETCHING) {
            TF_DEBUG(S3_DBG).Msg("S3: fetch_asset - Cache needed fetching\n");
            cached_result->second.state = CACHE_MISSING; // we'll set this up if fetching is successful
            bool success = fetch_object(path, cached_result->second);
            if (success) {

            } else {

            }
        } else {
            TF_DEBUG(S3_DBG).Msg("S3: fetch_asset - Cache does not need fetch\n");
        }
        return true;
    }

    bool S3::matches_schema(const std::string& path) {
        //TF_DEBUG(S3_DBG).Msg("S3: matches_schema %s\n", path.c_str());
        //constexpr auto schema_length_short = cexpr_strlen(usd_s3::S3_SUFFIX);
        //return path.compare(path.length()-3, schema_length_short, ".s3") == 0;
        constexpr auto schema_length_short = cexpr_strlen(usd_s3::S3_PREFIX);
        return path.compare(0, schema_length_short, "s3:") == 0;
    }

    double S3::get_timestamp(const std::string& asset_path) {
        if (s3_client == nullptr) {
            return 1.0;
        }

        //mutex_scoped_lock sc(connection_mutex);
        const auto cached_result = cached_requests.find(asset_path);
        if (cached_result == cached_requests.end() ||
                cached_result->second.state == CACHE_MISSING) {
            S3_WARN("[S3Resolver] %s is missing when querying timestamps!",
                    asset_path.c_str());
            return 1.0;
        } else {
            auto ret = 2.0; //get_timestamp_raw(connection, table_name, asset_path);
            if (ret == INVALID_TIME) {
                cached_result->second.state = CACHE_MISSING;
                ret = cached_result->second.timestamp;
            } else if (ret > cached_result->second.timestamp) {
                cached_result->second.state = CACHE_NEEDS_FETCHING;
            }
            return ret;
        }
    }

}
