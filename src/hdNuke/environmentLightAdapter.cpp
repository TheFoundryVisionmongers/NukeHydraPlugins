#include "environmentLightAdapter.h"

#include "sceneDelegate.h"
#include "adapterManager.h"
#include "utils.h"
#include "adapterFactory.h"
#include "nukeTexturePlugin.h"

#include <DDImage/Iop.h>
#include <DDImage/LightOp.h>

#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/renderIndex.h>

#include <pxr/base/gf/matrix4d.h>

#if PXR_METAL_SUPPORT_ENABLED
  #if PXR_VERSION >= 2105
    #include <pxr/imaging/hio/image.h>
  #else
    #include <pxr/imaging/garch/image.h>
  #endif
  #include <pxr/imaging/hdSt/resourceFactory.h>
#else
  #include <pxr/imaging/glf/texture.h>
#endif

PXR_NAMESPACE_OPEN_SCOPE

HdNukeEnvironmentLightAdapter::HdNukeEnvironmentLightAdapter(AdapterSharedState* statePtr)
    : HdNukeAdapter(statePtr)
{
}

HdNukeEnvironmentLightAdapter::~HdNukeEnvironmentLightAdapter()
{
}

bool HdNukeEnvironmentLightAdapter::SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<DD::Image::LightOp*>(),
          "HdNukeEnvironmentLightAdapter expects a LightOp")) {
        return false;
    }
    _lightOp = nukeData.UncheckedGet<DD::Image::LightOp*>();
    _hash = _lightOp->hash();

    HdNukeSceneDelegate* sceneDelegate = manager->GetSceneDelegate();
    HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();

    _envMapIop = static_cast<DD::Image::Iop*>(_lightOp->input_op(1));
    bool envMapUsable = true;
    if (_envMapIop != nullptr) {
        envMapUsable = uploadTexture(renderIndex);
    }

    renderIndex.InsertSprim(HdPrimTypeTokens->domeLight, sceneDelegate, GetPath());

    return envMapUsable;
}

bool HdNukeEnvironmentLightAdapter::Update(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<DD::Image::LightOp*>(),
          "HdNukeEnvironmentLightAdapter expects a LightOp")) {
        return false;
    }
    _lightOp = nukeData.UncheckedGet<DD::Image::LightOp*>();

    if (_hash == _lightOp->hash()) {
        return true;
    }
    _hash = _lightOp->hash();

    HdNukeSceneDelegate* sceneDelegate = manager->GetSceneDelegate();
    HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
    HdChangeTracker& changeTracker = renderIndex.GetChangeTracker();

    _envMapIop = static_cast<DD::Image::Iop*>(_lightOp->input_op(1));
    bool envMapUsable = true;
    if (_envMapIop != nullptr) {
        envMapUsable = uploadTexture(renderIndex);
    }
    else {
        NukeTexturePlugin::Instance().RemoveFile(_assetPath.GetAssetPath());
        _assetPath = SdfAssetPath();
    }

    changeTracker.MarkSprimDirty(GetPath(), DefaultDirtyBits);

    return envMapUsable;
}

void HdNukeEnvironmentLightAdapter::TearDown(HdNukeAdapterManager* manager)
{
    HdNukeSceneDelegate* sceneDelegate = manager->GetSceneDelegate();
    HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
    renderIndex.RemoveSprim(HdPrimTypeTokens->domeLight, GetPath());
    NukeTexturePlugin::Instance().RemoveFile(_assetPath.GetAssetPath());
    _assetPath = SdfAssetPath();
}

VtValue HdNukeEnvironmentLightAdapter::Get(const TfToken& key) const
{
    if (key == HdLightTokens->textureFile) {
        return VtValue{_assetPath};
    }
    if (key == HdTokens->transform) {
        // Dome lights seem to need its transform flipped on the x and y axis.
        static const GfMatrix4d flipMatrix(GfVec4d(-1.0, -1.0, 1.0, 1.0));
        return VtValue{flipMatrix * DDToGfMatrix4d(_lightOp->matrix())};
    }
    return HdNukeAdapter::Get(key);
}

bool HdNukeEnvironmentLightAdapter::uploadTexture(HdRenderIndex& renderIndex)
{
    HdResourceRegistrySharedPtr registry = renderIndex.GetResourceRegistry();
    _envMapIop->set_texturemap(GetSharedState()->_viewerContext, true);
    auto textureMap = _envMapIop->get_texturemap(GetSharedState()->_viewerContext);
    _envMapIop->unset_texturemap(GetSharedState()->_viewerContext);

    if (textureMap.buffer == nullptr) {
        return false;
    }

    std::string filename = _envMapIop->node_name() + ".nuke";
    auto file = NukeTexturePlugin::Instance().GetFile(filename);
    if (file == nullptr) {
        NukeTexturePlugin::Instance().AddFile(filename, textureMap);
    }

    TfToken filenameToken{filename};
    _assetPath = SdfAssetPath(filenameToken, filenameToken);

    registry->ReloadResource(HdResourceTypeTokens->texture, _assetPath.GetAssetPath());

    return true;
}

const TfToken& HdNukeEnvironmentLightAdapter::GetPrimType() const
{
    return HdPrimTypeTokens->domeLight;
}

const HdDirtyBits HdNukeEnvironmentLightAdapter::DefaultDirtyBits =
    HdLight::DirtyTransform | HdLight::DirtyParams | HdLight::DirtyShadowParams;

class EnvironmentLightCreator : public HdNukeAdapterFactory::AdapterCreator { public:
    HdNukeAdapterPtr Create(AdapterSharedState *sharedState) override
    {
        return std::make_shared<HdNukeEnvironmentLightAdapter>(sharedState);
    }
};

static const AdapterRegister<EnvironmentLightCreator> sRegisterEnvironmentCreator(HdNukeAdapterManagerPrimTypes->Environment);

PXR_NAMESPACE_CLOSE_SCOPE
