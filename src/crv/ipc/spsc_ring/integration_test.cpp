// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief Single-file proof of concept for a generic zero-copy SPSC stream shared across an address-space boundary.
///
/// The shared ring is an operational bridge over a validated ABI mapping. The read and write algorithms depend only
/// on its role-specific behavior, so isolated tests can substitute narrow delegates without exposing shared-memory
/// layout or atomic implementation details.
///
/// Creation is asymmetric and factory-owned:
///
///   reader_factory.create(capacity)
///       -> control session creates one driver-side writer
///       -> driver-side factory allocates and initializes the shared ring
///       -> userspace maps the exported allocation
///       -> userspace factory validates the mapping and returns an owning reader
///
/// The generic component knows only element_t and an opaque leaf-supplied contract id. It does not enumerate record
/// formats. Allocation, layout interpretation, atomic access, notification, control transport, mapping, and waiting
/// are injected values.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <atomic>
#include <bit>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace crv::ipc::spsc_stream {

using sequence_t = std::uint64_t;

inline constexpr auto control_alignment = std::size_t{64};
inline constexpr auto sequence_alignment = std::size_t{8};
inline constexpr auto magic = std::uint32_t{0x4352'5652}; // "CRVR"
inline constexpr auto abi_major = std::uint16_t{1};
inline constexpr auto abi_minor = std::uint16_t{0};

struct stream_contract_id_t
{
    std::uint64_t value{};

    constexpr auto operator==(stream_contract_id_t const&) const noexcept -> bool = default;
};

template <typename element_t>
concept record_type = std::is_standard_layout_v<element_t> && std::is_trivially_copyable_v<element_t>
    && std::is_nothrow_default_constructible_v<element_t> && std::is_trivially_destructible_v<element_t>;

namespace abi {

struct alignas(sequence_alignment) sequence_storage_t
{
    sequence_t value{};
};

struct alignas(control_alignment) description_t
{
    std::uint32_t magic_value{};
    std::uint16_t abi_major_value{};
    std::uint16_t abi_minor_value{};

    std::uint64_t contract_id{};
    std::uint32_t element_size{};
    std::uint32_t element_alignment{};

    sequence_t capacity{};
    sequence_t data_offset{};
    sequence_t mapping_bytes{};

    std::byte reserved[16]{};
};

struct alignas(control_alignment) writer_state_t
{
    sequence_storage_t sequence{};
    sequence_storage_t dropped{};
    sequence_storage_t faults{};
    std::byte reserved[40]{};
};

struct alignas(control_alignment) reader_state_t
{
    sequence_storage_t sequence{};
    std::byte reserved[56]{};
};

struct alignas(control_alignment) header_t
{
    description_t description{};
    writer_state_t writer{};
    reader_state_t reader{};
};

static_assert(sizeof(sequence_storage_t) == 8);
static_assert(alignof(sequence_storage_t) == sequence_alignment);
static_assert(sizeof(description_t) == 64);
static_assert(sizeof(writer_state_t) == 64);
static_assert(sizeof(reader_state_t) == 64);
static_assert(sizeof(header_t) == 192);
static_assert(alignof(header_t) == control_alignment);
static_assert(offsetof(header_t, writer) == 64);
static_assert(offsetof(header_t, reader) == 128);
static_assert(std::is_standard_layout_v<header_t>);
static_assert(std::is_trivially_copyable_v<header_t>);

} // namespace abi

struct allocation_request_t
{
    std::size_t bytes{};
    std::size_t alignment{};
};

enum class stream_error_t : std::uint8_t
{
    null_mapping,
    misaligned_header,
    mapping_too_small,
    invalid_capacity,
    size_overflow,
    bad_magic,
    unsupported_abi,
    wrong_contract,
    wrong_element_size,
    wrong_element_alignment,
    wrong_data_offset,
    invalid_mapping_size,
    misaligned_elements,
    allocation_failed,
    map_failed,
    already_exists,
};

enum class push_result_t : std::uint8_t
{
    published,
    full,
    invalid_reader_sequence,
};

template <typename access_t>
concept atomic_access = requires(access_t& access, abi::sequence_storage_t& storage, sequence_t value) {
    { access.load_relaxed(storage) } noexcept -> std::same_as<sequence_t>;
    { access.load_acquire(storage) } noexcept -> std::same_as<sequence_t>;
    { access.store_relaxed(storage, value) } noexcept -> std::same_as<void>;
    { access.store_release(storage, value) } noexcept -> std::same_as<void>;
};

class std_atomic_access_t
{
private:
    using reference_t = std::atomic_ref<sequence_t>;

    static_assert(reference_t::is_always_lock_free);
    static_assert(sequence_alignment >= reference_t::required_alignment);

    static auto reference(abi::sequence_storage_t& storage) noexcept -> reference_t
    {
        return reference_t{storage.value};
    }

public:
    auto load_relaxed(abi::sequence_storage_t& storage) const noexcept -> sequence_t
    {
        return reference(storage).load(std::memory_order_relaxed);
    }

    auto load_acquire(abi::sequence_storage_t& storage) const noexcept -> sequence_t
    {
        return reference(storage).load(std::memory_order_acquire);
    }

    void store_relaxed(abi::sequence_storage_t& storage, sequence_t value) const noexcept
    {
        reference(storage).store(value, std::memory_order_relaxed);
    }

    void store_release(abi::sequence_storage_t& storage, sequence_t value) const noexcept
    {
        reference(storage).store(value, std::memory_order_release);
    }
};

static_assert(atomic_access<std_atomic_access_t>);

template <typename notifier_t>
concept notifier = requires(notifier_t& value) {
    { value.notify() } noexcept -> std::same_as<void>;
};

struct null_notifier_t
{
    void notify() const noexcept {}
};

template <typename waiter_t>
concept waiter = requires(waiter_t& value, int timeout_ms) {
    { value.wait(timeout_ms) } noexcept -> std::same_as<bool>;
};

struct null_waiter_t
{
    auto wait(int) const noexcept -> bool { return false; }
};

template <record_type element_t, atomic_access atomic_access_t> class ring_t
{
public:
    using element_type = element_t;

    ring_t(abi::header_t& header, element_t* elements, sequence_t capacity, atomic_access_t atomic_access) noexcept
        : header_{&header}, elements_{elements}, capacity_{capacity}, atomic_access_{std::move(atomic_access)}
    {}

    constexpr auto capacity() const noexcept -> sequence_t { return capacity_; }
    constexpr auto mask() const noexcept -> sequence_t { return capacity_ - 1; }

    auto writer_sequence_relaxed() noexcept -> sequence_t
    {
        return atomic_access_.load_relaxed(header_->writer.sequence);
    }

    auto writer_sequence_acquire() noexcept -> sequence_t
    {
        return atomic_access_.load_acquire(header_->writer.sequence);
    }

    auto reader_sequence_relaxed() noexcept -> sequence_t
    {
        return atomic_access_.load_relaxed(header_->reader.sequence);
    }

    auto reader_sequence_acquire() noexcept -> sequence_t
    {
        return atomic_access_.load_acquire(header_->reader.sequence);
    }

    void write(sequence_t sequence, element_t const& element) noexcept { elements_[index(sequence)] = element; }

    auto read(sequence_t sequence) noexcept -> element_t const& { return elements_[index(sequence)]; }

    void publish_writer_sequence(sequence_t sequence) noexcept
    {
        atomic_access_.store_release(header_->writer.sequence, sequence);
    }

    void publish_reader_sequence(sequence_t sequence) noexcept
    {
        atomic_access_.store_release(header_->reader.sequence, sequence);
    }

    auto dropped_relaxed() noexcept -> sequence_t { return atomic_access_.load_relaxed(header_->writer.dropped); }

    void store_dropped_relaxed(sequence_t value) noexcept
    {
        atomic_access_.store_relaxed(header_->writer.dropped, value);
    }

    auto faults_relaxed() noexcept -> sequence_t { return atomic_access_.load_relaxed(header_->writer.faults); }

    void store_faults_relaxed(sequence_t value) noexcept
    {
        atomic_access_.store_relaxed(header_->writer.faults, value);
    }

    auto has_pending() noexcept -> bool
    {
        auto const writer = writer_sequence_acquire();
        auto const reader = reader_sequence_acquire();
        return writer != reader;
    }

private:
    constexpr auto index(sequence_t sequence) const noexcept -> std::size_t
    {
        return static_cast<std::size_t>(sequence & mask());
    }

    abi::header_t* header_;
    element_t* elements_;
    sequence_t capacity_;
    [[no_unique_address]] atomic_access_t atomic_access_;
};

template <record_type element_t> class ring_layout_t
{
public:
    static_assert(sizeof(element_t) <= std::numeric_limits<std::uint32_t>::max());
    static_assert(alignof(element_t) <= std::numeric_limits<std::uint32_t>::max());

    template <atomic_access atomic_access_t> using ring_type = ring_t<element_t, atomic_access_t>;

    explicit constexpr ring_layout_t(stream_contract_id_t contract_id) noexcept : contract_id_{contract_id} {}

    constexpr auto allocation_for(sequence_t capacity) const noexcept
        -> std::expected<allocation_request_t, stream_error_t>
    {
        if (!std::has_single_bit(capacity)) { return std::unexpected{stream_error_t::invalid_capacity}; }

        constexpr auto offset = data_offset();
        constexpr auto max_size = std::numeric_limits<std::size_t>::max();

        if (capacity > static_cast<sequence_t>((max_size - offset) / sizeof(element_t)))
        {
            return std::unexpected{stream_error_t::size_overflow};
        }

        return allocation_request_t{
            .bytes = offset + static_cast<std::size_t>(capacity) * sizeof(element_t),
            .alignment = allocation_alignment(),
        };
    }

    template <atomic_access atomic_access_t>
    auto initialize(std::span<std::byte> storage, sequence_t capacity, atomic_access_t atomic_access) const noexcept
        -> std::expected<ring_type<atomic_access_t>, stream_error_t>
    {
        auto const request = allocation_for(capacity);
        if (!request.has_value()) return std::unexpected{request.error()};

        if (auto const error = validate_storage(storage, request->bytes); error.has_value())
        {
            return std::unexpected{*error};
        }

        auto* header = ::new (storage.data()) abi::header_t{};
        header->description.magic_value = magic;
        header->description.abi_major_value = abi_major;
        header->description.abi_minor_value = abi_minor;
        header->description.contract_id = contract_id_.value;
        header->description.element_size = sizeof(element_t);
        header->description.element_alignment = alignof(element_t);
        header->description.capacity = capacity;
        header->description.data_offset = data_offset();
        header->description.mapping_bytes = storage.size();

        auto* elements = reinterpret_cast<element_t*>(storage.data() + data_offset());
        for (sequence_t index = 0; index != capacity; ++index)
        {
            std::construct_at(elements + static_cast<std::size_t>(index));
        }

        return ring_type<atomic_access_t>{*header, elements, capacity, std::move(atomic_access)};
    }

    template <atomic_access atomic_access_t>
    auto bind(std::span<std::byte> storage, atomic_access_t atomic_access) const noexcept
        -> std::expected<ring_type<atomic_access_t>, stream_error_t>
    {
        if (storage.data() == nullptr) return std::unexpected{stream_error_t::null_mapping};
        if (reinterpret_cast<std::uintptr_t>(storage.data()) % alignof(abi::header_t) != 0)
        {
            return std::unexpected{stream_error_t::misaligned_header};
        }
        if (storage.size() < sizeof(abi::header_t)) { return std::unexpected{stream_error_t::mapping_too_small}; }

        auto* header = std::start_lifetime_as<abi::header_t>(storage.data());
        auto const& description = header->description;

        if (description.magic_value != magic) return std::unexpected{stream_error_t::bad_magic};
        if (description.abi_major_value != abi_major) { return std::unexpected{stream_error_t::unsupported_abi}; }
        if (description.contract_id != contract_id_.value) { return std::unexpected{stream_error_t::wrong_contract}; }
        if (description.element_size != sizeof(element_t))
        {
            return std::unexpected{stream_error_t::wrong_element_size};
        }
        if (description.element_alignment != alignof(element_t))
        {
            return std::unexpected{stream_error_t::wrong_element_alignment};
        }
        if (!std::has_single_bit(description.capacity)) { return std::unexpected{stream_error_t::invalid_capacity}; }
        if (description.data_offset != data_offset()) { return std::unexpected{stream_error_t::wrong_data_offset}; }

        auto const request = allocation_for(description.capacity);
        if (!request.has_value()) return std::unexpected{request.error()};

        if (description.mapping_bytes < request->bytes || description.mapping_bytes > storage.size())
        {
            return std::unexpected{stream_error_t::invalid_mapping_size};
        }

        auto* element_bytes = storage.data() + description.data_offset;
        if (reinterpret_cast<std::uintptr_t>(element_bytes) % alignof(element_t) != 0)
        {
            return std::unexpected{stream_error_t::misaligned_elements};
        }
        auto* elements
            = std::start_lifetime_as_array<element_t>(element_bytes, static_cast<std::size_t>(description.capacity));

        return ring_type<atomic_access_t>{
            *header,
            elements,
            description.capacity,
            std::move(atomic_access),
        };
    }

private:
    static constexpr auto align_up(std::size_t value, std::size_t alignment) noexcept -> std::size_t
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    static constexpr auto data_offset() noexcept -> std::size_t
    {
        return align_up(sizeof(abi::header_t), alignof(element_t));
    }

    static constexpr auto allocation_alignment() noexcept -> std::size_t
    {
        return control_alignment < alignof(element_t) ? alignof(element_t) : control_alignment;
    }

    static auto validate_storage(std::span<std::byte> storage, std::size_t required) noexcept
        -> std::optional<stream_error_t>
    {
        if (storage.data() == nullptr) return stream_error_t::null_mapping;
        if (reinterpret_cast<std::uintptr_t>(storage.data()) % alignof(abi::header_t) != 0)
        {
            return stream_error_t::misaligned_header;
        }
        if (storage.size() < required) return stream_error_t::mapping_too_small;

        auto* elements = storage.data() + data_offset();
        if (reinterpret_cast<std::uintptr_t>(elements) % alignof(element_t) != 0)
        {
            return stream_error_t::misaligned_elements;
        }

        return std::nullopt;
    }

    stream_contract_id_t contract_id_;
};

template <typename ring_t, typename element_t>
concept writer_ring = record_type<element_t> && requires(ring_t& ring, sequence_t sequence, element_t const& element) {
    { ring.capacity() } noexcept -> std::same_as<sequence_t>;
    { ring.writer_sequence_relaxed() } noexcept -> std::same_as<sequence_t>;
    { ring.reader_sequence_acquire() } noexcept -> std::same_as<sequence_t>;
    { ring.write(sequence, element) } noexcept -> std::same_as<void>;
    { ring.publish_writer_sequence(sequence) } noexcept -> std::same_as<void>;
    { ring.dropped_relaxed() } noexcept -> std::same_as<sequence_t>;
    { ring.store_dropped_relaxed(sequence) } noexcept -> std::same_as<void>;
    { ring.faults_relaxed() } noexcept -> std::same_as<sequence_t>;
    { ring.store_faults_relaxed(sequence) } noexcept -> std::same_as<void>;
    { ring.has_pending() } noexcept -> std::same_as<bool>;
};

template <typename ring_t, typename element_t>
concept reader_ring = record_type<element_t> && requires(ring_t& ring, sequence_t sequence) {
    { ring.capacity() } noexcept -> std::same_as<sequence_t>;
    { ring.reader_sequence_relaxed() } noexcept -> std::same_as<sequence_t>;
    { ring.writer_sequence_acquire() } noexcept -> std::same_as<sequence_t>;
    { ring.read(sequence) } noexcept -> std::same_as<element_t const&>;
    { ring.publish_reader_sequence(sequence) } noexcept -> std::same_as<void>;
    { ring.dropped_relaxed() } noexcept -> std::same_as<sequence_t>;
    { ring.faults_relaxed() } noexcept -> std::same_as<sequence_t>;
};

template <record_type element_t, typename ring_t, notifier notifier_t>
    requires writer_ring<ring_t, element_t>
class ring_writer_t
{
public:
    using element_type = element_t;

    ring_writer_t(ring_t ring, notifier_t notifier_value) noexcept
        : ring_{std::move(ring)}, notifier_{std::move(notifier_value)}, head_{ring_.writer_sequence_relaxed()},
          dropped_{ring_.dropped_relaxed()}, faults_{ring_.faults_relaxed()}
    {}

    auto try_push(element_t const& element) noexcept -> push_result_t
    {
        if (faulted_) return push_result_t::invalid_reader_sequence;

        auto const tail = ring_.reader_sequence_acquire();
        auto const occupied = head_ - tail;
        auto const capacity = ring_.capacity();

        if (occupied > capacity)
        {
            ++faults_;
            ring_.store_faults_relaxed(faults_);
            faulted_ = true;
            return push_result_t::invalid_reader_sequence;
        }

        if (occupied == capacity)
        {
            ++dropped_;
            ring_.store_dropped_relaxed(dropped_);
            return push_result_t::full;
        }

        ring_.write(head_, element);
        ++head_;
        ring_.publish_writer_sequence(head_);
        notifier_.notify();
        return push_result_t::published;
    }

    auto has_pending() noexcept -> bool { return ring_.has_pending(); }
    constexpr auto faulted() const noexcept -> bool { return faulted_; }

private:
    ring_t ring_;
    [[no_unique_address]] notifier_t notifier_;
    sequence_t head_{};
    sequence_t dropped_{};
    sequence_t faults_{};
    bool faulted_{};
};

template <record_type element_t, typename ring_t>
    requires reader_ring<ring_t, element_t>
class ring_reader_t
{
public:
    explicit ring_reader_t(ring_t ring) noexcept : ring_{std::move(ring)}, tail_{ring_.reader_sequence_relaxed()} {}

    auto readable() noexcept -> std::span<element_t const>
    {
        if (corrupt_) return {};
        if (leased_ != 0) return {&ring_.read(tail_), leased_};

        auto const head = ring_.writer_sequence_acquire();
        auto const available = head - tail_;
        auto const capacity = ring_.capacity();

        if (available > capacity)
        {
            corrupt_ = true;
            return {};
        }

        auto const index = tail_ & (capacity - 1);
        auto const until_wrap = capacity - index;
        auto const contiguous = available < until_wrap ? available : until_wrap;

        leased_ = static_cast<std::size_t>(contiguous);
        return {&ring_.read(tail_), leased_};
    }

    auto consume(std::size_t count) noexcept -> bool
    {
        if (corrupt_) return false;
        if (count > leased_)
        {
            corrupt_ = true;
            return false;
        }
        if (count == 0) return true;

        tail_ += static_cast<sequence_t>(count);
        leased_ -= count;
        ring_.publish_reader_sequence(tail_);
        return true;
    }

    auto empty() noexcept -> bool
    {
        if (corrupt_) return true;
        if (leased_ != 0) return false;

        auto const head = ring_.writer_sequence_acquire();
        auto const available = head - tail_;
        auto const capacity = ring_.capacity();

        if (available > capacity)
        {
            corrupt_ = true;
            return true;
        }

        return available == 0;
    }

    auto dropped() noexcept -> sequence_t { return ring_.dropped_relaxed(); }
    auto writer_faults() noexcept -> sequence_t { return ring_.faults_relaxed(); }
    constexpr auto corrupt() const noexcept -> bool { return corrupt_; }

private:
    ring_t ring_;
    sequence_t tail_{};
    std::size_t leased_{};
    bool corrupt_{};
};

template <typename allocation_t, typename algorithm_t> class writer_t
{
public:
    using allocation_type = allocation_t;
    using algorithm_type = algorithm_t;
    using ticket_type = typename allocation_t::ticket_type;

    writer_t(allocation_t allocation, algorithm_t algorithm) noexcept
        : allocation_{std::move(allocation)}, algorithm_{std::move(algorithm)}
    {}

    writer_t(writer_t const&) = delete;
    auto operator=(writer_t const&) -> writer_t& = delete;
    writer_t(writer_t&&) noexcept = default;
    auto operator=(writer_t&&) noexcept -> writer_t& = delete;

    auto ticket() const noexcept -> ticket_type { return allocation_.ticket(); }

    auto try_push(typename algorithm_t::element_type const& element) noexcept { return algorithm_.try_push(element); }

    auto has_pending() noexcept -> bool { return algorithm_.has_pending(); }
    auto faulted() const noexcept -> bool { return algorithm_.faulted(); }

private:
    allocation_t allocation_;
    algorithm_t algorithm_;
};

template <typename session_t, typename region_t, typename algorithm_t, waiter waiter_t> class reader_t
{
public:
    reader_t(session_t session, region_t region, algorithm_t algorithm, waiter_t waiter_value) noexcept
        : session_{std::move(session)}, region_{std::move(region)}, algorithm_{std::move(algorithm)},
          waiter_{std::move(waiter_value)}
    {}

    reader_t(reader_t const&) = delete;
    auto operator=(reader_t const&) -> reader_t& = delete;
    reader_t(reader_t&&) noexcept = default;
    auto operator=(reader_t&&) noexcept -> reader_t& = delete;

    auto readable() noexcept { return algorithm_.readable(); }
    auto consume(std::size_t count) noexcept { return algorithm_.consume(count); }
    auto empty() noexcept { return algorithm_.empty(); }
    auto dropped() noexcept { return algorithm_.dropped(); }
    auto writer_faults() noexcept { return algorithm_.writer_faults(); }
    auto corrupt() const noexcept { return algorithm_.corrupt(); }

    auto wait_until_readable(int timeout_ms = -1) noexcept -> bool
    {
        if (timeout_ms < 0)
        {
            while (algorithm_.empty())
            {
                if (algorithm_.corrupt()) return false;
                if (!waiter_.wait(-1)) return false;
            }

            return true;
        }

        using clock_t = std::chrono::steady_clock;
        auto const deadline = clock_t::now() + std::chrono::milliseconds{timeout_ms};

        while (algorithm_.empty())
        {
            if (algorithm_.corrupt()) return false;

            auto const now = clock_t::now();
            if (now >= deadline) return false;

            auto const remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - now);

            auto const remaining_ms = remaining.count() > std::numeric_limits<int>::max()
                ? std::numeric_limits<int>::max()
                : static_cast<int>(remaining.count());

            if (!waiter_.wait(remaining_ms)) return false;
        }

        return true;
    }

    auto native_handle() const noexcept
        requires requires(session_t const& session) { session.native_handle(); }
    {
        return session_.native_handle();
    }

private:
    session_t session_;
    region_t region_;
    algorithm_t algorithm_;
    [[no_unique_address]] waiter_t waiter_;
};

template <record_type element_t, typename allocation_provider_t, typename layout_t, atomic_access atomic_access_t,
    notifier notifier_t>
class writer_factory_t
{
    static_assert(std::copy_constructible<atomic_access_t>);
    static_assert(std::copy_constructible<notifier_t>);

public:
    using allocation_type = typename allocation_provider_t::allocation_type;
    using ring_type = typename layout_t::template ring_type<atomic_access_t>;
    using algorithm_type = ring_writer_t<element_t, ring_type, notifier_t>;
    using writer_type = writer_t<allocation_type, algorithm_type>;
    using ticket_type = typename writer_type::ticket_type;

    writer_factory_t(allocation_provider_t allocation_provider, layout_t layout, atomic_access_t atomic_access,
        notifier_t notifier_value) noexcept
        : allocation_provider_{std::move(allocation_provider)}, layout_{std::move(layout)},
          atomic_access_{std::move(atomic_access)}, notifier_{std::move(notifier_value)}
    {}

    auto create(sequence_t capacity) -> std::expected<writer_type, stream_error_t>
    {
        auto const request = layout_.allocation_for(capacity);
        if (!request.has_value()) return std::unexpected{request.error()};

        auto allocation = allocation_provider_.allocate(*request);
        if (!allocation.has_value()) return std::unexpected{allocation.error()};

        auto ring = layout_.initialize(allocation->bytes(), capacity, atomic_access_);
        if (!ring.has_value()) return std::unexpected{ring.error()};

        auto algorithm = algorithm_type{std::move(*ring), notifier_};
        return writer_type{std::move(*allocation), std::move(algorithm)};
    }

private:
    [[no_unique_address]] allocation_provider_t allocation_provider_;
    [[no_unique_address]] layout_t layout_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    [[no_unique_address]] notifier_t notifier_;
};

template <typename writer_factory_t> class stream_service_t
{
public:
    using factory_type = writer_factory_t;
    using writer_type = typename writer_factory_t::writer_type;
    using ticket_type = typename writer_factory_t::ticket_type;

    explicit stream_service_t(writer_factory_t factory) noexcept : factory_{std::move(factory)} {}

    auto create(sequence_t capacity) -> std::expected<ticket_type, stream_error_t>
    {
        if (writer_.has_value()) return std::unexpected{stream_error_t::already_exists};

        auto writer = factory_.create(capacity);
        if (!writer.has_value()) return std::unexpected{writer.error()};

        auto ticket = writer->ticket();
        writer_.emplace(std::move(*writer));
        return ticket;
    }

    void release() noexcept { writer_.reset(); }

    auto writer() noexcept -> writer_type* { return writer_.has_value() ? &*writer_ : nullptr; }

    auto readable() noexcept -> bool { return writer_.has_value() && writer_->has_pending(); }

    constexpr auto has_writer() const noexcept -> bool { return writer_.has_value(); }

private:
    writer_factory_t factory_;
    std::optional<writer_type> writer_;
};

template <record_type element_t, typename control_t, typename mapper_t, typename layout_t,
    atomic_access atomic_access_t, waiter waiter_t>
class reader_factory_t
{
    static_assert(std::copy_constructible<atomic_access_t>);
    static_assert(std::copy_constructible<waiter_t>);

public:
    using session_type = typename control_t::session_type;
    using region_type = typename mapper_t::region_type;
    using ring_type = typename layout_t::template ring_type<atomic_access_t>;
    using algorithm_type = ring_reader_t<element_t, ring_type>;
    using reader_type = reader_t<session_type, region_type, algorithm_type, waiter_t>;

    reader_factory_t(control_t control, mapper_t mapper, layout_t layout, atomic_access_t atomic_access,
        waiter_t waiter_value) noexcept
        : control_{std::move(control)}, mapper_{std::move(mapper)}, layout_{std::move(layout)},
          atomic_access_{std::move(atomic_access)}, waiter_{std::move(waiter_value)}
    {}

    auto create(sequence_t capacity) -> std::expected<reader_type, stream_error_t>
    {
        auto session = control_.create_stream(capacity);
        if (!session.has_value()) return std::unexpected{session.error()};

        auto region = mapper_.map(session->ticket());
        if (!region.has_value()) return std::unexpected{region.error()};

        auto ring = layout_.bind(region->bytes(), atomic_access_);
        if (!ring.has_value()) return std::unexpected{ring.error()};

        auto algorithm = algorithm_type{std::move(*ring)};
        return reader_type{
            std::move(*session),
            std::move(*region),
            std::move(algorithm),
            waiter_,
        };
    }

private:
    [[no_unique_address]] control_t control_;
    [[no_unique_address]] mapper_t mapper_;
    [[no_unique_address]] layout_t layout_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    [[no_unique_address]] waiter_t waiter_;
};

} // namespace crv::ipc::spsc_stream

//
// Host-side stand-ins for allocation, mapping, notification, and control transport.
//

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <thread>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <unistd.h>

namespace crv::ipc::spsc_stream::test_support {

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

struct host_mapping_ticket_t
{
    int fd{-1};
    std::size_t bytes{};
};

class memfd_allocation_t
{
public:
    using ticket_type = host_mapping_ticket_t;

    memfd_allocation_t() noexcept = default;

    memfd_allocation_t(unique_fd_t fd, void* memory, std::size_t bytes) noexcept
        : fd_{std::move(fd)}, memory_{memory}, bytes_{bytes}
    {}

    memfd_allocation_t(memfd_allocation_t const&) = delete;
    auto operator=(memfd_allocation_t const&) -> memfd_allocation_t& = delete;

    memfd_allocation_t(memfd_allocation_t&& other) noexcept
        : fd_{std::move(other.fd_)}, memory_{std::exchange(other.memory_, MAP_FAILED)},
          bytes_{std::exchange(other.bytes_, 0)}
    {}

    auto operator=(memfd_allocation_t&& other) noexcept -> memfd_allocation_t&
    {
        if (this == &other) return *this;
        reset();
        fd_ = std::move(other.fd_);
        memory_ = std::exchange(other.memory_, MAP_FAILED);
        bytes_ = std::exchange(other.bytes_, 0);
        return *this;
    }

    ~memfd_allocation_t() { reset(); }

    auto bytes() noexcept -> std::span<std::byte> { return {static_cast<std::byte*>(memory_), bytes_}; }

    auto ticket() const noexcept -> ticket_type { return {.fd = fd_.get(), .bytes = bytes_}; }

private:
    void reset() noexcept
    {
        if (memory_ != MAP_FAILED) ::munmap(memory_, bytes_);
        memory_ = MAP_FAILED;
        bytes_ = 0;
    }

    unique_fd_t fd_;
    void* memory_{MAP_FAILED};
    std::size_t bytes_{};
};

class memfd_allocation_provider_t
{
public:
    using allocation_type = memfd_allocation_t;

    auto allocate(allocation_request_t request) const -> std::expected<allocation_type, stream_error_t>
    {
        auto fd = unique_fd_t{::memfd_create("crv-spsc-stream-poc", MFD_CLOEXEC)};
        if (!fd) return std::unexpected{stream_error_t::allocation_failed};

        if (::ftruncate(fd.get(), static_cast<off_t>(request.bytes)) != 0)
        {
            return std::unexpected{stream_error_t::allocation_failed};
        }

        auto* memory = ::mmap(nullptr, request.bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
        if (memory == MAP_FAILED) return std::unexpected{stream_error_t::allocation_failed};

        if (reinterpret_cast<std::uintptr_t>(memory) % request.alignment != 0)
        {
            ::munmap(memory, request.bytes);
            return std::unexpected{stream_error_t::allocation_failed};
        }

        return allocation_type{std::move(fd), memory, request.bytes};
    }
};

class mapped_region_t
{
public:
    mapped_region_t() noexcept = default;
    mapped_region_t(void* memory, std::size_t bytes) noexcept : memory_{memory}, bytes_{bytes} {}

    mapped_region_t(mapped_region_t const&) = delete;
    auto operator=(mapped_region_t const&) -> mapped_region_t& = delete;

    mapped_region_t(mapped_region_t&& other) noexcept
        : memory_{std::exchange(other.memory_, MAP_FAILED)}, bytes_{std::exchange(other.bytes_, 0)}
    {}

    auto operator=(mapped_region_t&& other) noexcept -> mapped_region_t&
    {
        if (this == &other) return *this;
        reset();
        memory_ = std::exchange(other.memory_, MAP_FAILED);
        bytes_ = std::exchange(other.bytes_, 0);
        return *this;
    }

    ~mapped_region_t() { reset(); }

    auto bytes() noexcept -> std::span<std::byte> { return {static_cast<std::byte*>(memory_), bytes_}; }

private:
    void reset() noexcept
    {
        if (memory_ != MAP_FAILED) ::munmap(memory_, bytes_);
        memory_ = MAP_FAILED;
        bytes_ = 0;
    }

    void* memory_{MAP_FAILED};
    std::size_t bytes_{};
};

class memfd_mapper_t
{
public:
    using region_type = mapped_region_t;

    auto map(host_mapping_ticket_t ticket) const -> std::expected<region_type, stream_error_t>
    {
        auto* memory = ::mmap(nullptr, ticket.bytes, PROT_READ | PROT_WRITE, MAP_SHARED, ticket.fd, 0);
        if (memory == MAP_FAILED) return std::unexpected{stream_error_t::map_failed};
        return region_type{memory, ticket.bytes};
    }
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
        return {.epoll_fd = epoll_fd_.get(), .signal_fd = signal_fd_.get()};
    }

private:
    unique_fd_t signal_fd_;
    unique_fd_t epoll_fd_;
};

template <typename service_t> class in_process_session_t
{
public:
    using ticket_type = typename service_t::ticket_type;

    in_process_session_t() noexcept = default;

    in_process_session_t(service_t& service, ticket_type ticket) noexcept
        : service_{&service}, ticket_{std::move(ticket)}
    {}

    in_process_session_t(in_process_session_t const&) = delete;
    auto operator=(in_process_session_t const&) -> in_process_session_t& = delete;

    in_process_session_t(in_process_session_t&& other) noexcept
        : service_{std::exchange(other.service_, nullptr)}, ticket_{std::move(other.ticket_)}
    {}

    auto operator=(in_process_session_t&& other) noexcept -> in_process_session_t&
    {
        if (this == &other) return *this;
        reset();
        service_ = std::exchange(other.service_, nullptr);
        ticket_ = std::move(other.ticket_);
        return *this;
    }

    ~in_process_session_t() { reset(); }

    auto ticket() const noexcept -> ticket_type { return ticket_; }

private:
    void reset() noexcept
    {
        if (service_ != nullptr) service_->release();
        service_ = nullptr;
    }

    service_t* service_{};
    ticket_type ticket_{};
};

template <typename service_t> class in_process_control_t
{
public:
    using session_type = in_process_session_t<service_t>;

    explicit in_process_control_t(service_t& service) noexcept : service_{&service} {}

    auto create_stream(sequence_t capacity) -> std::expected<session_type, stream_error_t>
    {
        auto ticket = service_->create(capacity);
        if (!ticket.has_value()) return std::unexpected{ticket.error()};
        return session_type{*service_, std::move(*ticket)};
    }

private:
    service_t* service_;
};

} // namespace crv::ipc::spsc_stream::test_support

//
// Leaf stream definitions.
//

namespace crv::mouse::streams {

struct raw_displacement_t
{
    std::uint64_t timestamp_ns{};
    std::int64_t x{};
    std::int64_t y{};

    constexpr auto operator==(raw_displacement_t const&) const noexcept -> bool = default;
};

struct velocity_sample_t
{
    std::uint64_t timestamp_ns{};
    std::uint64_t velocity{};

    constexpr auto operator==(velocity_sample_t const&) const noexcept -> bool = default;
};

inline constexpr auto raw_displacement_contract_id
    = crv::ipc::spsc_stream::stream_contract_id_t{0x5241'572D'4449'5350}; // "RAW-DISP"
inline constexpr auto velocity_sample_contract_id
    = crv::ipc::spsc_stream::stream_contract_id_t{0x5645'4C4F'4349'5459}; // "VELOCITY"

using raw_displacement_layout_t = crv::ipc::spsc_stream::ring_layout_t<raw_displacement_t>;
using velocity_sample_layout_t = crv::ipc::spsc_stream::ring_layout_t<velocity_sample_t>;

static_assert(sizeof(raw_displacement_t) == 24);
static_assert(alignof(raw_displacement_t) == 8);
static_assert(sizeof(velocity_sample_t) == 16);
static_assert(alignof(velocity_sample_t) == 8);

} // namespace crv::mouse::streams

//
// GoogleTest/GoogleMock tests.
//

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace crv::mouse::streams {
namespace {

using namespace crv::ipc::spsc_stream;
using namespace crv::ipc::spsc_stream::test_support;
using namespace crv::mouse::streams;

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

class mock_atomic_access_t
{
public:
    MOCK_METHOD(sequence_t, load_relaxed, (abi::sequence_storage_t & storage), (const, noexcept));
    MOCK_METHOD(sequence_t, load_acquire, (abi::sequence_storage_t & storage), (const, noexcept));
    MOCK_METHOD(void, store_relaxed, (abi::sequence_storage_t & storage, sequence_t value), (const, noexcept));
    MOCK_METHOD(void, store_release, (abi::sequence_storage_t & storage, sequence_t value), (const, noexcept));
};

struct mock_atomic_access_delegate_t
{
    mock_atomic_access_t* mock{};

    auto load_relaxed(abi::sequence_storage_t& storage) const noexcept -> sequence_t
    {
        return mock->load_relaxed(storage);
    }

    auto load_acquire(abi::sequence_storage_t& storage) const noexcept -> sequence_t
    {
        return mock->load_acquire(storage);
    }

    void store_relaxed(abi::sequence_storage_t& storage, sequence_t value) const noexcept
    {
        mock->store_relaxed(storage, value);
    }

    void store_release(abi::sequence_storage_t& storage, sequence_t value) const noexcept
    {
        mock->store_release(storage, value);
    }
};

class mock_writer_ring_t
{
public:
    MOCK_METHOD(sequence_t, capacity, (), (const, noexcept));
    MOCK_METHOD(sequence_t, writer_sequence_relaxed, (), (noexcept));
    MOCK_METHOD(sequence_t, reader_sequence_acquire, (), (noexcept));
    MOCK_METHOD(void, write, (sequence_t sequence, raw_displacement_t const& element), (noexcept));
    MOCK_METHOD(void, publish_writer_sequence, (sequence_t sequence), (noexcept));
    MOCK_METHOD(sequence_t, dropped_relaxed, (), (noexcept));
    MOCK_METHOD(void, store_dropped_relaxed, (sequence_t value), (noexcept));
    MOCK_METHOD(sequence_t, faults_relaxed, (), (noexcept));
    MOCK_METHOD(void, store_faults_relaxed, (sequence_t value), (noexcept));
    MOCK_METHOD(bool, has_pending, (), (noexcept));
};

struct mock_writer_ring_delegate_t
{
    mock_writer_ring_t* mock{};

    auto capacity() const noexcept -> sequence_t { return mock->capacity(); }
    auto writer_sequence_relaxed() noexcept -> sequence_t { return mock->writer_sequence_relaxed(); }
    auto reader_sequence_acquire() noexcept -> sequence_t { return mock->reader_sequence_acquire(); }
    void write(sequence_t sequence, raw_displacement_t const& element) noexcept { mock->write(sequence, element); }
    void publish_writer_sequence(sequence_t sequence) noexcept { mock->publish_writer_sequence(sequence); }
    auto dropped_relaxed() noexcept -> sequence_t { return mock->dropped_relaxed(); }
    void store_dropped_relaxed(sequence_t value) noexcept { mock->store_dropped_relaxed(value); }
    auto faults_relaxed() noexcept -> sequence_t { return mock->faults_relaxed(); }
    void store_faults_relaxed(sequence_t value) noexcept { mock->store_faults_relaxed(value); }
    auto has_pending() noexcept -> bool { return mock->has_pending(); }
};

class mock_reader_ring_t
{
public:
    MOCK_METHOD(sequence_t, capacity, (), (const, noexcept));
    MOCK_METHOD(sequence_t, reader_sequence_relaxed, (), (noexcept));
    MOCK_METHOD(sequence_t, writer_sequence_acquire, (), (noexcept));
    MOCK_METHOD(raw_displacement_t const&, read, (sequence_t sequence), (noexcept));
    MOCK_METHOD(void, publish_reader_sequence, (sequence_t sequence), (noexcept));
    MOCK_METHOD(sequence_t, dropped_relaxed, (), (noexcept));
    MOCK_METHOD(sequence_t, faults_relaxed, (), (noexcept));
};

struct mock_reader_ring_delegate_t
{
    mock_reader_ring_t* mock{};

    auto capacity() const noexcept -> sequence_t { return mock->capacity(); }
    auto reader_sequence_relaxed() noexcept -> sequence_t { return mock->reader_sequence_relaxed(); }
    auto writer_sequence_acquire() noexcept -> sequence_t { return mock->writer_sequence_acquire(); }
    auto read(sequence_t sequence) noexcept -> raw_displacement_t const& { return mock->read(sequence); }
    auto publish_reader_sequence(sequence_t sequence) noexcept -> void { mock->publish_reader_sequence(sequence); }
    auto dropped_relaxed() noexcept -> sequence_t { return mock->dropped_relaxed(); }
    auto faults_relaxed() noexcept -> sequence_t { return mock->faults_relaxed(); }
};

class mock_notifier_t
{
public:
    MOCK_METHOD(void, notify, (), (const, noexcept));
};

struct mock_notifier_delegate_t
{
    mock_notifier_t* mock{};
    void notify() const noexcept { mock->notify(); }
};

static_assert(atomic_access<mock_atomic_access_delegate_t>);
static_assert(writer_ring<mock_writer_ring_delegate_t, raw_displacement_t>);
static_assert(reader_ring<mock_reader_ring_delegate_t, raw_displacement_t>);
static_assert(notifier<mock_notifier_delegate_t>);

void store_sequence(abi::sequence_storage_t& storage, sequence_t value) noexcept
{
    storage.value = value;
}

TEST(SpscStreamLayout, OwnsSizingInitializationValidationAndTyping)
{
    raw_displacement_layout_t raw_layout{raw_displacement_contract_id};
    velocity_sample_layout_t velocity_layout{velocity_sample_contract_id};

    auto const request = raw_layout.allocation_for(8);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->alignment, 64u);

    auto allocation = memfd_allocation_provider_t{}.allocate(*request);
    ASSERT_TRUE(allocation.has_value());

    auto initialized = raw_layout.initialize(allocation->bytes(), 8, std_atomic_access_t{});
    ASSERT_TRUE(initialized.has_value());
    EXPECT_EQ(initialized->capacity(), 8u);

    auto rebound = raw_layout.bind(allocation->bytes(), std_atomic_access_t{});
    ASSERT_TRUE(rebound.has_value());
    EXPECT_EQ(rebound->capacity(), 8u);

    auto wrong_contract = velocity_layout.bind(allocation->bytes(), std_atomic_access_t{});
    ASSERT_FALSE(wrong_contract.has_value());
    EXPECT_EQ(wrong_contract.error(), stream_error_t::wrong_contract);
}

TEST(SpscStreamRing, GivesSharedMemoryOperationsSemanticAtomicBoundaries)
{
    abi::header_t header{};
    raw_displacement_t elements[4]{};
    StrictMock<mock_atomic_access_t> atomic;

    auto ring = ring_t<raw_displacement_t, mock_atomic_access_delegate_t>{
        header,
        elements,
        4,
        mock_atomic_access_delegate_t{.mock = &atomic},
    };

    EXPECT_CALL(atomic, load_relaxed(Ref(header.writer.sequence))).WillOnce(Return(3));
    EXPECT_EQ(ring.writer_sequence_relaxed(), 3u);

    EXPECT_CALL(atomic, load_acquire(Ref(header.reader.sequence))).WillOnce(Return(2));
    EXPECT_EQ(ring.reader_sequence_acquire(), 2u);

    EXPECT_CALL(atomic, store_release(Ref(header.writer.sequence), 4)).WillOnce(Invoke(store_sequence));
    ring.publish_writer_sequence(4);
    EXPECT_EQ(header.writer.sequence.value, 4u);
}

TEST(SpscStreamAlgorithms, WriterUsesOnlyTheInjectedWriterRing)
{
    StrictMock<mock_writer_ring_t> ring_mock;
    StrictMock<mock_notifier_t> notifier_mock;

    {
        InSequence sequence;
        EXPECT_CALL(ring_mock, writer_sequence_relaxed()).WillOnce(Return(0));
        EXPECT_CALL(ring_mock, dropped_relaxed()).WillOnce(Return(0));
        EXPECT_CALL(ring_mock, faults_relaxed()).WillOnce(Return(0));
    }

    auto writer = ring_writer_t<raw_displacement_t, mock_writer_ring_delegate_t, mock_notifier_delegate_t>{
        mock_writer_ring_delegate_t{.mock = &ring_mock},
        mock_notifier_delegate_t{.mock = &notifier_mock},
    };

    raw_displacement_t const sample{.timestamp_ns = 1, .x = 2, .y = 3};

    {
        InSequence sequence;
        EXPECT_CALL(ring_mock, reader_sequence_acquire()).WillOnce(Return(0));
        EXPECT_CALL(ring_mock, capacity()).WillOnce(Return(4));
        EXPECT_CALL(ring_mock, write(0, Ref(sample)));
        EXPECT_CALL(ring_mock, publish_writer_sequence(1));
        EXPECT_CALL(notifier_mock, notify());
    }

    EXPECT_EQ(writer.try_push(sample), push_result_t::published);
}

TEST(SpscStreamAlgorithms, ReaderUsesOnlyTheInjectedReaderRing)
{
    StrictMock<mock_reader_ring_t> ring_mock;
    raw_displacement_t const sample{.timestamp_ns = 1, .x = 2, .y = 3};

    EXPECT_CALL(ring_mock, reader_sequence_relaxed()).WillOnce(Return(0));

    auto reader = ring_reader_t<raw_displacement_t, mock_reader_ring_delegate_t>{
        mock_reader_ring_delegate_t{.mock = &ring_mock},
    };

    {
        InSequence sequence;
        EXPECT_CALL(ring_mock, writer_sequence_acquire()).WillOnce(Return(1));
        EXPECT_CALL(ring_mock, capacity()).WillOnce(Return(4));
        EXPECT_CALL(ring_mock, read(0)).WillOnce(ReturnRef(sample));
    }

    auto const readable = reader.readable();
    ASSERT_EQ(readable.size(), 1u);
    EXPECT_EQ(readable.front(), sample);

    EXPECT_CALL(ring_mock, publish_reader_sequence(1));
    EXPECT_TRUE(reader.consume(1));
}

TEST(SpscStreamWriter, SeparatesCapacityDropsFromLatchedReaderSequenceFaults)
{
    raw_displacement_layout_t layout{raw_displacement_contract_id};
    auto const request = layout.allocation_for(4);
    ASSERT_TRUE(request.has_value());

    auto allocation = memfd_allocation_provider_t{}.allocate(*request);
    ASSERT_TRUE(allocation.has_value());

    auto ring = layout.initialize(allocation->bytes(), 4, std_atomic_access_t{});
    ASSERT_TRUE(ring.has_value());

    using ring_type = std::remove_cvref_t<decltype(*ring)>;
    auto writer = ring_writer_t<raw_displacement_t, ring_type, null_notifier_t>{
        std::move(*ring),
        null_notifier_t{},
    };

    for (std::uint64_t value = 1; value <= 4; ++value)
    {
        EXPECT_EQ(writer.try_push({.timestamp_ns = value}), push_result_t::published);
    }

    EXPECT_EQ(writer.try_push({.timestamp_ns = 5}), push_result_t::full);

    auto& header = *reinterpret_cast<abi::header_t*>(allocation->bytes().data());
    EXPECT_EQ(header.writer.dropped.value, 1u);
    EXPECT_EQ(header.writer.faults.value, 0u);

    header.reader.sequence.value = 10;

    EXPECT_EQ(writer.try_push({.timestamp_ns = 6}), push_result_t::invalid_reader_sequence);
    EXPECT_EQ(header.writer.dropped.value, 1u);
    EXPECT_EQ(header.writer.faults.value, 1u);

    EXPECT_EQ(writer.try_push({.timestamp_ns = 7}), push_result_t::invalid_reader_sequence);
    EXPECT_EQ(header.writer.faults.value, 1u);
    EXPECT_TRUE(writer.faulted());
}

TEST(SpscStreamFactory, ReaderCreateComposesTheCompleteLifecycle)
{
    constexpr auto sample_count = std::uint64_t{250'000};
    constexpr auto checksum_mask = std::uint64_t{0x9E37'79B9'7F4A'7C15};

    velocity_sample_layout_t layout{velocity_sample_contract_id};
    using writer_factory_type = writer_factory_t<velocity_sample_t, memfd_allocation_provider_t,
        velocity_sample_layout_t, std_atomic_access_t, null_notifier_t>;

    auto service = stream_service_t<writer_factory_type>{writer_factory_type{
        memfd_allocation_provider_t{},
        layout,
        std_atomic_access_t{},
        null_notifier_t{},
    }};
    auto control = in_process_control_t{service};

    using reader_factory_type = reader_factory_t<velocity_sample_t, decltype(control), memfd_mapper_t,
        velocity_sample_layout_t, std_atomic_access_t, null_waiter_t>;

    auto factory = reader_factory_type{
        control,
        memfd_mapper_t{},
        layout,
        std_atomic_access_t{},
        null_waiter_t{},
    };

    auto reader_result = factory.create(1024);
    ASSERT_TRUE(reader_result.has_value());
    auto reader = std::move(*reader_result);
    ASSERT_TRUE(service.has_writer());
    ASSERT_NE(service.writer(), nullptr);

    std::atomic<bool> writer_failed{false};

    std::jthread writer_thread{[&] {
        auto& writer = *service.writer();
        for (std::uint64_t sequence = 1; sequence <= sample_count; ++sequence)
        {
            velocity_sample_t const sample{
                .timestamp_ns = sequence,
                .velocity = sequence ^ checksum_mask,
            };

            for (;;)
            {
                auto const result = writer.try_push(sample);
                if (result == push_result_t::published) break;
                if (result == push_result_t::invalid_reader_sequence)
                {
                    writer_failed.store(true, std::memory_order_relaxed);
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
        auto const batch = reader.readable();
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

        if (!reader.consume(batch.size())) data_failed = true;
    }

    writer_thread.join();

    EXPECT_FALSE(writer_failed.load(std::memory_order_relaxed));
    EXPECT_FALSE(data_failed);
    EXPECT_FALSE(reader.corrupt());
    EXPECT_TRUE(reader.empty());
}

TEST(SpscStreamFactory, RejectsSecondWriterAndSessionDestructionReleasesFirst)
{
    raw_displacement_layout_t layout{raw_displacement_contract_id};
    using writer_factory_type = writer_factory_t<raw_displacement_t, memfd_allocation_provider_t,
        raw_displacement_layout_t, std_atomic_access_t, null_notifier_t>;

    auto service = stream_service_t<writer_factory_type>{writer_factory_type{
        memfd_allocation_provider_t{},
        layout,
        std_atomic_access_t{},
        null_notifier_t{},
    }};
    auto control = in_process_control_t{service};

    using reader_factory_type = reader_factory_t<raw_displacement_t, decltype(control), memfd_mapper_t,
        raw_displacement_layout_t, std_atomic_access_t, null_waiter_t>;

    auto factory = reader_factory_type{
        control,
        memfd_mapper_t{},
        layout,
        std_atomic_access_t{},
        null_waiter_t{},
    };

    {
        auto first = factory.create(64);
        ASSERT_TRUE(first.has_value());
        EXPECT_TRUE(service.has_writer());

        auto second = factory.create(64);
        ASSERT_FALSE(second.has_value());
        EXPECT_EQ(second.error(), stream_error_t::already_exists);
    }

    EXPECT_FALSE(service.has_writer());
    EXPECT_TRUE(factory.create(64).has_value());
}

TEST(SpscStreamFactory, MappingFailureUnwindsTheDriverSession)
{
    class failing_mapper_t
    {
    public:
        using region_type = mapped_region_t;

        auto map(host_mapping_ticket_t) const -> std::expected<region_type, stream_error_t>
        {
            return std::unexpected{stream_error_t::map_failed};
        }
    };

    raw_displacement_layout_t layout{raw_displacement_contract_id};
    using writer_factory_type = writer_factory_t<raw_displacement_t, memfd_allocation_provider_t,
        raw_displacement_layout_t, std_atomic_access_t, null_notifier_t>;

    auto service = stream_service_t<writer_factory_type>{writer_factory_type{
        memfd_allocation_provider_t{},
        layout,
        std_atomic_access_t{},
        null_notifier_t{},
    }};
    auto control = in_process_control_t{service};

    using reader_factory_type = reader_factory_t<raw_displacement_t, decltype(control), failing_mapper_t,
        raw_displacement_layout_t, std_atomic_access_t, null_waiter_t>;

    auto factory = reader_factory_type{
        control,
        failing_mapper_t{},
        layout,
        std_atomic_access_t{},
        null_waiter_t{},
    };

    auto failed = factory.create(64);
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error(), stream_error_t::map_failed);
    EXPECT_FALSE(service.has_writer());
}

TEST(SpscStreamReader, WaitsThroughAnEpollShapedInjectedAdapter)
{
    constexpr auto sample_count = std::uint64_t{32};

    raw_displacement_layout_t layout{raw_displacement_contract_id};
    eventfd_epoll_channel_t events;

    using writer_factory_type = writer_factory_t<raw_displacement_t, memfd_allocation_provider_t,
        raw_displacement_layout_t, std_atomic_access_t, eventfd_notifier_t>;

    auto service = stream_service_t<writer_factory_type>{writer_factory_type{
        memfd_allocation_provider_t{},
        layout,
        std_atomic_access_t{},
        events.notifier(),
    }};
    auto control = in_process_control_t{service};

    using reader_factory_type = reader_factory_t<raw_displacement_t, decltype(control), memfd_mapper_t,
        raw_displacement_layout_t, std_atomic_access_t, epoll_waiter_t>;

    auto factory = reader_factory_type{
        control,
        memfd_mapper_t{},
        layout,
        std_atomic_access_t{},
        events.waiter(),
    };

    auto reader_result = factory.create(64);
    ASSERT_TRUE(reader_result.has_value());
    auto reader = std::move(*reader_result);
    EXPECT_FALSE(service.readable());

    std::jthread writer_thread{[&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        auto& writer = *service.writer();

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

    ASSERT_TRUE(reader.wait_until_readable(2'000));
    EXPECT_TRUE(service.readable());

    auto expected = std::uint64_t{1};
    while (expected <= sample_count)
    {
        auto const batch = reader.readable();
        if (batch.empty())
        {
            ASSERT_TRUE(reader.wait_until_readable(2'000));
            continue;
        }

        for (auto const& sample : batch)
        {
            EXPECT_EQ(sample.timestamp_ns, expected);
            EXPECT_EQ(sample.x, static_cast<std::int64_t>(expected));
            EXPECT_EQ(sample.y, -static_cast<std::int64_t>(expected));
            ++expected;
        }

        ASSERT_TRUE(reader.consume(batch.size()));
    }

    writer_thread.join();
    EXPECT_FALSE(reader.corrupt());
    EXPECT_FALSE(service.readable());
}

} // namespace
} // namespace crv::mouse::streams
