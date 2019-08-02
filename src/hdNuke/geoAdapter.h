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
#ifndef HDNUKE_GEOADAPTER_H
#define HDNUKE_GEOADAPTER_H

#include <pxr/pxr.h>

#include <pxr/base/gf/vec2f.h>

#include <pxr/imaging/hd/sceneDelegate.h>

#include <DDImage/GeoInfo.h>

#include "adapter.h"
#include "types.h"


PXR_NAMESPACE_OPEN_SCOPE


class HdNukeGeoAdapter : public HdNukeAdapter
{
public:
    HdNukeGeoAdapter(AdapterSharedState* statePtr);
    ~HdNukeGeoAdapter() override { }

    void Update(const DD::Image::GeoInfo& geo, HdDirtyBits dirtyBits,
                bool isInstanced);

    inline GfRange3d GetExtent() const { return _extent; }

    inline GfMatrix4d GetTransform() const { return _transform; }

    inline bool GetVisible() const { return _visible; }

    inline HdMeshTopology GetMeshTopology() const { return _topology; }

    VtValue Get(const TfToken& key) const override;

    const TfToken& GetPrimType() const override;

    HdPrimvarDescriptorVector
    GetPrimvarDescriptors(HdInterpolation interpolation) const;

    HdReprSelector GetReprSelector() const;

    //! Makes an adapter for an imaginary unit card at the origin. This is used
    //! as a prototype for instancing particle sprites.
    void MakeParticleSprite();

    //! Set if this geo is being used as a instancer prototype
    void SetIsInstanced(bool isInstanced);

    bool SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    bool Update(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    void TearDown(HdNukeAdapterManager* manager) override;

protected:
    void _RebuildPointList(const DD::Image::GeoInfo& geo);
    void _RebuildPrimvars(const DD::Image::GeoInfo& geo);
    void _RebuildMeshTopology(const DD::Image::GeoInfo& geo);
    virtual void _SetMaterial(HdNukeAdapterManager* manager);

    template <typename T>
    inline void _StorePrimvarScalar(TfToken& key, const T& value) {
        _primvarData.emplace(key, VtValue(value));
    }

    inline void _StorePrimvarArray(TfToken& key, VtValue&& array) {
        _primvarData.emplace(key, std::move(array));
    }

    HdReprSelector GetReprSelectorForGeo(const DD::Image::GeoInfo& geo) const;
    GfVec4f GetWireframeColor(const DD::Image::GeoInfo& geo) const;

    GfMatrix4d _transform;
    GfRange3d _extent;
    bool _visible = true;

    VtVec3fArray _points;
    VtVec2fArray _uvs;
    VtVec3fArray _colors;

    HdMeshTopology _topology;

    HdPrimvarDescriptorVector _constantPrimvarDescriptors;
    HdPrimvarDescriptorVector _uniformPrimvarDescriptors;
    HdPrimvarDescriptorVector _vertexPrimvarDescriptors;
    HdPrimvarDescriptorVector _faceVaryingPrimvarDescriptors;

    TfTokenMap<VtValue> _primvarData;

    HdReprSelector _reprSelector;
    GfVec4f _wireframeColor;
    GfVec3f _displayColor;
    float _pointSize = 1.0f;
    bool _isInstanced = false;
    DD::Image::GeoInfo* _geoInfo;
    SdfPath _materialId;
    DD::Image::Hash _hash;
    bool _castsShadow;
    GeoOpHashArray _opStateHashes;
};

using HdNukeGeoAdapterPtr = std::shared_ptr<HdNukeGeoAdapter>;


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_GEOADAPTER_H
