#ifndef HDNUKE_ENVIRONMENTLIGHTADAPTER_H
#define HDNUKE_ENVIRONMENTLIGHTADAPTER_H

#include <pxr/pxr.h>

#include <pxr/imaging/hd/types.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/imaging/hd/renderIndex.h>

#include <DDImage/Hash.h>

#include "adapter.h"

namespace DD
{
    namespace Image
    {
        class LightOp;
        class Iop;
    }
}

PXR_NAMESPACE_OPEN_SCOPE

class HdNukeEnvironmentLightAdapter : public HdNukeAdapter
{
public:
    HdNukeEnvironmentLightAdapter(AdapterSharedState* statePtr);
    ~HdNukeEnvironmentLightAdapter() override;

    bool SetUp(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    bool Update(HdNukeAdapterManager* manager, const VtValue& nukeData) override;
    void TearDown(HdNukeAdapterManager* manager) override;

    VtValue Get(const TfToken& key) const override;

    const TfToken& GetPrimType() const override;

    static const HdDirtyBits DefaultDirtyBits;
private:
    bool uploadTexture(HdRenderIndex& renderIndex);

    DD::Image::LightOp* _lightOp;
    DD::Image::Iop* _envMapIop;
    DD::Image::Hash _hash;
    HdTextureResourceSharedPtr _textureResource;
    SdfAssetPath _assetPath;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_ENVIRONMENTLIGHTADAPTER_H
