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
#ifndef HDNUKE_NUKETEXTUREPLUGIN_H
#define HDNUKE_NUKETEXTUREPLUGIN_H

#include "pxr/pxr.h"
#include "DDImage/Iop.h"

#include <string>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class NukeTexturePlugin {
public:
  static NukeTexturePlugin& Instance();

  NukeTexturePlugin();

  //! Add a file to the "filesystem". This must be called on the main thread.
  void AddFile( const std::string& path, const DD::Image::Iop::TextureImage& buffer );
  //! Remove a file from the "filesystem". This must be called on the main thread.
  void RemoveFile( const std::string& path );

  //! Return the Iop source for a texture "file".
  const DD::Image::Iop::TextureImage* GetFile( const std::string& path );

  //! Set the maximum texture size. Larger textures will be resized using nearest-neighbour filtering
  void SetMaxTextureSize(int size);

  //! Return the maximum texture size.
  int GetMaxTextureSize() const;

private:
  std::unique_ptr<class NukeTexturePluginImpl> _pimpl;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_NUKETEXTUREPLUGIN_H
