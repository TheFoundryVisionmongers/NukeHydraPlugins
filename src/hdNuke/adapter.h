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
#ifndef HDNUKE_ADAPTER_H
#define HDNUKE_ADAPTER_H

#include <memory>

#include <pxr/pxr.h>

#include <pxr/usd/sdf/path.h>
#include <pxr/base/vt/value.h>

#include "sharedState.h"


PXR_NAMESPACE_OPEN_SCOPE

class HdNukeAdapterManager;

class HdNukeAdapter
{
public:
    HdNukeAdapter(AdapterSharedState* statePtr)
        : _sharedState(statePtr) { }

    virtual ~HdNukeAdapter() { }

    inline const AdapterSharedState* GetSharedState() const {
        return _sharedState;
    }

    inline AdapterSharedState* GetSharedState() {
        return _sharedState;
    }

    virtual bool SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData) = 0;
    virtual bool Update(HdNukeAdapterManager* manager, const VtValue& nukeData) = 0;
    virtual void TearDown(HdNukeAdapterManager* manager) = 0;

    virtual VtValue Get(const TfToken& key) const { return VtValue{}; }

    virtual const TfToken& GetPrimType() const = 0;

    void SetUsed(bool used) { _used = used; }
    bool IsUsed() const { return _used; }

    void SetPath(const SdfPath& path) { _path = path; }
    const SdfPath& GetPath() const { return _path; }

private:
    AdapterSharedState* _sharedState;
    SdfPath _path;
    bool _used{false};
};

using HdNukeAdapterPtr = std::shared_ptr<HdNukeAdapter>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_ADAPTER_H
