#ifndef S3_RESOLVER_H
#define S3_RESOLVER_H

//#include <pxr/usd/usd/zipFile.h>

#include <pxr/usd/ar/defaultResolver.h>
#include <pxr/usd/ar/packageResolver.h>

#include <string>

#include "object.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class S3Resolver
///
/// S3 Resolver recognizes S3FileFormats
///
class S3Resolver
    : public ArPackageResolver
{
public:
    S3Resolver();
    ~S3Resolver() override;

    virtual std::string Resolve(
        const std::string& packagePath,
        const std::string& packagedPath) override;

    virtual std::shared_ptr<ArAsset> OpenAsset(
        const std::string& packagePath,
        const std::string& packagedPath) override;

    virtual void BeginCacheScope(
        VtValue* cacheScopeData) override;

    virtual void EndCacheScope(
        VtValue* cacheScopeData) override;

};

/// \class S3ResolverCache
///
/// Singleton thread-local scoped cache used by S3Resolver. This
/// allows other clients besides S3Resolver to take advantage of
/// caching of S3 files while a resolver scoped cache is active.
class S3ResolverCache
{
public:
    static S3ResolverCache& GetInstance();

    S3ResolverCache(const S3ResolverCache&) = delete;
    S3ResolverCache& operator=(const S3ResolverCache&) = delete;

    using AssetAndS3object = std::pair<std::shared_ptr<ArAsset>, S3object>;

    /// Returns the ArAsset and S3object for the given package path.
    /// If a cache scope is active in the current thread, the returned
    /// values will be cached and returned on subsequent calls to this
    /// function for the same packagePath.
    AssetAndS3object FindOrOpenS3File(const std::string& packagePath);

    /// Open a cache scope in the current thread. While a cache scope 
    /// is opened, the results of FindOrOpenS3File will be cached and 
    /// reused.
    void BeginCacheScope(VtValue* cacheScopeData);

    /// Close cache scope in the current thread. Once all cache scopes
    /// in the current thread are closed, cached s3 files will be
    /// dropped.
    void EndCacheScope(VtValue* cacheScopeData);

private:
    S3ResolverCache();

    struct _Cache;
    using _ThreadLocalCaches = ArThreadLocalScopedCache<_Cache>;
    using _CachePtr = _ThreadLocalCaches::CachePtr;
    _CachePtr _GetCurrentCache();

    AssetAndS3object _OpenS3object(const std::string& packagePath);

    _ThreadLocalCaches _caches;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // S3_RESOLVER_H