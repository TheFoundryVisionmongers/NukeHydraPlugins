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
#include "DDImage/Enumeration_KnobI.h"
#include "DDImage/Iop.h"

#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hdx/simpleLightTask.h>
#include <pxr/imaging/hdx/shadowMatrixComputation.h>
#include <pxr/imaging/cameraUtil/conformWindow.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/base/gf/frustum.h>
#if PXR_METAL_SUPPORT_ENABLED
#include <pxr/imaging/garch/simpleLight.h>
#else
#include <pxr/imaging/glf/simpleLight.h>
#endif

#include "lightAdapter.h"
#include "utils.h"
#include "adapterFactory.h"
#include "adapterManager.h"
#include "sceneDelegate.h"

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hdSt/renderDelegate.h>


using namespace DD::Image;

PXR_NAMESPACE_OPEN_SCOPE

namespace {
    Vector3 ConvertLightDirection(const DD::Image::LightOp* light, const Vector3 &direction) {
        auto rotate = dynamic_cast<ValueStuffI*>(light->knob("rotate"));
        auto rotateOrder = dynamic_cast<ValueStuffI*>(light->knob("rot_order"));
        if (rotate != nullptr && rotateOrder != nullptr) {
            Matrix4::RotationOrder order = static_cast<Matrix4::RotationOrder>(
              static_cast<int>(rotateOrder->get_value()));
            Matrix4 rotationMatrix = Matrix4::identity();
            rotationMatrix.rotate(order, {
              static_cast<float>(radians(rotate->get_value(0))),
              static_cast<float>(radians(rotate->get_value(1))),
              static_cast<float>(radians(rotate->get_value(2)))
            });
            return rotationMatrix.vtransform(direction);
        }
        return direction;
    }

    class ShadowMatrix : public HdxShadowMatrixComputation
    {
    public:
        ShadowMatrix(bool ortho, const GfVec3d &pos, const GfVec3d &rot)
        {
            GfFrustum frustum;
            frustum.SetProjectionType(ortho ? GfFrustum::Orthographic : GfFrustum::Perspective);
            frustum.SetWindow(GfRange2d(GfVec2d(-10, -10), GfVec2d(10, 10)));
            frustum.SetPosition(pos);
            frustum.SetViewDistance(1);
            frustum.SetRotation(GfRotation({0,0,1}, {rot[0], rot[1], rot[2]}));

            if (ortho) {
                frustum.SetNearFar(GfRange1d(-100, 100));
            }
            else {
                frustum.SetNearFar(GfRange1d(0.1, 100));
            }

            _shadowMatrix =
                frustum.ComputeViewMatrix() * frustum.ComputeProjectionMatrix();
        }

        virtual std::vector<GfMatrix4d> Compute(
            const GfVec4f &viewport, CameraUtilConformWindowPolicy policy)
        {
            return std::vector<GfMatrix4d>(1, _shadowMatrix);
        }
    private:
        GfMatrix4d _shadowMatrix;
    };

    TfToken GetHighestSupportedLightType(LightOp::LightType type, const HdRenderIndex& renderIndex) {
        const bool isHdStorm = dynamic_cast<HdStRenderDelegate*>(renderIndex.GetRenderDelegate()) !=  nullptr;
        TfToken lightType;
        if (isHdStorm && type != LightOp::eOtherLight) {
            // In HdStorm we need to use a simple light for directional, point and spot lights
            // so that we can control all parameters in the light adapter.
            lightType = HdPrimTypeTokens->simpleLight;
        }
        else {
            switch (type) {
                case LightOp::eDirectionalLight:
                    lightType = HdPrimTypeTokens->distantLight;
                    break;
                case LightOp::eSpotLight:
                    lightType = HdPrimTypeTokens->diskLight;
                    break;
                case LightOp::ePointLight:
                    lightType = HdPrimTypeTokens->sphereLight;
                    break;
                case LightOp::eOtherLight:
                    // XXX: The only other current type is an environment light, but
                    // the node is missing a lot of necessary options...
                    lightType = HdPrimTypeTokens->domeLight;
                    break;
                default:
                    return lightType;
            }
        }

        if (renderIndex.IsSprimTypeSupported(lightType)) {
            return lightType;
        }

        if (renderIndex.IsSprimTypeSupported(HdPrimTypeTokens->simpleLight)) {
            return HdPrimTypeTokens->simpleLight;
        }

        TF_WARN("Selected render delegate does not support %s or %s.", lightType.GetText(), HdPrimTypeTokens->simpleLight.GetText());

        return TfToken();
    }
}

HdNukeLightAdapter::HdNukeLightAdapter(AdapterSharedState* statePtr)
    : HdNukeAdapter(statePtr)
{
}

HdNukeLightAdapter::HdNukeLightAdapter(AdapterSharedState* statePtr,
                                       const LightOp* lightOp,
                                       const TfToken& lightType)
    : HdNukeAdapter(statePtr)
    , _light(lightOp)
    , _lightType(lightType)
    , _lastHash(lightOp->hash())
    , _castShadows(lightOp->cast_shadows())
{
}

void HdNukeLightAdapter::Update()
{
    _castShadows = _light->cast_shadows();
    _lastHash = _light->hash();
}

GfMatrix4d
HdNukeLightAdapter::GetTransform() const
{
    TF_VERIFY(_light);
    return DDToGfMatrix4d(_light->matrix());
}

bool HdNukeLightAdapter::SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<LightOp*>(),
          "HdNukeLightAdapter expects a LightOp")) {
        return false;
    }

    _light = nukeData.UncheckedGet<LightOp*>();
    _lastHash = _light->hash();

    HdNukeSceneDelegate* sceneDelegate = manager->GetSceneDelegate();
    HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();

    _lightType = GetHighestSupportedLightType(static_cast<LightOp::LightType>(_light->lightType()), renderIndex);
    _lastShadowCollectionHash = GetSharedState()->_shadowCollection.ComputeHash();

    renderIndex.InsertSprim(_lightType, sceneDelegate, GetPath());

    return true;
}

bool HdNukeLightAdapter::Update(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<LightOp*>(),
          "HdNukeLightAdapter expects a LightOp")) {
        return false;
    }
    _light = nukeData.UncheckedGet<LightOp*>();

    HdNukeSceneDelegate* sceneDelegate = manager->GetSceneDelegate();
    HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
    HdChangeTracker& changeTracker = renderIndex.GetChangeTracker();

    HdDirtyBits dirtyBits = HdLight::Clean;
    if (_lastHash != _light->hash()) {
        dirtyBits = DefaultDirtyBits;
        _lastHash = _light->hash();
    }

    auto shadowCollectionHash = GetSharedState()->_shadowCollection.ComputeHash();
    if (_lastShadowCollectionHash != shadowCollectionHash) {
        dirtyBits |= HdLight::DirtyCollection;
    }

    if (dirtyBits != HdLight::Clean) {
        changeTracker.MarkSprimDirty(GetPath(), dirtyBits);
    }

    _lastShadowCollectionHash = shadowCollectionHash;

    return true;
}

void HdNukeLightAdapter::TearDown(HdNukeAdapterManager* manager)
{
    HdNukeSceneDelegate* sceneDelegate = manager->GetSceneDelegate();
    HdRenderIndex& renderIndex = sceneDelegate->GetRenderIndex();
    renderIndex.RemoveSprim(_lightType, GetPath());
}

VtValue
HdNukeLightAdapter::GetLightParamValue(const TfToken& paramName) const
{
// TODO: We need a way to make sure the light still exists in the scene...
    TF_VERIFY(_light);

    if (paramName == HdLightTokens->color) {
        auto& pixel = _light->color();
        return VtValue(
            GfVec3f(pixel[Chan_Red], pixel[Chan_Green], pixel[Chan_Blue]));
    }
    else if (paramName == HdLightTokens->intensity or
             paramName == HdLightTokens->specular or
             paramName == HdLightTokens->diffuse) {
        return VtValue(_light->intensity());
    }
    else if (paramName == HdLightTokens->radius) {
        return VtValue(_light->sample_width());
    }
    else if (paramName == HdLightTokens->shadowColor) {
        return VtValue(GfVec3f(0));
    }
    else if (paramName == HdLightTokens->shadowEnable) {
        return VtValue(_light->cast_shadows() && _light->lightType() != LightOp::ePointLight);
    }
    else if (paramName == HdLightTokens->exposure){
        return VtValue(0.0f);
    }
    else if (paramName == HdTokens->transform)
    {
        return VtValue(GetTransform());
    }
    return VtValue();
}

VtValue HdNukeLightAdapter::Get(const TfToken& key) const
{
  auto lightType = _light->lightType();
  if (key == HdLightTokens->params) {
#if PXR_METAL_SUPPORT_ENABLED
      GarchSimpleLight simpleLight;
#else
      GlfSimpleLight simpleLight;
#endif
      Matrix4 matrix(_light->matrix());
      auto& pixel = _light->color();
      float intensity = _light->intensity();
      GfVec4f lightPosition(GfVec4f(matrix.a03, matrix.a13, matrix.a23, matrix.a33));
      GfVec3f lightAttenuation(1.0f, 0.0f, 0.0f);
      GfVec4f c(pixel[Chan_Red]*intensity, pixel[Chan_Green]*intensity, pixel[Chan_Blue]*intensity, 1.0f);
      simpleLight.SetDiffuse(c);
      simpleLight.SetSpecular(c);
      simpleLight.SetIsCameraSpaceLight(false);
      simpleLight.SetHasShadow(_light->cast_shadows());

      if (lightType == LightOp::eDirectionalLight) {
          Vector3 lightDirection = ConvertLightDirection(_light, {0,0,1});
          for (auto i=0u; i<3; ++i) {
              lightPosition[i] = lightDirection[i];
          }
          lightPosition[3] = 0;
      }
      else if (lightType == LightOp::eSpotLight) {
          float coneAngle = GetKnobValue(_light, "cone_angle", 30.0f);
          float conePenumbraAngle = GetKnobValue(_light, "cone_penumbra_angle", 0.0f);
          simpleLight.SetSpotCutoff(0.5 * (coneAngle + conePenumbraAngle));
          simpleLight.SetSpotFalloff(GetKnobValue(_light, "cone_falloff", 0.0f));
          Vector3 dir = ConvertLightDirection(_light, {0,0,-1});
          simpleLight.SetSpotDirection({dir[0], dir[1], dir[2]});
      }

      if (lightType != LightOp::eDirectionalLight) {
          std::string falloffType = GetKnobValue<std::string>(_light, "falloff_type");
          if (falloffType == "No Falloff") {
            lightAttenuation = GfVec3f(1.0f, 0.0f, 0.0f);
          }
          else if (falloffType == "Linear") {
            lightAttenuation = GfVec3f(0.0f, 1.0f, 0.0f);
          }
          else if (falloffType == "Quadratic" || falloffType == "Cubic") {
            lightAttenuation = GfVec3f(0.0f, 0.0f, 1.0f);
          }
      }

      simpleLight.SetPosition(lightPosition);
      simpleLight.SetAttenuation(lightAttenuation);
      return VtValue(simpleLight);
  }
  else if (_light->cast_shadows() && lightType != LightOp::ePointLight) {
      if (key == HdLightTokens->shadowParams) {
          auto lightType = _light->lightType();
          auto dir = ConvertLightDirection(_light, {0,0,1});
          Matrix4 matrix(_light->matrix());
          GfVec3f lightPosition(GfVec3f(matrix.a03, matrix.a13, matrix.a23));

          HdxShadowParams shadowParams;
          shadowParams.enabled = _light->cast_shadows();
          shadowParams.shadowMatrix = HdxShadowMatrixComputationSharedPtr(
                  new ShadowMatrix(
                      lightType == LightOp::eDirectionalLight /* ortho */,
                      lightPosition,
                      {dir[0], dir[1], dir[2]}));

          /*
           * Shadow parameters in Nuke and Hydra don't match one-to-one.
           * Therefore, these are only approximations.
           */
          // For directional lights
          float blurFactor = 0.001f;
          if (lightType == LightOp::eSpotLight) {
              blurFactor = 0.01f;
          }
          shadowParams.resolution = GetKnobValue(_light, "depthmap_width", 1024);
          shadowParams.bias = -0.0001f;
          shadowParams.blur = blurFactor * (GetKnobValue(_light, "samples", 0.0f) +
                                            GetKnobValue(_light, "sample_width", 0.0f) +
                                            GetKnobValue(_light, "shadow_jitter_scale", 0.0f));
          return VtValue(shadowParams);
      }
      else if (key == HdLightTokens->shadowCollection) {
          return VtValue{GetSharedState()->_shadowCollection};
      }
  }

  return GetLightParamValue(key);
}

const TfToken& HdNukeLightAdapter::GetPrimType() const
{
    return _lightType;
}

const HdDirtyBits HdNukeLightAdapter::DefaultDirtyBits =
    HdLight::DirtyTransform | HdLight::DirtyParams | HdLight::DirtyShadowParams;

class LightAdapterCreator : public HdNukeAdapterFactory::AdapterCreator
{
public:
    HdNukeAdapterPtr Create(AdapterSharedState* sharedState) override
    {
        return std::make_shared<HdNukeLightAdapter>(sharedState);
    }
};

static const AdapterRegister<LightAdapterCreator> sRegisterGenericIopCreator(HdNukeAdapterManagerPrimTypes->Light);

PXR_NAMESPACE_CLOSE_SCOPE
