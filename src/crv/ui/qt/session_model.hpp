// SPDX-License-Identifier: MIT

/// \file
/// \brief live control-session state for the Qt frontend
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <crv/model/config.hpp>
#include <crv/pipeline/configuration/apply_mode.hpp>
#include <crv/pipeline/control/client.hpp>
#include <crv/pipeline/control/session.hpp>
#include <algorithm>
#include <cassert>
#include <expected>
#include <optional>
#include <utility>
#include <vector>

namespace crv::qt {
namespace generic {

template <typename t_session_t> class session_model_t
{
public:
    using session_t = t_session_t;
    using devices_result_t = typename session_t::devices_result_t;
    using refresh_result_t = std::expected<void, typename devices_result_t::error_type>;
    using apply_result_t = typename session_t::apply_result_t;

    explicit session_model_t(session_t session) noexcept : session_{std::move(session)} {}

    auto refresh() -> refresh_result_t
    {
        auto next = session_.devices();
        if (!next) return std::unexpected{std::move(next.error())};

        auto const previous_id = selected_attachment_id();
        auto next_selection = std::optional<std::size_t>{};
        if (previous_id)
        {
            auto const it
                = std::ranges::find_if(*next, [&](auto const& attachment) { return attachment.id == *previous_id; });
            if (it != next->end()) next_selection = static_cast<std::size_t>(std::ranges::distance(next->begin(), it));
        }
        else if (!has_refreshed_ && next->size() == 1) next_selection = 0;

        attachments_ = std::move(*next);
        selected_index_ = next_selection;
        has_refreshed_ = true;
        return {};
    }

    auto attachments() const noexcept -> std::vector<pipeline::control::attachment_t> const& { return attachments_; }

    auto selected_attachment_index() const noexcept -> int_t
    {
        return selected_index_ ? static_cast<int_t>(*selected_index_) : int_t{-1};
    }

    auto set_selected_attachment_index(int_t index) noexcept -> void
    {
        if (index < 0 || static_cast<std::size_t>(index) >= attachments_.size())
        {
            selected_index_.reset();
            return;
        }
        selected_index_ = static_cast<std::size_t>(index);
    }

    auto clear_selected_attachment() noexcept -> void { selected_index_.reset(); }
    auto has_selected_attachment() const noexcept -> bool { return selected_index_.has_value(); }

    auto selected_attachment_id() const noexcept -> std::optional<pipeline::control::attachment_id_t>
    {
        if (!selected_index_) return std::nullopt;
        assert(*selected_index_ < attachments_.size());
        return attachments_[*selected_index_].id;
    }

    auto apply(model::device_t const& device, model::profile_t const& profile,
        pipeline::configuration::apply_mode_t mode) const -> apply_result_t
    {
        auto const attachment = selected_attachment_id();
        assert(attachment.has_value());
        return session_.apply(device, profile, *attachment, mode);
    }

private:
    session_t session_;
    std::vector<pipeline::control::attachment_t> attachments_;
    std::optional<std::size_t> selected_index_;
    bool has_refreshed_ = false;
};

} // namespace generic

using session_model_t = generic::session_model_t<pipeline::control::session_t>;

} // namespace crv::qt
