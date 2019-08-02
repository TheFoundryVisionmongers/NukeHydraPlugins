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
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/imaging/hd/tokens.h>

#include "utils.h"


using namespace DD::Image;

PXR_NAMESPACE_OPEN_SCOPE


VtValue
KnobToVtValue(const Knob* knob)
{
    if (not knob) {
        return VtValue();
    }

    switch(knob->ClassID()) {
        case FLOAT_KNOB:
            return VtValue(static_cast<float>(knob->get_value()));
        case DOUBLE_KNOB:
            return VtValue(knob->get_value());
        case BOOL_KNOB:
            return VtValue(static_cast<bool>(knob->get_value()));
        case INT_KNOB:
            return VtValue(static_cast<int>(knob->get_value()));
        case ENUMERATION_KNOB:
        {
            const Enumeration_KnobI* enumKnob =
                const_cast<Knob*>(knob)->enumerationKnob();
            return VtValue(TfToken(enumKnob->getSelectedItemString()));
        }
        case COLOR_KNOB:
        case XYZ_KNOB:
            return VtValue(GfVec3f(knob->get_value(0), knob->get_value(1),
                                   knob->get_value(2)));
        case ACOLOR_KNOB:
            return VtValue(GfVec4f(knob->get_value(0), knob->get_value(1),
                                   knob->get_value(2), knob->get_value(3)));
        case STRING_KNOB:
            {
                const char* rawString = knob->get_text();
                return VtValue(std::string(rawString ? rawString : ""));
            }
        case FILE_KNOB:
            {
                const char* rawString = knob->get_text();
                const std::string path(rawString ? rawString : "");
                return VtValue(SdfAssetPath(path, path));
            }
        default:
            TF_WARN("KnobToVtValue : No VtValue conversion implemented for "
                    "knob type ID: %d", knob->ClassID());
    }
    return VtValue();
}

TfToken GetRprimType(const GeoInfo& geoInfo)
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

SdfPath GetRprimSubPath(const GeoInfo& geoInfo, const TfToken& primType)
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

PXR_NAMESPACE_CLOSE_SCOPE
