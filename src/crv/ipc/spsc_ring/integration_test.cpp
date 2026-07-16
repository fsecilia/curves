// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief single-file proof of concept for a zero-copy SPSC ring shared between a kernel producer and a userspace
/// consumer
///
/// Part of this test is to see how far we can push std::atomic_ref in the kernel in the face of different compilers
/// and different stdlib implementations across platforms, specifically {gcc,clang}x{libstdc++,libc++}x{x64,arm}. To
/// that end, the production section uses std::atomic_ref on plain ABI storage.
///
/// The public surface is two endpoints, producer_t and consumer_t, minted exclusively by ring_factory_t. The factory
/// is the only component that can certify a mapping and construct endpoints over it; the typed view of the shared
/// memory is an implementation detail. Code that constructs endpoints takes its factory as a template parameter (see
/// the ring_factory concept), so tests can substitute factories that mint fakes.
///
/// std::expected is the second freestanding bet here, alongside std::atomic_ref: P2833 puts it on the C++26
/// freestanding track, and it needs no exception machinery as long as kernel-side code sticks to has_value() and
/// operator* rather than value().
///
/// The eventfd/epoll code is only a userspace stand-in for the driver's future waitqueue/poll implementation; the ring
/// itself has no Linux dependency.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

//
// production code
//

#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace crv::ipc::spsc_ring {

using sequence_t = std::uint64_t;

inline constexpr auto control_alignment = std::size_t{64};
inline constexpr auto sequence_alignment = std::size_t{8};
inline constexpr auto magic = std::uint32_t{0x4352'5652}; // "CRVR"
inline constexpr auto abi_major = std::uint16_t{1};
inline constexpr auto abi_minor = std::uint16_t{0};

enum class format_t : std::uint32_t
{
    raw_displacement = 1,
    velocity_sample = 2,
};

struct raw_displacement_t
{
    std::uint64_t timestamp_ns{};
    std::int64_t x{};
    std::int64_t y{};

    friend constexpr auto operator==(raw_displacement_t const&, raw_displacement_t const&) noexcept -> bool = default;
};

struct velocity_sample_t
{
    std::uint64_t timestamp_ns{};
    std::uint64_t velocity{};

    friend constexpr auto operator==(velocity_sample_t const&, velocity_sample_t const&) noexcept -> bool = default;
};

template <typename element_t> struct format_traits_t;

template <> struct format_traits_t<raw_displacement_t>
{
    static constexpr auto format = format_t::raw_displacement;
};

template <> struct format_traits_t<velocity_sample_t>
{
    static constexpr auto format = format_t::velocity_sample;
};

template <typename element_t>
concept record_type = requires { format_traits_t<element_t>::format; }
    && std::is_standard_layout_v<element_t> && std::is_trivially_copyable_v<element_t>
    && std::is_default_constructible_v<element_t> && std::is_trivially_destructible_v<element_t>;

struct alignas(sequence_alignment) sequence_storage_t
{
    sequence_t value{};
};

// Every control object has explicit bytes and a fixed asserted size. These are part of the kernel/userspace ABI.
struct alignas(control_alignment) description_t
{
    std::uint32_t magic_value{};
    std::uint16_t abi_major_value{};
    std::uint16_t abi_minor_value{};

    format_t format{};
    std::uint32_t element_size{};
    std::uint32_t element_alignment{};
    std::uint32_t reserved_0{};

    sequence_t capacity{};
    sequence_t data_offset{};
    sequence_t mapping_bytes{};

    std::byte reserved[16]{};
};

struct alignas(control_alignment) producer_state_t
{
    sequence_storage_t head{};
    sequence_storage_t dropped{};
    std::byte reserved[48]{};
};

struct alignas(control_alignment) consumer_state_t
{
    sequence_storage_t tail{};
    std::byte reserved[56]{};
};

struct alignas(control_alignment) header_t
{
    description_t description{};
    producer_state_t producer{};
    consumer_state_t consumer{};
};

static_assert(sizeof(sequence_storage_t) == 8);
static_assert(alignof(sequence_storage_t) == sequence_alignment);
static_assert(sizeof(description_t) == 64);
static_assert(sizeof(producer_state_t) == 64);
static_assert(sizeof(consumer_state_t) == 64);
static_assert(sizeof(header_t) == 192);
static_assert(alignof(header_t) == control_alignment);
static_assert(offsetof(header_t, producer) == 64);
static_assert(offsetof(header_t, consumer) == 128);
static_assert(std::is_standard_layout_v<header_t>);
static_assert(std::is_trivially_copyable_v<header_t>);

static_assert(sizeof(raw_displacement_t) == 24);
static_assert(alignof(raw_displacement_t) == 8);
static_assert(sizeof(velocity_sample_t) == 16);
static_assert(alignof(velocity_sample_t) == 8);

constexpr auto align_up(std::size_t value, std::size_t alignment) noexcept -> std::size_t
{
    return (value + alignment - 1) & -alignment;
}

template <record_type element_t> inline constexpr auto data_offset_v = align_up(sizeof(header_t), alignof(element_t));

template <record_type element_t> constexpr auto required_bytes(sequence_t capacity) noexcept -> std::size_t
{
    if (capacity > static_cast<sequence_t>(
            (std::numeric_limits<std::size_t>::max() - data_offset_v<element_t>) / sizeof(element_t)))
    {
        return 0;
    }

    return data_offset_v<element_t> + static_cast<std::size_t>(capacity) * sizeof(element_t);
}

enum class bind_error_t : std::uint8_t
{
    none,
    null_mapping,
    misaligned_header,
    mapping_too_small,
    bad_magic,
    unsupported_abi,
    wrong_format,
    wrong_element_size,
    wrong_element_alignment,
    invalid_capacity,
    wrong_data_offset,
    invalid_mapping_size,
    misaligned_elements,
};

enum class push_result_t : std::uint8_t
{
    published,
    full,
    invalid_consumer_sequence,
};

namespace detail {

// A certified, typed window over a mapping. Instances exist only downstream of validation (bind_view) or
// initialization (initialize_mapping); nothing outside this namespace can mint one, and there is no null state.
template <record_type element_t> class view_t
{
public:
    view_t(header_t& header, element_t* elements, sequence_t capacity) noexcept
        : header_{&header}, elements_{elements}, capacity_{capacity}
    {}

    constexpr auto header() const noexcept -> header_t& { return *header_; }
    constexpr auto capacity() const noexcept -> sequence_t { return capacity_; }
    constexpr auto mask() const noexcept -> sequence_t { return capacity_ - 1; }

    constexpr auto slot(sequence_t sequence) const noexcept -> element_t&
    {
        return elements_[static_cast<std::size_t>(sequence & mask())];
    }

private:
    header_t* header_;
    element_t* elements_;
    sequence_t capacity_;
};

template <record_type element_t>
auto bind_view(void* memory, std::size_t bytes) noexcept -> std::expected<view_t<element_t>, bind_error_t>
{
    if (memory == nullptr) return std::unexpected{bind_error_t::null_mapping};

    if (reinterpret_cast<std::uintptr_t>(memory) % alignof(header_t) != 0)
    {
        return std::unexpected{bind_error_t::misaligned_header};
    }

    if (bytes < sizeof(header_t)) return std::unexpected{bind_error_t::mapping_too_small};

    auto* header = static_cast<header_t*>(memory);
    auto const& description = header->description;

    if (description.magic_value != magic) return std::unexpected{bind_error_t::bad_magic};
    if (description.abi_major_value != abi_major) return std::unexpected{bind_error_t::unsupported_abi};
    if (description.format != format_traits_t<element_t>::format) return std::unexpected{bind_error_t::wrong_format};
    if (description.element_size != sizeof(element_t)) return std::unexpected{bind_error_t::wrong_element_size};
    if (description.element_alignment != alignof(element_t))
    {
        return std::unexpected{bind_error_t::wrong_element_alignment};
    }
    if (!std::has_single_bit(description.capacity)) return std::unexpected{bind_error_t::invalid_capacity};

    if (description.capacity > std::numeric_limits<std::size_t>::max())
    {
        return std::unexpected{bind_error_t::invalid_capacity};
    }

    if (description.data_offset != data_offset_v<element_t>) return std::unexpected{bind_error_t::wrong_data_offset};

    auto const required = required_bytes<element_t>(description.capacity);
    if (required == 0 || description.mapping_bytes < required || description.mapping_bytes > bytes)
    {
        return std::unexpected{bind_error_t::invalid_mapping_size};
    }

    auto* data = static_cast<std::byte*>(memory) + description.data_offset;
    if (reinterpret_cast<std::uintptr_t>(data) % alignof(element_t) != 0)
    {
        return std::unexpected{bind_error_t::misaligned_elements};
    }

    return view_t<element_t>{
        *header,
        reinterpret_cast<element_t*>(data),
        description.capacity,
    };
}

} // namespace detail

template <typename access_t>
concept atomic_access = requires(access_t access, sequence_storage_t& storage, sequence_t value) {
    { access.load_relaxed(storage) } noexcept -> std::same_as<sequence_t>;
    { access.load_acquire(storage) } noexcept -> std::same_as<sequence_t>;
    { access.store_relaxed(storage, value) } noexcept -> std::same_as<void>;
    { access.store_release(storage, value) } noexcept -> std::same_as<void>;
};

struct std_atomic_access_t
{
private:
    using reference_t = std::atomic_ref<sequence_t>;

    static_assert(reference_t::is_always_lock_free);
    static_assert(sequence_alignment >= reference_t::required_alignment);

    static auto reference(sequence_storage_t& storage) noexcept -> reference_t { return reference_t{storage.value}; }

public:
    auto load_relaxed(sequence_storage_t& storage) const noexcept -> sequence_t
    {
        return reference(storage).load(std::memory_order_relaxed);
    }

    auto load_acquire(sequence_storage_t& storage) const noexcept -> sequence_t
    {
        return reference(storage).load(std::memory_order_acquire);
    }

    void store_relaxed(sequence_storage_t& storage, sequence_t value) const noexcept
    {
        reference(storage).store(value, std::memory_order_relaxed);
    }

    void store_release(sequence_storage_t& storage, sequence_t value) const noexcept
    {
        reference(storage).store(value, std::memory_order_release);
    }
};

static_assert(atomic_access<std_atomic_access_t>);

template <typename notifier_t>
concept notifier = requires(notifier_t value) {
    { value.notify() } noexcept -> std::same_as<void>;
};

struct null_notifier_t
{
    void notify() const noexcept {}
};

template <typename waiter_t>
concept waiter = requires(waiter_t value, int timeout_ms) {
    { value.wait(timeout_ms) } noexcept -> std::same_as<bool>;
};

// Degrades consumer_t::wait_until_readable to a single nonblocking poll.
struct null_waiter_t
{
    auto wait(int) const noexcept -> bool { return false; }
};

template <record_type element_t> struct ring_factory_t;

template <record_type element_t, atomic_access atomic_access_t = std_atomic_access_t,
    notifier notifier_t = null_notifier_t>
class producer_t
{
public:
    auto try_push(element_t const& element) noexcept -> push_result_t
    {
        auto const tail = atomic_access_.load_acquire(view_.header().consumer.tail);
        auto const occupied = head_ - tail;

        // The consumer controls tail from userspace. Reject impossible values
        // before using the sequence as an index.
        if (occupied > view_.capacity())
        {
            record_drop();
            return push_result_t::invalid_consumer_sequence;
        }

        if (occupied == view_.capacity())
        {
            record_drop();
            return push_result_t::full;
        }

        view_.slot(head_) = element;
        ++head_;

        // Publish the fully written record, then wake the consumer.
        atomic_access_.store_release(view_.header().producer.head, head_);
        notifier_.notify();
        return push_result_t::published;
    }

private:
    friend struct ring_factory_t<element_t>;

    producer_t(detail::view_t<element_t> view, atomic_access_t atomic_access, notifier_t notifier_value) noexcept
        : view_{view}, atomic_access_{std::move(atomic_access)}, notifier_{std::move(notifier_value)},
          head_{atomic_access_.load_relaxed(view_.header().producer.head)},
          dropped_{atomic_access_.load_relaxed(view_.header().producer.dropped)}
    {}

    void record_drop() noexcept
    {
        ++dropped_;
        atomic_access_.store_relaxed(view_.header().producer.dropped, dropped_);
    }

    detail::view_t<element_t> view_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    [[no_unique_address]] notifier_t notifier_;
    sequence_t head_{};
    sequence_t dropped_{};
};

template <record_type element_t, atomic_access atomic_access_t = std_atomic_access_t, waiter waiter_t = null_waiter_t>
class consumer_t
{
public:
    auto readable() noexcept -> std::span<element_t const>
    {
        if (leased_ != 0)
        {
            return {
                &view_.slot(tail_),
                leased_,
            };
        }

        auto const head = atomic_access_.load_acquire(view_.header().producer.head);
        auto const available = head - tail_;

        if (available > view_.capacity())
        {
            corrupt_ = true;
            return {};
        }

        auto const index = tail_ & view_.mask();
        auto const until_wrap = view_.capacity() - index;
        auto const contiguous = available < until_wrap ? available : until_wrap;

        leased_ = static_cast<std::size_t>(contiguous);
        return {
            &view_.slot(tail_),
            leased_,
        };
    }

    auto consume(std::size_t count) noexcept -> bool
    {
        if (count > leased_)
        {
            corrupt_ = true;
            return false;
        }

        if (count == 0) return true;

        tail_ += static_cast<sequence_t>(count);
        leased_ -= count;

        // Release each consumed slot only after all reads from it are complete.
        atomic_access_.store_release(view_.header().consumer.tail, tail_);
        return true;
    }

    auto empty() noexcept -> bool
    {
        if (leased_ != 0) return false;

        auto const head = atomic_access_.load_acquire(view_.header().producer.head);
        auto const available = head - tail_;

        if (available > view_.capacity())
        {
            corrupt_ = true;
            return true;
        }

        return available == 0;
    }

    auto wait_until_readable(int timeout_ms = -1) noexcept -> bool
    {
        while (empty())
        {
            if (corrupt()) return false;
            if (!waiter_.wait(timeout_ms)) return false;
        }

        return true;
    }

    auto dropped() noexcept -> sequence_t { return atomic_access_.load_relaxed(view_.header().producer.dropped); }

    constexpr auto corrupt() const noexcept -> bool { return corrupt_; }

private:
    friend struct ring_factory_t<element_t>;

    consumer_t(detail::view_t<element_t> view, atomic_access_t atomic_access, waiter_t waiter_value) noexcept
        : view_{view}, atomic_access_{std::move(atomic_access)}, waiter_{std::move(waiter_value)},
          tail_{atomic_access_.load_relaxed(view_.header().consumer.tail)}
    {}

    detail::view_t<element_t> view_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    [[no_unique_address]] waiter_t waiter_;
    sequence_t tail_{};
    std::size_t leased_{};
    bool corrupt_{};
};

// The sole minter of endpoints. Binding validates the mapping's ABI description before any sequence in it is
// trusted, so a producer_t or consumer_t existing at all certifies its mapping. Code that constructs endpoints
// should take its factory as a template parameter so tests can substitute one that mints fakes.
template <record_type element_t> struct ring_factory_t
{
    template <notifier notifier_t = null_notifier_t, atomic_access atomic_access_t = std_atomic_access_t>
    auto bind_producer(void* memory, std::size_t bytes, notifier_t notifier_value = {},
        atomic_access_t atomic_access_value = {}) const noexcept
        -> std::expected<producer_t<element_t, atomic_access_t, notifier_t>, bind_error_t>
    {
        auto view = detail::bind_view<element_t>(memory, bytes);
        if (!view.has_value()) return std::unexpected{view.error()};

        return producer_t<element_t, atomic_access_t, notifier_t>{
            *view,
            std::move(atomic_access_value),
            std::move(notifier_value),
        };
    }

    template <waiter waiter_t = null_waiter_t, atomic_access atomic_access_t = std_atomic_access_t>
    auto bind_consumer(void* memory, std::size_t bytes, waiter_t waiter_value = {},
        atomic_access_t atomic_access_value = {}) const noexcept
        -> std::expected<consumer_t<element_t, atomic_access_t, waiter_t>, bind_error_t>
    {
        auto view = detail::bind_view<element_t>(memory, bytes);
        if (!view.has_value()) return std::unexpected{view.error()};

        return consumer_t<element_t, atomic_access_t, waiter_t>{
            *view,
            std::move(atomic_access_value),
            std::move(waiter_value),
        };
    }
};

template <typename factory_t>
concept ring_factory = requires(factory_t const factory, void* memory, std::size_t bytes) {
    factory.bind_producer(memory, bytes);
    factory.bind_consumer(memory, bytes);
};

static_assert(ring_factory<ring_factory_t<raw_displacement_t>>);
static_assert(ring_factory<ring_factory_t<velocity_sample_t>>);

template <record_type element_t>
auto initialize_mapping(void* memory, std::size_t bytes, sequence_t capacity) noexcept -> bind_error_t
{
    if (memory == nullptr) return bind_error_t::null_mapping;
    if (reinterpret_cast<std::uintptr_t>(memory) % alignof(header_t) != 0) return bind_error_t::misaligned_header;
    if (!std::has_single_bit(capacity)) return bind_error_t::invalid_capacity;

    auto const required = required_bytes<element_t>(capacity);
    if (required == 0) return bind_error_t::invalid_capacity;
    if (bytes < required) return bind_error_t::mapping_too_small;

    auto* header = ::new (memory) header_t{};
    header->description.magic_value = magic;
    header->description.abi_major_value = abi_major;
    header->description.abi_minor_value = abi_minor;
    header->description.format = format_traits_t<element_t>::format;
    header->description.element_size = sizeof(element_t);
    header->description.element_alignment = alignof(element_t);
    header->description.capacity = capacity;
    header->description.data_offset = data_offset_v<element_t>;
    header->description.mapping_bytes = bytes;

    auto* elements = reinterpret_cast<element_t*>(static_cast<std::byte*>(memory) + data_offset_v<element_t>);

    for (sequence_t index = 0; index != capacity; ++index)
    {
        std::construct_at(elements + static_cast<std::size_t>(index));
    }

    return bind_error_t::none;
}

// Only the initializing owner may call this, on memory it initialized.
template <record_type element_t> void destroy_mapping(void* memory) noexcept
{
    auto* header = static_cast<header_t*>(memory);
    auto const capacity = header->description.capacity;
    auto* elements = reinterpret_cast<element_t*>(static_cast<std::byte*>(memory) + data_offset_v<element_t>);

    for (sequence_t index = 0; index != capacity; ++index)
    {
        std::destroy_at(elements + static_cast<std::size_t>(index));
    }

    std::destroy_at(header);
}

} // namespace crv::ipc::spsc_ring

//
// Test-only stand-ins and host-side owners
//

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <thread>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>

namespace crv::ipc::spsc_ring::test_support {

// Host allocation owner. The future kernel owner will supply page-backed
// storage and perform the same one-time initialization before exposing it.
template <record_type element_t> class owned_mapping_t
{
public:
    explicit owned_mapping_t(sequence_t capacity)
        : bytes_{required_bytes<element_t>(capacity)}, alignment_{std::max(control_alignment, alignof(element_t))}
    {
        if (bytes_ == 0) std::abort();

        memory_ = ::operator new(bytes_, std::align_val_t{alignment_});
        if (initialize_mapping<element_t>(memory_, bytes_, capacity) != bind_error_t::none) std::abort();
    }

    owned_mapping_t(owned_mapping_t const&) = delete;
    auto operator=(owned_mapping_t const&) -> owned_mapping_t& = delete;

    ~owned_mapping_t()
    {
        if (memory_ == nullptr) return;

        destroy_mapping<element_t>(memory_);
        ::operator delete(memory_, std::align_val_t{alignment_});
    }

    auto memory() const noexcept -> void* { return memory_; }
    auto bytes() const noexcept -> std::size_t { return bytes_; }

private:
    void* memory_{};
    std::size_t bytes_{};
    std::size_t alignment_{};
};

class unique_fd_t
{
public:
    constexpr unique_fd_t() noexcept = default;
    explicit constexpr unique_fd_t(int value) noexcept : value_{value} {}

    unique_fd_t(unique_fd_t const&) = delete;
    auto operator=(unique_fd_t const&) -> unique_fd_t& = delete;

    constexpr unique_fd_t(unique_fd_t&& other) noexcept : value_{std::exchange(other.value_, -1)} {}

    constexpr auto operator=(unique_fd_t&& other) noexcept -> unique_fd_t&
    {
        if (this == &other) return *this;
        reset();
        value_ = std::exchange(other.value_, -1);
        return *this;
    }

    ~unique_fd_t() { reset(); }

    constexpr auto get() const noexcept -> int { return value_; }
    constexpr explicit operator bool() const noexcept { return value_ >= 0; }

private:
    void reset() noexcept
    {
        if (value_ >= 0) ::close(value_);
        value_ = -1;
    }

    int value_{-1};
};

struct eventfd_notifier_t
{
    int fd{-1};

    void notify() const noexcept
    {
        auto const one = std::uint64_t{1};

        for (;;)
        {
            auto const written = ::write(fd, &one, sizeof(one));
            if (written == static_cast<ssize_t>(sizeof(one))) return;
            if (written < 0 && errno == EINTR) continue;
            if (written < 0 && errno == EAGAIN) return;
            std::abort();
        }
    }
};

struct epoll_waiter_t
{
    int epoll_fd{-1};
    int signal_fd{-1};

    auto wait(int timeout_ms) const noexcept -> bool
    {
        epoll_event event{};
        int count{};

        do { count = ::epoll_wait(epoll_fd, &event, 1, timeout_ms); } while (count < 0 && errno == EINTR);

        if (count <= 0) return false;

        // eventfd provides persistent edge state for this userspace stand-in.
        // The real device fd will be level-readable while the ring is nonempty.
        for (;;)
        {
            std::uint64_t value{};
            auto const read_size = ::read(signal_fd, &value, sizeof(value));
            if (read_size == static_cast<ssize_t>(sizeof(value))) continue;
            if (read_size < 0 && errno == EINTR) continue;
            if (read_size < 0 && errno == EAGAIN) break;
            if (read_size == 0) break;
            return false;
        }

        return true;
    }
};

class eventfd_epoll_channel_t
{
public:
    eventfd_epoll_channel_t()
        : signal_fd_{::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)}, epoll_fd_{::epoll_create1(EPOLL_CLOEXEC)}
    {
        if (!signal_fd_ || !epoll_fd_) std::abort();

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = signal_fd_.get();

        if (::epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, signal_fd_.get(), &event) != 0) std::abort();
    }

    auto notifier() const noexcept -> eventfd_notifier_t { return {.fd = signal_fd_.get()}; }

    auto waiter() const noexcept -> epoll_waiter_t
    {
        return {
            .epoll_fd = epoll_fd_.get(),
            .signal_fd = signal_fd_.get(),
        };
    }

private:
    unique_fd_t signal_fd_;
    unique_fd_t epoll_fd_;
};

template <record_type element_t> class aliased_mapping_t
{
public:
    explicit aliased_mapping_t(sequence_t capacity)
    {
        auto const page_size = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
        bytes_ = align_up(required_bytes<element_t>(capacity), page_size);

        fd_ = ::memfd_create("crv-spsc-ring-test", MFD_CLOEXEC);
        if (fd_ < 0 || ::ftruncate(fd_, static_cast<off_t>(bytes_)) != 0) std::abort();

        writer_memory_ = ::mmap(nullptr, bytes_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        reader_memory_ = ::mmap(nullptr, bytes_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);

        if (writer_memory_ == MAP_FAILED || reader_memory_ == MAP_FAILED || writer_memory_ == reader_memory_)
        {
            std::abort();
        }

        if (initialize_mapping<element_t>(writer_memory_, bytes_, capacity) != bind_error_t::none) std::abort();
    }

    aliased_mapping_t(aliased_mapping_t const&) = delete;
    auto operator=(aliased_mapping_t const&) -> aliased_mapping_t& = delete;

    ~aliased_mapping_t()
    {
        if (writer_memory_ != MAP_FAILED)
        {
            destroy_mapping<element_t>(writer_memory_);
            ::munmap(writer_memory_, bytes_);
        }
        if (reader_memory_ != MAP_FAILED) ::munmap(reader_memory_, bytes_);
        if (fd_ >= 0) ::close(fd_);
    }

    auto writer_memory() const noexcept -> void* { return writer_memory_; }
    auto reader_memory() const noexcept -> void* { return reader_memory_; }
    auto bytes() const noexcept -> std::size_t { return bytes_; }
    auto mappings_are_distinct() const noexcept -> bool { return writer_memory_ != reader_memory_; }

private:
    int fd_{-1};
    void* writer_memory_{MAP_FAILED};
    void* reader_memory_{MAP_FAILED};
    std::size_t bytes_{};
};

} // namespace crv::ipc::spsc_ring::test_support

//
// Explicit GoogleTest/GoogleMock tests
//

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using namespace crv::ipc::spsc_ring;
using namespace crv::ipc::spsc_ring::test_support;

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Ref;
using ::testing::Return;
using ::testing::StrictMock;

class mock_atomic_access_t
{
public:
    MOCK_METHOD(sequence_t, load_relaxed, (sequence_storage_t & storage), (const, noexcept));
    MOCK_METHOD(sequence_t, load_acquire, (sequence_storage_t & storage), (const, noexcept));
    MOCK_METHOD(void, store_relaxed, (sequence_storage_t & storage, sequence_t value), (const, noexcept));
    MOCK_METHOD(void, store_release, (sequence_storage_t & storage, sequence_t value), (const, noexcept));
};

struct mock_atomic_access_delegate_t
{
    mock_atomic_access_t* mock{};

    auto load_relaxed(sequence_storage_t& storage) const noexcept -> sequence_t { return mock->load_relaxed(storage); }

    auto load_acquire(sequence_storage_t& storage) const noexcept -> sequence_t { return mock->load_acquire(storage); }

    void store_relaxed(sequence_storage_t& storage, sequence_t value) const noexcept
    {
        mock->store_relaxed(storage, value);
    }

    void store_release(sequence_storage_t& storage, sequence_t value) const noexcept
    {
        mock->store_release(storage, value);
    }
};

static_assert(std::copyable<mock_atomic_access_delegate_t>);
static_assert(atomic_access<mock_atomic_access_delegate_t>);

// The factory seam: a substituted factory mints its own endpoint types, so code templated on ring_factory can be
// driven entirely by fakes.
struct fake_ring_factory_t
{
    struct fake_producer_t
    {
        auto try_push(raw_displacement_t const&) noexcept -> push_result_t { return push_result_t::published; }
    };

    struct fake_consumer_t
    {
        auto readable() noexcept -> std::span<raw_displacement_t const> { return {}; }
        auto consume(std::size_t) noexcept -> bool { return true; }
    };

    auto bind_producer(void*, std::size_t) const noexcept -> std::expected<fake_producer_t, bind_error_t>
    {
        return fake_producer_t{};
    }

    auto bind_consumer(void*, std::size_t) const noexcept -> std::expected<fake_consumer_t, bind_error_t>
    {
        return fake_consumer_t{};
    }
};

static_assert(ring_factory<fake_ring_factory_t>);

void store_sequence(sequence_storage_t& storage, sequence_t value) noexcept
{
    storage.value = value;
}

TEST(SpscRingFactory, ValidatesAbiDescription)
{
    owned_mapping_t<raw_displacement_t> mapping{8};

    EXPECT_TRUE(ring_factory_t<raw_displacement_t>{}.bind_consumer(mapping.memory(), mapping.bytes()).has_value());

    auto const wrong_format = ring_factory_t<velocity_sample_t>{}.bind_consumer(mapping.memory(), mapping.bytes());
    ASSERT_FALSE(wrong_format.has_value());
    EXPECT_EQ(wrong_format.error(), bind_error_t::wrong_format);

    // Adversarial images are produced the way an adversary would produce them: through the raw mapping.
    auto& header = *static_cast<header_t*>(mapping.memory());
    auto const old_magic = header.description.magic_value;
    header.description.magic_value = 0;

    auto const bad_magic = ring_factory_t<raw_displacement_t>{}.bind_producer(mapping.memory(), mapping.bytes());
    ASSERT_FALSE(bad_magic.has_value());
    EXPECT_EQ(bad_magic.error(), bind_error_t::bad_magic);

    header.description.magic_value = old_magic;
}

TEST(SpscRingAtomicSemantics, UsesAcquireReleaseAtThePublicationBoundaries)
{
    owned_mapping_t<raw_displacement_t> mapping{4};
    auto& header = *static_cast<header_t*>(mapping.memory());
    ring_factory_t<raw_displacement_t> const factory;

    StrictMock<mock_atomic_access_t> producer_atomic;

    {
        InSequence sequence;
        EXPECT_CALL(producer_atomic, load_relaxed(Ref(header.producer.head))).WillOnce(Return(0));
        EXPECT_CALL(producer_atomic, load_relaxed(Ref(header.producer.dropped))).WillOnce(Return(0));
    }

    auto bound_producer = factory.bind_producer(
        mapping.memory(), mapping.bytes(), null_notifier_t{}, mock_atomic_access_delegate_t{.mock = &producer_atomic});
    ASSERT_TRUE(bound_producer.has_value());
    auto& producer = *bound_producer;

    {
        InSequence sequence;
        EXPECT_CALL(producer_atomic, load_acquire(Ref(header.consumer.tail))).WillOnce(Return(0));
        EXPECT_CALL(producer_atomic, store_release(Ref(header.producer.head), 1)).WillOnce(Invoke(store_sequence));
    }

    EXPECT_EQ(producer.try_push({.timestamp_ns = 1, .x = 2, .y = 3}), push_result_t::published);

    StrictMock<mock_atomic_access_t> consumer_atomic;
    EXPECT_CALL(consumer_atomic, load_relaxed(Ref(header.consumer.tail))).WillOnce(Return(0));

    auto bound_consumer = factory.bind_consumer(
        mapping.memory(), mapping.bytes(), null_waiter_t{}, mock_atomic_access_delegate_t{.mock = &consumer_atomic});
    ASSERT_TRUE(bound_consumer.has_value());
    auto& consumer = *bound_consumer;

    EXPECT_CALL(consumer_atomic, load_acquire(Ref(header.producer.head))).WillOnce(Return(1));

    auto const readable = consumer.readable();
    ASSERT_EQ(readable.size(), 1u);
    EXPECT_EQ(readable.front(), (raw_displacement_t{.timestamp_ns = 1, .x = 2, .y = 3}));

    EXPECT_CALL(consumer_atomic, store_release(Ref(header.consumer.tail), 1)).WillOnce(Invoke(store_sequence));

    EXPECT_TRUE(consumer.consume(1));
}

TEST(SpscRing, WrapsAndDropsNewRecordsWhenFull)
{
    owned_mapping_t<raw_displacement_t> mapping{4};
    ring_factory_t<raw_displacement_t> const factory;

    auto bound_producer = factory.bind_producer(mapping.memory(), mapping.bytes());
    auto bound_consumer = factory.bind_consumer(mapping.memory(), mapping.bytes());
    ASSERT_TRUE(bound_producer.has_value());
    ASSERT_TRUE(bound_consumer.has_value());
    auto& producer = *bound_producer;
    auto& consumer = *bound_consumer;

    for (std::uint64_t value = 1; value <= 4; ++value)
    {
        EXPECT_EQ(producer.try_push({
                      .timestamp_ns = value,
                      .x = static_cast<std::int64_t>(value * 10),
                      .y = -static_cast<std::int64_t>(value * 10),
                  }),
            push_result_t::published);
    }

    EXPECT_EQ(producer.try_push({.timestamp_ns = 5}), push_result_t::full);
    EXPECT_EQ(consumer.dropped(), 1u);

    auto first = consumer.readable();
    ASSERT_EQ(first.size(), 4u);
    EXPECT_EQ(first.front().timestamp_ns, 1u);
    EXPECT_EQ(first.back().timestamp_ns, 4u);
    ASSERT_TRUE(consumer.consume(3));

    for (std::uint64_t value = 5; value <= 7; ++value)
    {
        EXPECT_EQ(producer.try_push({.timestamp_ns = value}), push_result_t::published);
    }

    auto wrapped_tail = consumer.readable();
    ASSERT_EQ(wrapped_tail.size(), 1u);
    EXPECT_EQ(wrapped_tail.front().timestamp_ns, 4u);
    ASSERT_TRUE(consumer.consume(1));

    auto wrapped_head = consumer.readable();
    ASSERT_EQ(wrapped_head.size(), 3u);
    EXPECT_EQ(wrapped_head[0].timestamp_ns, 5u);
    EXPECT_EQ(wrapped_head[1].timestamp_ns, 6u);
    EXPECT_EQ(wrapped_head[2].timestamp_ns, 7u);
    EXPECT_TRUE(consumer.consume(wrapped_head.size()));
    EXPECT_TRUE(consumer.empty());
    EXPECT_FALSE(consumer.corrupt());
}

TEST(SpscRing, TransfersRecordsAcrossDistinctSharedMappingAliases)
{
    constexpr auto sample_count = std::uint64_t{250'000};
    constexpr auto checksum_mask = std::uint64_t{0x9E37'79B9'7F4A'7C15};

    aliased_mapping_t<velocity_sample_t> mapping{1024};
    ASSERT_TRUE(mapping.mappings_are_distinct());

    auto bound_consumer = ring_factory_t<velocity_sample_t>{}.bind_consumer(mapping.reader_memory(), mapping.bytes());
    ASSERT_TRUE(bound_consumer.has_value());
    auto& consumer = *bound_consumer;

    std::atomic<bool> producer_failed{false};

    std::jthread producer_thread{[&] {
        auto bound_producer
            = ring_factory_t<velocity_sample_t>{}.bind_producer(mapping.writer_memory(), mapping.bytes());
        if (!bound_producer.has_value())
        {
            producer_failed.store(true, std::memory_order_relaxed);
            return;
        }
        auto& producer = *bound_producer;

        for (std::uint64_t sequence = 1; sequence <= sample_count; ++sequence)
        {
            velocity_sample_t const sample{
                .timestamp_ns = sequence,
                .velocity = sequence ^ checksum_mask,
            };

            for (;;)
            {
                auto const result = producer.try_push(sample);
                if (result == push_result_t::published) break;
                if (result == push_result_t::invalid_consumer_sequence)
                {
                    producer_failed.store(true, std::memory_order_relaxed);
                    return;
                }
                std::this_thread::yield();
            }
        }
    }};

    auto expected = std::uint64_t{1};
    auto data_failed = false;

    while (expected <= sample_count)
    {
        auto const batch = consumer.readable();
        if (batch.empty())
        {
            std::this_thread::yield();
            continue;
        }

        for (auto const& sample : batch)
        {
            if (sample.timestamp_ns != expected || sample.velocity != (expected ^ checksum_mask))
            {
                data_failed = true;
            }
            ++expected;
        }

        if (!consumer.consume(batch.size())) data_failed = true;
    }

    producer_thread.join();

    EXPECT_FALSE(producer_failed.load(std::memory_order_relaxed));
    EXPECT_FALSE(data_failed);
    EXPECT_FALSE(consumer.corrupt());
    EXPECT_TRUE(consumer.empty());
}

TEST(SpscRingConsumer, WaitsThroughAnEpollShapedStandIn)
{
    constexpr auto sample_count = std::uint64_t{32};

    owned_mapping_t<raw_displacement_t> mapping{64};
    eventfd_epoll_channel_t channel;

    auto bound_consumer
        = ring_factory_t<raw_displacement_t>{}.bind_consumer(mapping.memory(), mapping.bytes(), channel.waiter());
    ASSERT_TRUE(bound_consumer.has_value());
    auto& consumer = *bound_consumer;

    std::jthread producer_thread{[&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

        auto bound_producer
            = ring_factory_t<raw_displacement_t>{}.bind_producer(mapping.memory(), mapping.bytes(), channel.notifier());
        if (!bound_producer.has_value()) std::abort();
        auto& producer = *bound_producer;

        for (std::uint64_t sequence = 1; sequence <= sample_count; ++sequence)
        {
            auto const result = producer.try_push({
                .timestamp_ns = sequence,
                .x = static_cast<std::int64_t>(sequence),
                .y = -static_cast<std::int64_t>(sequence),
            });

            if (result != push_result_t::published) std::abort();
        }
    }};

    ASSERT_TRUE(consumer.wait_until_readable(2'000));

    auto expected = std::uint64_t{1};
    while (expected <= sample_count)
    {
        auto const batch = consumer.readable();
        if (batch.empty())
        {
            ASSERT_TRUE(consumer.wait_until_readable(2'000));
            continue;
        }

        for (auto const& sample : batch)
        {
            EXPECT_EQ(sample.timestamp_ns, expected);
            EXPECT_EQ(sample.x, static_cast<std::int64_t>(expected));
            EXPECT_EQ(sample.y, -static_cast<std::int64_t>(expected));
            ++expected;
        }

        ASSERT_TRUE(consumer.consume(batch.size()));
    }

    producer_thread.join();
    EXPECT_FALSE(consumer.corrupt());
}

} // namespace
