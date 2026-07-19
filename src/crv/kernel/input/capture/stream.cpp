// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "stream.hpp"

namespace crv {

/// adapter over the opaque linux c byte sink
class kernel_capture_byte_sink_t
{
public:
    explicit constexpr kernel_capture_byte_sink_t(crv_capture_byte_sink_t* sink) noexcept : sink_{sink} {}

    [[nodiscard]] auto try_write_exact(std::span<std::byte const> bytes) const noexcept -> bool
    {
        return crv_capture_byte_sink_try_write_exact(sink_, bytes.data(), bytes.size());
    }

private:
    crv_capture_byte_sink_t* sink_;
};

[[nodiscard]] auto make_scratch(crv_capture_producer_context_t& context) noexcept -> std::span<std::byte>
{
    if (context.scratch == nullptr) return {};

    return {
        static_cast<std::byte*>(context.scratch),
        context.scratch_size,
    };
}

[[nodiscard]] auto make_producer(crv_capture_producer_context_t& context) noexcept
{
    return crv::capture_stream_producer_t{
        crv::kernel_capture_byte_sink_t{context.sink},
        make_scratch(context),
        context.state,
    };
}

} // namespace crv

extern "C" int crv_capture_producer_begin_session(crv_capture_producer_context_t* context)
{
    if (context == nullptr || context->sink == nullptr) return false;

    return crv::make_producer(*context).begin_session();
}

extern "C" enum crv_capture_push_result_t crv_capture_producer_try_push(crv_capture_producer_context_t* context,
    crv_capture_u64_t timestamp_ns, crv_input_value_t const* values, crv_capture_size_t count,
    crv_capture_size_t capacity)
{
    if (context == nullptr || context->sink == nullptr) return CRV_CAPTURE_INVALID_INPUT;

    return crv::make_producer(*context).try_push(timestamp_ns, values, count, capacity);
}
