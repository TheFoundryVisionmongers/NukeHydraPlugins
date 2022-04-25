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
#ifndef HDNUKE_HYDRAOPBASES_H
#define HDNUKE_HYDRAOPBASES_H

#include <pxr/pxr.h>

#include <pxr/imaging/hd/types.h>
#include <pxr/imaging/hd/material.h>

#include <DDImage/Op.h>

#include <bitset>
#include <unordered_set>

namespace DD
{
    namespace Image
    {
        class ViewerContext;
    }
}

PXR_NAMESPACE_OPEN_SCOPE


class HydraOpManager;
class HdNukeSceneDelegate;


class HydraOp
{
public:
    HydraOp() { }
    virtual ~HydraOp();

    virtual void Populate(HydraOpManager* manager) = 0;
};


class HydraPrimOp : public HydraOp
{
public:
    HydraPrimOp() : HydraOp() { }
    virtual ~HydraPrimOp();

    virtual const TfToken& GetPrimTypeName() const = 0;

    inline HdDirtyBits GetDirtyBits() const { return _dirtyBits; }
    inline bool IsDirty() const { return _dirtyBits != 0; }
    inline void MarkDirty(HdDirtyBits bits) {
        _dirtyBits = bits;
    }
    inline void MarkClean() { _dirtyBits = 0; }

    static const HdDirtyBits DefaultDirtyBits;

private:
    HdDirtyBits _dirtyBits = 0;
};


/// A struct that is passed to the CreateMaterial method of HydraMaterialOp containing
/// data needed to create the material network.
struct HydraMaterialContext {
  enum Flags {
    eUseTextures   = (1 << 0),   //< If set we want to generate texture nodes, solid shading if unset.
    eForceEmissive = (1 << 1),   //< If set we want to use emissive texture to simulate Nuke's flat shading.
    eForceOpaque   = (1 << 2),   //< If set we want to force the opacity to be 1.

    eNumFlags = 3
  };
  using MaterialFlags = std::bitset<eNumFlags>;

  HydraMaterialContext(DD::Image::ViewerContext* viewerContext, const pxr::HdMaterialNetworkMap& map, const pxr::TfToken& output, const MaterialFlags&& flags)
    : _viewerContext(viewerContext)
    , _map(map)
    , _output(output)
    , _materialFlags(std::move(flags))
    , _materialIsOpaque(true)
  {
  }

  HydraMaterialContext(const HydraMaterialContext& other) = default;

  HdMaterialNetwork& network() { return _map.map[_output]; }

  void setFlags(const MaterialFlags& flags) { _materialFlags |= flags; }

  bool hasFlags(const MaterialFlags& flags) const { return (_materialFlags & flags) != 0; }

  DD::Image::ViewerContext* _viewerContext;
  DD::Image::Iop* _materialOp;
  pxr::HdMaterialNetworkMap _map;
  pxr::TfToken _output;
  MaterialFlags _materialFlags;
  std::unordered_set<std::string> _queuedTextures; ///< Textures that have been queued to be generated.
  bool _materialIsOpaque;                          ///< Indicates that the material is opaque when using UsdPreviewSurface.

  bool operator==(const HydraMaterialContext& b) const
  {
    return _viewerContext == b._viewerContext
           && _materialOp == b._materialOp
           && _map == b._map
           && _output == b._output
           && _materialFlags == b._materialFlags
           && _queuedTextures == b._queuedTextures;
  }
};

/// Base class for Ops which implement Hydra material nodes.
class HydraMaterialOp {
public:
  virtual ~HydraMaterialOp();

  /// Override this to generate a material node for the given Op, which will usually
  /// be "this" except in the case of proxy material ops.
  virtual pxr::HdMaterialNode* CreateMaterial(DD::Image::Op* op, HydraMaterialContext& context, const pxr::SdfPath& materialId, pxr::HdMaterialNode* parentNode ) const = 0;

  /// A convenience function to create a material network for an input op. This
  /// will call CreateMaterial and connect the nodes.
  static pxr::HdMaterialNode* CreateMaterialInput(DD::Image::Op* op, HydraMaterialContext& context, const pxr::SdfPath& materialId, pxr::HdMaterialNode* parentNode, const pxr::TfToken& parentInput );
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_HYDRAOPBASES_H
