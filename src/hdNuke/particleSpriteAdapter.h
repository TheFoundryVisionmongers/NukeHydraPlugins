#ifndef HDNUKE_PARTICLESPRITEADAPTER_H
#define HDNUKE_PARTICLESPRITEADAPTER_H

#include <pxr/pxr.h>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include "adapter.h"
#include "geoAdapter.h"
#include "types.h"

#include <DDImage/Hash.h>


PXR_NAMESPACE_OPEN_SCOPE

class HdNukeParticleSpriteAdapter : public HdNukeGeoAdapter
{
public:
    HdNukeParticleSpriteAdapter(AdapterSharedState* statePtr)
        : HdNukeGeoAdapter(statePtr) { }
    ~HdNukeParticleSpriteAdapter() override { }

    bool SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    bool Update(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    void TearDown(HdNukeAdapterManager* manager) override;
    VtValue Get(const TfToken &key) const override;

    //! Makes an adapter for an imaginary unit card at the origin. This is used
    //! as a prototype for instancing particle sprites.
    void MakeParticleSprite();

    void UpdateParticles(const DD::Image::GeoInfo& geoInfo);

    const TfToken& GetPrimType() const override;

private:
    void _SetMaterial(HdNukeAdapterManager* manager) override;
    DD::Image::Hash _hash;
    SdfPath _instancerPath;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_PARTICLESPRITEADAPTER_H

