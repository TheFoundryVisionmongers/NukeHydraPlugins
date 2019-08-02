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
#include <pxr/usd/usdGeom/tokens.h>

#include <pxr/imaging/pxOsd/tokens.h>
#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/tokens.h>

#include "geoAdapter.h"
#include "tokens.h"
#include "utils.h"
#include "adapterFactory.h"
#include "adapterManager.h"
#include "sceneDelegate.h"

#include <DDImage/GeoOp.h>
#include <DDImage/Iop.h>
#include <DDImage/RenderParticles.h>

using namespace DD::Image;

PXR_NAMESPACE_OPEN_SCOPE


// This token is not made public anywhere in USD
TF_DEFINE_PRIVATE_TOKENS(
    HdNukeGeoAdapterTokens,

    (overrideWireframeColor)
);

HdNukeGeoAdapter::HdNukeGeoAdapter(AdapterSharedState* statePtr)
    : HdNukeAdapter(statePtr)
{
}

void
HdNukeGeoAdapter::Update(const GeoInfo& geo, HdDirtyBits dirtyBits,
                         bool isInstanced)
{
    if (dirtyBits == HdChangeTracker::Clean) {
        return;
    }
    // XXX: For objects instanced by particle systems, Nuke includes the source
    // object's transform in the final transform of the instance. However,
    // because the render delegate will still query the scene delegate for the
    // attributes of the source Rprim (including transform), and then try to
    // concatenate them with the instance transform *itself*, we need to reset
    // the source transform here so it doesn't get applied twice.
    if (isInstanced) {
        _transform.SetIdentity();
    }
    else if (dirtyBits & HdChangeTracker::DirtyTransform) {
        _transform = DDToGfMatrix4d(geo.matrix);
    }

    if (dirtyBits & HdChangeTracker::DirtyVisibility) {
        if (GetSharedState()->interactive) {
          _visible = geo.display3d != DISPLAY_OFF;
        }
        else {
          _visible = geo.render_mode != RENDER_OFF;
        }
    }

    if (dirtyBits & HdChangeTracker::DirtyPrimvar) {
        _wireframeColor = GetWireframeColor(geo);
    }

    if (dirtyBits & HdChangeTracker::DirtyRepr) {
        _reprSelector = GetReprSelectorForGeo(geo);
    }

    if (dirtyBits & HdChangeTracker::DirtyTopology) {
        _RebuildMeshTopology(geo);
    }

    if (dirtyBits & HdChangeTracker::DirtyPoints) {
        _RebuildPointList(geo);
    }

    if (dirtyBits & (HdChangeTracker::DirtyPrimvar
                     | HdChangeTracker::DirtyNormals
                     | HdChangeTracker::DirtyTopology
                     | HdChangeTracker::DirtyWidths)) {
        _RebuildPrimvars(geo);
    }

    if (dirtyBits & HdChangeTracker::DirtyExtent) {
        const Vector3& min = geo.bbox().min();
        const Vector3& max = geo.bbox().max();
        _extent.SetMin(GfVec3d(min.x, min.y, min.z));
        _extent.SetMax(GfVec3d(max.x, max.y, max.z));
    }
}

HdPrimvarDescriptorVector
HdNukeGeoAdapter::GetPrimvarDescriptors(HdInterpolation interpolation) const
{
    switch (interpolation) {
        case HdInterpolationConstant:
            return _constantPrimvarDescriptors;
        case HdInterpolationUniform:
            return _uniformPrimvarDescriptors;
        case HdInterpolationVertex:
            return _vertexPrimvarDescriptors;
        case HdInterpolationFaceVarying:
            return _faceVaryingPrimvarDescriptors;
        default:
            return HdPrimvarDescriptorVector();
    }
}

void
HdNukeGeoAdapter::_RebuildMeshTopology(const GeoInfo& geo)
{
    const uint32_t numPrimitives = geo.primitives();

    size_t totalFaces = 0;
    size_t totalVerts = 0;
    const Primitive** primArray = geo.primitive_array();
    for (size_t primIndex = 0; primIndex < numPrimitives; primIndex++)
    {
        totalFaces += primArray[primIndex]->faces();
        totalVerts += primArray[primIndex]->vertices();
    }

    VtIntArray faceVertexCounts;
    faceVertexCounts.reserve(totalFaces);
    VtIntArray faceVertexIndices;
    faceVertexIndices.reserve(totalVerts);

    std::vector<uint32_t> faceVertices(16);

    for (uint32_t primIndex = 0; primIndex < numPrimitives; primIndex++)
    {
        const Primitive* prim = primArray[primIndex];
        const PrimitiveType primType = prim->getPrimitiveType();
        if (primType == ePoint or primType == eParticles
                or primType == eParticlesSprite) {
            // TODO: Warn/debug
            continue;
        }

        for (uint32_t faceIndex = 0; faceIndex < prim->faces(); faceIndex++)
        {
            const uint32_t numFaceVertices = prim->face_vertices(faceIndex);
            if (numFaceVertices > faceVertices.size()) {
              faceVertices.resize(numFaceVertices);
            }
            faceVertexCounts.push_back(numFaceVertices);

            prim->get_face_vertices(faceIndex, faceVertices.data());
            for (uint32_t faceVertexIndex = 0;
                 faceVertexIndex < numFaceVertices; faceVertexIndex++)
            {
                faceVertexIndices.push_back(
                    prim->vertex(faceVertices[faceVertexIndex]));
            }
        }
    }

    _topology = HdMeshTopology(PxOsdOpenSubdivTokens->smooth,
                               UsdGeomTokens->rightHanded, faceVertexCounts,
                               faceVertexIndices);
}

void HdNukeGeoAdapter::_RebuildPointList(const GeoInfo& geo)
{
    const PointList* pointList = geo.point_list();
    if (ARCH_UNLIKELY(!pointList)) {
        _points.clear();
        return;
    }

    const auto* rawPoints = reinterpret_cast<const GfVec3f*>(pointList->data());
    _points.assign(rawPoints, rawPoints + pointList->size());
}

VtValue
HdNukeGeoAdapter::Get(const TfToken& key) const
{
// TODO: Attach node name as primvar
    if (key == HdTokens->transform) {
        return VtValue{GetTransform()};
    }
    if (key == HdTokens->points) {
        return VtValue(_points);
    }
    else if (key == HdTokens->displayColor) {
      if (_colors.size() != 0) {
        return VtValue(_colors);
      }
      return VtValue(_displayColor);
    }
    else if (key == HdNukeGeoAdapterTokens->overrideWireframeColor) {
        return VtValue(_wireframeColor);
    }
    else if (key == HdNukeTokens->st) {
        return VtValue(_uvs);
    }
    else if (key == HdNukeTokens->materialId) {
        return VtValue{_materialId};
    }
    else if (key == HdNukeTokens->extent) {
        return VtValue{GetExtent()};
    }
    else if (key == HdNukeTokens->meshTopology) {
        return VtValue{GetMeshTopology()};
    }
    else if (key == HdNukeTokens->visible) {
        return VtValue{GetVisible()};
    }
    else if (key == HdNukeTokens->doubleSided) {
        return VtValue{true};
    }
    else if (key == HdNukeTokens->reprSelector) {
        return VtValue{GetReprSelector()};
    }

    auto it = _primvarData.find(key);
    if (it != _primvarData.end()) {
        return it->second;
    }

    // Deal with fallbacks for keys which may have have been handled already
    if (key == HdTokens->widths) {
      return VtValue(_pointSize);
    }

    TF_WARN("HdNukeGeoAdapter::Get : Unrecognized key: %s", key.GetText());
    return VtValue();
}

const TfToken& HdNukeGeoAdapter::GetPrimType() const
{
    auto primType = _geoInfo->primitive(0)->getPrimitiveType();
    if (primType == ePoint || primType == DD::Image::eParticles) {
        return HdPrimTypeTokens->points;
    }
    return HdPrimTypeTokens->mesh;
}

void
HdNukeGeoAdapter::_RebuildPrimvars(const GeoInfo& geo)
{
    // Group_Object      -> HdInterpolationConstant
    // Group_Primitives  -> HdInterpolationUniform
    // Group_Points (?)  -> HdInterpolationVertex
    // Group_Vertices    -> HdInterpolationFaceVarying

    static HdPrimvarDescriptor displayColorDescriptor(
            HdTokens->displayColor, HdInterpolationConstant,
            HdPrimvarRoleTokens->color);
    static HdPrimvarDescriptor overrideWireframeColorDescriptor(
            HdNukeGeoAdapterTokens->overrideWireframeColor, HdInterpolationConstant,
            HdNukeGeoAdapterTokens->overrideWireframeColor);
    static HdPrimvarDescriptor widthsColorDescriptor(
            HdTokens->widths, HdInterpolationConstant,
            TfToken());

    // XXX: Hydra doesn't officially state that a `points` descriptor is
    // required (even for Rprim types with implied points), and there's a good
    // case to be made that it *shouldn't* be, but Storm currently seems to rely
    // on it when generating GLSL code, so we take the conservative approach.
    static HdPrimvarDescriptor pointsDescriptor(
            HdTokens->points, HdInterpolationVertex,
            HdPrimvarRoleTokens->point);

    // TODO: Try to reuse existing descriptors?
    _constantPrimvarDescriptors.clear();
    _constantPrimvarDescriptors.push_back(overrideWireframeColorDescriptor);
    _uniformPrimvarDescriptors.clear();
    _vertexPrimvarDescriptors.clear();
    _vertexPrimvarDescriptors.push_back(pointsDescriptor);
    _faceVaryingPrimvarDescriptors.clear();

    _primvarData.clear();
    _primvarData.reserve(geo.get_attribcontext_count());

    _colors.clear();

    // TODO: Look up color from GeoInfo
    _displayColor = GetSharedState()->defaultDisplayColor;

    bool haveVertexWidths = false;

    for (const auto& attribCtx : geo.get_cache_pointer()->attributes)
    {
        if (attribCtx.empty()) {
            continue;
        }

        TfToken primvarName(attribCtx.name);
        TfToken role;

        if (primvarName == HdNukeTokens->Cf) {
            // Ignore displayColor for instances because otherwise
            // our displayColor overrides the instancer's one.
            if (_isInstanced) {
              continue;
            }
            primvarName = HdTokens->displayColor;
            role = HdPrimvarRoleTokens->color;
        }
        else if (primvarName == HdNukeTokens->uv) {
            primvarName = HdNukeTokens->st;
            role = HdPrimvarRoleTokens->textureCoordinate;
        }
        else if (primvarName == HdNukeTokens->N) {
            primvarName = HdTokens->normals;
            role = HdPrimvarRoleTokens->normal;
        }
        else if (primvarName == HdNukeTokens->size) {
            primvarName = HdTokens->widths;
            haveVertexWidths = true;
        }
        else if (primvarName == HdNukeTokens->PW) {
            role = HdPrimvarRoleTokens->point;
        }
        else if (primvarName == HdNukeTokens->vel) {
            primvarName = HdTokens->velocities;
            role = HdPrimvarRoleTokens->vector;
        }
        else {
            role = HdPrimvarRoleTokens->none;
        }

        switch (attribCtx.group) {
            case Group_Object:
                _constantPrimvarDescriptors.emplace_back(
                    primvarName, HdInterpolationConstant, role);
                break;
            case Group_Primitives:
                _uniformPrimvarDescriptors.emplace_back(
                    primvarName, HdInterpolationUniform, role);
                break;
            case Group_Points:
                _vertexPrimvarDescriptors.emplace_back(
                    primvarName, HdInterpolationVertex, role);
                break;
            case Group_Vertices:
                _faceVaryingPrimvarDescriptors.emplace_back(
                    primvarName, HdInterpolationFaceVarying, role);
                break;
            default:
                continue;
        }

        // Store attribute data
        const Attribute& attribute = *attribCtx.attribute;
        const AttribType attrType = attribute.type();

        // XXX: Special case for UVs. Nuke stores UVs as Vector4
        // (homogeneous 3D coordinates), but USD/Hydra conventions stipulate Vec2f.
        // Thus, we do type conversion in the case of a float vecter attr with
        // width > 2, just to be nice.
        if (primvarName == HdNukeTokens->st && attrType == VECTOR4_ATTRIB)
        {
            const auto size = attribute.size();
            _uvs.resize(size);
            float* outPtr = reinterpret_cast<float*>(_uvs.data());
            for (size_t i = 0; i < size; i++) {
                Vector4 uv = attribute.vector4(i).divide_w();
                *outPtr++ = uv.x;
                *outPtr++ = uv.y;
            }
            continue;
        }

        // Cf is Vector4, but displayColor needs Vector3
        if (primvarName == HdTokens->displayColor && attrType == VECTOR4_ATTRIB)
        {
            const auto size = attribute.size();
            _colors.resize(size);
            Vector4* dataPtr = static_cast<Vector4*>(attribute.array());
            GfVec3f* outPtr = _colors.data();
            for (size_t i = 0; i < size; i++, dataPtr++) {
                *outPtr++ = GfVec3f(dataPtr->x, dataPtr->y, dataPtr->z);
            }
            _StorePrimvarArray(primvarName, VtValue(_colors));
            continue;
        }

        // General-purpose attribute conversions
        if (attribute.size() == 1) {
            void* rawData = attribute.array();
            float* floatData = static_cast<float*>(rawData);

            switch (attrType) {
                case FLOAT_ATTRIB:
                    _StorePrimvarScalar(primvarName, floatData[0]);
                    break;
                case INT_ATTRIB:
                    _StorePrimvarScalar(primvarName,
                                        static_cast<int32_t*>(rawData)[0]);
                    break;
                case STRING_ATTRIB:
                    _StorePrimvarScalar(primvarName,
                                        std::string(static_cast<char**>(rawData)[0]));
                    break;
                case STD_STRING_ATTRIB:
                    _StorePrimvarScalar(primvarName,
                                        static_cast<std::string*>(rawData)[0]);
                    break;
                case VECTOR2_ATTRIB:
                    _StorePrimvarScalar(primvarName, GfVec2f(floatData));
                    break;
                case VECTOR3_ATTRIB:
                case NORMAL_ATTRIB:
                    _StorePrimvarScalar(primvarName, GfVec3f(floatData));
                    break;
                case VECTOR4_ATTRIB:
                    _StorePrimvarScalar(primvarName, GfVec4f(floatData));
                    break;
                case MATRIX3_ATTRIB:
                    {
                        GfMatrix3f gfMatrix;
                        std::copy(floatData, floatData + 9, gfMatrix.data());
                        _StorePrimvarScalar(primvarName, gfMatrix);
                    }
                    break;
                case MATRIX4_ATTRIB:
                    {
                        GfMatrix4f gfMatrix;
                        std::copy(floatData, floatData + 16, gfMatrix.data());
                        _StorePrimvarScalar(primvarName, gfMatrix);
                    }
                    break;
                default:
                    TF_WARN("HdNukeGeoAdapter::_RebuildPrimvars : Unhandled "
                            "attribute type: %d", attrType);
                    continue;
            }
        }
        else {
            switch (attrType) {
                case FLOAT_ATTRIB:
                    _StorePrimvarArray(primvarName,
                                       DDAttrToVtArrayValue<float>(attribute));
                    break;
                case INT_ATTRIB:
                    _StorePrimvarArray(primvarName,
                                       DDAttrToVtArrayValue<int32_t>(attribute));
                    break;
                case VECTOR2_ATTRIB:
                    _StorePrimvarArray(primvarName,
                                       DDAttrToVtArrayValue<GfVec2f>(attribute));
                    break;
                case VECTOR3_ATTRIB:
                case NORMAL_ATTRIB:
                    _StorePrimvarArray(primvarName,
                                       DDAttrToVtArrayValue<GfVec3f>(attribute));
                    break;
                case VECTOR4_ATTRIB:
                    _StorePrimvarArray(primvarName,
                                       DDAttrToVtArrayValue<GfVec4f>(attribute));
                    break;
                case MATRIX3_ATTRIB:
                    _StorePrimvarArray(primvarName,
                                       DDAttrToVtArrayValue<GfMatrix3f>(attribute));
                    break;
                case MATRIX4_ATTRIB:
                    _StorePrimvarArray(primvarName,
                                       DDAttrToVtArrayValue<GfMatrix4f>(attribute));
                    break;
                case STD_STRING_ATTRIB:
                    _StorePrimvarArray(primvarName,
                                       DDAttrToVtArrayValue<std::string>(attribute));
                    break;
                default:
                    TF_WARN("HdNukeGeoAdapter::_RebuildPrimvars : Unhandled "
                            "attribute type: %d", attrType);
                    continue;

                // XXX: Ignoring char* array attrs for now... not sure whether they
                // need special-case handling.
                // case STRING_ATTRIB:
            }
        }
    }

    // Deal with Particles primitives default point size. We must only do this if we didn't have per-vertex sizes.
    // This is problematic because Nuke's point size is in screen space but Hydra point size are in object space.
    // This means that we can only ever approximate the size as it'll change with distance from the camera.
    // Calculate an approximate world space width using the camera and viewport settings.
    if (!haveVertexWidths) {
      const Primitive* firstPrim = geo.primitive(0);
      if (firstPrim != nullptr && firstPrim->getPrimitiveType() == eParticles && !_points.empty()) {
        _constantPrimvarDescriptors.push_back(widthsColorDescriptor);
        // Use the first point to work out an approximate size. Maybe the centroid would be better.
        const GfVec3f& p = _points[0];
        const DD::Image::Vector3 point(p[0], p[1], p[2]);
        const DD::Image::Vector3 eye = GetSharedState()->modelView.transform(point);
        const DD::Image::Vector3 v = GetSharedState()->viewModel.transform(DD::Image::Vector3(1, 1, 1));
        const float width = v.length();
        DD::Image::Vector4 diameter = GetSharedState()->projMatrix.transform(DD::Image::Vector4(width, 0, eye.z, 1));
        const float screenPointSize = (diameter.x/diameter.w)*GetSharedState()->viewportHeight*0.5f;
        _pointSize = GetRenderParticlesPointSize(firstPrim)/screenPointSize;
      }
    }

    if (_colors.size() == 0 && !_isInstanced) {
      // We can't declare displayColor with two different interpolations, so only add it
      // as constant if we didn't already add it as vertex interpolation.
      _constantPrimvarDescriptors.push_back(displayColorDescriptor);
    }
}

HdReprSelector
HdNukeGeoAdapter::GetReprSelector() const
{
    return _reprSelector;
}

HdReprSelector
HdNukeGeoAdapter::GetReprSelectorForGeo(const DD::Image::GeoInfo& geo) const
{
    if (GetSharedState()->interactive) {
        switch (geo.display3d) {
        case DISPLAY_WIREFRAME:
            return HdReprSelector(HdReprTokens->refinedWire, HdReprTokens->wire);
        case DISPLAY_SOLID_LINES:
        case DISPLAY_TEXTURED_LINES:
            return HdReprSelector(HdReprTokens->refinedWireOnSurf, HdReprTokens->wireOnSurf);
        case DISPLAY_OFF:
        case DISPLAY_SOLID:
        case DISPLAY_TEXTURED:
        default:
            return HdReprSelector(HdReprTokens->refined, HdReprTokens->refined);
        }
    }
    else {
        switch (geo.render_mode) {
        case RENDER_WIREFRAME:
            return HdReprSelector(HdReprTokens->refinedWire, HdReprTokens->wire);
        case RENDER_SOLID_LINES:
        case RENDER_TEXTURED_LINES:
            return HdReprSelector(HdReprTokens->refinedWireOnSurf, HdReprTokens->wireOnSurf);
        case RENDER_OFF:
        case RENDER_SOLID:
        case RENDER_TEXTURED:
        default:
            return HdReprSelector(HdReprTokens->refined, HdReprTokens->refined);
        }
    }
}

GfVec4f
HdNukeGeoAdapter::GetWireframeColor(const DD::Image::GeoInfo& geo) const
{
    // This matches Nuke's wireframe color
    unsigned int color;
    if (geo.valid_source_node_gl_color) {
        color = geo.source_node_gl_color;
    }
    else {
        color = geo.source_geo->node_gl_color();
    }

    const float scaleFactor = 1.25f;
    return GfVec4f(
        float(std::min(((color >> 24) & 255) * scaleFactor, 255.0f)) / 255.0f,
        float(std::min(((color >> 16) & 255) * scaleFactor, 255.0f)) / 255.0f,
        float(std::min(((color >> 8) & 255) * scaleFactor, 255.0f)) / 255.0f,
        1.0f
    );
}

void
HdNukeGeoAdapter::MakeParticleSprite()
{
  _points.push_back(pxr::GfVec3f(-0.5f, -0.5f, 0.0f));
  _points.push_back(pxr::GfVec3f(-0.5f,  0.5f, 0.0f));
  _points.push_back(pxr::GfVec3f( 0.5f,  0.5f, 0.0f));
  _points.push_back(pxr::GfVec3f( 0.5f, -0.5f, 0.0f));

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

void HdNukeGeoAdapter::SetIsInstanced(bool isInstanced)
{
  _isInstanced = isInstanced;
}

namespace
{
    Iop* materialOpForGeo(const GeoInfo* geoInfo)
    {
      Iop* materialOp = geoInfo->material;
      if ( materialOp != nullptr && std::strcmp(materialOp->Class(), "Black") != 0 ) {
        return materialOp;
      }
      return nullptr;
    }

    uint32_t UpdateHashArray(const GeoOp* op, GeoOpHashArray& hashes)
    {
        uint32_t updateMask = 0;  // XXX: The mask enum in GeoInfo.h is untyped...
        for (uint32_t i = 0; i < Group_Last; i++) {
            const Hash groupHash(op->hash(i));
            if (groupHash != hashes[i]) {
                updateMask |= 1 << i;
            }
            hashes[i] = groupHash;
        }
        // This is a workround to allow animated geometry to work
        // correctly. ReadGeo deosn't set Mask_Points even though
        // all the points in the model may move from frame to frame.
        if (updateMask & Mask_Primitives) {
            updateMask |= Mask_Points;
        }
        return updateMask;
    }

    HdDirtyBits DirtyBitsFromUpdateMask(uint32_t updateMask)
    {
        HdDirtyBits dirtyBits = HdChangeTracker::Clean;
        if (updateMask & Mask_Object) {
            // Mask_Object gets set for render mode changes as well
            dirtyBits |= HdChangeTracker::DirtyVisibility
                        | HdChangeTracker::DirtyRepr;
        }

        if (updateMask & (Mask_Primitives | Mask_Vertices)) {
            // Some readers only set Mask_Primitives when their filename changes, so we need to include the transform as well.
            dirtyBits |= HdChangeTracker::DirtyTopology | HdChangeTracker::DirtyTransform;
        }
        if (updateMask & Mask_Points) {
            dirtyBits |= (HdChangeTracker::DirtyPoints
                         | HdChangeTracker::DirtyExtent);
        }
        if (updateMask & Mask_Matrix) {
            dirtyBits |= HdChangeTracker::DirtyTransform;
        }
        if (updateMask & Mask_Attributes) {
            // We shouldn't need to set DirtyPoints here, but it we
            // don't, hdStorm will generate invalid shaders.
            dirtyBits |= (HdChangeTracker::DirtyPrimvar
                          | HdChangeTracker::DirtyNormals
                          | HdChangeTracker::DirtyPoints
                          | HdChangeTracker::DirtyWidths);
        }
        return dirtyBits;
    }
}

bool HdNukeGeoAdapter::SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<GeoInfo*>(),
          "HdNukeGeoAdapter expects a GeoInfo")) {
        return false;
    }

    _geoInfo = nukeData.UncheckedGet<GeoInfo*>();
    GeoOp* sourceOp = op_cast<GeoOp*>(_geoInfo->final_geo);
    _hash = sourceOp->Op::hash();

    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    auto& changeTracker = renderIndex.GetChangeTracker();

    _SetMaterial(manager);

    Update(*_geoInfo, HdChangeTracker::AllDirty, false);
    if (GetVisible()) {
        renderIndex.InsertRprim(GetPrimType(), sceneDelegate, GetPath());
    }
    else {
        renderIndex.RemoveRprim(GetPath());
    }
    changeTracker.MarkRprimDirty(GetPath());

    _castsShadow = _geoInfo->renderState.castShadow;
    if (!_castsShadow) {
        auto excludePaths = GetSharedState()->_shadowCollection.GetExcludePaths();
        excludePaths.push_back(GetPath());
        GetSharedState()->_shadowCollection.SetExcludePaths(excludePaths);
        changeTracker.AddCollection(HdNukeTokens->shadowCollection);
    }

    UpdateHashArray(sourceOp, _opStateHashes);

    return true;
}

bool HdNukeGeoAdapter::Update(HdNukeAdapterManager* manager, const VtValue& nukeData)
{
    if (!TF_VERIFY(nukeData.IsHolding<GeoInfo*>(),
          "HdNukeGeoAdapter expects a GeoInfo")) {
        return false;
    }
    _geoInfo = nukeData.UncheckedGet<GeoInfo*>();

    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    auto& changeTracker = renderIndex.GetChangeTracker();
    GeoOp* sourceOp = op_cast<GeoOp*>(_geoInfo->final_geo);

    if (_hash != sourceOp->Op::hash()) {
        auto dirtyBits = DirtyBitsFromUpdateMask(UpdateHashArray(sourceOp, _opStateHashes));
        Update(*_geoInfo, dirtyBits, false);
        changeTracker.MarkRprimDirty(GetPath(), dirtyBits);

        if (GetVisible()) {
            renderIndex.InsertRprim(GetPrimType(), sceneDelegate, GetPath());
        }
        else {
            renderIndex.RemoveRprim(GetPath());
        }
    }

    _SetMaterial(manager);
    changeTracker.MarkRprimDirty(GetPath(), HdChangeTracker::DirtyMaterialId);

    if (!_castsShadow && _geoInfo->renderState.castShadow) {
        auto excludePaths = GetSharedState()->_shadowCollection.GetExcludePaths();
        for (auto it = excludePaths.begin(); it != excludePaths.end(); ++it) {
            if (*it == GetPath()) {
                excludePaths.erase(it);
                break;
            }
        }
        GetSharedState()->_shadowCollection.SetExcludePaths(excludePaths);
        changeTracker.AddCollection(HdNukeTokens->shadowCollection);
    }
    else if (_castsShadow && !_geoInfo->renderState.castShadow) {
        auto excludePaths = GetSharedState()->_shadowCollection.GetExcludePaths();
        excludePaths.push_back(GetPath());
        GetSharedState()->_shadowCollection.SetExcludePaths(excludePaths);
        changeTracker.AddCollection(HdNukeTokens->shadowCollection);
    }
    _castsShadow = _geoInfo->renderState.castShadow;

    _hash = sourceOp->Op::hash();
    return true;
}

void HdNukeGeoAdapter::TearDown(HdNukeAdapterManager* manager)
{
    auto sceneDelegate = manager->GetSceneDelegate();
    auto& renderIndex = sceneDelegate->GetRenderIndex();
    renderIndex.RemoveRprim(GetPath());
}

void HdNukeGeoAdapter::_SetMaterial(HdNukeAdapterManager* manager)
{
    if (GetPrimType() == HdPrimTypeTokens->points) {
        auto sceneDelegate = manager->GetSceneDelegate();
        auto promise = manager->Request(TfToken{"defaultParticleMaterialId"}, sceneDelegate->DefaultParticleMaterialId());
        _materialId = promise->path;
    }
    else if (auto material = materialOpForGeo(_geoInfo)) {
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
}

class GeoAdapterCreator : public HdNukeAdapterFactory::AdapterCreator { public:
    HdNukeAdapterPtr Create(AdapterSharedState *sharedState) override
    {
        return std::make_shared<HdNukeGeoAdapter>(sharedState);
    }
};

static const AdapterRegister<GeoAdapterCreator> sRegisterEnvironmentCreator(HdNukeAdapterManagerPrimTypes->GenericGeoInfo);

PXR_NAMESPACE_CLOSE_SCOPE
