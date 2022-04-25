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
#ifndef HDNUKE_SCENEDELEGATE_H
#define HDNUKE_SCENEDELEGATE_H

#include <pxr/pxr.h>

#include <pxr/imaging/hd/sceneDelegate.h>

#include <pxr/usdImaging/usdImaging/delegate.h>

#include <DDImage/GeoOp.h>
#include <DDImage/Scene.h>
#include <DDImage/ViewerContext.h>

#include "adapterManager.h"
#include "delegateConfig.h"
#include "geoAdapter.h"
#include "instancerAdapter.h"
#include "lightAdapter.h"
#include "materialAdapter.h"
#include "sharedState.h"
#include "types.h"


PXR_NAMESPACE_OPEN_SCOPE


class GfVec3f;

class HydraOp;
class HydraLightOp;
class HydraOpManager;

class HdNukeSceneDelegate : public HdSceneDelegate
{
public:
    HdNukeSceneDelegate(HdRenderIndex* renderIndex);
    HdNukeSceneDelegate(HdRenderIndex* renderIndex, const SdfPath& delegateId);

    ~HdNukeSceneDelegate() override { }

    HdMeshTopology GetMeshTopology(const SdfPath& id) override;

    GfRange3d GetExtent(const SdfPath& id) override;

    GfMatrix4d GetTransform(const SdfPath& id) override;

    bool GetVisible(const SdfPath& id) override;
    bool GetDoubleSided(const SdfPath& id) override;

    VtValue Get(const SdfPath& id, const TfToken& key) override;

    SdfPath GetInstancerId(const SdfPath& primId) override;

    VtIntArray GetInstanceIndices(const SdfPath& instancerId,
                                  const SdfPath& prototypeId) override;
    SdfPath GetMaterialId(const SdfPath& rprimId) override;

    VtValue GetMaterialResource(const SdfPath& materialId) override;

    VtValue GetLightParamValue(const SdfPath &id,
                               const TfToken& paramName) override;

    HdPrimvarDescriptorVector
    GetPrimvarDescriptors(const SdfPath& id,
                          HdInterpolation interpolation) override;

    inline const HdNukeDelegateConfig& GetConfig() const { return _config; }

    TfToken GetRprimType(const DD::Image::GeoInfo& geoInfo) const;
    SdfPath GetRprimSubPath(const DD::Image::GeoInfo& geoInfo,
                            const TfToken& primType) const;

    inline const SdfPath& DefaultMaterialId() const { return _defaultMaterialId; }
    inline const SdfPath& DefaultParticleMaterialId() const { return _defaultParticleMaterialId; }

    HdNukeGeoAdapterPtr GetGeoAdapter(const SdfPath& id) const;
    HdNukeInstancerAdapterPtr GetInstancerAdapter(const SdfPath& id) const;
    HdNukeLightAdapterPtr GetLightAdapter(const SdfPath& id) const;
    HdNukeMaterialAdapterPtr GetMaterialAdapter(const SdfPath& id) const;
    HydraLightOp* GetHydraLightOp(const SdfPath& id) const;

    HdReprSelector GetReprSelector(SdfPath const &id) override;

    void SetDefaultDisplayColor(GfVec3f color);
    void SetSelectedColor(GfVec4f color);

    void BeginSync();
    void EndSync();

    void SyncFromGeoOp(DD::Image::ViewerContext* context, DD::Image::GeoOp* geoOp);
    void SyncHydraOp(HydraOp* hydraOp);

    void ClearNukePrims();
    void ClearHydraPrims();
    void ClearAll();

    void SetUseEmissiveTextures(bool enable) { GetSharedState()->useEmissiveTextures = enable; }
    void SetSyncLights(bool sync) { _syncLights = sync; }

    /// Set interactive mode. This causes reprs to come from geo display mode instead of render mode.
    void SetInteractive(bool interactive);

    void SyncNukeGeometry(DD::Image::ViewerContext* context, const std::vector<DD::Image::GeoInfo*>& geoList);
    void SyncNukeLights(DD::Image::ViewerContext* context, const std::vector<DD::Image::LightOp*>& lights);
    void SyncNukeEnvLights(DD::Image::ViewerContext* context, const std::vector<DD::Image::LightOp*>& lights);

    // A scene delegate shouldn't need to know about the camera and viewport,
    // but this is necessary to perform billboarding of particles.
    void SetCameraMatrices(const DD::Image::Matrix4& modelMatrix, const DD::Image::Matrix4& projMatrix);
    void SetViewport(int viewportWidth, int viewportHeight);

    AdapterSharedState* GetSharedState() { return &sharedState; }

protected:
    void SyncNukeGeometry(DD::Image::ViewerContext* context, DD::Image::GeometryList* geoList);
    void SyncNukeLights(DD::Image::ViewerContext* context, std::vector<DD::Image::LightContext*> lights);

    void ClearNukeGeo();
    void ClearNukeLights();
    void ClearNukeMaterials();

private:
    friend class HydraOpManager;

    HdNukeDelegateConfig _config;
    HdNukeAdapterManager _adapterManager;

    DD::Image::Scene _scene;

    SdfPathMap<HydraLightOp*> _hydraLightOps;
    SdfPathMap<std::unique_ptr<UsdImagingDelegate>> _usdDelegates;

    AdapterSharedState sharedState;
    SdfPath _defaultMaterialId;
    SdfPath _defaultParticleMaterialId;
    bool _syncLights;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_SCENEDELEGATE_H
