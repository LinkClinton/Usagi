﻿#pragma once

#include <Usagi/Engine/Core/Element.hpp>

#include "Subsystem.hpp"

namespace usagi
{
template <typename... RequiredComponents>
class ConstrainedSubsystem : public Subsystem
{
public:
    bool handles(Element *element) override
    {
        return (... && element->hasComponent<RequiredComponents>());
    }
};
}
