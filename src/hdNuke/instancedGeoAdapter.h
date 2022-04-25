#ifndef HDNUKE_INSTANCEDGEOADAPTER_H
#define HDNUKE_INSTANCEDGEOADAPTER_H

#include <pxr/pxr.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>

#include "adapter.h"
#include "geoAdapter.h"
#include "types.h"

#include <DDImage/Hash.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdNukeInstancedGeoAdapter : public HdNukeGeoAdapter
{
public:
    HdNukeInstancedGeoAdapter(AdapterSharedState* statePtr)
        : HdNukeGeoAdapter(statePtr) { }
    ~HdNukeInstancedGeoAdapter() override { }

    bool SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    bool Update(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    void TearDown(HdNukeAdapterManager* manager) override;
    VtValue Get(const TfToken& key) const override;

private:
    DD::Image::Hash _hash;
    SdfPath _instancerPath;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_INSTANCEDGEOADAPTER_H
