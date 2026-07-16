// SPDX-License-Identifier: GPL-2.0+ OR MIT
///
/// Single-file proof of concept for a zero-copy SPSC ring shared between a kernel producer and a userspace consumer.
///
/// This file intentionally uses std::atomic_ref on plain ABI storage. The Linux eventfd side is only a userspace
/// stand-in for the driver's future waitqueue/.poll implementation; the ring itself has no Linux dependency.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>

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
    return value + (alignment - value % alignment) % alignment;
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

template <record_type element_t> class view_t
{
public:
    constexpr view_t() noexcept = default;

    static auto bind(void* memory, std::size_t bytes, view_t& output) noexcept -> bind_error_t
    {
        if (memory == nullptr) return bind_error_t::null_mapping;

        if (reinterpret_cast<std::uintptr_t>(memory) % alignof(header_t) != 0) return bind_error_t::misaligned_header;

        if (bytes < sizeof(header_t)) return bind_error_t::mapping_too_small;

        auto* header = static_cast<header_t*>(memory);
        auto const& description = header->description;

        if (description.magic_value != magic) return bind_error_t::bad_magic;

        if (description.abi_major_value != abi_major) return bind_error_t::unsupported_abi;

        if (description.format != format_traits_t<element_t>::format) return bind_error_t::wrong_format;

        if (description.element_size != sizeof(element_t)) return bind_error_t::wrong_element_size;

        if (description.element_alignment != alignof(element_t)) return bind_error_t::wrong_element_alignment;

        if (!std::has_single_bit(description.capacity)) return bind_error_t::invalid_capacity;

        if (description.capacity > std::numeric_limits<std::size_t>::max()) return bind_error_t::invalid_capacity;

        if (description.data_offset != data_offset_v<element_t>) return bind_error_t::wrong_data_offset;

        auto const required = required_bytes<element_t>(description.capacity);
        if (required == 0 || description.mapping_bytes < required || description.mapping_bytes > bytes)
        {
            return bind_error_t::invalid_mapping_size;
        }

        auto* data = static_cast<std::byte*>(memory) + description.data_offset;
        if (reinterpret_cast<std::uintptr_t>(data) % alignof(element_t) != 0) return bind_error_t::misaligned_elements;

        output = view_t{
            *header,
            reinterpret_cast<element_t*>(data),
            description.capacity,
        };

        return bind_error_t::none;
    }

    constexpr auto header() noexcept -> header_t& { return *header_; }
    constexpr auto header() const noexcept -> header_t const& { return *header_; }

    constexpr auto elements() noexcept -> std::span<element_t>
    {
        return {elements_, static_cast<std::size_t>(capacity_)};
    }

    constexpr auto elements() const noexcept -> std::span<element_t const>
    {
        return {elements_, static_cast<std::size_t>(capacity_)};
    }

    constexpr auto capacity() const noexcept -> sequence_t { return capacity_; }
    constexpr auto mask() const noexcept -> sequence_t { return capacity_ - 1; }

    constexpr auto slot(sequence_t sequence) noexcept -> element_t&
    {
        return elements_[static_cast<std::size_t>(sequence & mask())];
    }

    constexpr auto slot(sequence_t sequence) const noexcept -> element_t const&
    {
        return elements_[static_cast<std::size_t>(sequence & mask())];
    }

private:
    constexpr view_t(header_t& header, element_t* elements, sequence_t capacity) noexcept
        : header_{&header}, elements_{elements}, capacity_{capacity}
    {}

    header_t* header_{};
    element_t* elements_{};
    sequence_t capacity_{};
};

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

enum class push_result_t : std::uint8_t
{
    published,
    full,
    invalid_consumer_sequence,
};

template <record_type element_t, atomic_access atomic_access_t = std_atomic_access_t> class producer_t
{
public:
    explicit producer_t(view_t<element_t> view, atomic_access_t atomic_access = {}) noexcept
        : view_{view}, atomic_access_{std::move(atomic_access)},
          head_{atomic_access_.load_relaxed(view_.header().producer.head)},
          dropped_{atomic_access_.load_relaxed(view_.header().producer.dropped)}
    {}

    auto try_push(element_t const& element) noexcept -> push_result_t
    {
        auto const tail = atomic_access_.load_acquire(view_.header().consumer.tail);
        auto const occupied = head_ - tail;

        // The consumer controls tail from userspace. Never allow an impossible
        // value to become an out-of-bounds index.
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

        // Publish the fully written record.
        atomic_access_.store_release(view_.header().producer.head, head_);
        return push_result_t::published;
    }

private:
    void record_drop() noexcept
    {
        ++dropped_;
        atomic_access_.store_relaxed(view_.header().producer.dropped, dropped_);
    }

    view_t<element_t> view_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    sequence_t head_{};
    sequence_t dropped_{};
};

template <record_type element_t, atomic_access atomic_access_t = std_atomic_access_t> class consumer_t
{
public:
    explicit consumer_t(view_t<element_t> view, atomic_access_t atomic_access = {}) noexcept
        : view_{view}, atomic_access_{std::move(atomic_access)},
          tail_{atomic_access_.load_relaxed(view_.header().consumer.tail)}
    {}

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
        auto const contiguous = std::min(available, until_wrap);

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

    auto dropped() noexcept -> sequence_t { return atomic_access_.load_relaxed(view_.header().producer.dropped); }

    constexpr auto corrupt() const noexcept -> bool { return corrupt_; }

private:
    view_t<element_t> view_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    sequence_t tail_{};
    std::size_t leased_{};
    bool corrupt_{};
};

struct null_notifier_t
{
    void notify() const noexcept {}
};

template <typename notifier_t>
concept notifier = requires(notifier_t value) {
    { value.notify() } noexcept -> std::same_as<void>;
};

template <record_type element_t, atomic_access atomic_access_t = std_atomic_access_t,
    notifier notifier_t = null_notifier_t>
class writer_t
{
public:
    explicit writer_t(
        view_t<element_t> view, notifier_t notifier_value = {}, atomic_access_t atomic_access_value = {}) noexcept
        : producer_{view, std::move(atomic_access_value)}, notifier_{std::move(notifier_value)}
    {}

    auto try_push(element_t const& element) noexcept -> push_result_t
    {
        auto const result = producer_.try_push(element);
        if (result == push_result_t::published) notifier_.notify();
        return result;
    }

private:
    producer_t<element_t, atomic_access_t> producer_;
    [[no_unique_address]] notifier_t notifier_;
};

template <typename waiter_t>
concept waiter = requires(waiter_t value, int timeout_ms) {
    { value.wait(timeout_ms) } noexcept -> std::same_as<bool>;
};

template <record_type element_t, atomic_access atomic_access_t, waiter waiter_t> class reader_t
{
public:
    explicit reader_t(view_t<element_t> view, waiter_t waiter_value, atomic_access_t atomic_access_value = {}) noexcept
        : consumer_{view, std::move(atomic_access_value)}, waiter_{std::move(waiter_value)}
    {}

    auto wait_until_readable(int timeout_ms = -1) noexcept -> bool
    {
        while (consumer_.empty())
        {
            if (consumer_.corrupt()) return false;
            if (!waiter_.wait(timeout_ms)) return false;
        }

        return true;
    }

    auto readable() noexcept -> std::span<element_t const> { return consumer_.readable(); }

    auto consume(std::size_t count) noexcept -> bool { return consumer_.consume(count); }

    auto dropped() noexcept -> sequence_t { return consumer_.dropped(); }
    auto corrupt() const noexcept -> bool { return consumer_.corrupt(); }

private:
    consumer_t<element_t, atomic_access_t> consumer_;
    [[no_unique_address]] waiter_t waiter_;
};

template <record_type element_t>
auto initialize_mapping(void* memory, std::size_t bytes, sequence_t capacity) noexcept -> bool
{
    if (memory == nullptr || reinterpret_cast<std::uintptr_t>(memory) % alignof(header_t) != 0
        || !std::has_single_bit(capacity))
    {
        return false;
    }

    auto const required = required_bytes<element_t>(capacity);
    if (required == 0 || bytes < required) return false;

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
        std::construct_at(elements + static_cast<std::size_t>(index));

    return true;
}

template <record_type element_t> void destroy_mapping(view_t<element_t> view) noexcept
{
    for (sequence_t index = 0; index != view.capacity(); ++index) std::destroy_at(&view.slot(index));

    std::destroy_at(&view.header());
}

// Test/host allocation owner. The future kernel owner will supply vmalloc'ed
// pages and perform the same one-time initialization before exposing them.
template <record_type element_t> class owned_mapping_t
{
public:
    explicit owned_mapping_t(sequence_t capacity)
        : bytes_{required_bytes<element_t>(capacity)}, alignment_{std::max(control_alignment, alignof(element_t))}
    {
        if (!std::has_single_bit(capacity) || bytes_ == 0) std::abort();

        memory_ = ::operator new(bytes_, std::align_val_t{alignment_});
        if (!initialize_mapping<element_t>(memory_, bytes_, capacity)) std::abort();

        auto const error = view_t<element_t>::bind(memory_, bytes_, view_);
        if (error != bind_error_t::none) std::abort();
    }

    owned_mapping_t(owned_mapping_t const&) = delete;
    auto operator=(owned_mapping_t const&) -> owned_mapping_t& = delete;

    ~owned_mapping_t()
    {
        if (memory_ == nullptr) return;

        destroy_mapping(view_);
        ::operator delete(memory_, std::align_val_t{alignment_});
    }

    auto view() const noexcept -> view_t<element_t> { return view_; }
    auto memory() const noexcept -> void* { return memory_; }
    auto bytes() const noexcept -> std::size_t { return bytes_; }

private:
    void* memory_{};
    std::size_t bytes_{};
    std::size_t alignment_{};
    view_t<element_t> view_{};
};

// The following Linux-userland classes stand in for the future kernel
// waitqueue/.poll half and exercise the user-side epoll wrapper end to end.
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

        // eventfd is edge persistence for this userland stand-in. The real
        // device fd will be level-readable while the shared ring is nonempty.
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

        if (!initialize_mapping<element_t>(writer_memory_, bytes_, capacity)) std::abort();

        if (view_t<element_t>::bind(writer_memory_, bytes_, writer_view_) != bind_error_t::none
            || view_t<element_t>::bind(reader_memory_, bytes_, reader_view_) != bind_error_t::none)
        {
            std::abort();
        }
    }

    aliased_mapping_t(aliased_mapping_t const&) = delete;
    auto operator=(aliased_mapping_t const&) -> aliased_mapping_t& = delete;

    ~aliased_mapping_t()
    {
        if (writer_memory_ != MAP_FAILED)
        {
            destroy_mapping(writer_view_);
            ::munmap(writer_memory_, bytes_);
        }
        if (reader_memory_ != MAP_FAILED) ::munmap(reader_memory_, bytes_);
        if (fd_ >= 0) ::close(fd_);
    }

    auto writer_view() const noexcept -> view_t<element_t> { return writer_view_; }
    auto reader_view() const noexcept -> view_t<element_t> { return reader_view_; }
    auto mappings_are_distinct() const noexcept -> bool { return writer_memory_ != reader_memory_; }

private:
    int fd_{-1};
    void* writer_memory_{MAP_FAILED};
    void* reader_memory_{MAP_FAILED};
    std::size_t bytes_{};
    view_t<element_t> writer_view_{};
    view_t<element_t> reader_view_{};
};

} // namespace crv::ipc::spsc_ring

namespace {

using namespace crv::ipc::spsc_ring;

#define CHECK(expression)                                                                         \
    do                                                                                            \
    {                                                                                             \
        if (!(expression))                                                                        \
        {                                                                                         \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            return false;                                                                         \
        }                                                                                         \
    } while (false)

enum class atomic_operation_t : std::uint8_t
{
    load_relaxed,
    load_acquire,
    store_relaxed,
    store_release,
};

struct atomic_event_t
{
    atomic_operation_t operation{};
    sequence_storage_t* storage{};
    sequence_t value{};
};

struct atomic_log_t
{
    std::array<atomic_event_t, 32> events{};
    std::size_t size{};

    void clear() noexcept { size = 0; }

    void record(atomic_operation_t operation, sequence_storage_t& storage, sequence_t value) noexcept
    {
        if (size == events.size()) std::abort();
        events[size++] = {
            .operation = operation,
            .storage = &storage,
            .value = value,
        };
    }
};

struct recording_atomic_access_t
{
    atomic_log_t* log{};

    auto load_relaxed(sequence_storage_t& storage) const noexcept -> sequence_t
    {
        log->record(atomic_operation_t::load_relaxed, storage, storage.value);
        return storage.value;
    }

    auto load_acquire(sequence_storage_t& storage) const noexcept -> sequence_t
    {
        log->record(atomic_operation_t::load_acquire, storage, storage.value);
        return storage.value;
    }

    void store_relaxed(sequence_storage_t& storage, sequence_t value) const noexcept
    {
        storage.value = value;
        log->record(atomic_operation_t::store_relaxed, storage, value);
    }

    void store_release(sequence_storage_t& storage, sequence_t value) const noexcept
    {
        storage.value = value;
        log->record(atomic_operation_t::store_release, storage, value);
    }
};

static_assert(atomic_access<recording_atomic_access_t>);

auto test_view_validation() -> bool
{
    owned_mapping_t<raw_displacement_t> mapping{8};

    view_t<raw_displacement_t> raw_view;
    CHECK(view_t<raw_displacement_t>::bind(mapping.memory(), mapping.bytes(), raw_view) == bind_error_t::none);

    view_t<velocity_sample_t> wrong_view;
    CHECK(view_t<velocity_sample_t>::bind(mapping.memory(), mapping.bytes(), wrong_view) == bind_error_t::wrong_format);

    auto const old_magic = raw_view.header().description.magic_value;
    raw_view.header().description.magic_value = 0;

    view_t<raw_displacement_t> bad_magic_view;
    CHECK(
        view_t<raw_displacement_t>::bind(mapping.memory(), mapping.bytes(), bad_magic_view) == bind_error_t::bad_magic);

    raw_view.header().description.magic_value = old_magic;
    return true;
}

auto test_atomic_semantics() -> bool
{
    owned_mapping_t<raw_displacement_t> mapping{4};
    auto view = mapping.view();

    atomic_log_t producer_log;
    producer_t<raw_displacement_t, recording_atomic_access_t> producer{
        view,
        recording_atomic_access_t{.log = &producer_log},
    };

    CHECK(producer_log.size == 2);
    CHECK(producer_log.events[0].operation == atomic_operation_t::load_relaxed);
    CHECK(producer_log.events[0].storage == &view.header().producer.head);
    CHECK(producer_log.events[1].operation == atomic_operation_t::load_relaxed);
    CHECK(producer_log.events[1].storage == &view.header().producer.dropped);

    producer_log.clear();
    CHECK(producer.try_push({.timestamp_ns = 1, .x = 2, .y = 3}) == push_result_t::published);

    CHECK(producer_log.size == 2);
    CHECK(producer_log.events[0].operation == atomic_operation_t::load_acquire);
    CHECK(producer_log.events[0].storage == &view.header().consumer.tail);
    CHECK(producer_log.events[1].operation == atomic_operation_t::store_release);
    CHECK(producer_log.events[1].storage == &view.header().producer.head);
    CHECK(producer_log.events[1].value == 1);

    atomic_log_t consumer_log;
    consumer_t<raw_displacement_t, recording_atomic_access_t> consumer{
        view,
        recording_atomic_access_t{.log = &consumer_log},
    };

    CHECK(consumer_log.size == 1);
    CHECK(consumer_log.events[0].operation == atomic_operation_t::load_relaxed);
    CHECK(consumer_log.events[0].storage == &view.header().consumer.tail);

    consumer_log.clear();
    auto const readable = consumer.readable();
    CHECK(readable.size() == 1);
    auto const expected_record = raw_displacement_t{.timestamp_ns = 1, .x = 2, .y = 3};
    CHECK(readable[0] == expected_record);
    CHECK(consumer_log.size == 1);
    CHECK(consumer_log.events[0].operation == atomic_operation_t::load_acquire);
    CHECK(consumer_log.events[0].storage == &view.header().producer.head);

    consumer_log.clear();
    CHECK(consumer.consume(1));
    CHECK(consumer_log.size == 1);
    CHECK(consumer_log.events[0].operation == atomic_operation_t::store_release);
    CHECK(consumer_log.events[0].storage == &view.header().consumer.tail);
    CHECK(consumer_log.events[0].value == 1);

    return true;
}

auto test_wraparound_and_drop_new() -> bool
{
    owned_mapping_t<raw_displacement_t> mapping{4};
    producer_t<raw_displacement_t> producer{mapping.view()};
    consumer_t<raw_displacement_t> consumer{mapping.view()};

    for (std::uint64_t value = 1; value <= 4; ++value)
    {
        CHECK(producer.try_push({
                  .timestamp_ns = value,
                  .x = static_cast<std::int64_t>(value * 10),
                  .y = -static_cast<std::int64_t>(value * 10),
              })
            == push_result_t::published);
    }

    CHECK(producer.try_push({.timestamp_ns = 5}) == push_result_t::full);
    CHECK(consumer.dropped() == 1);

    auto first = consumer.readable();
    CHECK(first.size() == 4);
    CHECK(first.front().timestamp_ns == 1);
    CHECK(first.back().timestamp_ns == 4);
    CHECK(consumer.consume(3));

    for (std::uint64_t value = 5; value <= 7; ++value)
    {
        CHECK(producer.try_push({.timestamp_ns = value}) == push_result_t::published);
    }

    auto wrapped_tail = consumer.readable();
    CHECK(wrapped_tail.size() == 1);
    CHECK(wrapped_tail[0].timestamp_ns == 4);
    CHECK(consumer.consume(1));

    auto wrapped_head = consumer.readable();
    CHECK(wrapped_head.size() == 3);
    CHECK(wrapped_head[0].timestamp_ns == 5);
    CHECK(wrapped_head[1].timestamp_ns == 6);
    CHECK(wrapped_head[2].timestamp_ns == 7);
    CHECK(consumer.consume(wrapped_head.size()));
    CHECK(consumer.empty());
    CHECK(!consumer.corrupt());

    return true;
}

auto test_threaded_spsc() -> bool
{
    constexpr auto sample_count = std::uint64_t{250'000};
    constexpr auto checksum_mask = std::uint64_t{0x9E37'79B9'7F4A'7C15};

    aliased_mapping_t<velocity_sample_t> mapping{1024};
    CHECK(mapping.mappings_are_distinct());
    std::atomic<bool> producer_failed{false};

    std::thread producer_thread{[&] {
        producer_t<velocity_sample_t> producer{mapping.writer_view()};

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

    consumer_t<velocity_sample_t> consumer{mapping.reader_view()};
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

    CHECK(!producer_failed.load(std::memory_order_relaxed));
    CHECK(!data_failed);
    CHECK(!consumer.corrupt());
    CHECK(consumer.empty());
    return true;
}

auto test_epoll_shaped_integration() -> bool
{
    constexpr auto sample_count = std::uint64_t{32};

    owned_mapping_t<raw_displacement_t> mapping{64};
    eventfd_epoll_channel_t channel;

    reader_t<raw_displacement_t, std_atomic_access_t, epoll_waiter_t> reader{
        mapping.view(),
        channel.waiter(),
    };

    std::thread producer_thread{[&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});

        writer_t<raw_displacement_t, std_atomic_access_t, eventfd_notifier_t> writer{
            mapping.view(),
            channel.notifier(),
        };

        for (std::uint64_t sequence = 1; sequence <= sample_count; ++sequence)
        {
            auto const result = writer.try_push({
                .timestamp_ns = sequence,
                .x = static_cast<std::int64_t>(sequence),
                .y = -static_cast<std::int64_t>(sequence),
            });

            if (result != push_result_t::published) std::abort();
        }
    }};

    CHECK(reader.wait_until_readable(2'000));

    auto expected = std::uint64_t{1};
    while (expected <= sample_count)
    {
        auto const batch = reader.readable();
        if (batch.empty())
        {
            CHECK(reader.wait_until_readable(2'000));
            continue;
        }

        for (auto const& sample : batch)
        {
            CHECK(sample.timestamp_ns == expected);
            CHECK(sample.x == static_cast<std::int64_t>(expected));
            CHECK(sample.y == -static_cast<std::int64_t>(expected));
            ++expected;
        }

        CHECK(reader.consume(batch.size()));
    }

    producer_thread.join();
    CHECK(!reader.corrupt());
    return true;
}

using test_function_t = auto (*)() -> bool;

struct test_case_t
{
    char const* name;
    test_function_t function;
};

} // namespace

int main()
{
    auto const tests = std::array{
        test_case_t{"view validation", &test_view_validation},
        test_case_t{"atomic semantics", &test_atomic_semantics},
        test_case_t{"wraparound and drop-new", &test_wraparound_and_drop_new},
        test_case_t{"threaded SPSC", &test_threaded_spsc},
        test_case_t{"epoll-shaped integration", &test_epoll_shaped_integration},
    };

    for (auto const& test : tests)
    {
        std::printf("[ RUN      ] %s\n", test.name);
        if (!test.function())
        {
            std::printf("[  FAILED  ] %s\n", test.name);
            return EXIT_FAILURE;
        }
        std::printf("[       OK ] %s\n", test.name);
    }

    std::printf("[  PASSED  ] %zu tests\n", tests.size());
    return EXIT_SUCCESS;
}
