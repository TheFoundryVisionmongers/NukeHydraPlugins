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

#ifndef HDNUKE_ADAPTERFACTORY_H
#define HDNUKE_ADAPTERFACTORY_H

#include "types.h"

#include <pxr/pxr.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/path.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class HdNukeAdapter;
using HdNukeAdapterPtr = std::shared_ptr<HdNukeAdapter>;
struct AdapterSharedState;

/// Factory class for HdNukeAdapter objects.
/// This is to create adapters from different types of Nuke operations (based
/// on DD::Image::Op::Class). Register an HdNukeAdapterFactory::AdapterCreator in the
/// factory instance and its HdNukeAdapterFactory::AdapterCreator::Create method will
/// be used to create new HdNukeAdapter objects.
class HdNukeAdapterFactory
{
public:
    /// Base adapter creator class.
    /// Subclasses specify which HdNukeAdapter subclass to instantiate.
    class AdapterCreator
    {
    public:
        virtual ~AdapterCreator() = default;

        /// Subclasses must implement this method to create HdNukeAdapter
        /// objects.
        virtual HdNukeAdapterPtr Create(AdapterSharedState*) = 0;
    };

    /// Returns the single HdNukeAdapterFactory instance.
    static HdNukeAdapterFactory& Instance();

    /// Registers an AdapterCreator \p creator instance to be used when trying
    /// to create an HdNukeAdapter of \p className type.
    /// @return   The AdapterCreator currently associated with \p className or nullptr.
    std::shared_ptr<AdapterCreator> RegisterAdapterCreator(const TfToken& className, const std::shared_ptr<AdapterCreator>& creator);

    /// Returns the AdapterCreator instance associated with the \p className.
    std::shared_ptr<AdapterCreator> GetAdapterCreator(const TfToken& className);

    /// Creates an HdNukeAdapter object using the AdapterCreator associated with \p className.
    HdNukeAdapterPtr Create(const TfToken& className, AdapterSharedState* sharedState);

    /// Removes all AdapterCreator from the factory.
    void Clear();
private:
    std::unordered_map<TfToken, std::shared_ptr<AdapterCreator>, TfToken::HashFunctor> _creators;
};

/// Convenience class to register a HdNukeAdapterFactory::AdapterCreator
template<class T>
class AdapterRegister
{
public:
    AdapterRegister(const TfToken& type) : _creator(std::make_shared<T>())
    {
        HdNukeAdapterFactory::Instance().RegisterAdapterCreator(type, _creator);
    }
private:
    std::shared_ptr<T> _creator;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDNUKE_ADAPTERFACTORY_H
