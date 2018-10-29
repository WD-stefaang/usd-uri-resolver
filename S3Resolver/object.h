
#ifndef USD_S3OBJECT_H
#define USD_S3OBJECT_H

#include <pxr/pxr.h>
#include <pxr/usd/usd/api.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

/// \class S3object
///
/// Class for reading an S3 object, based on zipFile.h
/// This class is primarily intended to support the .s3 file format. 
///

class ArAsset;

class S3object {
private:
    class _Impl;

public:
    /// Opens the s3 object at \p filePath. 
    /// Returns invalid object on error.
    USD_API
    static S3object Open(const std::string& filePath);

    /// Opens the s3 object \p asset.
    /// Returns invalid object on error.
    USD_API
    static S3object Open(const std::shared_ptr<ArAsset>& asset);

    /// Create an invalid S3object object.
    S3object();
    ~S3object();

    /// Return true if this object is valid, false otherwise.
    USD_API
    explicit operator bool() const { return static_cast<bool>(_impl); }

    /// \class FileInfo
    /// Information for a file in the zip archive.
    class FileInfo
    {
    public:
        /// Offset of the beginning of this file's data from the start of
        /// the zip archive.
        size_t dataOffset = 0;

        /// Size of this file as stored in the zip archive. If this file is
        /// compressed, this is its compressed size. Otherwise, this is the
        /// same as the uncompressed size.
        size_t size = 0;

        /// Uncompressed size of this file. This may not be the same as the
        /// size of the file as stored in the zip archive.
        size_t uncompressedSize = 0;

        /// Compression method for this file. See section 4.4.5 of the zip
        /// file specification for valid values. In particular, a value of 0
        /// means this file is stored with no compression.
        uint16_t compressionMethod = 0;
    };

    /// Returns iterator pointing to the first file in the zip archive.
    // USD_API
    // Iterator begin() const;

    /// Returns iterator pointing to the first file in the zip archive.
    // Iterator cbegin() const { return begin(); }

    /// Returns end iterator for this zip archive.
    // USD_API
    // Iterator end() const;

    /// Returns end iterator for this zip archive.
    // Iterator cend() const { return end(); }

    /// Returns iterator to the file with the given \p path in this zip
    /// archive, or end() if no such file exists.
    // USD_API
    // Iterator Find(const std::string& path) const;

    /// Print out listing of contents of this zip archive to stdout.
    /// For diagnostic purposes only.
    USD_API
    void DumpContents() const;

private:
    S3object(std::shared_ptr<_Impl>&& impl);

    std::shared_ptr<_Impl> _impl;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_S3OBJECT_H
