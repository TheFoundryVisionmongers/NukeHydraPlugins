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

#ifndef HDNUKE_TYPES_H
#define HDNUKE_TYPES_H

#include <unordered_map>
#include <unordered_set>

#include <pxr/usd/sdf/path.h>

#include <DDImage/GeoInfo.h>


namespace std
{
    template<>
    struct hash<DD::Image::Hash>
    {
        size_t operator()(const DD::Image::Hash& h) const
        {
            return h.value();
        }
    };
}  // namespace std


PXR_NAMESPACE_OPEN_SCOPE


using GeoOpHashArray = std::array<DD::Image::Hash, DD::Image::Group_Last>;
using GeoInfoVector = std::vector<DD::Image::GeoInfo*>;

template <typename T>
using SdfPathMap = std::unordered_map<SdfPath, T, SdfPath::Hash>;

using SdfPathUnorderedSet = std::unordered_set<SdfPath, SdfPath::Hash>;

template <typename T>
using TfTokenMap = std::unordered_map<TfToken, T, TfToken::HashFunctor>;

template <typename T>
using GeoOpPtrMap = std::unordered_map<DD::Image::GeoOp*, T>;

PXR_NAMESPACE_CLOSE_SCOPE


#endif  // HDNUKE_TYPES_H
