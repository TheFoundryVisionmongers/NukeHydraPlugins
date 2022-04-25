#include "particleSpriteAdapter.h"

#include "adapterFactory.h"
#include "adapterManager.h"
#include "sceneDelegate.h"
#include "tokens.h"

#include <DDImage/GeoInfo.h>
#include <DDImage/Iop.h>

#include <pxr/imaging/hd/tokens.h>

using namespace DD::Image;

PXR_NAMESPACE_OPEN_SCOPE

namespace {
    template<typename T>
    T GetGeoInfoAttrib( const DD::Image::GeoInfo& info, DD::Image::GroupType groupType, const char* attribName, DD::Image::AttribType attribType )
    {
        const DD::Image::AttribContext* context = info.get_typed_group_attribcontext( groupType, attribName, attribType );
        if (!context || context->empty() || !context->attribute ) {
            return nullptr;
        }

        return reinterpret_cast<T>(context->attribute->array());
    }

    Iop* materialOpForGeo(const GeoInfo* geoInfo)
    {
      Iop* materialOp = geoInfo->material;
      if ( materialOp != nullptr && std::strcmp(materialOp->Class(), "Black") != 0 ) {
        return materialOp;
      }
      return nullptr;
    }
}

class DefaultParticleMaterialAdapter : public HdNukeMaterialAdapter
{
public:

    DefaultParticleMaterialAdapter(AdapterSharedState* statePtr)
        : HdNukeMaterialAdapter(statePtr) { }
    ~DefaultParticleMaterialAdapter() override { }

    bool SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData) override
    {
        auto sceneDelegate = manager->GetSceneDelegate();
        auto& renderIndex = sceneDelegate->GetRenderIndex();

        auto materialNetwork = HdNukeMaterialAdapter::GetParticlesMaterialResource(sceneDelegate->DefaultParticleMaterialId());

        SetMaterialNetwork(materialNetwork);

        renderIndex.InsertSprim(HdPrimTypeTokens->material, sceneDelegate, sceneDelegate->DefaultParticleMaterialId());

        return true;
    }

    bool Update(HdNukeAdapterManager* manager, const VtValue& nukeData) override
    {
        return true;
    }

    void TearDown(HdNukeAdapterManager* manager) override
    {
        auto sceneDelegate = manager->GetSceneDelegate();
        auto& renderIndex = sceneDelegate->GetRenderIndex();
        renderIndex.RemoveSprim(HdPrimTypeTokens->material, sceneDelegate->DefaultParticleMaterialId());
    }
};

void
HdNukeParticleSpriteAdapter::MakeParticleSprite()
{
    _points.clear();
    _points.push_back(pxr::GfVec3f(-0.5f, -0.5f, 0.0f));
    _points.push_back(pxr::GfVec3f(-0.5f,  0.5f, 0.0f));
    _points.push_back(pxr::GfVec3f( 0.5f,  0.5f, 0.0f));
    _points.push_back(pxr::GfVec3f( 0.5f, -0.5f, 0.0f));

    _uvs.clear();
    _uvs.push_back(pxr::GfVec2f(0, 0));
    _uvs.push_back(pxr::GfVec2f(0, 1));
    _uvs.push_back(pxr::GfVec2f(1, 1));
    _uvs.push_back(pxr::GfVec2f(1, 0));

    VtIntArray faceVertexCounts;
    faceVertexCounts.push_back(4);

    VtIntArray faceVertexIndices;
    faceVertexIndices.push_back(0);
    faceVertexIndices.push_back(1);
    faceVertexIndices.push_back(2);
    faceVertexIndices.push_back(3);

    _topology = HdMeshTopology(PxOsdOpenSubdivTokens->none,
                               UsdGeomTokens->rightHanded, faceVertexCounts,
                               faceVertexIndices);

    _extent.SetMin(GfVec3d(-0.5f, -0.5f, -0.001f));
    _extent.SetMax(GfVec3d( 0.5f,  0.5f, 0.001f));

    _reprSelector = HdReprSelector(HdReprTokens->refined, HdReprTokens->refined);

    _transform.SetIdentity();

    static HdPrimvarDescriptor pointsDescriptor(
            HdTokens->points, HdInterpolationVertex,
            HdPrimvarRoleTokens->point);
    static HdPrimvarDescriptor uvsDescriptor(
            HdNukeTokens->st, HdInterpolationVertex,
            HdPrimvarRoleTokens->textureCoordinate);
    static HdPrimvarDescriptor displayColorDescriptor(
            HdTokens->displayColor, HdInterpolationConstant,
            HdPrimvarRoleTokens->color);

    _constantPrimvarDescriptors.clear();

    _vertexPrimvarDescriptors.clear();
    _vertexPrimvarDescriptors.push_back(pointsDescriptor);
    _vertexPrimvarDescriptors.push_back(uvsDescriptor);

    _displayColor = GfVec3f(1, 1, 1);
}

bool HdNukeParticleSpriteAdapter::SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<GeoInfo*>(),
          "HdNukeGeoAdapter expects a GeoInfo")) {
        return false;
    }
    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();

    _geoInfo = nukeData.UncheckedGet<GeoInfo*>();
    _hash = _geoInfo->source_geo->Op::hash();

    _SetMaterial(manager);

    MakeParticleSprite();

    _instancerPath = GetPath().AppendChild(HdInstancerTokens->instancer);
    auto instancerPromise = manager->Request(HdNukeAdapterManagerPrimTypes->Instancer, _instancerPath, nukeData);

    if (_geoInfo->display3d != DD::Image::DISPLAY_OFF) {
        renderIndex.InsertRprim(GetPrimType(), sceneDelegate, GetPath());
    }
    else {
        renderIndex.RemoveRprim(GetPath());
    }

    return true;
}

bool HdNukeParticleSpriteAdapter::Update(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<GeoInfo*>(),
          "HdNukeGeoAdapter expects a GeoInfo")) {
        return false;
    }
    _geoInfo = nukeData.UncheckedGet<GeoInfo*>();
    auto sceneDelegate = manager->GetSceneDelegate();

    MakeParticleSprite();

    _instancerPath = GetPath().AppendChild(HdInstancerTokens->instancer);
    auto instancerPromise = manager->Request(HdNukeAdapterManagerPrimTypes->Instancer, _instancerPath, nukeData);

    if (_hash != _geoInfo->source_geo->Op::hash()) {
        auto& renderIndex = sceneDelegate->GetRenderIndex();
        auto& changeTracker = renderIndex.GetChangeTracker();
        if (_geoInfo->display3d != DD::Image::DISPLAY_OFF) {
            renderIndex.InsertRprim(GetPrimType(), sceneDelegate, GetPath());
        }
        else {
            renderIndex.RemoveRprim(GetPath());
        }
        changeTracker.MarkRprimDirty(GetPath());
    }

    _SetMaterial(manager);

    _hash = _geoInfo->source_geo->Op::hash();

    return true;
}

void HdNukeParticleSpriteAdapter::TearDown(HdNukeAdapterManager* manager)
{
    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    renderIndex.RemoveRprim(GetPath());
}

VtValue HdNukeParticleSpriteAdapter::Get(const TfToken& key) const
{
    if (key == HdNukeTokens->instancerId) {
        return VtValue(_instancerPath);
    }
    return HdNukeGeoAdapter::Get(key);
}

void HdNukeParticleSpriteAdapter::_SetMaterial(HdNukeAdapterManager* manager)
{
    if (Iop* material = materialOpForGeo(_geoInfo)) {
        HdMaterialNetworkMap materialNetwork;
        TfToken output = HdMaterialTerminalTokens->surface;
        bool textures = _geoInfo->display3d == DISPLAY_TEXTURED_LINES || _geoInfo->display3d == DISPLAY_TEXTURED || _geoInfo->display3d == DISPLAY_UNCHANGED;
        HydraMaterialContext::MaterialFlags flags =
          (GetSharedState()->useEmissiveTextures ? HydraMaterialContext::eForceEmissive : 0)
          | (textures ? HydraMaterialContext::eUseTextures : 0);
        HydraMaterialContext materialContext(GetSharedState()->_viewerContext, materialNetwork, output, std::move(flags));
        materialContext._materialOp = material;

        auto promise = manager->Request(materialContext);
        _materialId = promise->path;
    }
    else {
        auto sceneDelegate = manager->GetSceneDelegate();
        auto promise = manager->Request(TfToken{"defaultParticleMaterialId"}, sceneDelegate->DefaultParticleMaterialId());
        _materialId = promise->path;
    }
}

const TfToken& HdNukeParticleSpriteAdapter::GetPrimType() const
{
    return HdPrimTypeTokens->mesh;
}

class ParticleSpriteAdapterCreator : public HdNukeAdapterFactory::AdapterCreator {
public:
    HdNukeAdapterPtr Create(AdapterSharedState *sharedState) override
    {
        return std::make_shared<HdNukeParticleSpriteAdapter>(sharedState);
    }
};

static const AdapterRegister<ParticleSpriteAdapterCreator> sRegisterParticleSpriteCreator(HdNukeAdapterManagerPrimTypes->ParticleSprite);


class DefaultParticleMaterialAdapterCreator : public HdNukeAdapterFactory::AdapterCreator {
public:
    HdNukeAdapterPtr Create(AdapterSharedState *sharedState) override
    {
        return std::make_shared<DefaultParticleMaterialAdapter>(sharedState);
    }
};

static const AdapterRegister<DefaultParticleMaterialAdapterCreator> sRegisterParticleMaterialCreator(TfToken{"defaultParticleMaterialId"});

PXR_NAMESPACE_CLOSE_SCOPE

