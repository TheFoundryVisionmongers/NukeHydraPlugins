#include "instancedGeoAdapter.h"

#include "adapterFactory.h"
#include "adapterManager.h"
#include "sceneDelegate.h"
#include "types.h"
#include "tokens.h"

#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

bool HdNukeInstancedGeoAdapter::SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<GeoInfoVector>(),
          "HdNukeInstancedGeoAdapter expects a GeoInfoVector")) {
        return false;
    }
    auto geoInfoVector = nukeData.UncheckedGet<GeoInfoVector>();
    _geoInfo = geoInfoVector.front();

    _hash = geoInfoVector.front()->source_geo->Op::hash();

    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();

    _instancerPath = GetPath().AppendChild(HdInstancerTokens->instancer);
    auto instancerPromise = manager->Request(HdNukeAdapterManagerPrimTypes->Instancer, _instancerPath, nukeData);

    renderIndex.InsertRprim(GetPrimType(), sceneDelegate, GetPath());

    HdNukeGeoAdapter::Update(*geoInfoVector.front(), HdChangeTracker::AllDirty, true);
    _SetMaterial(manager);

    return true;
}

bool HdNukeInstancedGeoAdapter::Update(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<GeoInfoVector>(),
          "HdNukeInstancedGeoAdapter expects a GeoInfoVector")) {
        return false;
    }
    auto geoInfoVector = nukeData.UncheckedGet<GeoInfoVector>();
    _geoInfo = geoInfoVector.front();

    if (_hash != geoInfoVector.front()->source_geo->Op::hash()) {
        auto sceneDelegate = manager->GetSceneDelegate();
        auto& renderIndex = sceneDelegate->GetRenderIndex();
        auto& changeTracker = renderIndex.GetChangeTracker();

        changeTracker.MarkRprimDirty(GetPath());
        HdNukeGeoAdapter::Update(*geoInfoVector.front(), HdChangeTracker::AllDirty, true);
    }

    _instancerPath = GetPath().AppendChild(HdInstancerTokens->instancer);
    auto instancerPromise = manager->Request(HdNukeAdapterManagerPrimTypes->Instancer, _instancerPath, nukeData);
    _SetMaterial(manager);

    _hash = geoInfoVector.front()->source_geo->Op::hash();
    return true;
}

void HdNukeInstancedGeoAdapter::TearDown(HdNukeAdapterManager* manager)
{
    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    renderIndex.RemoveRprim(GetPath());
}

VtValue HdNukeInstancedGeoAdapter::Get(const TfToken& key) const
{
    if (key == HdNukeTokens->instancerId) {
        return VtValue(_instancerPath);
    }
    return HdNukeGeoAdapter::Get(key);
}

class InstancedGeoAdapterCreator : public HdNukeAdapterFactory::AdapterCreator {
public:
    HdNukeAdapterPtr Create(AdapterSharedState *sharedState) override
    {
        return std::make_shared<HdNukeInstancedGeoAdapter>(sharedState);
    }
};

static const AdapterRegister<InstancedGeoAdapterCreator> sRegisterInstancedGeoCreator(HdNukeAdapterManagerPrimTypes->InstancedGeo);

PXR_NAMESPACE_CLOSE_SCOPE
