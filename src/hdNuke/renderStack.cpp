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
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usdImaging/usdImaging/version.h>
#if USD_IMAGING_API_VERSION >= 13
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#endif
#include "pxr/base/plug/registry.h"

#include "renderStack.h"

#include <DDImage/plugins.h>

#include <mutex>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE


HydraRenderStack::HydraRenderStack(HdRendererPlugin* pluginPtr)
        : rendererPlugin(pluginPtr),
          primCollection(HdTokens->geometry,
                         HdReprSelector(HdReprTokens->refined))
{
    HdRenderDelegate* renderDelegate = rendererPlugin->CreateRenderDelegate();
#if USD_IMAGING_API_VERSION >= 13
#if USD_IMAGING_API_VERSION >= 14
    static std::unique_ptr<Hgi> hgi(Hgi::CreatePlatformDefaultHgi());
#else
    static std::unique_ptr<Hgi> hgi(Hgi::GetPlatformDefaultHgi());
#endif
    static HdDriver driver{HgiTokens->renderDriver, VtValue(hgi.get())};
    renderIndex = HdRenderIndex::New(renderDelegate, {&driver});
#else
    renderIndex = HdRenderIndex::New(renderDelegate);
#endif

    // Load our USD plugins. We have a plugin which is used
    // for transferring textures directly from Nuke Iops to
    // USD without having to write files to disk.
    static std::once_flag loadPluginsFlag;

    std::call_once(loadPluginsFlag, [] {
      const char* pluginPathPtr = DD::Image::plugin_find("plugInfo.json");
      if (pluginPathPtr != nullptr) {
        std::string pluginPath(pluginPathPtr);
        auto it = pluginPath.find_last_of("/\\");
        if ( it != std::string::npos ) {
          std::string pluginsFolder = pluginPath.substr(0, it);
          PlugRegistry::GetInstance().RegisterPlugins(pluginsFolder);
        }
      }
    });

    nukeDelegate = new HdNukeSceneDelegate(renderIndex);

    static SdfPath taskControllerId("/HdNuke_TaskController");
    taskController = new HdxTaskController(renderIndex, taskControllerId);

    taskController->SetCollection(primCollection);
}

HydraRenderStack::~HydraRenderStack()
{
    if (taskController != nullptr) {
        delete taskController;
    }

    if (nukeDelegate != nullptr) {
        delete nukeDelegate;
    }

    HdRenderDelegate* renderDelegate = nullptr;
    if (renderIndex != nullptr) {
        renderDelegate = renderIndex->GetRenderDelegate();
        delete renderIndex;
    }

    if (rendererPlugin != nullptr) {
        if (renderDelegate != nullptr) {
            rendererPlugin->DeleteRenderDelegate(renderDelegate);
        }

        HdRendererPluginRegistry::GetInstance().ReleasePlugin(rendererPlugin);
    }
}

std::vector<HdRenderBuffer*>
HydraRenderStack::GetRenderBuffers() const
{
    std::vector<HdRenderBuffer*> buffers;
    if (renderIndex == nullptr or taskController == nullptr) {
        return buffers;
    }

    auto bprimIds = renderIndex->GetBprimSubtree(
        HdPrimTypeTokens->renderBuffer, taskController->GetControllerId());
    buffers.reserve(bprimIds.size());

    for (const auto& bprimId : bprimIds)
    {
        buffers.push_back(
            static_cast<HdRenderBuffer*>(renderIndex->GetBprim(
                HdPrimTypeTokens->renderBuffer, bprimId)));
    }
    return buffers;
}

/* static */
HydraRenderStack*
HydraRenderStack::Create(TfToken pluginId)
{
    auto& pluginRegistry = HdRendererPluginRegistry::GetInstance();
    if (not pluginRegistry.IsRegisteredPlugin(pluginId)) {
        return nullptr;
    }

    HdRendererPlugin* plugin = pluginRegistry.GetRendererPlugin(pluginId);
    if (plugin == nullptr) {
        return nullptr;
    }
    if (not plugin->IsSupported()) {
        pluginRegistry.ReleasePlugin(plugin);
        return nullptr;
    }

    return new HydraRenderStack(plugin);
}


PXR_NAMESPACE_CLOSE_SCOPE
