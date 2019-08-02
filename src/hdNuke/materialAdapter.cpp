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

#include "adapterFactory.h"
#include "adapterManager.h"
#include "materialAdapter.h"
#include "opBases.h"
#include "sceneDelegate.h"
#include "nukeTexturePlugin.h"
#include "types.h"
#include "tokens.h"

#include <DDImage/Knob.h>
#include <DDImage/Knobs.h>
#include <DDImage/Iop.h>
#include <DDImage/ViewerContext.h>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdr/registry.h>
#include <pxr/imaging/hio/glslfx.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/usdImaging/usdImaging/tokens.h>


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    HdNukeMaterialTokens,

    (rgba)
    (diffuseColor)
);

HdNukeMaterialAdapter::HdNukeMaterialAdapter(AdapterSharedState* statePtr)
    : HdNukeAdapter(statePtr)
{
}

HdNukeMaterialAdapter::HdNukeMaterialAdapter(AdapterSharedState* statePtr, const SdfPath& materialId)
    : HdNukeAdapter(statePtr)
    , _materialId(materialId)
{
}

bool
HdNukeMaterialAdapter::Update(DD::Image::ViewerContext* viewerContext, DD::Image::Op* materialOp, HydraMaterialContext::MaterialFlags&& flags)
{
    DD::Image::Hash hash = materialOp->hash();
    hash << static_cast<int>(flags.to_ulong());
    if (hash != _hash) {
        _hash = hash;

        // The material has changed. Rebuild its network.
        HdMaterialNetworkMap map;

        SdfPath path = _materialId.AppendChild(TfToken("Surface"));
        HydraMaterialContext context(viewerContext, map, HdMaterialTerminalTokens->surface, std::move(flags));
        HydraMaterialOp::CreateMaterialInput(materialOp, context, path, nullptr, HdNukeMaterialTokens->diffuseColor );

        _textureNeedsReloading = true;
        if (context.hasFlags(HydraMaterialContext::eUseTextures) && !context._queuedTextures.empty()) {
          _hash.newvalue();
          _textureNeedsReloading = false;
        }

        map.terminals.push_back(path);
        _materialNetwork = VtValue::Take(map);
        return true;
    }
    return false;
}

bool HdNukeMaterialAdapter::SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<HydraMaterialContext>(),
          "HdNukeMaterialAdapter expects a Iop")) {
        return false;
    }
    auto materialCtx = nukeData.UncheckedGet<HydraMaterialContext>();
    _iop = materialCtx._materialOp;

    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();

    // Material network
    bool created = createMaterialNetwork(renderIndex, materialCtx);
    renderIndex.InsertSprim(HdPrimTypeTokens->material, sceneDelegate, GetPath());

    _hash = _iop->hash();
    _hash << static_cast<int>(materialCtx._materialFlags.to_ulong());

    return created;
}

bool HdNukeMaterialAdapter::Update(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<HydraMaterialContext>(),
          "HdNukeMaterialAdapter expects a Iop")) {
        return false;
    }
    auto materialCtx = nukeData.UncheckedGet<HydraMaterialContext>();
    _iop = materialCtx._materialOp;

    DD::Image::Hash hash = _iop->hash();
    hash << static_cast<int>(materialCtx._materialFlags.to_ulong());

    if (_hash == hash) {
        return true;
    }

    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    auto& changeTracker = renderIndex.GetChangeTracker();

    // Material network
    bool created = createMaterialNetwork(renderIndex, materialCtx);
    changeTracker.MarkSprimDirty(GetPath(), HdChangeTracker::AllDirty);
    _hash = _iop->hash();

    return created;
}

void HdNukeMaterialAdapter::TearDown(HdNukeAdapterManager* manager)
{
    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    for (const auto& assetPath : _textures) {
        NukeTexturePlugin::Instance().RemoveFile(assetPath.GetAssetPath());
    }
    renderIndex.RemoveSprim(HdPrimTypeTokens->material, GetPath());
}

bool HdNukeMaterialAdapter::createMaterialNetwork(HdRenderIndex& renderIndex, HydraMaterialContext& materialCtx)
{
    SdfPath surfacePath = GetPath().AppendChild(TfToken("Surface"));
    HdMaterialNetworkMap& material = materialCtx._map;
    HydraMaterialOp::CreateMaterialInput(_iop, materialCtx, surfacePath, nullptr, HdNukeMaterialTokens->diffuseColor);
    material.terminals.push_back(surfacePath);

    HdResourceRegistrySharedPtr registry = renderIndex.GetResourceRegistry();
    auto it = material.map.find(HdMaterialTerminalTokens->surface);
    if (it != material.map.end()) {
        const HdMaterialNetwork& network = it->second;
        for (auto node : network.nodes) {
            if (node.identifier == UsdImagingTokens->UsdUVTexture) {
                auto fileParam = node.parameters[TfToken("file")].Get<SdfAssetPath>();
                auto assetPath = fileParam.GetAssetPath();
                registry->ReloadResource(HdResourceTypeTokens->texture, assetPath);
                _textures.insert(fileParam);
            }
        }
    }

    _materialNetwork = VtValue::Take(material);
    return materialCtx._queuedTextures.empty();
}

VtValue HdNukeMaterialAdapter::Get(const TfToken& key) const
{
    if (key == HdNukeTokens->materialResource) {
        return _materialNetwork;
    }
    return HdNukeAdapter::Get(key);
}

const TfToken& HdNukeMaterialAdapter::GetPrimType() const
{
    return HdPrimTypeTokens->material;
}

VtValue
HdNukeMaterialAdapter::GetParticlesMaterialResource(const SdfPath &materialId)
{
  // Do not indent this string. The whitespace is important.
  static constexpr const char* sShaderSource =
R"(-- glslfx version 0.1
-- configuration
{
  "parameters": {
  },
  "techniques" : {
    "default" : {
      "surfaceShader" : {
        "source": [ "Default.ParticleShader" ]
      }
    }
  }
}
-- glsl Default.ParticleShader
vec4 surfaceShader(vec4 Peye, vec3 Neye, vec4 color, vec4 patchCoord)
{
  return color;
}
)";

  // register the shader
  static SdrShaderNodeConstPtr _sdrSurfaceNode;
  if (!_sdrSurfaceNode) {
    SdrRegistry &shaderReg = SdrRegistry::GetInstance();
    _sdrSurfaceNode =
        shaderReg.GetShaderNodeFromSourceCode(sShaderSource, HioGlslfxTokens->glslfx, NdrTokenMap());
  }

  HdMaterialNetworkMap material;
  HdMaterialNetwork& network = material.map[HdMaterialTerminalTokens->surface];
  HdMaterialNode terminal;
  terminal.path = materialId;
  terminal.identifier = _sdrSurfaceNode->GetIdentifier();

  material.terminals.push_back(terminal.path);
  network.nodes.emplace_back(std::move(terminal));

  return VtValue::Take(material);
}

/* static */
VtValue
HdNukeMaterialAdapter::GetPreviewMaterialResource(const SdfPath& materialId)
{
    HdMaterialNode node;
    node.identifier = UsdImagingTokens->UsdPreviewSurface;
    node.path = materialId.AppendChild(TfToken("Surface"));
    node.parameters = GetPreviewSurfaceParameters();

    HdMaterialNetwork network;
    network.nodes.push_back(node);

    HdMaterialNetworkMap map;
    map.map.emplace(HdMaterialTerminalTokens->surface, network);
    return VtValue::Take(map);
}

/* static */
std::map<TfToken, VtValue>
HdNukeMaterialAdapter::GetPreviewSurfaceParameters()
{
    static std::once_flag scanPreviewSurfaceParamsFlag;
    static std::map<TfToken, VtValue> previewSurfaceParams;

    std::call_once(scanPreviewSurfaceParamsFlag, [] {
        auto& registry = SdrRegistry::GetInstance();
        SdrShaderNodeConstPtr sdrNode = registry.GetShaderNodeByIdentifier(
            UsdImagingTokens->UsdPreviewSurface);
        if (TF_VERIFY(sdrNode)) {
            for (const auto& inputName : sdrNode->GetInputNames())
            {
                auto shaderInput = sdrNode->GetInput(inputName);
                if (TF_VERIFY(shaderInput)) {
                    previewSurfaceParams.emplace(
                        inputName, shaderInput->GetDefaultValue());
                }
            }
        }
    });

    return previewSurfaceParams;
}


static std::map<TfToken, HydraMaterialOp*> sProxyMaterialOps;

MaterialProxyRegistry& MaterialProxyRegistry::Instance()
{
  static MaterialProxyRegistry sInstance;
  return sInstance;
}

const void MaterialProxyRegistry::RegisterMaterialProxy(const char* className, HydraMaterialOp* proxy)
{
  sProxyMaterialOps[TfToken(className)] = proxy;
}

const HydraMaterialOp* MaterialProxyRegistry::GetMaterialProxy(const char* className) const
{
  return sProxyMaterialOps[TfToken(className)];
}

const HydraMaterialOp* MaterialProxyRegistry::GetMaterialProxy(DD::Image::Op* op) const
{
  if (op == nullptr) {
    return nullptr;
  }
  const HydraMaterialOp* materialOp = dynamic_cast<const HydraMaterialOp*>(op);
  if (materialOp == nullptr) {
    materialOp = sProxyMaterialOps[TfToken(op->Class())];
    if (materialOp == nullptr && DD::Image::op_cast<DD::Image::Iop*>(op) != nullptr) {
      materialOp = sProxyMaterialOps[TfToken("_GenericIop")];
    }
  }
  return materialOp;
}


// A very basic first attempt at producing a UsdPreviewSurface network from a Nuke material network.
// This only needs to be good enough for preview rendering in hdStorm for the moment.
// None of the Nuke material nodes map at all well onto UsdPreviewSurface so most netowrks won't
// work particularly well. The simple case of textured or coloured geometry will mostly work though.
pxr::HdMaterialNode* HydraMaterialOp::CreateMaterialInput(DD::Image::Op* op, HydraMaterialContext& context, const pxr::SdfPath& materialId, pxr::HdMaterialNode* parentNode, const pxr::TfToken& parentInput )
{
  pxr::HdMaterialNode* node = nullptr;

  if (const HydraMaterialOp* materialOp = MaterialProxyRegistry::Instance().GetMaterialProxy(op)) {
    node = materialOp->CreateMaterial(op, context, materialId, parentNode);
  }

  if (parentNode != nullptr && node != nullptr) {
    HdMaterialNetwork& network = context.network();
    HdMaterialRelationship rel;
    rel.inputId = node->path;
    rel.inputName = HdNukeMaterialTokens->rgba;
    rel.outputId = parentNode->path;
    rel.outputName = parentInput;
    network.relationships.emplace_back(std::move(rel));
  }

  return node;
}

class MaterialAdapterCreator : public HdNukeAdapterFactory::AdapterCreator
{
public:
    HdNukeAdapterPtr Create(AdapterSharedState* sharedState) override
    {
        return std::make_shared<HdNukeMaterialAdapter>(sharedState);
    }
};

static const AdapterRegister<MaterialAdapterCreator> sRegisterGenericIopCreator(HdNukeAdapterManagerPrimTypes->Material);

PXR_NAMESPACE_CLOSE_SCOPE
