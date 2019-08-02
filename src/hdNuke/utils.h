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
#ifndef HDNUKE_UTILS_H
#define HDNUKE_UTILS_H

#include <pxr/base/gf/matrix3f.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/value.h>

#include <pxr/usd/sdf/path.h>

#include <DDImage/Matrix3.h>
#include <DDImage/Matrix4.h>
#include <DDImage/Op.h>
#include <DDImage/Enumeration_KnobI.h>
#include <DDImage/Knobs.h>


#include <cctype>

namespace DD
{
    namespace Image
    {
        class GeoInfo;
    }
}

PXR_NAMESPACE_OPEN_SCOPE


inline GfMatrix3f DDToGfMatrix3f(const DD::Image::Matrix3& nukeMatrix);
inline GfMatrix4f DDToGfMatrix4f(const DD::Image::Matrix4& nukeMatrix);
inline GfMatrix4d DDToGfMatrix4d(const DD::Image::Matrix4& nukeMatrix);

inline SdfPath GetPathFromOp(const DD::Image::Op* op);

template <typename T>
inline VtValue DDAttrToVtArrayValue(const DD::Image::Attribute& geoAttr);

template <typename T>
inline void ConvertHdBufferData(void* src, float* dest, size_t numPixels,
                                size_t numComponents, bool packed);

VtValue KnobToVtValue(const DD::Image::Knob* knob);

template<typename T>
inline T _KnobValue(DD::Image::Knob *knob);

template<>
inline std::string _KnobValue(DD::Image::Knob *knob);

template<typename T>
inline T GetKnobValue(const DD::Image::Op *op, const std::string &knobName, const T &defaultValue=T());
//
// Definitions
//

inline GfMatrix3f DDToGfMatrix3f(const DD::Image::Matrix3& nukeMatrix)
{
    GfMatrix3f gfMatrix;
    std::copy(nukeMatrix.array(), nukeMatrix.array() + 9, gfMatrix.data());
    return gfMatrix;
}

inline GfMatrix4f DDToGfMatrix4f(const DD::Image::Matrix4& nukeMatrix)
{
    GfMatrix4f gfMatrix;
    std::copy(nukeMatrix.array(), nukeMatrix.array() + 16, gfMatrix.data());
    return gfMatrix;
}

inline GfMatrix4d DDToGfMatrix4d(const DD::Image::Matrix4& nukeMatrix)
{
    GfMatrix4d gfMatrix;
    std::copy(nukeMatrix.array(), nukeMatrix.array() + 16, gfMatrix.data());
    return gfMatrix;
}

inline SdfPath GetPathFromOp(const DD::Image::Op* op)
{
    std::string tail(op->node_name());
    for (char &c : tail) {
        if (c == '.') {
            c = '/';
        }
        else if (std::isspace(c)) {
            c = '_';
        }
    }
    return SdfPath(tail);
}

template <typename T>
inline VtValue
DDAttrToVtArrayValue(const DD::Image::Attribute& geoAttr)
{
    VtArray<T> array;
    const T* dataPtr = static_cast<const T*>(geoAttr.array());
    array.assign(dataPtr, dataPtr + geoAttr.size());
    return VtValue::Take(array);
}

template <typename T>
inline void
ConvertHdBufferData(void* src, float* dest, size_t numPixels,
                    size_t numComponents, bool packed)
{
    const T* data;
    if (packed or numComponents == 1) {
        data = static_cast<T*>(src);
        for (size_t i = 0; i < numPixels * numComponents; i++)
        {
            dest[i] = static_cast<float>(data[i]);
        }
    }
    else {
        for (size_t chan = 0; chan < numComponents; chan++)
        {
            data = static_cast<T*>(src) + chan;
            for (size_t i = 0; i < numPixels; i++)
            {
                *dest++ = static_cast<float>(data[i * numComponents]);
            }
        }
    }
}


template<typename T>
inline T _KnobValue(DD::Image::Knob *knob)
{
    return static_cast<T>(knob->get_value());
}

template<>
inline std::string _KnobValue(DD::Image::Knob *knob)
{
    if (auto enumKnob = knob->enumerationKnob()) {
        return enumKnob->getSelectedItemString();
    }
    return "";
}

template<typename T>
inline T GetKnobValue(const DD::Image::Op *op, const std::string &knobName, const T &defaultValue)
{
    if (auto knob = op->knob(knobName.c_str())) {
        return _KnobValue<T>(knob);
    }
    TF_WARN("Knob %s does not exist in Node %s.", knobName.c_str(), op->node_name().c_str());
    return defaultValue;
}

/// Returns the primType for the given \p geoInfo.
TfToken GetRprimType(const DD::Image::GeoInfo& geoInfo);

/// Computes the RPrim subpath for the \p geoInfo given its \p primType.
SdfPath GetRprimSubPath(const DD::Image::GeoInfo& geoInfo, const TfToken& primType);

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_UTILS_H
