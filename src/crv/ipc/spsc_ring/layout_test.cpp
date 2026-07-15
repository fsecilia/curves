// SPDX-License-Identifier: MIT

/// \file
/// \copyright Copyright (C) 2026 Frank Secilia

#include "layout.hpp"
#include <crv/test/test.hpp>

namespace crv::ipc::spsc_ring {
namespace {

struct element_t
{
    uint32_t x;
    uint32_t y;
};

static_assert(magic == 0x43525652);
static_assert(abi_major == 1);
static_assert(abi_minor == 0);
static_assert(alignment == 64);

static_assert(sizeof(consumer_state_t) == alignment);

static_assert(offsetof(header_t, description) == 0);
static_assert(offsetof(header_t, producer) == alignment);
static_assert(offsetof(header_t, consumer) == alignment * 2);
static_assert(sizeof(header_t) == alignment * 3);

static_assert(data_offset<element_t>() == alignment * 3);

struct ipc_spsc_ring_test_t : Test
{};

TEST_F(ipc_spsc_ring_test_t, byte_size)
{
    // 0 capacity is header size
    EXPECT_EQ(byte_size<element_t>(0), 192);

    // 192-byte header + (10 elements * 8 bytes each) = 272 bytes
    EXPECT_EQ(byte_size<element_t>(10), 272);
}

struct ipc_spsc_ring_test_to_elements_t : ipc_spsc_ring_test_t
{
    alignas(alignment) std::byte buffer[256]{};
};

TEST_F(ipc_spsc_ring_test_to_elements_t, to_elements_mutable)
{
    void* memory = static_cast<void*>(buffer);

    auto* elements = to_elements<element_t>(memory);

    EXPECT_EQ(reinterpret_cast<std::byte*>(elements), buffer + 192);
}

TEST_F(ipc_spsc_ring_test_to_elements_t, to_elements_const)
{
    void const* memory = static_cast<void*>(buffer);

    auto const* elements = to_elements<element_t>(memory);

    EXPECT_EQ(reinterpret_cast<std::byte const*>(elements), buffer + 192);
}

} // namespace
} // namespace crv::ipc::spsc_ring
