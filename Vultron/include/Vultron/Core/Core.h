#pragma once

#include <memory>

namespace Vultron
{
    // Unique pointer alias
    template <typename T>
    using Ptr = std::unique_ptr<T>;

    template <typename T, typename... Args>
    Ptr<T> MakePtr(Args... args)
    {
        return std::move(std::make_unique<T>(args...));
    }
}