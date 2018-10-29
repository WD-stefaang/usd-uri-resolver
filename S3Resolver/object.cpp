#include "object.h"

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <fstream>

#include <string>

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_OPEN_SCOPE

class S3object::_Impl
{
public:
    _Impl(std::shared_ptr<const char>&& buffer_, size_t size_)
        : storage(std::move(buffer_))
        , buffer(storage.get())
        , size(size_)
    { }

    std::shared_ptr<const char> storage;

    // This is the same as storage.get(), but saved separately to simplify
    // code so they don't have to call storage.get() all the time.
    const char* buffer;
    size_t size;
};

S3object
S3object::Open(const std::string& filePath)
{
    std::shared_ptr<ArAsset> asset = ArGetResolver().OpenAsset(filePath);
    if (!asset) {
        return S3object();
    }

    return Open(asset);
}

S3object
S3object::Open(const std::shared_ptr<ArAsset>& asset)
{
    if (!asset) {
        TF_CODING_ERROR("Invalid asset");
        return S3object();
    }

    std::shared_ptr<const char> buffer = asset->GetBuffer();
    if (!buffer) {
        TF_RUNTIME_ERROR("Could not retrieve buffer from asset");
        return S3object();
    }

    return S3object(std::shared_ptr<_Impl>(
        new _Impl(std::move(buffer), asset->GetSize())));
}

S3object::S3object(std::shared_ptr<_Impl>&& impl)
    : _impl(std::move(impl))
{
}

S3object::S3object()
{
}

S3object::~S3object()
{
}

void 
S3object::DumpContents() const
{
    printf("    Offset\t      Comp\t    Uncomp\tName\n");
    printf("    ------\t      ----\t    ------\t----\n");

    // size_t n = 0;
    // for (auto it = begin(), e = end(); it != e; ++it, ++n) {
    //     const FileInfo info = it.GetFileInfo();
    //     printf("%10zu\t%10zu\t%10zu\t%s\n", 
    //         info.dataOffset, info.size, info.uncompressedSize, 
    //         it->c_str());
    // }

    //printf("----------\n");
    //printf("%zu files total\n", n);
    Aws::S3::S3Client s3_client;

    Aws::S3::Model::GetObjectRequest object_request;
    const Aws::String&& key_name = "kitchen.usdz";
    object_request.WithBucket("hello").WithKey(key_name);

    auto get_object_outcome = s3_client.GetObject(object_request);

    if (get_object_outcome.IsSuccess())
    {
        Aws::OFStream local_file;
        local_file.open(key_name.c_str(), std::ios::out | std::ios::binary);
        local_file << get_object_outcome.GetResult().GetBody().rdbuf();
        std::cout << "Done!" << std::endl;
    }
    else
    {
        std::cout << "GetObject error: " <<
            get_object_outcome.GetError().GetExceptionName() << " " <<
            get_object_outcome.GetError().GetMessage() << std::endl;
    }

}


PXR_NAMESPACE_CLOSE_SCOPE
