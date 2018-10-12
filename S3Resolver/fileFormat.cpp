#include "fileFormat.h"
#include "resolver.h"

#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/usd/usdaFileFormat.h>
#include <pxr/usd/usd/usdzFileFormat.h>
#include <pxr/usd/usd/zipFile.h>

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
}

S3FileFormat::~S3FileFormat()
{
}

bool 
S3FileFormat::IsPackage() const
{
    return true;
}

namespace
{

std::string
_GetFirstFileInZipFile(const std::string& zipFilePath)
{
    const UsdZipFile zipFile = S3ResolverCache::GetInstance()
        .FindOrOpenZipFile(zipFilePath).second;
    if (!zipFile) {
        return std::string();
    }

    const UsdZipFile::Iterator firstFileIt = zipFile.begin();
    return (firstFileIt == zipFile.end()) ? std::string() : *firstFileIt;
}

} // end anonymous namespace

std::string 
S3FileFormat::GetPackageRootLayerPath(
    const std::string& resolvedPath) const
{
    TRACE_FUNCTION();
    return _GetFirstFileInZipFile(resolvedPath);
}

SdfAbstractDataRefPtr
S3FileFormat::InitData(const FileFormatArguments& args) const
{
    return SdfFileFormat::InitData(args);
}

bool
S3FileFormat::CanRead(const std::string& filePath) const
{
    TRACE_FUNCTION();

    const std::string firstFile = _GetFirstFileInZipFile(filePath);
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

    const std::string firstFile = _GetFirstFileInZipFile(resolvedPath);
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
    return SdfFileFormat::FindById(UsdUsdaFileFormatTokens->Id)->
        ReadFromString(layerBase, str);
}

bool 
S3FileFormat::WriteToString(
    const SdfLayerBase* layerBase,
    std::string* str,
    const std::string& comment) const
{
    return SdfFileFormat::FindById(UsdUsdaFileFormatTokens->Id)->
        WriteToString(layerBase, str, comment);
}

bool
S3FileFormat::WriteToStream(
    const SdfSpecHandle &spec,
    std::ostream& out,
    size_t indent) const
{
    return SdfFileFormat::FindById(UsdUsdaFileFormatTokens->Id)->
        WriteToStream(spec, out, indent);
}

bool 
S3FileFormat::_IsStreamingLayer(
    const SdfLayerBase& layer) const
{
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE