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
#include <pxr/imaging/hd/tokens.h>
#include <pxr/base/gf/rotation.h>

#include "instancerAdapter.h"
#include "adapterFactory.h"
#include "adapterManager.h"
#include "sceneDelegate.h"
#include "types.h"
#include "tokens.h"

#include <DDImage/DDMath.h>
#include <DDImage/Iop.h>
#include <DDImage/GeoInfo.h>
#include <DDImage/Format.h>

#include <numeric>

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
}

void
HdNukeInstancerAdapter::Update(const GeoInfoVector& geoInfoPtrs)
{
    _instanceXforms.resize(geoInfoPtrs.size());
    _colors.resize(geoInfoPtrs.size());
    for (size_t i = 0; i < geoInfoPtrs.size(); i++)
    {
        const float* matrixPtr = geoInfoPtrs[i]->matrix.array();
        std::copy(matrixPtr, matrixPtr + 16, _instanceXforms[i].data());
        const auto cf = GetGeoInfoAttrib<const DD::Image::Vector4*>(*geoInfoPtrs[i], DD::Image::Group_Object, "Cf", DD::Image::VECTOR4_ATTRIB);
        if (cf != nullptr) {
          const DD::Image::Vector4& color = cf[0];
          _colors[i] = GfVec3f(color.x, color.y, color.z);
        }
        else {
          _colors[i] = GfVec3f(0.0f);
        }
    }
}

void
HdNukeInstancerAdapter::UpdateParticles(const DD::Image::GeoInfo& geoInfo)
{
    _colors.clear();

    // Update the transforms for each particle sprite instance
    const DD::Image::PointList* pointList = geoInfo.point_list();
    if (pointList != nullptr) {
        const auto count = pointList->size();
        const auto velocity = GetGeoInfoAttrib<const DD::Image::Vector3*>(geoInfo, DD::Image::Group_Points, "vel", DD::Image::VECTOR3_ATTRIB);
        const auto spin = GetGeoInfoAttrib<const float*>(geoInfo, DD::Image::Group_Points, "spin", DD::Image::FLOAT_ATTRIB);
        const auto size = GetGeoInfoAttrib<const float*>(geoInfo, DD::Image::Group_Points, "size", DD::Image::FLOAT_ATTRIB);
        const auto cf = GetGeoInfoAttrib<const DD::Image::Vector4*>(geoInfo, DD::Image::Group_Points, "Cf", DD::Image::VECTOR4_ATTRIB);

        _instanceXforms.resize(count);
        if (cf != nullptr ) {
            _colors.resize(count);
        }

        float aspectRatio = 1.0f;
        if (geoInfo.material) {
            const DD::Image::Format& f = geoInfo.material->format();
            aspectRatio = static_cast<float>(f.pixel_aspect()) * static_cast<float>(f.width()) / static_cast<float>(f.height());
        }

        for (size_t i = 0; i < count; i++) {
            // Get the rotation matrix to face the sprite towards the camera
            const DD::Image::Matrix4& modelView = GetSharedState()->modelView;
            const DD::Image::Matrix4& viewModel = GetSharedState()->viewModel;

            // Make axes for XY plane of the particle sprite
            DD::Image::Vector3 xAxis(1.0f, 0.0f, 0.0f);
            DD::Image::Vector3 yAxis(0.0f, 1.0f, 0.0f);

            if (velocity != nullptr) {
                // Orient the sprite according to the velocity
                // Transform the velocity from world coordinate to camera coordinate
                xAxis = modelView.vtransform(velocity[i]);
                xAxis.z = 0.0f;
                xAxis.normalize();
                yAxis = DD::Image::Vector3( -xAxis.y, xAxis.x, 0.0f );
            }
            else if (spin != nullptr) {
                // Rotate the particle directional vector according to particle spin
                DD::Image::Matrix4 rotMatrix = DD::Image::Matrix4::identity();
                rotMatrix.rotationZ(spin[i]);
                xAxis = rotMatrix.vtransform(xAxis);
                yAxis = rotMatrix.vtransform(yAxis);
            }

            // Transform the particle plane to world coordinates
            xAxis = viewModel.vtransform(xAxis);
            yAxis = viewModel.vtransform(yAxis);

            xAxis.normalize();
            yAxis.normalize();

            // Make a Z axis
            DD::Image::Vector3 zAxis = xAxis.cross(yAxis);

            // Apply the particle scale
            const float scale = size[i];
            xAxis *= scale*aspectRatio;
            yAxis *= scale;
            zAxis *= scale;

            // Apply the particle translation
            const DD::Image::Vector3& position = geoInfo.matrix.transform((*pointList)[i]);

            // Make a complete matrix from the axes and translation
            pxr::GfMatrix4d billboardMatrix(
              xAxis.x, xAxis.y, xAxis.z, 0.0,
              yAxis.x, yAxis.y, yAxis.z, 0.0,
              zAxis.x, zAxis.y, zAxis.z, 0.0,
              position.x, position.y, position.z, 1.0
            );

            _instanceXforms[i] = billboardMatrix;

            if (cf != nullptr ) {
                const DD::Image::Vector4& color = cf[i];
                _colors[i] = GfVec3f(color.x, color.y, color.z);
            }
        }
    }
}

VtValue
HdNukeInstancerAdapter::Get(const TfToken& key) const
{
    if (key == HdInstancerTokens->instanceTransform) {
        return VtValue(_instanceXforms);
    }
    if (key == HdTokens->displayColor) {
        return VtValue(_colors);
    }
    if (key == HdNukeTokens->instanceCount) {
        VtIntArray result(InstanceCount());
        std::iota(result.begin(), result.end(), 0);
        return VtValue{result};
    }
    return VtValue();
}

bool HdNukeInstancerAdapter::SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();

    if (nukeData.IsHolding<DD::Image::GeoInfo*>()) {
        auto geoInfo = nukeData.UncheckedGet<DD::Image::GeoInfo*>();
        UpdateParticles(*geoInfo);
    }
    else if (nukeData.IsHolding<GeoInfoVector>()) {
        auto geoInfoVector = nukeData.UncheckedGet<GeoInfoVector>();
        Update(geoInfoVector);
    }

    renderIndex.InsertInstancer(sceneDelegate, GetPath());
    return true;
}

bool HdNukeInstancerAdapter::Update(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    auto& changeTracker = renderIndex.GetChangeTracker();

    if (nukeData.IsHolding<DD::Image::GeoInfo*>()) {
        auto geoInfo = nukeData.UncheckedGet<DD::Image::GeoInfo*>();
        // Work round an hdStorm bug causing a crash if the number of instances changes.
        // Destroy the existing instancer and create a new one.
        const DD::Image::PointList* pointList = geoInfo->point_list();
        if (pointList != nullptr) {
            const auto count = pointList->size();
            if (InstanceCount() != count) {
                SdfPath parentPath = GetPath().GetParentPath();
                renderIndex.RemoveRprim(parentPath);
                renderIndex.RemoveInstancer(GetPath());
                renderIndex.InsertInstancer(sceneDelegate, GetPath());
                renderIndex.InsertRprim(HdPrimTypeTokens->mesh, sceneDelegate, parentPath, GetPath());
            }
        }

        UpdateParticles(*geoInfo);
    }
    else if (nukeData.IsHolding<GeoInfoVector>()) {
        auto geoInfoVector = nukeData.UncheckedGet<GeoInfoVector>();
        // Work round an hdStorm bug causing a crash if the number of instances changes.
        // Destroy the existing instancer and create a new one.
        const auto count = geoInfoVector.size();
        if (InstanceCount() != count) {
            SdfPath parentPath = GetPath().GetParentPath();
            renderIndex.RemoveRprim(parentPath);
            renderIndex.RemoveInstancer(GetPath());
            renderIndex.InsertInstancer(sceneDelegate, GetPath());
            renderIndex.InsertRprim(HdPrimTypeTokens->mesh, sceneDelegate, parentPath, GetPath());
        }

        Update(geoInfoVector);
    }

    changeTracker.MarkInstancerDirty(GetPath());
    return true;
}

void HdNukeInstancerAdapter::TearDown(HdNukeAdapterManager* manager)
{
    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    renderIndex.RemoveInstancer(GetPath());
}

const TfToken& HdNukeInstancerAdapter::GetPrimType() const
{
    return HdInstancerTokens->instancer;
}

class InstancerAdapterCreator : public HdNukeAdapterFactory::AdapterCreator { public:
    HdNukeAdapterPtr Create(AdapterSharedState *sharedState) override
    {
        return std::make_shared<HdNukeInstancerAdapter>(sharedState);
    }
};

static const AdapterRegister<InstancerAdapterCreator> sRegisterEnvironmentCreator(HdNukeAdapterManagerPrimTypes->Instancer);
PXR_NAMESPACE_CLOSE_SCOPE
