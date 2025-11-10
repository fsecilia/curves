#include "presenter.hpp"
#include <curves/ui/renderer.hpp>

namespace curves::ui {

Presenter::Presenter() {
  model_ = std::make_unique<lib::Model>();
  selected_point_index_ = std::nullopt;
}

auto Presenter::on_mouse_press(lib::Point2D normalized_pos) -> bool {
  selected_point_index_ = std::nullopt;
  const auto click_radius_sq = kClickRadius * kClickRadius;

  // Check if we clicked near any point
  for (std::size_t i = 0; i < model_->get_point_count(); ++i) {
    // Use kClickRadius (which is normalized) vs normalized distance
    if (model_->get_points()[i].distance_sq(normalized_pos) < click_radius_sq) {
      selected_point_index_ = i;
      return true;
    }
  }
  return false;
}

auto Presenter::on_mouse_move(lib::Point2D normalized_pos) -> void {
  // If a point is selected, tell the model to move it
  if (selected_point_index_.has_value()) {
    model_->move_point(selected_point_index_.value(), normalized_pos);
  }
}

auto Presenter::on_mouse_release() -> void {
  selected_point_index_ = std::nullopt;
}

auto Presenter::get_curve_points() const -> const std::vector<lib::Point2D>& {
  return model_->get_points();
}

auto Presenter::get_selected_point_index() const -> std::optional<std::size_t> {
  return selected_point_index_;
}

auto Presenter::render(IRenderer& renderer) const -> void {
  // --- 1. Get State from presenter ---
  const auto& points = get_curve_points();
  auto selected_index = get_selected_point_index();

  if (points.empty()) {
    return;
  }

  // --- 2. Draw Grid/Background ---
  renderer.fill_background(Color::Background);

  // --- 3. Draw the Curve Line ---
  renderer.set_pen(Color::Primary, 2.0);  // 2.0 pixel width
  renderer.set_no_brush();
  renderer.draw_polyline(points);

  // --- 4. Draw the Points ---
  for (std::size_t i = 0; i < points.size(); ++i) {
    const auto& point_pos = points[i];

    // Highlight the selected point
    if (selected_index.has_value() && selected_index.value() == i) {
      renderer.set_brush(Color::White);
      renderer.set_pen(Color::Primary, 2.0);
      renderer.draw_ellipse(point_pos, kSelectedPointRadius,
                            kSelectedPointRadius);
    } else {
      renderer.set_brush(Color::Primary);
      renderer.set_no_pen();
      renderer.draw_ellipse(point_pos, kPointRadius, kPointRadius);
    }
  }
}

}  // namespace curves::ui
