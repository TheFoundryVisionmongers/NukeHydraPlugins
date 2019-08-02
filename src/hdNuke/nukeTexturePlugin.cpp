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

#include "DDImage/Iop.h"
#include "DDImage/Execute.h"

#include <map>
#include <thread>
#include <mutex>

using namespace DD::Image;

PXR_NAMESPACE_OPEN_SCOPE

class NukeTexturePluginImpl {
public:
  /// An entry in our fake filesystem. We reference-count these as the same textures may be used
  /// multiple times in a scene or used in two different scene delegates.
  class FileEntry {
  public:
    FileEntry(const Iop::TextureImage& buffer) : _textureBuffer{buffer}
    {
      assert(_textureBuffer.buffer != nullptr);
    }

    int _refCount = 1;
    Iop::TextureImage _textureBuffer;
  };

  void AddFile( const std::string& path, const Iop::TextureImage& buffer );
  void RemoveFile( const std::string& path );

  const Iop::TextureImage* GetFile( const std::string& path );

  std::mutex _mutex;
  std::map<std::string, std::unique_ptr<FileEntry>> _files;
  int _maxTextureSize = 512;
};

void NukeTexturePluginImpl::AddFile( const std::string& path, const Iop::TextureImage& buffer )
{
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _files.find(path);
  if (it != _files.end()) {
    it->second->_refCount++;
    it->second->_textureBuffer = buffer;
  }
  else {
    _files[path] = std::make_unique<FileEntry>(buffer);
  }
}

void NukeTexturePluginImpl::RemoveFile( const std::string& path )
{
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _files.find(path);
  if (it != _files.end()) {
    if (--it->second->_refCount <= 0) {
      _files.erase(path);
    }
  }
}

const Iop::TextureImage* NukeTexturePluginImpl::GetFile( const std::string& path )
{
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _files.find(path);
  if (it != _files.end()) {
    return &(it->second->_textureBuffer);
  }
  return nullptr;
}


NukeTexturePlugin& NukeTexturePlugin::Instance()
{
  static NukeTexturePlugin sInstance;
  return sInstance;
}

NukeTexturePlugin::NukeTexturePlugin()
  : _pimpl(new NukeTexturePluginImpl())
{
}

void NukeTexturePlugin::AddFile( const std::string& path, const Iop::TextureImage& buffer )
{
  _pimpl->AddFile(path, buffer);
}

void NukeTexturePlugin::RemoveFile( const std::string& path ) {
  _pimpl->RemoveFile(path);
}

const Iop::TextureImage* NukeTexturePlugin::GetFile( const std::string &path )
{
  return _pimpl->GetFile(path);
}

void NukeTexturePlugin::SetMaxTextureSize(int size)
{
  _pimpl->_maxTextureSize = size;
}

int NukeTexturePlugin::GetMaxTextureSize() const
{
  return _pimpl->_maxTextureSize;
}

PXR_NAMESPACE_CLOSE_SCOPE
