
#include <pxr/base/tf/pathUtils.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/type.h>

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/defineResolver.h>
#include <pxr/usd/ar/definePackageResolver.h>
#include <pxr/usd/ar/defaultResolver.h>
#include <pxr/usd/ar/packageResolver.h>
#include <pxr/usd/ar/assetInfo.h>
#include <pxr/usd/ar/resolverContext.h>

#include <pxr/usd/ar/packageResolver.h>
#include <pxr/usd/ar/threadLocalScopedCache.h>
#include <pxr/usd/usd/zipFile.h>

#include <tbb/concurrent_hash_map.h>
#include <memory>

#include "resolver.h"
#include "s3.h"

/*
 * Depending on the asset count and access frequency, it could be better to store the
 * resolver paths in a sorted vector, rather than a map. That's way faster when we are
 * doing significantly more queries inserts.
 */

PXR_NAMESPACE_OPEN_SCOPE

#define S3_WARN TF_WARN

namespace {
    usd_s3::S3 g_s3;
}

S3ResolverCache&
S3ResolverCache::GetInstance()
{
    static S3ResolverCache cache;
    return cache;
}

S3ResolverCache::S3ResolverCache()
{
}

struct S3ResolverCache::_Cache
{
    using _Map = tbb::concurrent_hash_map<std::string, AssetAndZipFile>;
    _Map _pathToEntryMap;
};

void 
S3ResolverCache::BeginCacheScope(
    VtValue* cacheScopeData)
{
    _caches.BeginCacheScope(cacheScopeData);
}

void
S3ResolverCache::EndCacheScope(
    VtValue* cacheScopeData)
{
    _caches.EndCacheScope(cacheScopeData);
}

S3ResolverCache::_CachePtr 
S3ResolverCache::_GetCurrentCache()
{
    return _caches.GetCurrentCache();
}

S3ResolverCache::AssetAndZipFile
S3ResolverCache::_OpenZipFile(const std::string& path)
{
    AssetAndZipFile result;
    result.first = ArGetResolver().OpenAsset(path);
    if (result.first) {
        result.second = UsdZipFile::Open(result.first);
    }
    return result;
}

S3ResolverCache::AssetAndZipFile 
S3ResolverCache::FindOrOpenZipFile(const std::string& packagePath)
{
    _CachePtr currentCache = _GetCurrentCache();
    if (currentCache) {
        _Cache::_Map::accessor accessor;
        if (currentCache->_pathToEntryMap.insert(
                accessor, std::make_pair(packagePath, AssetAndZipFile()))) {
            accessor->second = _OpenZipFile(packagePath);
        }
        return accessor->second;
    }

    return  _OpenZipFile(packagePath);
}

// ------------------------------------------------------------


AR_DEFINE_PACKAGE_RESOLVER(S3Resolver, ArPackageResolver)

S3Resolver::S3Resolver() 
{
    S3_WARN("[S3Resolver] initiated");
}

S3Resolver::~S3Resolver()
{
    // g_s3.clear();
}

void 
S3Resolver::BeginCacheScope(
    VtValue* cacheScopeData)
{
    S3ResolverCache::GetInstance().BeginCacheScope(cacheScopeData);
}

void
S3Resolver::EndCacheScope(
    VtValue* cacheScopeData)
{
    S3ResolverCache::GetInstance().EndCacheScope(cacheScopeData);
}

std::string 
S3Resolver::Resolve(
    const std::string& packagePath,
    const std::string& packagedPath)
{
    std::shared_ptr<ArAsset> asset;
    UsdZipFile zipFile;
    std::tie(asset, zipFile) = S3ResolverCache::GetInstance()
        .FindOrOpenZipFile(packagePath);

    if (!zipFile) {
        return std::string();
    }
    return zipFile.Find(packagedPath) != zipFile.end() ? 
        packagedPath : std::string();
}

namespace
{

class _Asset
    : public ArAsset
{
private:
    std::shared_ptr<ArAsset> _sourceAsset;
    UsdZipFile _zipFile;
    const char* _dataInZipFile;
    size_t _offsetInZipFile;
    size_t _sizeInZipFile;

public:
    explicit _Asset(std::shared_ptr<ArAsset>&& sourceAsset,
                    UsdZipFile&& zipFile,
                    const char* dataInZipFile,
                    size_t offsetInZipFile,
                    size_t sizeInZipFile)
        : _sourceAsset(std::move(sourceAsset))
        , _zipFile(std::move(zipFile))
        , _dataInZipFile(dataInZipFile)
        , _offsetInZipFile(offsetInZipFile)
        , _sizeInZipFile(sizeInZipFile)
    {
    }

    virtual size_t GetSize() override
    {
        return _sizeInZipFile;
    }

    virtual std::shared_ptr<const char> GetBuffer() override
    {
        struct _Deleter
        {
            void operator()(const char* b)
            {
                zipFile = UsdZipFile();
            }
            UsdZipFile zipFile;
        };

        _Deleter d;
        d.zipFile = _zipFile;

        return std::shared_ptr<const char>(_dataInZipFile, d);
    }

    virtual size_t Read(void* buffer, size_t count, size_t offset)
    {
        memcpy(buffer, _dataInZipFile + offset, count);
        return count;
    }
    
    virtual std::pair<FILE*, size_t> GetFileUnsafe() override
    {
        std::pair<FILE*, size_t> result = _sourceAsset->GetFileUnsafe();
        if (result.first) {
            result.second += _offsetInZipFile;
        }
        return result;
    }
};

} // end anonymous namespace

std::shared_ptr<ArAsset> 
S3Resolver::OpenAsset(
    const std::string& packagePath,
    const std::string& packagedPath)
{
    std::shared_ptr<ArAsset> asset;
    UsdZipFile zipFile;
    std::tie(asset, zipFile) = S3ResolverCache::GetInstance()
        .FindOrOpenZipFile(packagePath);

    if (!zipFile) {
        return nullptr;
    }

    auto iter = zipFile.Find(packagedPath);
    if (iter == zipFile.end()) {
        return nullptr;
    }

    const UsdZipFile::FileInfo info = iter.GetFileInfo();
    return std::shared_ptr<ArAsset>(
        new _Asset(
            std::move(asset), std::move(zipFile),
            iter.GetFile(), info.dataOffset, info.size));
}

PXR_NAMESPACE_CLOSE_SCOPE
