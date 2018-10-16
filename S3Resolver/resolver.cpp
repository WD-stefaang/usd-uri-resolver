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
#include "object.h"
#include "debugCodes.h"

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
    using _Map = tbb::concurrent_hash_map<std::string, AssetAndS3object>;
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

S3ResolverCache::AssetAndS3object
S3ResolverCache::_OpenS3object(const std::string& path)
{
    AssetAndS3object result;
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3RC open files3 %s\n", path.c_str());
    result.first = ArGetResolver().OpenAsset(path);
    if (result.first) {
        result.second = S3object::Open(result.first);
    }
    return result;
}

S3ResolverCache::AssetAndS3object 
S3ResolverCache::FindOrOpenS3object(const std::string& packagePath)
{
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3RC Find or open s3 %s\n", packagePath.c_str());
    _CachePtr currentCache = _GetCurrentCache();
    if (currentCache) {
        _Cache::_Map::accessor accessor;
        if (currentCache->_pathToEntryMap.insert(
                accessor, std::make_pair(packagePath, AssetAndS3object()))) {
            accessor->second = _OpenS3object(packagePath);
        }
        return accessor->second;
    }

    return  _OpenS3object(packagePath);
}

// ------------------------------------------------------------


AR_DEFINE_RESOLVER(S3Resolver, ArResolver)

S3Resolver::S3Resolver() : ArDefaultResolver()
{
    TF_DEBUG(USD_S3_RESOLVER).Msg("Loading the S3Resolver\n");
}

S3Resolver::~S3Resolver()
{}

bool S3Resolver::IsRelativePath(const std::string& path)
{
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3Resolver is relative path %s: %s\n", path.c_str());
    return !g_s3.matches_schema(path) && ArDefaultResolver::IsRelativePath(path);
}

std::string S3Resolver::Resolve(const std::string& path)
{
    return S3Resolver::ResolveWithAssetInfo(path, nullptr);
}


// std::string S3Resolver::IsRelativePath(const std::string& path)
// {

// }

std::string S3Resolver::ComputeLocalPath(const std::string& path) 
{    
    std::string ans = ArDefaultResolver::ComputeLocalPath(path);
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3Resolver compute local path %s: %s\n", path.c_str(), ans.c_str());
    return ans;
};
std::string S3Resolver::ComputeNormalizedPath(const std::string& path) 
{
    std::string ans = ArDefaultResolver::ComputeNormalizedPath(path);
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3Resolver compute local path %s: %s\n", path.c_str(), ans.c_str());
    return ans;
};


std::string S3Resolver::ResolveWithAssetInfo(
    const std::string& path,
    ArAssetInfo* assetInfo)
{
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3Resolver resolve path %s \n", path.c_str());    
    return g_s3.matches_schema(path) ?
        g_s3.resolve_name(path) :
        ArDefaultResolver::ResolveWithAssetInfo(path, assetInfo);        
}

bool S3Resolver::FetchToLocalResolvedPath(const std::string& path, const std::string& resolvedPath)
{
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3Resolver DOWNLOAD THIS STUFF: %s\n", path.c_str());
    return true;
}


// ------------------------------------------------------------


AR_DEFINE_PACKAGE_RESOLVER(S3objectResolver, ArPackageResolver)

S3objectResolver::S3objectResolver() 
{
    TF_DEBUG(USD_S3_RESOLVER).Msg("Loading my S3objectResolver hurray\n");
}

S3objectResolver::~S3objectResolver()
{
    g_s3.clear();
}

void 
S3objectResolver::BeginCacheScope(
    VtValue* cacheScopeData)
{ 
    S3ResolverCache::GetInstance().BeginCacheScope(cacheScopeData);
}

void
S3objectResolver::EndCacheScope(
    VtValue* cacheScopeData)
{    
    S3ResolverCache::GetInstance().EndCacheScope(cacheScopeData);
}

std::string 
S3objectResolver::Resolve(
    const std::string& packagePath,
    const std::string& packagedPath)
{
    std::shared_ptr<ArAsset> asset;
    S3object s3file;
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3 Resolve! \n");
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3 Resolve! %s\n", packagePath.c_str());
    std::tie(asset, s3file) = S3ResolverCache::GetInstance()
        .FindOrOpenS3object(packagePath);

    if (!s3file) {
        return std::string();
    }
    // TODO: implement iterators
    //return s3file.Find(packagedPath) != s3file.end() ? 
    //    packagedPath : std::string();
    return packagedPath;
}

namespace
{

class _Asset
    : public ArAsset
{
private:
    std::shared_ptr<ArAsset> _sourceAsset;
    S3object _s3file;
    const char* _dataInZipFile;
    size_t _offsetInZipFile;
    size_t _sizeInZipFile;

public:
    explicit _Asset(std::shared_ptr<ArAsset>&& sourceAsset,
                    S3object&& s3file,
                    const char* dataInZipFile,
                    size_t offsetInZipFile,
                    size_t sizeInZipFile)
        : _sourceAsset(std::move(sourceAsset))
        , _s3file(std::move(s3file))
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
                s3file = S3object();
            }
            S3object s3file;
        };

        _Deleter d;
        d.s3file = _s3file;

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

/*
    open an asset in the S3 bucket
*/
std::shared_ptr<ArAsset> 
S3objectResolver::OpenAsset(
    const std::string& packagePath,
    const std::string& packagedPath)
{
    std::shared_ptr<ArAsset> asset;
    TF_DEBUG(USD_S3_RESOLVER).Msg("S3R openasset %s in package %s\n", packagedPath.c_str(), packagePath.c_str());
    S3object s3file;
    std::tie(asset, s3file) = S3ResolverCache::GetInstance()
        .FindOrOpenS3object(packagePath);

    if (!s3file) {
        return nullptr;
    }
    // TODO
    return nullptr;
    // auto iter = s3file.Find(packagedPath);
    // if (iter == s3file.end()) {
    //     return nullptr;
    // }

    // const S3object::FileInfo info = iter.GetFileInfo();
    // return std::shared_ptr<ArAsset>(
    //     new _Asset(
    //         std::move(asset), std::move(s3file),
    //         iter.GetFile(), info.dataOffset, info.size));
}

PXR_NAMESPACE_CLOSE_SCOPE
