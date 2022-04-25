// Copyright 2019-present Nathan Rusch
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "nukeTexturePlugin.h"

#include "pxr/usdImaging/usdImaging/version.h"
  #if PXR_VERSION >= 2105
    #include "pxr/imaging/hio/image.h"
  #else
    #include "pxr/imaging/garch/image.h"
  #endif
#include "pxr/imaging/glf/utils.h"

#include "pxr/base/arch/defines.h"
#include "pxr/base/arch/pragmas.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/type.h"

#include "DDImage/Tile.h"
#include "DDImage/Iop.h"
#include "DDImage/Execute.h"
#include "DDImage/Row.h"

#include <thread>
#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

//#if PXR_METAL_SUPPORT_ENABLED
#if PXR_VERSION >= 2105
class Garch_NukeImage : public HioImage {
public:
    typedef HioImage Base;
#else
class Garch_NukeImage : public GarchImage {
public:
    typedef GarchImage Base;
#endif

    Garch_NukeImage();

    // HioImage overrides
    std::string const & GetFilename() const override;
    int GetWidth() const override;
    int GetHeight() const override;
#if PXR_VERSION >= 2105
    HioFormat GetFormat() const override;
#else
    GLenum GetFormat() const override;
    GLenum GetType() const override;
#endif
    int GetBytesPerPixel() const override;
    int GetNumMipLevels() const override;

    bool IsColorSpaceSRGB() const override;

    bool GetMetadata(TfToken const & key, VtValue * value) const override;
#if PXR_VERSION >= 2105
    bool GetSamplerMetadata(HioAddressDimension dim, HioAddressMode * param) const override;
#else
    bool GetSamplerMetadata(GLenum pname, VtValue * param) const override;
#endif

    bool Read(StorageSpec const & storage) override;
    bool ReadCropped(int const cropTop,
               int const cropBottom,
               int const cropLeft,
               int const cropRight,
               StorageSpec const & storage) override;

    bool Write(StorageSpec const & storage,
                       VtDictionary const & metadata) override;

protected:
#if PXR_VERSION >= 2105
    bool _OpenForReading(std::string const & filename,
                                 int subimage,
                                 int mip,
                                 SourceColorSpace sourceColorSpace,
                                 bool suppressErrors) override;
#else
    bool _OpenForReading(std::string const & filename, int subimage,
                         int mip, bool suppressErrors) override;
#endif
    bool _OpenForWriting(std::string const & filename) override;

private:
    std::string _filename;
    int _width = 0;
    int _height = 0;
    int _nchannels = 4;
    const DD::Image::Iop::TextureImage* _textureBuffer = nullptr;
};

TF_REGISTRY_FUNCTION(TfType)
{
    typedef Garch_NukeImage Image;
    TfType t = TfType::Define<Image, TfType::Bases<Image::Base> >();
#if PXR_VERSION >= 2105
    t.SetFactory< HioImageFactory<Image> >();
#else
    t.SetFactory< GlfImageFactory<Image> >();
#endif
}


Garch_NukeImage::Garch_NukeImage()
    : _width(128)
    , _height(128)
    , _nchannels(4)
{
}

/* virtual */
std::string const &
Garch_NukeImage::GetFilename() const
{
    return _filename;
}


/* virtual */
int
Garch_NukeImage::GetWidth() const
{
    return _width;
}

/* virtual */
int
Garch_NukeImage::GetHeight() const
{
    return _height;
}

#if PXR_VERSION >= 2105
/* virtual */
HioFormat
Garch_NukeImage::GetFormat() const
{
  switch (_nchannels) {
      case 1:
          return HioFormatUNorm8srgb;
      case 2:
          return HioFormatUNorm8Vec2srgb;
      case 3:
          return HioFormatUNorm8Vec3srgb;
      case 4:
          return HioFormatUNorm8Vec4srgb;
      default:
          return HioFormatUNorm8srgb;
  }
}
#else
/* virtual */
GLenum
Garch_NukeImage::GetFormat() const
{
    switch (_nchannels) {
        case 1:
            return GL_RED;
        case 2:
            return GL_RG;
        case 3:
            return GL_RGB;
        case 4:
            return GL_RGBA;
        default:
            return 1;
    }
}

/* virtual */
GLenum
Garch_NukeImage::GetType() const
{
    return GL_UNSIGNED_BYTE;
}
#endif

/* virtual */
int
Garch_NukeImage::GetBytesPerPixel() const
{
    return 1 * _nchannels;
}

/* virtual */
bool
Garch_NukeImage::IsColorSpaceSRGB() const
{
    return true;
}


/* virtual */
bool
Garch_NukeImage::GetMetadata(TfToken const & key, VtValue * value) const
{
    return false;
}

/* virtual */
bool
#if PXR_VERSION >= 2105
Garch_NukeImage::GetSamplerMetadata(HioAddressDimension dim, HioAddressMode * param) const
#else
Garch_NukeImage::GetSamplerMetadata(GLenum pname, VtValue * param) const
#endif
{
    return false;
}

/* virtual */
int
Garch_NukeImage::GetNumMipLevels() const
{
    return 1;
}

/* virtual */
#if PXR_VERSION >= 2105
bool
Garch_NukeImage::_OpenForReading(std::string const & filename,
                                 int subimage,
                                 int mip,
                                 SourceColorSpace sourceColorSpace,
                                 bool suppressErrors)
#else
bool
Garch_NukeImage::_OpenForReading(std::string const & filename, int subimage,
                                int mip, bool suppressErrors)
#endif
{
    _filename = filename;
    _width = _height = 128;
    _nchannels = 4;

    _textureBuffer = NukeTexturePlugin::Instance().GetFile(_filename);
    if (_textureBuffer != nullptr && _textureBuffer->buffer != nullptr) {
      //const auto& info = _textureBuffer->_info;
      _width = _textureBuffer->width;
      _height = _textureBuffer->height;
      return true;
    }
    return false;
}

/* virtual */
bool
Garch_NukeImage::Read(StorageSpec const & storage)
{
    return ReadCropped(0, 0, 0, 0, storage);
}

/* virtual */
/// Reads the image named _filename into storage.  If needed, the image is
/// cropped and/or resized.  The _width and _height are updated to match
/// storage.width and storage.height
bool
Garch_NukeImage::ReadCropped(int const cropTop,
                            int const cropBottom,
                            int const cropLeft,
                            int const cropRight,
                            StorageSpec const & storage)
{
    int chanStride = _nchannels;

    if ( _textureBuffer != nullptr && _textureBuffer->buffer != nullptr ) {
      unsigned char* dstBase = static_cast<unsigned char*>(storage.data);
      auto bufferSize = _textureBuffer->width * _textureBuffer->height * _textureBuffer->depth;

      // We need to swap red and blue channels since HioFormat doesn't
      // have a BGRA format
      for (auto i = 0u; i < bufferSize; i += chanStride) {
        dstBase[i] = _textureBuffer->buffer.get()[i + 2];
        dstBase[i + 1] = _textureBuffer->buffer.get()[i + 1];
        dstBase[i + 2] = _textureBuffer->buffer.get()[i];
        dstBase[i + 3] = _textureBuffer->buffer.get()[i + 3];
      }
      return true;
    }

    // Produce a checkerboard pattern for debugging purposes
    unsigned char* p = reinterpret_cast<unsigned char*>(storage.data);
    for ( int y = 0; y < storage.height; y++ ) {
      int yc = (y/4) & 1;
      for ( int x = 0; x < storage.width; x++ ) {
        int xc = (x/4) & 1;
        p[0] = xc & yc ? 255 : 0;
        p[1] = xc ^ yc ? 255 : 0;
        p[2] = xc | yc ? 255 : 0;
        p[3] = 255;
        p += chanStride;
      }
    }

    return true;
}

/* virtual */
bool
Garch_NukeImage::_OpenForWriting(std::string const & filename)
{
    return false;
}

/* virtual */
bool
Garch_NukeImage::Write(StorageSpec const & storageIn,
                      VtDictionary const & metadata)
{
    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE
