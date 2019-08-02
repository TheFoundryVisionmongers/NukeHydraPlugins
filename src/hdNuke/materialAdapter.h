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
#ifndef HDNUKE_MATERIALADAPTER_H
#define HDNUKE_MATERIALADAPTER_H

#include <map>

#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/assetPath.h>

#include "adapter.h"
#include "opBases.h"

namespace DD
{
    namespace Image
    {
        class Op;
        class ViewerContext;
    }
}

PXR_NAMESPACE_OPEN_SCOPE


class TfToken;
class VtValue;


class HdNukeMaterialAdapter : public HdNukeAdapter
{
public:
    HdNukeMaterialAdapter(AdapterSharedState* statePtr);
    HdNukeMaterialAdapter(AdapterSharedState* statePtr, const SdfPath& materialId);
    ~HdNukeMaterialAdapter() override { }

    SdfPath GetMaterialId() const { return _materialId; }

    //! Update the material. Returns true is anything changed
    bool Update(DD::Image::ViewerContext* viewerContext, DD::Image::Op* materialOp, HydraMaterialContext::MaterialFlags&& flags);

    void SetMaterialNetwork(const VtValue& materialNetwork)
    {
        _materialNetwork = materialNetwork;
    }

    const VtValue& GetMaterialNetwork() const
    {
        return _materialNetwork;
    }

    const DD::Image::Hash GetHash() const
    {
        return _hash;
    }

    void SetTextureNeedsReloading(bool needsReloading)
    {
        _textureNeedsReloading = needsReloading;
    }

    bool TextureNeedsReloading() const
    {
        return _textureNeedsReloading;
    }

    bool SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    bool Update(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    void TearDown(HdNukeAdapterManager* manager) override;

    VtValue Get(const TfToken& key) const override;
    const TfToken& GetPrimType() const override;

    static VtValue GetPreviewMaterialResource(const SdfPath& materialId);
    static VtValue GetParticlesMaterialResource(const SdfPath& materialId);

    static std::map<TfToken, VtValue> GetPreviewSurfaceParameters();

private:
    bool createMaterialNetwork(HdRenderIndex& renderIndex, HydraMaterialContext& materialCtx);

    DD::Image::Iop* _iop;
    std::unordered_set<SdfAssetPath, SdfAssetPath::Hash> _textures;

    SdfPath _materialId;
    DD::Image::Hash _hash;
    VtValue _materialNetwork;
    bool _textureNeedsReloading = true;
};

using HdNukeMaterialAdapterPtr = std::shared_ptr<HdNukeMaterialAdapter>;


/// The register for material proxies. This is for handling existing Nuke material ops
/// without having to modify their source code. Register a proxy class with the op's
/// class name and it'll be used to generate a material network for the op.
class MaterialProxyRegistry {
public:
    static MaterialProxyRegistry& Instance();

    /// Register a proxy object for an Op class
    const void RegisterMaterialProxy(const char* className, HydraMaterialOp* proxy);

    /// Get a proxy object for an Op class
    const HydraMaterialOp* GetMaterialProxy(const char* className) const;

    /// Get the proxy object for an Op. This will return a generic Iop proxy for
    /// Iop which have no registered proxy. It will return nullptr for non-Iops.
    const HydraMaterialOp* GetMaterialProxy(DD::Image::Op* op) const;
};

/// A convenience class to make it easy to register material proxies
template<class T>
class RegisterMaterialProxy {
public:
    RegisterMaterialProxy(const char* className)
    {
      MaterialProxyRegistry::Instance().RegisterMaterialProxy(className, &_proxy);
    }

private:
    T _proxy;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_MATERIALADAPTER_H
