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

#include <GL/glew.h>

#include <pxr/base/gf/vec3f.h>

#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/usdImaging/usdImaging/version.h>
#include <pxr/base/tf/fileUtils.h>
#include <pxr/imaging/hd/light.h>
#include "DDImage/Iop.h"
#if PXR_METAL_SUPPORT_ENABLED
  #include <pxr/imaging/garch/texture.h>
  #include <pxr/imaging/garch/textureRegistry.h>
  #if PXR_VERSION >= 2105
    #include <pxr/imaging/hio/image.h>
  #else
    #include <pxr/imaging/garch/image.h>
  #endif
  #include <pxr/imaging/hdSt/resourceFactory.h>
#else
  #include <pxr/imaging/glf/texture.h>
  #include <pxr/imaging/glf/textureHandle.h>
  #include <pxr/imaging/glf/textureRegistry.h>
#endif
#include <pxr/imaging/hdSt/textureResource.h>
#include <pxr/imaging/hdSt/renderDelegate.h>
#include <pxr/usdImaging/usdImaging/tokens.h>
#include <pxr/imaging/hd/material.h>

#include "hydraOpManager.h"
#include "lightOp.h"
#include "materialAdapter.h"
#include "sceneDelegate.h"
#include "tokens.h"
#include "utils.h"
#include "nukeTexturePlugin.h"

#include <numeric>
#include <cstring>
#include <algorithm>

using namespace DD::Image;

PXR_NAMESPACE_OPEN_SCOPE

HdNukeSceneDelegate::HdNukeSceneDelegate(HdRenderIndex* renderIndex)
    : HdSceneDelegate(renderIndex, HdNukeDelegateConfig::DefaultDelegateID())
    , _config(HdNukeDelegateConfig::DefaultDelegateID())
    , _adapterManager(this)
{
    _defaultMaterialId = GetConfig().MaterialRoot().AppendChild(
           HdNukePathTokens->defaultSurface);
    _defaultParticleMaterialId = GetConfig().MaterialRoot().AppendChild(
           HdNukePathTokens->defaultParticleMaterial);
    sharedState._shadowCollection.SetRootPath(GetConfig().GeoRoot());
}

HdNukeSceneDelegate::HdNukeSceneDelegate(HdRenderIndex* renderIndex,
                                         const SdfPath& delegateId)
    : HdSceneDelegate(renderIndex, delegateId)
    , _config(delegateId)
    , _adapterManager(this)
{
    _defaultMaterialId = GetConfig().MaterialRoot().AppendChild(
           HdNukePathTokens->defaultSurface);
    _defaultParticleMaterialId = GetConfig().MaterialRoot().AppendChild(
           HdNukePathTokens->defaultParticleMaterial);
    sharedState._shadowCollection.SetRootPath(GetConfig().GeoRoot());
}

HdMeshTopology
HdNukeSceneDelegate::GetMeshTopology(const SdfPath& id)
{
    if (auto adapter = _adapterManager.GetAdapter(id)) {
        return adapter->Get(HdNukeTokens->meshTopology).UncheckedGet<HdMeshTopology>();
    }
    return HdMeshTopology{};
}

GfRange3d
HdNukeSceneDelegate::GetExtent(const SdfPath& id)
{
    if (auto adapter = _adapterManager.GetAdapter(id)) {
        return adapter->Get(HdNukeTokens->extent).UncheckedGet<GfRange3d>();
    }
    return GfRange3d{};
}

GfMatrix4d
HdNukeSceneDelegate::GetTransform(const SdfPath& id)
{
    auto adapter = _adapterManager.GetAdapter(id);
    if (adapter != nullptr) {
        return adapter->Get(HdTokens->transform).UncheckedGet<GfMatrix4d>();
    }

    if (id.HasPrefix(GetConfig().HydraLightRoot())) {
        return GetHydraLightOp(id)->GetTransform();
    }

    TF_WARN("HdNukeSceneDelegate::GetTransform : Unrecognized prim id: %s",
            id.GetText());
    return GfMatrix4d(1);
}

bool
HdNukeSceneDelegate::GetVisible(const SdfPath& id)
{
    if (auto adapter = _adapterManager.GetAdapter(id)) {
        adapter->Get(HdNukeTokens->visible).UncheckedGet<bool>();
    }
    return true;
}

bool
HdNukeSceneDelegate::GetDoubleSided(const SdfPath& id)
{
    if (auto adapter = _adapterManager.GetAdapter(id)) {
        adapter->Get(HdNukeTokens->doubleSided).UncheckedGet<bool>();
    }
    return true;
}

VtValue
HdNukeSceneDelegate::Get(const SdfPath& id, const TfToken& key)
{
    auto adapter = _adapterManager.GetAdapter(id);
    if (adapter != nullptr) {
        return adapter->Get(key);
    }

    TF_WARN("HdNukeSceneDelegate::Get : Unrecognized prim id: %s (key: %s)",
            id.GetText(), key.GetText());
    return VtValue();
}

VtIntArray
HdNukeSceneDelegate::GetInstanceIndices(const SdfPath& instancerId,
                                        const SdfPath& prototypeId)
{
    if (auto adapter = _adapterManager.GetAdapter(instancerId)) {
        return adapter->Get(HdNukeTokens->instanceCount).UncheckedGet<VtIntArray>();
    }
    return VtIntArray();
}

HdReprSelector HdNukeSceneDelegate::GetReprSelector(SdfPath const &id)
{
    if (auto adapter = _adapterManager.GetAdapter(id)) {
        return adapter->Get(HdNukeTokens->reprSelector).UncheckedGet<HdReprSelector>();
    }
    return HdReprSelector(HdReprTokens->refined);
}

SdfPath
HdNukeSceneDelegate::GetMaterialId(const SdfPath& rprimId)
{
    if (auto adapter = _adapterManager.GetAdapter(rprimId)) {
        return adapter->Get(HdNukeTokens->materialId).UncheckedGet<SdfPath>();
    }
    return DefaultMaterialId();
}

VtValue
HdNukeSceneDelegate::GetMaterialResource(const SdfPath& materialId)
{
    if (auto adapter = _adapterManager.GetAdapter(materialId)) {
        return adapter->Get(HdNukeTokens->materialResource);
    }
    return HdNukeMaterialAdapter::GetPreviewMaterialResource(materialId);
}

HdPrimvarDescriptorVector
HdNukeSceneDelegate::GetPrimvarDescriptors(const SdfPath& id,
                                           HdInterpolation interpolation)
{
    if (interpolation == HdInterpolationInstance) {
        HdPrimvarDescriptorVector primvars;
        primvars.emplace_back(HdInstancerTokens->instanceTransform, interpolation);
        primvars.emplace_back(HdTokens->displayColor, interpolation);
        return primvars;
    }
    if (auto adapter = std::dynamic_pointer_cast<HdNukeGeoAdapter>(_adapterManager.GetAdapter(id))) {
        return adapter->GetPrimvarDescriptors(interpolation);
    }
    return HdPrimvarDescriptorVector();
}

VtValue
HdNukeSceneDelegate::GetLightParamValue(const SdfPath& id,
                                        const TfToken& paramName)
{
    if (auto adapter = _adapterManager.GetAdapter(id)) {
        return adapter->Get(paramName);
    }

    if (id.HasPrefix(GetConfig().HydraLightRoot())) {
        return GetHydraLightOp(id)->GetLightParamValue(paramName);
    }
    return VtValue();
}

HdNukeMaterialAdapterPtr
HdNukeSceneDelegate::GetMaterialAdapter(const SdfPath& id) const
{
    return nullptr;
}

HydraLightOp*
HdNukeSceneDelegate::GetHydraLightOp(const SdfPath& id) const
{
    auto it = _hydraLightOps.find(id);
    return it == _hydraLightOps.end() ? nullptr : it->second;
}

TfToken
HdNukeSceneDelegate::GetRprimType(const GeoInfo& geoInfo) const
{
    const Primitive* firstPrim = geoInfo.primitive(0);
    if (firstPrim) {
        switch (firstPrim->getPrimitiveType()) {
            case eTriangle:
            case ePolygon:
            case eMesh:
            case ePolyMesh:
                return HdPrimTypeTokens->mesh;
            case eParticlesSprite:
            case ePoint:
            case eParticles:
                return HdPrimTypeTokens->points;
            default:
                break;
        }
    }
    return TfToken();
}

SdfPath
HdNukeSceneDelegate::GetRprimSubPath(const GeoInfo& geoInfo,
                                     const TfToken& primType) const
{
    if (primType.IsEmpty()) {
        return SdfPath();
    }

    // Use a combination of the RPrim type name and the GeoInfo's source hash
    // to produce a (relatively) stable prim ID.
    std::ostringstream buf;
    buf << primType << '_' << std::hex << geoInfo.src_id().value();

    // Look for an object-level "name" attribute on the geo, and if one is
    // found, use that in conjunction with the Rprim type name and hash.
    const auto* nameCtx = geoInfo.get_group_attribcontext(Group_Object, "name");
    if (nameCtx and not nameCtx->empty()
            and (nameCtx->type == STRING_ATTRIB
                 or nameCtx->type == STD_STRING_ATTRIB))
    {
        void* rawData = nameCtx->attribute->array();
        std::string attrValue;
        if (nameCtx->type == STD_STRING_ATTRIB) {
            attrValue = static_cast<std::string*>(rawData)[0];
        }
        else {
            attrValue = std::string(static_cast<char**>(rawData)[0]);
        }

        // Replace any characters that are meaningful in SdfPath
        std::replace_if(attrValue.begin(), attrValue.end(),
          [](char c) { return c == ' ' || c == '<' || c == '>' || c == '[' || c == ']' || c == ':' || c == '.'; },
          '_');

        SdfPath result(attrValue);
        if (result.IsAbsolutePath()) {
            buf << result.MakeRelativePath(SdfPath::AbsoluteRootPath());
        }
        else {
          buf << result;
        }
    }

    return SdfPath(buf.str());
}

void
HdNukeSceneDelegate::SetDefaultDisplayColor(GfVec3f color)
{
    if (color == sharedState.defaultDisplayColor) {
        return;
    }

    sharedState.defaultDisplayColor = color;
    const auto& meshPrims = _adapterManager.GetPathsForPrimType(HdPrimTypeTokens->mesh);
    if (!meshPrims.empty()) {
        HdChangeTracker& tracker = GetRenderIndex().GetChangeTracker();
        for (const auto& p : meshPrims) {
            tracker.MarkPrimvarDirty(p, HdTokens->displayColor);
        }
    }
}

void
HdNukeSceneDelegate::SyncNukeGeometry(DD::Image::ViewerContext* context, const std::vector<GeoInfo*>& geoList)
{
    sharedState._viewerContext = context;

    GeoOpPtrMap<std::unordered_map<Hash, GeoInfoVector>> geoSourceMap;
    for (GeoInfo* geoInfo : geoList) {
        GeoOp* sourceOp = op_cast<GeoOp*>(geoInfo->final_geo);
        geoSourceMap[sourceOp][geoInfo->src_id()].push_back(geoInfo);
    }

    for (auto e : geoSourceMap) {
        for (auto w : e.second) {
            if (w.second.size() == 1) {
                _adapterManager.Request(w.second.front());
            }
            else {
                _adapterManager.Request(w.second);
            }
        }
    }
}

void
HdNukeSceneDelegate::SyncNukeGeometry(DD::Image::ViewerContext* context, GeometryList* geoList)
{
    std::vector<GeoInfo*> geoInfos;
    geoInfos.reserve(geoList->size());
    for (auto i = 0u; i < geoList->size(); i++) {
        geoInfos.push_back(&geoList->object(i));
    }
    SyncNukeGeometry(context, geoInfos);
}

void
HdNukeSceneDelegate::SyncNukeLights(DD::Image::ViewerContext* context, const std::vector<DD::Image::LightOp*>& lightOps)
{
    sharedState._viewerContext = context;

    for (const auto& light : lightOps) {
        _adapterManager.Request(light);
    }
}

void
HdNukeSceneDelegate::SyncNukeLights(DD::Image::ViewerContext* context, std::vector<LightContext*> lights)
{
    std::vector<DD::Image::LightOp*> lightOps;
    lightOps.reserve(lights.size());
    std::transform(lights.begin(), lights.end(), std::back_inserter(lightOps), [](const LightContext* lightCtx) { return lightCtx->light(); });
    SyncNukeLights(context, lightOps);
}

void
HdNukeSceneDelegate::SyncFromGeoOp(DD::Image::ViewerContext* context, GeoOp* geoOp)
{
    TF_VERIFY(geoOp);

    if (not geoOp->valid()) {
        TF_CODING_ERROR("SyncFromGeoOp called with unvalidated GeoOp");
        return;
    }

    geoOp->build_scene(_scene);

    SyncNukeGeometry(context, _scene.object_list());
    SyncNukeLights(context, _scene.lights);

    HdRenderIndex& renderIndex = GetRenderIndex();

    // XXX: Temporary, until Hydra material ops are implemented
    if (renderIndex.IsSprimTypeSupported(HdPrimTypeTokens->material)) {
        renderIndex.InsertSprim(
            HdPrimTypeTokens->material, this, DefaultMaterialId());
    }
}

void
HdNukeSceneDelegate::SyncHydraOp(HydraOp* hydraOp)
{
    HydraOpManager manager(this);
    manager.UpdateIndex(hydraOp);
}

void HdNukeSceneDelegate::BeginSync()
{
    _adapterManager.TryFulfillPromises();
    _adapterManager.SetAllUnused();
}

void HdNukeSceneDelegate::EndSync()
{
    _adapterManager.RemoveUnusedAdapters();
}


void HdNukeSceneDelegate::ClearAll()
{
    ClearNukePrims();
    ClearHydraPrims();
}

void
HdNukeSceneDelegate::ClearNukePrims()
{
    ClearNukeGeo();
    ClearNukeLights();
    ClearNukeMaterials();
}

void
HdNukeSceneDelegate::ClearNukeGeo()
{
    _adapterManager.Clear();
    GetRenderIndex().RemoveSubtree(GetConfig().GeoRoot(), this);
}

void
HdNukeSceneDelegate::ClearNukeLights()
{
    static const TfToken lightTypes[] = {
        HdPrimTypeTokens->distantLight,
        HdPrimTypeTokens->diskLight,
        HdPrimTypeTokens->sphereLight,
        HdPrimTypeTokens->domeLight,
        HdPrimTypeTokens->simpleLight,
    };

    for (const auto& lightType : lightTypes) {
        const auto& lightPaths = _adapterManager.GetPathsForPrimType(lightType);
        if (!lightPaths.empty()) {
            for (const auto& p : lightPaths) {
                _adapterManager.Remove(p);
            }
        }
    }
    GetRenderIndex().RemoveSubtree(GetConfig().NukeLightRoot(), this);
}

void
HdNukeSceneDelegate::ClearNukeMaterials()
{
    const auto& matPaths = _adapterManager.GetPathsForPrimType(HdPrimTypeTokens->material);
    if (!matPaths.empty()) {
        for (const auto& p : matPaths) {
            _adapterManager.Remove(p);
        }
    }
    GetRenderIndex().RemoveSubtree(GetConfig().MaterialRoot(), this);
}

void
HdNukeSceneDelegate::ClearHydraPrims()
{
    _hydraLightOps.clear();
    GetRenderIndex().RemoveSubtree(GetConfig().HydraLightRoot(), this);
}

void
HdNukeSceneDelegate::SetInteractive(bool interactive)
{
    sharedState.interactive = interactive;
}

void
HdNukeSceneDelegate::SetCameraMatrices(const DD::Image::Matrix4& modelMatrix, const DD::Image::Matrix4& projMatrix)
{
    sharedState.modelView = modelMatrix;
    sharedState.viewModel = sharedState.modelView.inverse();
    sharedState.projMatrix = projMatrix;
}

void
HdNukeSceneDelegate::SetViewport(int viewportWidth, int viewportHeight)
{
    sharedState.viewportWidth = viewportWidth;
    sharedState.viewportHeight = viewportHeight;
}

PXR_NAMESPACE_CLOSE_SCOPE
