#include "fileFormat.h"
#include "resolver.h"
#include "debugCodes.h"
#include "object.h"

#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/usd/usdaFileFormat.h>
#include <pxr/usd/usd/usdzFileFormat.h>

#include <pxr/usd/ar/packageUtils.h>
#include <pxr/usd/ar/resolver.h>

#include <pxr/base/trace/trace.h>

#include <pxr/base/tf/fileUtils.h>
#include <pxr/base/tf/pathUtils.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/staticData.h>

PXR_NAMESPACE_OPEN_SCOPE


using std::string;

TF_DEFINE_PUBLIC_TOKENS(S3FileFormatTokens, S3_FILE_FORMAT_TOKENS);

TF_REGISTRY_FUNCTION(TfType)
{
    SDF_DEFINE_FILE_FORMAT(S3FileFormat, SdfFileFormat);
}

S3FileFormat::S3FileFormat()
    : SdfFileFormat(S3FileFormatTokens->Id,
                    S3FileFormatTokens->Version,
                    S3FileFormatTokens->Target,
                    S3FileFormatTokens->Id)
{
    TF_DEBUG(USD_S3_FILEFORMAT).Msg("FileFormat S3FF constructor\n");
    //bool test = ArGetResolver().FetchToLocalResolvedPath("abc", "def");
    
}

S3FileFormat::~S3FileFormat()
{
}

bool 
S3FileFormat::IsPackage() const
{
    TF_DEBUG(USD_S3_FILEFORMAT).Msg("S3FF ispackage? yeaaah\n");
    return true;
}

namespace
{
// this used to be _GetFirstFileInZipFile
// now just returns root.usdz
std::string
_GetBucketRootFile(const std::string& bucket)
{
    TF_DEBUG(USD_S3_FILEFORMAT).Msg("S3FF get root file in bucket %s\n", bucket.c_str());
    const S3object s3file = S3ResolverCache::GetInstance()
        .FindOrOpenS3object(bucket).second;
    if (!s3file) {
        return std::string();
    }

    //const S3object::Iterator firstFileIt = s3file.begin();
    //return (firstFileIt == s3file.end()) ? std::string() : *firstFileIt;
    return std::string(bucket + "root.usdz");
}

} // end anonymous namespace

std::string 
S3FileFormat::GetPackageRootLayerPath(
    const std::string& resolvedPath) const
{
    TF_DEBUG(USD_S3_FILEFORMAT).Msg("S3FF get bucket root layer %s\n", resolvedPath.c_str());
    TRACE_FUNCTION();
    // TODO: define which object in the bucket is the root layer
    // for now, just go for root.usdz
    return _GetBucketRootFile(resolvedPath);
}

SdfAbstractDataRefPtr
S3FileFormat::InitData(const FileFormatArguments& args) const
{
    TF_DEBUG(USD_S3_FILEFORMAT).Msg("S3FF InitData\n");
    return SdfFileFormat::InitData(args);
}

bool
S3FileFormat::CanRead(const std::string& filePath) const
{
    TF_DEBUG(USD_S3_FILEFORMAT).Msg("S3FF can read \n");
    TRACE_FUNCTION();

    const std::string firstFile = _GetBucketRootFile(filePath);
    if (firstFile.empty()) {
        return false;
    }

    const SdfFileFormatConstPtr packagedFileFormat = 
        SdfFileFormat::FindByExtension(firstFile);
    if (!packagedFileFormat) {
        return false;
    }

    const std::string packageRelativePath = 
        ArJoinPackageRelativePath(filePath, firstFile);
    return packagedFileFormat->CanRead(packageRelativePath);
}

bool
S3FileFormat::Read(
    const SdfLayerBasePtr& layerBase,
    const std::string& resolvedPath,
    bool metadataOnly) const
{
    TRACE_FUNCTION();

    const std::string firstFile = _GetBucketRootFile(resolvedPath);
    if (firstFile.empty()) {
        return false;
    }

    const SdfFileFormatConstPtr packagedFileFormat = 
        SdfFileFormat::FindByExtension(firstFile);
    if (!packagedFileFormat) {
        return false;
    }

    const std::string packageRelativePath = 
        ArJoinPackageRelativePath(resolvedPath, firstFile);
    return packagedFileFormat->Read(
        layerBase, packageRelativePath, metadataOnly);
}

bool
S3FileFormat::WriteToFile(
    const SdfLayerBase* layerBase,
    const std::string& filePath,
    const std::string& comment,
    const FileFormatArguments& args) const
{
    TF_CODING_ERROR("Writing s3 usdz layers is not allowed via this API.");
    return false;
}

bool 
S3FileFormat::ReadFromString(
    const SdfLayerBasePtr& layerBase,
    const std::string& str) const
{
    return SdfFileFormat::FindById(S3FileFormatTokens->Id)->
        ReadFromString(layerBase, str);
}

bool 
S3FileFormat::WriteToString(
    const SdfLayerBase* layerBase,
    std::string* str,
    const std::string& comment) const
{
    return SdfFileFormat::FindById(S3FileFormatTokens->Id)->
        WriteToString(layerBase, str, comment);
}

bool
S3FileFormat::WriteToStream(
    const SdfSpecHandle &spec,
    std::ostream& out,
    size_t indent) const
{
    return SdfFileFormat::FindById(S3FileFormatTokens->Id)->
        WriteToStream(spec, out, indent);
}

bool 
S3FileFormat::_IsStreamingLayer(
    const SdfLayerBase& layer) const
{
    TF_DEBUG(USD_S3_FILEFORMAT).Msg("S3FF is streaminglyaer? yes \n");
    return true;
}

bool 
S3FileFormat::_LayersAreFileBased() const
{
    TF_DEBUG(USD_S3_FILEFORMAT).Msg("S3FF is filebased? yes \n");
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE