// SPDX-License-Identifier: MIT

/// \file
/// \brief i18n provider interface and default identity implementation
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <string>

namespace crv::i18n {

//
// providers
//

/// base provider interface
struct provider_i
{
    /// translates using source as a key within given context
    virtual auto translate(char const* context, char const* source) const -> std::string = 0;

protected:
    ~provider_i();
};

/// concrete implementation that returns the given source verbatim
struct identity_provider_t final : provider_i
{
    auto translate(char const* context, char const* source) const -> std::string override;
};

//
// global provider instance
//

/// sets global provider
auto provider(provider_i const& new_provider) noexcept -> provider_i const&;

/// gets global provider
auto provider() noexcept -> provider_i const&;

/// raii for assigning and restoring global provider
class scoped_provider_t
{
public:
    explicit scoped_provider_t(provider_i const& new_provider) noexcept : prev_{&provider(new_provider)} {}
    ~scoped_provider_t() { provider(*prev_); }

    scoped_provider_t(scoped_provider_t const&) = delete;
    auto operator=(scoped_provider_t const&) -> scoped_provider_t& = delete;

private:
    provider_i const* prev_;
};

//
// transltion macros
//

auto translate(char const* context, char const* source) -> std::string;

#define CRV_TR_C(context, source) ::crv::i18n::translate(context, source)
#define CRV_TR(source) CRV_TR_C("", source)

} // namespace crv::i18n
