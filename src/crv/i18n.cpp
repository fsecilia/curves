// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "i18n.hpp"

namespace crv::i18n {

//
// providers
//

provider_i::~provider_i() = default;

auto identity_provider_t::translate(char const*, char const* source) const -> std::string
{
    return nullptr == source ? std::string{} : std::string{source};
}

//
// global provider instance
//

static auto const default_provider = identity_provider_t{};
static auto global_provider = static_cast<provider_i const*>(&default_provider);

auto provider(provider_i const& new_provider) noexcept -> provider_i const&
{
    auto const& result = provider();
    global_provider = &new_provider;
    return result;
}

auto provider() noexcept -> provider_i const&
{
    return *global_provider;
}

//
// transltion macros
//

auto translate(char const* context, char const* source) -> std::string
{
    return provider().translate(context, source);
}

} // namespace crv::i18n
