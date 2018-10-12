#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <map>

namespace usd_s3 {
    class S3 {
    public:
        S3();
        ~S3();
        void clear();

        std::string resolve_name(const std::string& path);
        bool fetch_asset(const std::string& path);
        bool matches_schema(const std::string& path);
        double get_timestamp(const std::string& path);

    private:
        
    };
}
