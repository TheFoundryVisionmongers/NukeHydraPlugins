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
#ifndef HDNUKE_INSTANCERADAPTER_H
#define HDNUKE_INSTANCERADAPTER_H

#include <pxr/pxr.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>

#include "adapter.h"
#include "types.h"


PXR_NAMESPACE_OPEN_SCOPE


class HdNukeInstancerAdapter : public HdNukeAdapter
{
public:
    HdNukeInstancerAdapter(AdapterSharedState* statePtr)
        : HdNukeAdapter(statePtr) { }
    ~HdNukeInstancerAdapter() override { }

    void Update(const GeoInfoVector& geoInfoPtrs);
    void UpdateParticles(const DD::Image::GeoInfo& geoInfo);

    VtValue Get(const TfToken& key) const override;

    const TfToken& GetPrimType() const override;

    inline size_t InstanceCount() const { return _instanceXforms.size(); }

    bool SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    bool Update(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    void TearDown(HdNukeAdapterManager* manager) override;
private:
    VtMatrix4dArray _instanceXforms;
    VtVec3fArray _colors;
};

using HdNukeInstancerAdapterPtr = std::shared_ptr<HdNukeInstancerAdapter>;


PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_INSTANCERADAPTER_H
