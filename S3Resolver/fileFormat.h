#ifndef S3_FILE_FORMAT_H
#define S3_FILE_FORMAT_H

#include <pxr/pxr.h>
#include <pxr/usd/usd/api.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/usd/usd/usdzFileFormat.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

#define S3_FILE_FORMAT_TOKENS  \
    ((Id,      "s3"))              \
    ((Version, "1.0"))               \
    ((Target,  "usd"))

TF_DECLARE_PUBLIC_TOKENS(S3FileFormatTokens, S3_FILE_FORMAT_TOKENS);

TF_DECLARE_WEAK_AND_REF_PTRS(S3FileFormat);
TF_DECLARE_WEAK_PTRS(SdfLayerBase);


/// \class S3FileFormat
///
/// File format for package .s3 files.
class S3FileFormat : public SdfFileFormat
{
public:
    using SdfFileFormat::FileFormatArguments;

    USD_API
    virtual bool IsPackage() const;

    USD_API
    virtual std::string GetPackageRootLayerPath(
        const std::string& resolvedPath) const;

    USD_API
    virtual SdfAbstractDataRefPtr
    InitData(const FileFormatArguments& args) const override;

    USD_API
    virtual bool CanRead(const std::string &file) const override;

    USD_API
    virtual bool Read(
        const SdfLayerBasePtr& layerBase,
        const std::string& resolvedPath,
        bool metadataOnly) const override;

    USD_API
    virtual bool WriteToFile(
        const SdfLayerBase* layerBase,
        const std::string& filePath,
        const std::string& comment = std::string(),
        const FileFormatArguments& args = FileFormatArguments()) const override;

    USD_API
    virtual bool ReadFromString(
        const SdfLayerBasePtr& layerBase,
        const std::string& str) const override;

    USD_API
    virtual bool WriteToString(
        const SdfLayerBase* layerBase,
        std::string* str,
        const std::string& comment = std::string()) const override;

    USD_API
    virtual bool WriteToStream(
        const SdfSpecHandle &spec,
        std::ostream& out,
        size_t indent) const override;

protected:
    SDF_FILE_FORMAT_FACTORY_ACCESS;

private:
    S3FileFormat();
    virtual ~S3FileFormat();

    virtual bool _IsStreamingLayer(const SdfLayerBase& layer) const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // S3_FILE_FORMAT_H