// Copyright 2019-present The Foundry Visionmongers Ltd.
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

#include "adapterManager.h"

#include "adapterFactory.h"
#include "sceneDelegate.h"
#include "utils.h"
#include "adapter.h"
#include "geoAdapter.h"
#include "opBases.h"

#include <DDImage/GeoInfo.h>
#include <DDImage/GeoOp.h>
#include <DDImage/LightContext.h>
#include <DDImage/LightOp.h>
#include <DDImage/Iop.h>

using namespace DD::Image;

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(HdNukeAdapterManagerPrimTypes, HDNUKEADAPTERMANAGER_PRIM_TYPES);

HdNukeAdapterManager::HdNukeAdapterManager(HdNukeSceneDelegate* sceneDelegate)
  : _sceneDelegate{sceneDelegate}
{
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::Request(const TfToken& adapterType, const SdfPath& path, const VtValue& nukeData)
{
    auto fullPath = path.IsAbsolutePath() ? path : _sceneDelegate->GetConfig().DefaultDelegateID().AppendPath(path);

    _requestedAdapters.insert(fullPath);

    auto iter = _adapters.find(fullPath);
    if (iter != _adapters.end()) {
        // If we have requested this adapter before but it is not fulfilled try
        // to fulfill it.
        if (auto unfulfilled = GetUnfulfilledPromise(fullPath)) {
            if (iter->second.adapter->SetUp(this, nukeData)) {
                unfulfilled->adapter = iter->second.adapter;
                _unfulfilledPromises.erase(fullPath);
            }
            return unfulfilled;
        }
        // An adapter may not be able to correctly update itself. In this case,
        // it becomes unfulfilled.
        bool fulfilled = iter->second.adapter->Update(this, nukeData);
        if (!fulfilled) {
            auto promise = std::make_shared<AdapterPromise>(fullPath, nullptr);
            _unfulfilledPromises[fullPath] = promise;
            return promise;
        }
        return std::make_shared<AdapterPromise>(fullPath, iter->second.adapter);
    }

    HdNukeAdapterFactory& factory = HdNukeAdapterFactory::Instance();
    auto insert = _adapters.emplace(fullPath, AdapterInfo{
        factory.Create(adapterType, _sceneDelegate->GetSharedState())});
    AdapterInfo& info = insert.first->second;

    HdNukeAdapterPtr adapter = info.adapter;
    adapter->SetUsed(true);
    adapter->SetPath(fullPath);
    bool fulfilled = adapter->SetUp(this, nukeData);

    info.primType = adapter->GetPrimType();
    info.nukeData = nukeData;

    _adaptersByPrimType[info.primType].insert(fullPath);

    if (fulfilled) {
        return std::make_shared<AdapterPromise>(fullPath, adapter);
    }

    auto promise = std::make_shared<AdapterPromise>(fullPath, nullptr);
    _unfulfilledPromises[fullPath] = promise;

    return promise;
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::Request(GeoInfo* geoInfo, const SdfPath& parentPath)
{
    const SdfPath& geoRoot = _sceneDelegate->GetConfig().GeoRoot();
    GeoOp* sourceOp = op_cast<GeoOp*>(geoInfo->final_geo);
    SdfPath subtree = geoRoot.AppendPath(GetPathFromOp(sourceOp))
                             .AppendPath(GetRprimSubPath(*geoInfo, GetRprimType(*geoInfo)));

    auto primType = HdNukeAdapterManagerPrimTypes->GenericGeoInfo;
    if (geoInfo->primitive(0)->getPrimitiveType() == eParticlesSprite) {
        primType = HdNukeAdapterManagerPrimTypes->ParticleSprite;
    }

    return Request(primType, subtree, VtValue{geoInfo});
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::Request(DD::Image::GeoOp* geoOp, const SdfPath& parentPath)
{
    DD::Image::Scene scene;
    geoOp->build_scene(scene);

    auto geoInfos = scene.object_list();
    auto lights = scene.lights;

    for (std::size_t i = 0; i < geoInfos->size(); ++i) {
        Request(&(geoInfos->object(i)));
    }
    for (const auto& lightCtx : lights) {
        Request(lightCtx->light());
    }
    return nullptr;
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::Request(DD::Image::LightOp* lightOp, const SdfPath& parentPath)
{
    const SdfPath& lightRoot = _sceneDelegate->GetConfig().NukeLightRoot();
    auto finalPath = lightRoot.AppendPath(GetPathFromOp(lightOp));

    TfToken lightType;
    if (lightOp->lightType() == LightOp::eOtherLight) {
        lightType = HdNukeAdapterManagerPrimTypes->Environment;
    }
    else {
        lightType = HdNukeAdapterManagerPrimTypes->Light;
    }

    return Request(lightType, finalPath, VtValue{lightOp});
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::Request(Iop* op, const SdfPath& parentPath)
{
    auto sharedState = _sceneDelegate->GetSharedState();
    HdMaterialNetworkMap materialNetwork;
    TfToken output = HdMaterialTerminalTokens->surface;
    HydraMaterialContext::MaterialFlags flags =
      HydraMaterialContext::eUseTextures
      | (sharedState->useEmissiveTextures ? HydraMaterialContext::eForceEmissive : 0);

    HydraMaterialContext materialContext(sharedState->_viewerContext, materialNetwork, output, std::move(flags));
    materialContext._materialOp = op;

    return Request(materialContext, parentPath);
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::Request(const HydraMaterialContext materialCtx, const SdfPath& parentPath)
{
    const SdfPath& materialsRoot = _sceneDelegate->GetConfig().MaterialRoot();
    auto finalPath = materialsRoot.AppendPath(GetPathFromOp(materialCtx._materialOp));

    return Request(HdNukeAdapterManagerPrimTypes->Material, finalPath, VtValue{materialCtx});
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::Request(GeoInfoVector& instances, const SdfPath& parentPath)
{
    auto geoInfo = instances.front();
    const SdfPath& geoRoot = _sceneDelegate->GetConfig().GeoRoot();
    GeoOp* sourceOp = op_cast<GeoOp*>(geoInfo->final_geo);
    SdfPath subtree = geoRoot.AppendPath(GetPathFromOp(sourceOp))
                             .AppendPath(GetRprimSubPath(*geoInfo, GetRprimType(*geoInfo)));

    return Request(HdNukeAdapterManagerPrimTypes->InstancedGeo, subtree, VtValue{instances});
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::AddAdapter(const HdNukeAdapterPtr& adapter, const TfToken& primType, const SdfPath& path)
{
    auto fullPath = path.IsAbsolutePath() ? path : _sceneDelegate->GetConfig().DefaultDelegateID().AppendPath(path);
    _adapters[fullPath] = AdapterInfo{adapter, primType};
    _adaptersByPrimType[primType].insert(fullPath);
    return std::make_shared<AdapterPromise>(fullPath, adapter);
}

unsigned int HdNukeAdapterManager::TryFulfillPromises()
{
    for (auto iter = _unfulfilledPromises.begin(); iter != _unfulfilledPromises.end();) {
        auto adapter = GetAdapter(iter->first);
        if (adapter->SetUp(this, _adapters[iter->first].nukeData)) {
            iter->second->adapter = adapter;
            iter = _unfulfilledPromises.erase(iter);
        }
        else {
            ++iter;
        }
    }

    return static_cast<unsigned int>(_unfulfilledPromises.size());
}

HdNukeAdapterPtr HdNukeAdapterManager::GetAdapter(const SdfPath& path) const
{
    auto iter = _adapters.find(path);
    if (iter != _adapters.end()) {
        return iter->second.adapter;
    }
    return nullptr;
}

const TfToken& HdNukeAdapterManager::GetPrimType(const SdfPath& path)
{
    auto iter = _adapters.find(path);
    if (iter != _adapters.end()) {
        return iter->second.primType;
    }
    static const TfToken &sEmpty{};
    return sEmpty;
}

void HdNukeAdapterManager::Remove(const SdfPath& path)
{
    _unfulfilledPromises.erase(path);

    auto it = _adapters.find(path);
    if (it != _adapters.end()) {
        AdapterInfo info = it->second;
        info.adapter->TearDown(this);
        _adaptersByPrimType[info.primType].erase(path);
        _adapters.erase(it);
    }
}

void HdNukeAdapterManager::Clear()
{
    for (const auto& adapter : _adapters) {
        adapter.second.adapter->TearDown(this);
    }
    _adapters.clear();
    _adaptersByPrimType.clear();
    _unfulfilledPromises.clear();
}

const SdfPathUnorderedSet& HdNukeAdapterManager::GetPathsForPrimType(const TfToken& type)
{
    return _adaptersByPrimType[type];
}

SdfPathUnorderedSet HdNukeAdapterManager::GetPathsForSubTree(const SdfPath& path)
{
    SdfPathUnorderedSet paths;
    for (const auto& adapter : _adapters) {
        if (adapter.first.HasPrefix(path)) {
            paths.insert(adapter.first);
        }
    }
    return paths;
}

HdNukeAdapterManager::AdapterPromisePtr HdNukeAdapterManager::GetUnfulfilledPromise(const SdfPath& path)
{
    auto iter = _unfulfilledPromises.find(path);
    if (iter != _unfulfilledPromises.end()) {
        return iter->second;
    }
    return nullptr;
}

void HdNukeAdapterManager::SetUsed(bool used, const TfToken& primType)
{
    const auto& primPaths = GetPathsForPrimType(primType);
    for (const auto& path : primPaths) {
        GetAdapter(path)->SetUsed(used);
    }
}

void HdNukeAdapterManager::SetAllUnused()
{
    for (const auto& adapter : _adapters) {
        adapter.second.adapter->SetUsed(false);
    }
}

const SdfPathUnorderedSet& HdNukeAdapterManager::GetRequestedAdapters() const
{
    return _requestedAdapters;
}

void HdNukeAdapterManager::RemoveUnusedAdapters()
{
    SdfPathUnorderedSet toRemove;
    for (auto& adapter : _adapters) {
        if (_requestedAdapters.find(adapter.first) == _requestedAdapters.end()) {
            toRemove.insert(adapter.first);
        }
    }
    for (const auto& path : toRemove) {
        Remove(path);
    }
    _requestedAdapters.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE
