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

#ifndef HDNUKE_ADAPTERMANAGER_H
#define HDNUKE_ADAPTERMANAGER_H

#include "types.h"

#include <pxr/pxr.h>
#include <pxr/imaging/hd/api.h>
#include <pxr/base/vt/value.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/usd/sdf/path.h>

#include <memory>

namespace DD
{
    namespace Image
    {
        class Op;
        class Iop;
        class GeoOp;
        class LightOp;
        class GeoInfo;
    }
}

PXR_NAMESPACE_OPEN_SCOPE

class HdNukeSceneDelegate;

class HdNukeAdapter;
using HdNukeAdapterPtr = std::shared_ptr<HdNukeAdapter>;
struct AdapterSharedState;
struct HydraMaterialContext;

#define HDNUKEADAPTERMANAGER_PRIM_TYPES   \
  ((GenericGeoInfo, "_GenericGeoInfo"))   \
  (Light)                                 \
  (Material)                              \
  (Instancer)                             \
  (Environment)                           \
  (ParticleSprite)                        \
  (InstancedGeo)

TF_DECLARE_PUBLIC_TOKENS(HdNukeAdapterManagerPrimTypes, HD_API, HDNUKEADAPTERMANAGER_PRIM_TYPES);

/// Class to manage the creation, lifetime and dependencies of HdNukeAdapter
/// objects.
///
/// Client code is able to request different types of adapters, either by name
/// as registered in HdNukeAdapterFactory or by a Nuke Op. Because a
/// HdNukeAdapter object might not be fully set up after a request (e.g. due to
/// asynchronous tasks), requests return an HdNukeAdapterManager::AdapterPromise
/// object which consists of a placeholder for an HdNukeAdapter object that will
/// be eventually fulfilled after the requested adapter is able to finalize its
/// set up. Attempts to fulfill the promises are done by calling
/// HdNukeAdapterManager::TryFulfillPromises.
///
/// Calling a new adapter using any of the HdNukeAdapterManager::Request
/// methods creates a new HdNukeAdapter object and calls its
/// HdNukeAdapter::SetUp, giving it an opportunity to initialize itself.
/// Adapters can also request new adapters (e.g. traversing Nuke's node graph
/// upstream).
///
/// When trying to fulfill promises the HdNukeAdapter::SetUp method is called
/// which gives it another opportunity to set itself up. If it's still unable to
/// do so, the promise is kept as unfulfilled.
///
/// Removing an adapter calls its HdNukeAdapter::TearDown.
///
/// Automatically removing unused adapters is done by calling
/// HdNukeAdapterManager::RemoveUnusedAdapters which checks for adapters that
/// were not requested after the last time this method was called.
class HdNukeAdapterManager
{
public:
    /// Provides a placeholder for a HdNukeAdapter that might not be ready at
    /// the time HdNukeAdapterManager::Request returns
    /// A promise is considered unfulfilled if AdapterPromise::adapter is nullptr.
    struct AdapterPromise
    {
        AdapterPromise(const SdfPath& adapterPath, const HdNukeAdapterPtr& adapterPtr)
          : path{adapterPath}
          , adapter{adapterPtr}
        {
        }
        SdfPath path;               ///< Path pointing to the adapter. This is always filled.
        HdNukeAdapterPtr adapter;   ///< Points to the adapter if the promise has been fulfilled.
    };
    using AdapterPromisePtr = std::shared_ptr<AdapterPromise>;

    /// Constructs the HdNukeAdapterManager which always needs a HdNukeSceneDelegate.
    HdNukeAdapterManager(HdNukeSceneDelegate* sceneDelegate);
    /// Non copy-constructable.
    HdNukeAdapterManager(const HdNukeAdapterManager&) = delete;
    /// Non assignable.
    HdNukeAdapterManager& operator=(const HdNukeAdapterManager&) = delete;

    /// Returns the scene delegate associated with this adapter manager.
    inline HdNukeSceneDelegate* GetSceneDelegate() const { return _sceneDelegate; }

    /// Requests an adapter of the given \p adapterType at \p path.
    /// If an adapter already exists at \p path, it is returned, otherwise a
    /// new adapter is created.
    /// The \p nukeData argument is passed to HdNukeAdapter::SetUp. It is the
    /// callee responsibility to pass the correct data to this parameter.
    AdapterPromisePtr Request(const TfToken& adapterType, const SdfPath& path, const VtValue& nukeData = {});

    /// Convenience method to request a HdNukeAdapter from a DD::Image::GeoInfo.
    AdapterPromisePtr Request(DD::Image::GeoInfo* geoInfo, const SdfPath& parentPath = {});

    /// Convenience method to request a HdNukeAdapter from a DD::Image::GeoOp.
    AdapterPromisePtr Request(DD::Image::GeoOp* geoOp, const SdfPath& parentPath = {});

    /// Convenience method to request a HdNukeAdapter from a DD::Image::LightOp.
    AdapterPromisePtr Request(DD::Image::LightOp* lightOp, const SdfPath& parentPath = {});

    /// Convenience method to request a HdNukeAdapter from a DD::Image::Iop.
    AdapterPromisePtr Request(DD::Image::Iop* op, const SdfPath& parentPath = {});

    /// Convenience method to request a HdNukeAdapter with a HydraMaterialContext.
    AdapterPromisePtr Request(const HydraMaterialContext materialCtx, const SdfPath& parentPath = {});

    /// Convenience method to request a HdNukeAdapter for a set of instanced DD::Image::GeoInfo.
    AdapterPromisePtr Request(GeoInfoVector& instances, const SdfPath& parentPath = {});

    /// Adds an adapter that was created externally.
    /// This method might be removed in the near future.
    AdapterPromisePtr AddAdapter(const HdNukeAdapterPtr& adapter, const
        TfToken& primType, const SdfPath& path);

    /// Attempts to fulfill currently unfulfilled promises.
    /// @return The number of promises left unfulfilled.
    unsigned int TryFulfillPromises();

    /// Returns the number of promises that have not been fulfilled.
    std::size_t GetUnfulfilledPromisesCount() const { return _unfulfilledPromises.size(); }

    /// Returns the promise for an adapter at \p path if it is not fulfilled or
    /// nullptr otherwise.
    AdapterPromisePtr GetUnfulfilledPromise(const SdfPath& path);

    /// Returns the adapter at \p path.
    HdNukeAdapterPtr GetAdapter(const SdfPath& path) const;
    /// Returns the prim type associated with the adapter at \p path.
    const TfToken& GetPrimType(const SdfPath& path);

    /// Returns all the paths to adapters associated with the given prim \p type.
    const std::unordered_set<SdfPath, SdfPath::Hash>& GetPathsForPrimType(const TfToken& type);

    /// Returns the paths to adapters that have the \p path as prefix.
    SdfPathUnorderedSet GetPathsForSubTree(const SdfPath& path);

    /// Remove the adapter at the \p path.
    void Remove(const SdfPath& path);

    /// Removes all the adapters.
    void Clear();

    /// Sets whether all adapters of \p primType are used or not.
    void SetUsed(bool used, const TfToken& primType);

    /// Sets all adapters as unused.
    void SetAllUnused();

    /// Returns the path to all adapters that have been requested since the
    /// last call to HdNukeAdapterManager::RemoveUnusedAdapters.
    const SdfPathUnorderedSet& GetRequestedAdapters() const;

    /// Removes all unused adapters.
    void RemoveUnusedAdapters();

private:
    HdNukeSceneDelegate* _sceneDelegate;

    struct AdapterInfo
    {
        HdNukeAdapterPtr adapter;
        TfToken primType;
        VtValue nukeData;
        SdfPathUnorderedSet dependencies;
    };

    SdfPathMap<AdapterInfo> _adapters;
    TfTokenMap<SdfPathUnorderedSet> _adaptersByPrimType;

    SdfPathMap<AdapterPromisePtr> _unfulfilledPromises;
    SdfPathUnorderedSet _requestedAdapters;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDNUKE_ADAPTERMANAGER_H
