// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <QPainter>
#include <QRectF>

namespace crv {

class grid_renderer_t
{
public:
    auto draw(QPainter* painter, QRectF const& screen_rect, QRectF const& domain) -> void;

private:
    auto step(float_t range, int_t target_tick_count) const noexcept -> float_t;
};

} // namespace crv
