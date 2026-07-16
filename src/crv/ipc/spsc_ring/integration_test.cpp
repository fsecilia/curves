// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief Single-file proof of concept for a generic zero-copy SPSC stream with
///        driver-side and userspace-side factories.
///
/// The public creation path is deliberately asymmetric:
///
///   consumer_factory.create()
///       -> control channel creates one driver stream
///       -> driver service delegates allocation and initialization to producer_factory
///       -> control channel returns an owning lease plus an export ticket
///       -> userspace mapper maps the ticket
///       -> consumer_factory validates the mapping and returns an owning reader instance
///
/// The ring core knows only element_t and an opaque leaf-supplied contract id. It does
/// not enumerate application record formats. Allocation, control transport, mapping,
/// atomic access, notification, and waiting are all injected dependencies.
///
/// The test support uses memfd aliases and an in-process control channel to model the
/// future ioctl + mmap path. The ring itself has no Linux dependency.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
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

struct alignas(sequence_alignment) sequence_storage_t
{
    sequence_t value{};
};

// These are intentionally passive ABI storage objects. Behavior lives in mapping_contract_t,
// ring_mapping_t, and the endpoints.
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

struct alignas(control_alignment) producer_state_t
{
    sequence_storage_t head{};
    sequence_storage_t dropped{};
    sequence_storage_t faults{};
    std::byte reserved[40]{};
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

struct allocation_request_t
{
    std::size_t bytes{};
    std::size_t alignment{};
};

enum class mapping_error_t : std::uint8_t
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
};

enum class allocation_error_t : std::uint8_t
{
    failed,
};

enum class map_error_t : std::uint8_t
{
    failed,
};

enum class push_result_t : std::uint8_t
{
    published,
    full,
    invalid_consumer_sequence,
};

// A typed interpretation of a mapping. It is a real component, not a miscellaneous detail.
// Production instances are returned by mapping_contract_t::initialize() or bind(). Endpoint
// tests may substitute another type satisfying ring_mapping.
template <record_type element_t> class ring_mapping_t
{
public:
    using element_type = element_t;

    constexpr ring_mapping_t(header_t& header, element_t* elements, sequence_t capacity) noexcept
        : header_{&header}, elements_{elements}, capacity_{capacity}
    {}

    constexpr auto capacity() const noexcept -> sequence_t { return capacity_; }
    constexpr auto mask() const noexcept -> sequence_t { return capacity_ - 1; }

    constexpr auto slot(sequence_t sequence) const noexcept -> element_t&
    {
        return elements_[static_cast<std::size_t>(sequence & mask())];
    }

    constexpr auto producer_head() const noexcept -> sequence_storage_t& { return header_->producer.head; }
    constexpr auto producer_dropped() const noexcept -> sequence_storage_t& { return header_->producer.dropped; }
    constexpr auto producer_faults() const noexcept -> sequence_storage_t& { return header_->producer.faults; }
    constexpr auto consumer_tail() const noexcept -> sequence_storage_t& { return header_->consumer.tail; }

private:
    header_t* header_;
    element_t* elements_;
    sequence_t capacity_;
};

template <typename mapping_t, typename element_t>
concept ring_mapping = record_type<element_t> && requires(mapping_t mapping, sequence_t sequence) {
    { mapping.capacity() } noexcept -> std::same_as<sequence_t>;
    { mapping.mask() } noexcept -> std::same_as<sequence_t>;
    { mapping.slot(sequence) } noexcept -> std::same_as<element_t&>;
    { mapping.producer_head() } noexcept -> std::same_as<sequence_storage_t&>;
    { mapping.producer_dropped() } noexcept -> std::same_as<sequence_storage_t&>;
    { mapping.producer_faults() } noexcept -> std::same_as<sequence_storage_t&>;
    { mapping.consumer_tail() } noexcept -> std::same_as<sequence_storage_t&>;
};

static_assert(ring_mapping<ring_mapping_t<std::uint64_t>, std::uint64_t>);

// Owns the complete shared-memory layout contract: sizing, alignment, initialization,
// validation, and typed interpretation.
template <record_type element_t> class mapping_contract_t
{
public:
    using element_type = element_t;
    using mapping_type = ring_mapping_t<element_t>;

    static_assert(sizeof(element_t) <= std::numeric_limits<std::uint32_t>::max());
    static_assert(alignof(element_t) <= std::numeric_limits<std::uint32_t>::max());

    explicit constexpr mapping_contract_t(stream_contract_id_t id) noexcept : id_{id} {}

    constexpr auto id() const noexcept -> stream_contract_id_t { return id_; }

    constexpr auto allocation_for(sequence_t capacity) const noexcept
        -> std::expected<allocation_request_t, mapping_error_t>
    {
        if (!std::has_single_bit(capacity)) { return std::unexpected{mapping_error_t::invalid_capacity}; }

        constexpr auto offset = data_offset();
        constexpr auto max_size = std::numeric_limits<std::size_t>::max();

        if (capacity > static_cast<sequence_t>((max_size - offset) / sizeof(element_t)))
        {
            return std::unexpected{mapping_error_t::size_overflow};
        }

        return allocation_request_t{
            .bytes = offset + static_cast<std::size_t>(capacity) * sizeof(element_t),
            .alignment = allocation_alignment(),
        };
    }

    auto initialize(std::span<std::byte> storage, sequence_t capacity) const noexcept
        -> std::expected<mapping_type, mapping_error_t>
    {
        auto const request = allocation_for(capacity);
        if (!request.has_value()) return std::unexpected{request.error()};

        if (auto const error = validate_storage(storage, request->bytes); error.has_value())
        {
            return std::unexpected{*error};
        }

        auto* header = ::new (storage.data()) header_t{};
        header->description.magic_value = magic;
        header->description.abi_major_value = abi_major;
        header->description.abi_minor_value = abi_minor;
        header->description.contract_id = id_.value;
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

        return mapping_type{*header, elements, capacity};
    }

    auto bind(std::span<std::byte> storage) const noexcept -> std::expected<mapping_type, mapping_error_t>
    {
        if (storage.data() == nullptr) return std::unexpected{mapping_error_t::null_mapping};
        if (reinterpret_cast<std::uintptr_t>(storage.data()) % alignof(header_t) != 0)
        {
            return std::unexpected{mapping_error_t::misaligned_header};
        }
        if (storage.size() < sizeof(header_t)) { return std::unexpected{mapping_error_t::mapping_too_small}; }

        auto* header = reinterpret_cast<header_t*>(storage.data());
        auto const& description = header->description;

        if (description.magic_value != magic) return std::unexpected{mapping_error_t::bad_magic};
        if (description.abi_major_value != abi_major) { return std::unexpected{mapping_error_t::unsupported_abi}; }
        if (description.contract_id != id_.value) { return std::unexpected{mapping_error_t::wrong_contract}; }
        if (description.element_size != sizeof(element_t))
        {
            return std::unexpected{mapping_error_t::wrong_element_size};
        }
        if (description.element_alignment != alignof(element_t))
        {
            return std::unexpected{mapping_error_t::wrong_element_alignment};
        }
        if (!std::has_single_bit(description.capacity)) { return std::unexpected{mapping_error_t::invalid_capacity}; }
        if (description.data_offset != data_offset()) { return std::unexpected{mapping_error_t::wrong_data_offset}; }

        auto const request = allocation_for(description.capacity);
        if (!request.has_value()) return std::unexpected{request.error()};

        if (description.mapping_bytes < request->bytes || description.mapping_bytes > storage.size())
        {
            return std::unexpected{mapping_error_t::invalid_mapping_size};
        }

        auto* elements_bytes = storage.data() + description.data_offset;
        if (reinterpret_cast<std::uintptr_t>(elements_bytes) % alignof(element_t) != 0)
        {
            return std::unexpected{mapping_error_t::misaligned_elements};
        }

        return mapping_type{
            *header,
            reinterpret_cast<element_t*>(elements_bytes),
            description.capacity,
        };
    }

private:
    static constexpr auto align_up(std::size_t value, std::size_t alignment) noexcept -> std::size_t
    {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    static constexpr auto data_offset() noexcept -> std::size_t
    {
        return align_up(sizeof(header_t), alignof(element_t));
    }

    static constexpr auto allocation_alignment() noexcept -> std::size_t
    {
        return control_alignment < alignof(element_t) ? alignof(element_t) : control_alignment;
    }

    static auto validate_storage(std::span<std::byte> storage, std::size_t required) noexcept
        -> std::optional<mapping_error_t>
    {
        if (storage.data() == nullptr) return mapping_error_t::null_mapping;
        if (reinterpret_cast<std::uintptr_t>(storage.data()) % alignof(header_t) != 0)
        {
            return mapping_error_t::misaligned_header;
        }
        if (storage.size() < required) return mapping_error_t::mapping_too_small;

        auto* elements = storage.data() + data_offset();
        if (reinterpret_cast<std::uintptr_t>(elements) % alignof(element_t) != 0)
        {
            return mapping_error_t::misaligned_elements;
        }

        return std::nullopt;
    }

    stream_contract_id_t id_;
};

template <typename contract_t, typename element_t>
concept mapping_contract
    = record_type<element_t> && requires(contract_t contract, std::span<std::byte> storage, sequence_t capacity) {
          typename contract_t::mapping_type;
          requires ring_mapping<typename contract_t::mapping_type, element_t>;
          {
              contract.allocation_for(capacity)
          } noexcept -> std::same_as<std::expected<allocation_request_t, mapping_error_t>>;
          {
              contract.initialize(storage, capacity)
          } noexcept -> std::same_as<std::expected<typename contract_t::mapping_type, mapping_error_t>>;
          {
              contract.bind(storage)
          } noexcept -> std::same_as<std::expected<typename contract_t::mapping_type, mapping_error_t>>;
      };

template <typename access_t>
concept atomic_access = requires(access_t& access, sequence_storage_t& storage, sequence_t value) {
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

template <record_type element_t, typename mapping_t, atomic_access atomic_access_t = std_atomic_access_t,
    notifier notifier_t = null_notifier_t>
    requires ring_mapping<mapping_t, element_t>
class producer_t
{
public:
    producer_t(mapping_t mapping, atomic_access_t atomic_access, notifier_t notifier_value) noexcept
        : mapping_{std::move(mapping)}, atomic_access_{std::move(atomic_access)}, notifier_{std::move(notifier_value)},
          head_{atomic_access_.load_relaxed(mapping_.producer_head())},
          dropped_{atomic_access_.load_relaxed(mapping_.producer_dropped())},
          faults_{atomic_access_.load_relaxed(mapping_.producer_faults())}
    {}

    auto try_push(element_t const& element) noexcept -> push_result_t
    {
        if (faulted_) return push_result_t::invalid_consumer_sequence;

        auto const tail = atomic_access_.load_acquire(mapping_.consumer_tail());
        auto const occupied = head_ - tail;

        // Userspace owns tail. An impossible sequence poisons this producer instance.
        // Count the transition once; subsequent pushes report the latched fault without
        // inflating either ABI-visible counter.
        if (occupied > mapping_.capacity())
        {
            record_fault();
            faulted_ = true;
            return push_result_t::invalid_consumer_sequence;
        }

        if (occupied == mapping_.capacity())
        {
            record_drop();
            return push_result_t::full;
        }

        mapping_.slot(head_) = element;
        ++head_;

        atomic_access_.store_release(mapping_.producer_head(), head_);
        notifier_.notify();
        return push_result_t::published;
    }

    constexpr auto faulted() const noexcept -> bool { return faulted_; }

private:
    void record_drop() noexcept
    {
        ++dropped_;
        atomic_access_.store_relaxed(mapping_.producer_dropped(), dropped_);
    }

    void record_fault() noexcept
    {
        ++faults_;
        atomic_access_.store_relaxed(mapping_.producer_faults(), faults_);
    }

    mapping_t mapping_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    [[no_unique_address]] notifier_t notifier_;
    sequence_t head_{};
    sequence_t dropped_{};
    sequence_t faults_{};
    bool faulted_{};
};

template <record_type element_t, typename mapping_t, atomic_access atomic_access_t = std_atomic_access_t,
    waiter waiter_t = null_waiter_t>
    requires ring_mapping<mapping_t, element_t>
class consumer_t
{
public:
    consumer_t(mapping_t mapping, atomic_access_t atomic_access, waiter_t waiter_value) noexcept
        : mapping_{std::move(mapping)}, atomic_access_{std::move(atomic_access)}, waiter_{std::move(waiter_value)},
          tail_{atomic_access_.load_relaxed(mapping_.consumer_tail())}
    {}

    auto readable() noexcept -> std::span<element_t const>
    {
        if (corrupt_) return {};

        if (leased_ != 0) { return {&mapping_.slot(tail_), leased_}; }

        auto const head = atomic_access_.load_acquire(mapping_.producer_head());
        auto const available = head - tail_;

        if (available > mapping_.capacity())
        {
            corrupt_ = true;
            return {};
        }

        auto const index = tail_ & mapping_.mask();
        auto const until_wrap = mapping_.capacity() - index;
        auto const contiguous = available < until_wrap ? available : until_wrap;

        leased_ = static_cast<std::size_t>(contiguous);
        return {&mapping_.slot(tail_), leased_};
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
        atomic_access_.store_release(mapping_.consumer_tail(), tail_);
        return true;
    }

    auto empty() noexcept -> bool
    {
        if (corrupt_) return true;
        if (leased_ != 0) return false;

        auto const head = atomic_access_.load_acquire(mapping_.producer_head());
        auto const available = head - tail_;

        if (available > mapping_.capacity())
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
            if (corrupt_) return false;
            if (!waiter_.wait(timeout_ms)) return false;
        }

        return true;
    }

    auto dropped() noexcept -> sequence_t { return atomic_access_.load_relaxed(mapping_.producer_dropped()); }

    auto producer_faults() noexcept -> sequence_t { return atomic_access_.load_relaxed(mapping_.producer_faults()); }

    constexpr auto corrupt() const noexcept -> bool { return corrupt_; }

private:
    mapping_t mapping_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    [[no_unique_address]] waiter_t waiter_;
    sequence_t tail_{};
    std::size_t leased_{};
    bool corrupt_{};
};

template <typename allocation_t>
concept writable_allocation = requires(allocation_t allocation) {
    typename allocation_t::ticket_type;
    { allocation.bytes() } noexcept -> std::same_as<std::span<std::byte>>;
    { std::as_const(allocation).ticket() } noexcept -> std::same_as<typename allocation_t::ticket_type>;
};

template <typename provider_t>
concept allocation_provider = requires(provider_t& provider, allocation_request_t request) {
    typename provider_t::allocation_type;
    requires writable_allocation<typename provider_t::allocation_type>;
    {
        provider.allocate(request)
    } -> std::same_as<std::expected<typename provider_t::allocation_type, allocation_error_t>>;
};

enum class producer_create_stage_t : std::uint8_t
{
    layout,
    allocation,
    initialization,
};

struct producer_create_error_t
{
    producer_create_stage_t stage{};
    mapping_error_t mapping_error{};
    allocation_error_t allocation_error{};
};

template <writable_allocation allocation_t, typename producer_t> class producer_instance_t
{
public:
    using allocation_type = allocation_t;
    using producer_type = producer_t;
    using ticket_type = typename allocation_t::ticket_type;

    producer_instance_t(allocation_t allocation, producer_t producer) noexcept
        : allocation_{std::move(allocation)}, producer_{std::move(producer)}
    {}

    producer_instance_t(producer_instance_t const&) = delete;
    auto operator=(producer_instance_t const&) -> producer_instance_t& = delete;
    producer_instance_t(producer_instance_t&&) noexcept = default;
    auto operator=(producer_instance_t&&) noexcept -> producer_instance_t& = delete;

    auto ticket() const noexcept -> ticket_type { return allocation_.ticket(); }
    auto writer() noexcept -> producer_t& { return producer_; }

private:
    // Reverse destruction order destroys the endpoint before releasing its storage.
    allocation_t allocation_;
    producer_t producer_;
};

template <record_type element_t, allocation_provider allocation_provider_t, typename contract_t,
    atomic_access atomic_access_t = std_atomic_access_t, notifier notifier_t = null_notifier_t>
    requires mapping_contract<contract_t, element_t>
class producer_factory_t
{
    static_assert(std::copy_constructible<atomic_access_t>);
    static_assert(std::copy_constructible<notifier_t>);

public:
    using allocation_type = typename allocation_provider_t::allocation_type;
    using mapping_type = typename contract_t::mapping_type;
    using producer_type = producer_t<element_t, mapping_type, atomic_access_t, notifier_t>;
    using instance_type = producer_instance_t<allocation_type, producer_type>;
    using ticket_type = typename instance_type::ticket_type;

    producer_factory_t(allocation_provider_t allocation_provider, contract_t contract,
        atomic_access_t atomic_access = {}, notifier_t notifier_value = {}) noexcept
        : allocation_provider_{std::move(allocation_provider)}, contract_{std::move(contract)},
          atomic_access_{std::move(atomic_access)}, notifier_{std::move(notifier_value)}
    {}

    auto create(sequence_t capacity) -> std::expected<instance_type, producer_create_error_t>
    {
        auto const request = contract_.allocation_for(capacity);
        if (!request.has_value())
        {
            return std::unexpected{producer_create_error_t{
                .stage = producer_create_stage_t::layout,
                .mapping_error = request.error(),
            }};
        }

        auto allocation = allocation_provider_.allocate(*request);
        if (!allocation.has_value())
        {
            return std::unexpected{producer_create_error_t{
                .stage = producer_create_stage_t::allocation,
                .allocation_error = allocation.error(),
            }};
        }

        auto mapping = contract_.initialize(allocation->bytes(), capacity);
        if (!mapping.has_value())
        {
            return std::unexpected{producer_create_error_t{
                .stage = producer_create_stage_t::initialization,
                .mapping_error = mapping.error(),
            }};
        }

        auto producer = producer_type{
            std::move(*mapping),
            atomic_access_,
            notifier_,
        };

        return instance_type{std::move(*allocation), std::move(producer)};
    }

private:
    [[no_unique_address]] allocation_provider_t allocation_provider_;
    [[no_unique_address]] contract_t contract_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    [[no_unique_address]] notifier_t notifier_;
};

template <typename factory_t>
concept producer_factory = requires(factory_t factory, sequence_t capacity) {
    typename factory_t::instance_type;
    typename factory_t::ticket_type;
    {
        factory.create(capacity)
    } -> std::same_as<std::expected<typename factory_t::instance_type, producer_create_error_t>>;
};

struct create_request_t
{
    sequence_t capacity{};
};

enum class driver_create_error_t : std::uint8_t
{
    already_exists,
    producer_creation_failed,
};

template <typename ticket_t> struct created_stream_t
{
    ticket_t ticket;
    std::uint64_t generation{};
};

template <producer_factory producer_factory_t> class driver_stream_service_t
{
public:
    using factory_type = producer_factory_t;
    using instance_type = typename producer_factory_t::instance_type;
    using ticket_type = typename producer_factory_t::ticket_type;
    using created_type = created_stream_t<ticket_type>;
    using producer_type = typename instance_type::producer_type;

    explicit driver_stream_service_t(producer_factory_t factory) noexcept : factory_{std::move(factory)} {}

    auto create(create_request_t request) -> std::expected<created_type, driver_create_error_t>
    {
        if (instance_.has_value()) { return std::unexpected{driver_create_error_t::already_exists}; }

        auto instance = factory_.create(request.capacity);
        if (!instance.has_value())
        {
            last_create_error_ = instance.error();
            return std::unexpected{driver_create_error_t::producer_creation_failed};
        }

        last_create_error_.reset();
        auto const generation = ++generation_;
        auto ticket = instance->ticket();
        instance_.emplace(std::move(*instance));
        return created_type{.ticket = std::move(ticket), .generation = generation};
    }

    void release(std::uint64_t generation) noexcept
    {
        if (instance_.has_value() && generation == generation_) instance_.reset();
    }

    auto writer() noexcept -> producer_type*
    {
        if (!instance_.has_value()) return nullptr;
        return &instance_->writer();
    }

    constexpr auto has_instance() const noexcept -> bool { return instance_.has_value(); }

    auto last_create_error() const noexcept -> std::optional<producer_create_error_t> { return last_create_error_; }

private:
    producer_factory_t factory_;
    std::optional<instance_type> instance_;
    std::optional<producer_create_error_t> last_create_error_;
    std::uint64_t generation_{};
};

template <typename lease_t>
concept stream_lease = requires(lease_t lease) {
    typename lease_t::ticket_type;
    { lease.ticket() } noexcept -> std::same_as<typename lease_t::ticket_type>;
};

template <typename control_t>
concept control_channel = requires(control_t& control, create_request_t request) {
    typename control_t::lease_type;
    requires stream_lease<typename control_t::lease_type>;
    {
        control.create_stream(request)
    } -> std::same_as<std::expected<typename control_t::lease_type, driver_create_error_t>>;
};

template <typename region_t>
concept mapped_region = requires(region_t region) {
    { region.bytes() } noexcept -> std::same_as<std::span<std::byte>>;
};

template <typename mapper_t, typename ticket_t>
concept userspace_mapper = requires(mapper_t& mapper, ticket_t ticket) {
    typename mapper_t::region_type;
    requires mapped_region<typename mapper_t::region_type>;
    { mapper.map(ticket) } -> std::same_as<std::expected<typename mapper_t::region_type, map_error_t>>;
};

enum class consumer_create_stage_t : std::uint8_t
{
    driver,
    map,
    bind,
};

struct consumer_create_error_t
{
    consumer_create_stage_t stage{};
    driver_create_error_t driver_error{};
    map_error_t map_error{};
    mapping_error_t mapping_error{};
};

template <stream_lease lease_t, mapped_region region_t, typename consumer_t> class consumer_instance_t
{
public:
    using lease_type = lease_t;
    using region_type = region_t;
    using consumer_type = consumer_t;

    consumer_instance_t(lease_t lease, region_t region, consumer_t consumer) noexcept
        : lease_{std::move(lease)}, region_{std::move(region)}, consumer_{std::move(consumer)}
    {}

    consumer_instance_t(consumer_instance_t const&) = delete;
    auto operator=(consumer_instance_t const&) -> consumer_instance_t& = delete;
    consumer_instance_t(consumer_instance_t&&) noexcept = default;
    auto operator=(consumer_instance_t&&) noexcept -> consumer_instance_t& = delete;

    auto readable() noexcept { return consumer_.readable(); }
    auto consume(std::size_t count) noexcept { return consumer_.consume(count); }
    auto empty() noexcept { return consumer_.empty(); }
    auto wait_until_readable(int timeout_ms = -1) noexcept { return consumer_.wait_until_readable(timeout_ms); }
    auto dropped() noexcept { return consumer_.dropped(); }
    auto producer_faults() noexcept { return consumer_.producer_faults(); }
    auto corrupt() const noexcept { return consumer_.corrupt(); }

private:
    // Reverse destruction order: endpoint, mapped region, then control lease. The lease
    // teardown can therefore release the driver allocation only after userspace unmapped it.
    lease_t lease_;
    region_t region_;
    consumer_t consumer_;
};

template <record_type element_t, control_channel control_channel_t, typename mapper_t, typename contract_t,
    atomic_access atomic_access_t = std_atomic_access_t, waiter waiter_t = null_waiter_t>
    requires mapping_contract<contract_t, element_t>
    && userspace_mapper<mapper_t, typename control_channel_t::lease_type::ticket_type>
class consumer_factory_t
{
    static_assert(std::copy_constructible<atomic_access_t>);
    static_assert(std::copy_constructible<waiter_t>);

public:
    using lease_type = typename control_channel_t::lease_type;
    using ticket_type = typename lease_type::ticket_type;
    using region_type = typename mapper_t::region_type;
    using mapping_type = typename contract_t::mapping_type;
    using consumer_type = consumer_t<element_t, mapping_type, atomic_access_t, waiter_t>;
    using instance_type = consumer_instance_t<lease_type, region_type, consumer_type>;

    consumer_factory_t(create_request_t request, control_channel_t control_channel, mapper_t mapper,
        contract_t contract, atomic_access_t atomic_access = {}, waiter_t waiter_value = {}) noexcept
        : request_{request}, control_channel_{std::move(control_channel)}, mapper_{std::move(mapper)},
          contract_{std::move(contract)}, atomic_access_{std::move(atomic_access)}, waiter_{std::move(waiter_value)}
    {}

    auto create() -> std::expected<instance_type, consumer_create_error_t>
    {
        auto lease = control_channel_.create_stream(request_);
        if (!lease.has_value())
        {
            return std::unexpected{consumer_create_error_t{
                .stage = consumer_create_stage_t::driver,
                .driver_error = lease.error(),
            }};
        }

        auto region = mapper_.map(lease->ticket());
        if (!region.has_value())
        {
            return std::unexpected{consumer_create_error_t{
                .stage = consumer_create_stage_t::map,
                .map_error = region.error(),
            }};
        }

        auto mapping = contract_.bind(region->bytes());
        if (!mapping.has_value())
        {
            return std::unexpected{consumer_create_error_t{
                .stage = consumer_create_stage_t::bind,
                .mapping_error = mapping.error(),
            }};
        }

        auto consumer = consumer_type{
            std::move(*mapping),
            atomic_access_,
            waiter_,
        };

        return instance_type{
            std::move(*lease),
            std::move(*region),
            std::move(consumer),
        };
    }

private:
    create_request_t request_;
    [[no_unique_address]] control_channel_t control_channel_;
    [[no_unique_address]] mapper_t mapper_;
    [[no_unique_address]] contract_t contract_;
    [[no_unique_address]] atomic_access_t atomic_access_;
    [[no_unique_address]] waiter_t waiter_;
};

} // namespace crv::ipc::spsc_stream

//
// Host-side stand-ins for allocation, mmap, notification, and ioctl.
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

    auto allocate(allocation_request_t request) const -> std::expected<allocation_type, allocation_error_t>
    {
        auto fd = unique_fd_t{::memfd_create("crv-spsc-stream-poc", MFD_CLOEXEC)};
        if (!fd) return std::unexpected{allocation_error_t::failed};

        if (::ftruncate(fd.get(), static_cast<off_t>(request.bytes)) != 0)
        {
            return std::unexpected{allocation_error_t::failed};
        }

        auto* memory = ::mmap(nullptr, request.bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), 0);
        if (memory == MAP_FAILED) return std::unexpected{allocation_error_t::failed};

        if (reinterpret_cast<std::uintptr_t>(memory) % request.alignment != 0)
        {
            ::munmap(memory, request.bytes);
            return std::unexpected{allocation_error_t::failed};
        }

        return allocation_type{std::move(fd), memory, request.bytes};
    }
};

static_assert(allocation_provider<memfd_allocation_provider_t>);

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

    auto map(host_mapping_ticket_t ticket) const -> std::expected<region_type, map_error_t>
    {
        auto* memory = ::mmap(nullptr, ticket.bytes, PROT_READ | PROT_WRITE, MAP_SHARED, ticket.fd, 0);
        if (memory == MAP_FAILED) return std::unexpected{map_error_t::failed};
        return region_type{memory, ticket.bytes};
    }
};

static_assert(mapped_region<mapped_region_t>);
static_assert(userspace_mapper<memfd_mapper_t, host_mapping_ticket_t>);

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

template <typename service_t> class in_process_stream_lease_t
{
public:
    using ticket_type = typename service_t::ticket_type;

    in_process_stream_lease_t() noexcept = default;

    in_process_stream_lease_t(service_t& service, typename service_t::created_type created) noexcept
        : service_{&service}, ticket_{std::move(created.ticket)}, generation_{created.generation}
    {}

    in_process_stream_lease_t(in_process_stream_lease_t const&) = delete;
    auto operator=(in_process_stream_lease_t const&) -> in_process_stream_lease_t& = delete;

    in_process_stream_lease_t(in_process_stream_lease_t&& other) noexcept
        : service_{std::exchange(other.service_, nullptr)}, ticket_{std::move(other.ticket_)},
          generation_{std::exchange(other.generation_, 0)}
    {}

    auto operator=(in_process_stream_lease_t&& other) noexcept -> in_process_stream_lease_t&
    {
        if (this == &other) return *this;
        reset();
        service_ = std::exchange(other.service_, nullptr);
        ticket_ = std::move(other.ticket_);
        generation_ = std::exchange(other.generation_, 0);
        return *this;
    }

    ~in_process_stream_lease_t() { reset(); }

    auto ticket() noexcept -> ticket_type { return ticket_; }

private:
    void reset() noexcept
    {
        if (service_ != nullptr) service_->release(generation_);
        service_ = nullptr;
        generation_ = 0;
    }

    service_t* service_{};
    ticket_type ticket_{};
    std::uint64_t generation_{};
};

template <typename service_t> class in_process_control_channel_t
{
public:
    using lease_type = in_process_stream_lease_t<service_t>;

    explicit in_process_control_channel_t(service_t& service) noexcept : service_{&service} {}

    auto create_stream(create_request_t request) -> std::expected<lease_type, driver_create_error_t>
    {
        auto created = service_->create(request);
        if (!created.has_value()) return std::unexpected{created.error()};
        return lease_type{*service_, std::move(*created)};
    }

private:
    service_t* service_;
};

} // namespace crv::ipc::spsc_stream::test_support

//
// Leaf stream definitions. The generic ring has no knowledge of these types or ids.
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

using raw_displacement_contract_t = crv::ipc::spsc_stream::mapping_contract_t<raw_displacement_t>;
using velocity_sample_contract_t = crv::ipc::spsc_stream::mapping_contract_t<velocity_sample_t>;

static_assert(sizeof(raw_displacement_t) == 24);
static_assert(alignof(raw_displacement_t) == 8);
static_assert(sizeof(velocity_sample_t) == 16);
static_assert(alignof(velocity_sample_t) == 8);

} // namespace crv::mouse::streams

//
// GoogleTest/GoogleMock tests.
//

#include <chrono>
#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace {

using namespace crv::ipc::spsc_stream;
using namespace crv::ipc::spsc_stream::test_support;
using namespace crv::mouse::streams;

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Ref;
using ::testing::Return;
using ::testing::StrictMock;

template <record_type element_t, std::size_t capacity_v> struct fake_mapping_storage_t
{
    static_assert(std::has_single_bit(capacity_v));

    header_t header{};
    std::array<element_t, capacity_v> elements{};
};

template <record_type element_t, std::size_t capacity_v> class fake_mapping_t
{
public:
    explicit fake_mapping_t(fake_mapping_storage_t<element_t, capacity_v>& storage) noexcept : storage_{&storage} {}

    constexpr auto capacity() const noexcept -> sequence_t { return capacity_v; }
    constexpr auto mask() const noexcept -> sequence_t { return capacity_v - 1; }

    auto slot(sequence_t sequence) const noexcept -> element_t&
    {
        return storage_->elements[static_cast<std::size_t>(sequence & mask())];
    }

    auto producer_head() const noexcept -> sequence_storage_t& { return storage_->header.producer.head; }
    auto producer_dropped() const noexcept -> sequence_storage_t& { return storage_->header.producer.dropped; }
    auto producer_faults() const noexcept -> sequence_storage_t& { return storage_->header.producer.faults; }
    auto consumer_tail() const noexcept -> sequence_storage_t& { return storage_->header.consumer.tail; }

private:
    fake_mapping_storage_t<element_t, capacity_v>* storage_;
};

static_assert(ring_mapping<fake_mapping_t<raw_displacement_t, 4>, raw_displacement_t>);

class mock_atomic_access_t
{
public:
    MOCK_METHOD(sequence_t, load_relaxed, (sequence_storage_t & storage), (const, noexcept));
    MOCK_METHOD(sequence_t, load_acquire, (sequence_storage_t & storage), (const, noexcept));
    MOCK_METHOD(void, store_relaxed, (sequence_storage_t & storage, sequence_t value), (const, noexcept));
    MOCK_METHOD(void, store_release, (sequence_storage_t & storage, sequence_t value), (const, noexcept));
};

// Test-only seam: production owns this small value normally; the delegate merely routes
// calls to a noncopyable GoogleMock object.
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

void store_sequence(sequence_storage_t& storage, sequence_t value) noexcept
{
    storage.value = value;
}

TEST(SpscStreamMappingContract, OwnsSizingInitializationValidationAndTyping)
{
    raw_displacement_contract_t raw_contract{raw_displacement_contract_id};
    velocity_sample_contract_t velocity_contract{velocity_sample_contract_id};

    auto const request = raw_contract.allocation_for(8);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->alignment, 64u);

    auto allocation = memfd_allocation_provider_t{}.allocate(*request);
    ASSERT_TRUE(allocation.has_value());

    auto initialized = raw_contract.initialize(allocation->bytes(), 8);
    ASSERT_TRUE(initialized.has_value());
    EXPECT_EQ(initialized->capacity(), 8u);

    auto rebound = raw_contract.bind(allocation->bytes());
    ASSERT_TRUE(rebound.has_value());
    EXPECT_EQ(rebound->capacity(), 8u);

    auto wrong_contract = velocity_contract.bind(allocation->bytes());
    ASSERT_FALSE(wrong_contract.has_value());
    EXPECT_EQ(wrong_contract.error(), mapping_error_t::wrong_contract);
}

TEST(SpscStreamEndpoints, UseInjectedMappingAndAtomicSeamsWithoutFriends)
{
    fake_mapping_storage_t<raw_displacement_t, 4> storage;
    auto mapping = fake_mapping_t<raw_displacement_t, 4>{storage};

    StrictMock<mock_atomic_access_t> producer_atomic;
    {
        InSequence sequence;
        EXPECT_CALL(producer_atomic, load_relaxed(Ref(storage.header.producer.head))).WillOnce(Return(0));
        EXPECT_CALL(producer_atomic, load_relaxed(Ref(storage.header.producer.dropped))).WillOnce(Return(0));
        EXPECT_CALL(producer_atomic, load_relaxed(Ref(storage.header.producer.faults))).WillOnce(Return(0));
    }

    auto producer = producer_t<raw_displacement_t, decltype(mapping), mock_atomic_access_delegate_t>{
        mapping,
        mock_atomic_access_delegate_t{.mock = &producer_atomic},
        null_notifier_t{},
    };

    {
        InSequence sequence;
        EXPECT_CALL(producer_atomic, load_acquire(Ref(storage.header.consumer.tail))).WillOnce(Return(0));
        EXPECT_CALL(producer_atomic, store_release(Ref(storage.header.producer.head), 1))
            .WillOnce(Invoke(store_sequence));
    }

    EXPECT_EQ(producer.try_push({.timestamp_ns = 1, .x = 2, .y = 3}), push_result_t::published);

    StrictMock<mock_atomic_access_t> consumer_atomic;
    EXPECT_CALL(consumer_atomic, load_relaxed(Ref(storage.header.consumer.tail))).WillOnce(Return(0));

    auto consumer = consumer_t<raw_displacement_t, decltype(mapping), mock_atomic_access_delegate_t>{
        mapping,
        mock_atomic_access_delegate_t{.mock = &consumer_atomic},
        null_waiter_t{},
    };

    EXPECT_CALL(consumer_atomic, load_acquire(Ref(storage.header.producer.head))).WillOnce(Return(1));

    auto const readable = consumer.readable();
    ASSERT_EQ(readable.size(), 1u);
    EXPECT_EQ(readable.front(), (raw_displacement_t{.timestamp_ns = 1, .x = 2, .y = 3}));

    EXPECT_CALL(consumer_atomic, store_release(Ref(storage.header.consumer.tail), 1)).WillOnce(Invoke(store_sequence));
    EXPECT_TRUE(consumer.consume(1));
}

TEST(SpscStreamProducer, SeparatesCapacityDropsFromLatchedConsumerSequenceFaults)
{
    fake_mapping_storage_t<raw_displacement_t, 4> storage;
    auto mapping = fake_mapping_t<raw_displacement_t, 4>{storage};

    auto producer = producer_t<raw_displacement_t, decltype(mapping)>{
        mapping,
        std_atomic_access_t{},
        null_notifier_t{},
    };

    for (std::uint64_t value = 1; value <= 4; ++value)
    {
        EXPECT_EQ(producer.try_push({.timestamp_ns = value}), push_result_t::published);
    }

    EXPECT_EQ(producer.try_push({.timestamp_ns = 5}), push_result_t::full);
    EXPECT_EQ(storage.header.producer.dropped.value, 1u);
    EXPECT_EQ(storage.header.producer.faults.value, 0u);

    storage.header.consumer.tail.value = 10;

    EXPECT_EQ(producer.try_push({.timestamp_ns = 6}), push_result_t::invalid_consumer_sequence);
    EXPECT_EQ(storage.header.producer.dropped.value, 1u);
    EXPECT_EQ(storage.header.producer.faults.value, 1u);

    EXPECT_EQ(producer.try_push({.timestamp_ns = 7}), push_result_t::invalid_consumer_sequence);
    EXPECT_EQ(storage.header.producer.dropped.value, 1u);
    EXPECT_EQ(storage.header.producer.faults.value, 1u);
    EXPECT_TRUE(producer.faulted());
}

TEST(SpscStreamFactory, ConsumerCreateComposesTheCompleteLifecycle)
{
    constexpr auto sample_count = std::uint64_t{250'000};
    constexpr auto checksum_mask = std::uint64_t{0x9E37'79B9'7F4A'7C15};

    velocity_sample_contract_t contract{velocity_sample_contract_id};
    using producer_factory_type
        = producer_factory_t<velocity_sample_t, memfd_allocation_provider_t, velocity_sample_contract_t>;

    auto service = driver_stream_service_t<producer_factory_type>{producer_factory_type{
        memfd_allocation_provider_t{},
        contract,
    }};
    auto control = in_process_control_channel_t{service};

    using consumer_factory_type
        = consumer_factory_t<velocity_sample_t, decltype(control), memfd_mapper_t, velocity_sample_contract_t>;

    auto consumer_factory = consumer_factory_type{
        create_request_t{.capacity = 1024},
        control,
        memfd_mapper_t{},
        contract,
    };

    auto reader_result = consumer_factory.create();
    ASSERT_TRUE(reader_result.has_value());
    auto reader = std::move(*reader_result);
    ASSERT_TRUE(service.has_instance());
    ASSERT_NE(service.writer(), nullptr);

    std::atomic<bool> producer_failed{false};

    std::jthread producer_thread{[&] {
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

    producer_thread.join();

    EXPECT_FALSE(producer_failed.load(std::memory_order_relaxed));
    EXPECT_FALSE(data_failed);
    EXPECT_FALSE(reader.corrupt());
    EXPECT_TRUE(reader.empty());
}

TEST(SpscStreamFactory, RejectsSecondInstanceAndLeaseDestructionReleasesFirst)
{
    raw_displacement_contract_t contract{raw_displacement_contract_id};
    using producer_factory_type
        = producer_factory_t<raw_displacement_t, memfd_allocation_provider_t, raw_displacement_contract_t>;

    auto service = driver_stream_service_t<producer_factory_type>{producer_factory_type{
        memfd_allocation_provider_t{},
        contract,
    }};
    auto control = in_process_control_channel_t{service};

    using consumer_factory_type
        = consumer_factory_t<raw_displacement_t, decltype(control), memfd_mapper_t, raw_displacement_contract_t>;

    auto factory = consumer_factory_type{
        create_request_t{.capacity = 64},
        control,
        memfd_mapper_t{},
        contract,
    };

    {
        auto first = factory.create();
        ASSERT_TRUE(first.has_value());
        EXPECT_TRUE(service.has_instance());

        auto second = factory.create();
        ASSERT_FALSE(second.has_value());
        EXPECT_EQ(second.error().stage, consumer_create_stage_t::driver);
        EXPECT_EQ(second.error().driver_error, driver_create_error_t::already_exists);
    }

    EXPECT_FALSE(service.has_instance());
    EXPECT_TRUE(factory.create().has_value());
}

TEST(SpscStreamFactory, MappingFailureUnwindsDriverInstanceThroughLease)
{
    class failing_mapper_t
    {
    public:
        using region_type = mapped_region_t;

        auto map(host_mapping_ticket_t) const -> std::expected<region_type, map_error_t>
        {
            return std::unexpected{map_error_t::failed};
        }
    };

    raw_displacement_contract_t contract{raw_displacement_contract_id};
    using producer_factory_type
        = producer_factory_t<raw_displacement_t, memfd_allocation_provider_t, raw_displacement_contract_t>;

    auto service = driver_stream_service_t<producer_factory_type>{producer_factory_type{
        memfd_allocation_provider_t{},
        contract,
    }};
    auto control = in_process_control_channel_t{service};

    using consumer_factory_type
        = consumer_factory_t<raw_displacement_t, decltype(control), failing_mapper_t, raw_displacement_contract_t>;

    auto factory = consumer_factory_type{
        create_request_t{.capacity = 64},
        control,
        failing_mapper_t{},
        contract,
    };

    auto failed = factory.create();
    ASSERT_FALSE(failed.has_value());
    EXPECT_EQ(failed.error().stage, consumer_create_stage_t::map);
    EXPECT_FALSE(service.has_instance());
}

TEST(SpscStreamConsumer, WaitsThroughAnEpollShapedInjectedChannel)
{
    constexpr auto sample_count = std::uint64_t{32};

    raw_displacement_contract_t contract{raw_displacement_contract_id};
    eventfd_epoll_channel_t events;

    using producer_factory_type = producer_factory_t<raw_displacement_t, memfd_allocation_provider_t,
        raw_displacement_contract_t, std_atomic_access_t, eventfd_notifier_t>;

    auto service = driver_stream_service_t<producer_factory_type>{producer_factory_type{
        memfd_allocation_provider_t{},
        contract,
        std_atomic_access_t{},
        events.notifier(),
    }};
    auto control = in_process_control_channel_t{service};

    using consumer_factory_type = consumer_factory_t<raw_displacement_t, decltype(control), memfd_mapper_t,
        raw_displacement_contract_t, std_atomic_access_t, epoll_waiter_t>;

    auto factory = consumer_factory_type{
        create_request_t{.capacity = 64},
        control,
        memfd_mapper_t{},
        contract,
        std_atomic_access_t{},
        events.waiter(),
    };

    auto reader_result = factory.create();
    ASSERT_TRUE(reader_result.has_value());
    auto reader = std::move(*reader_result);

    std::jthread producer_thread{[&] {
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

    producer_thread.join();
    EXPECT_FALSE(reader.corrupt());
}

} // namespace
