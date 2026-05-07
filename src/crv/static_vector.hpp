// SPDX-License-Identifier: MIT

/// \file
/// \brief vector with static capacity
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <crv/lib.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <compare>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace crv {

/// vector-like container with static capacity
///
/// This type presents a hybrid vector/array api operating over a fixed array.
template <typename t_value_t, std::size_t t_fixed_capacity>
    requires std::is_trivial_v<t_value_t>
class static_vector_t
{
public:
    using value_t = t_value_t;
    static constexpr auto fixed_capacity = t_fixed_capacity;

    using data_t = std::array<value_t, fixed_capacity>;
    using size_t = std::size_t;

    using value_type = data_t::value_type;
    using size_type = data_t::size_type;
    using difference_type = data_t::difference_type;
    using pointer = data_t::pointer;
    using const_pointer = data_t::const_pointer;
    using iterator = data_t::iterator;
    using const_iterator = data_t::const_iterator;

    constexpr static_vector_t() = default;

    template <std::input_iterator iterator_t> constexpr static_vector_t(iterator_t begin, iterator_t end) noexcept
    {
        init(begin, end);
    }

    constexpr static_vector_t(std::initializer_list<value_t> initializer_list) noexcept
        : static_vector_t{std::begin(initializer_list), std::end(initializer_list)}
    {}

    template <std::ranges::input_range range_t>
    constexpr static_vector_t(std::from_range_t, range_t&& range)
        : static_vector_t(std::ranges::begin(range), std::ranges::end(range))
    {}

    constexpr static_vector_t(value_t const& value, size_type count = 1) noexcept { init(value, count); }

    constexpr auto operator<=>(static_vector_t const& other) const noexcept -> auto
    {
        return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end());
    }

    constexpr auto operator==(static_vector_t const& other) const noexcept -> bool
    {
        return std::ranges::equal(*this, other);
    }

    auto operator=(static_vector_t const& other) const noexcept -> static_vector_t&
    {
        if (this != &other) assign(other.begin(), other.end());
        return *this;
    }

    auto operator=(static_vector_t&& other) const noexcept -> static_vector_t&
    {
        if (this != &other) assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
        return *this;
    }

    template <typename other_value_t, std::size_t other_fixed_capacity>
    auto operator=(static_vector_t<other_value_t, other_fixed_capacity> const& other) noexcept -> static_vector_t&
    {
        return assign_other(other);
    }

    template <typename other_value_t, std::size_t other_fixed_capacity>
    auto operator=(static_vector_t<other_value_t, other_fixed_capacity>&& other) noexcept -> static_vector_t&
    {
        return assign_other(std::move(other));
    }

    constexpr auto begin(this auto&& self) noexcept -> auto { return std::begin(self.data_); }
    constexpr auto end(this auto&& self) noexcept -> auto { return self.begin() + self.size(); }
    constexpr auto cbegin() const noexcept -> const_iterator { return begin(); }
    constexpr auto cend() const noexcept -> const_iterator { return end(); }

    constexpr auto empty() const noexcept -> bool { return size_ == 0; }
    constexpr auto full() const noexcept -> bool { return size_ == capacity(); }
    constexpr auto size() const noexcept -> size_t { return size_; }
    constexpr auto capacity() const noexcept -> size_t { return max_size(); }
    constexpr auto max_size() const noexcept -> size_t { return fixed_capacity; }

    constexpr auto front(this auto&& self) noexcept -> decltype(auto)
    {
        assert(!self.empty() && "static_vector_t::front(): underflow");
        return self.data_[0];
    }

    constexpr auto back(this auto&& self) noexcept -> decltype(auto)
    {
        assert(!self.empty() && "static_vector_t::back(): underflow");
        return self.data_[self.size_ - 1];
    }

    constexpr auto data(this auto&& self) noexcept -> auto { return self.data_.data(); }

    constexpr auto operator[](this auto&& self, std::size_t index) noexcept -> decltype(auto)
    {
        assert(index < self.size_ && "static_vector_t: index out of range");
        return self.data_[index];
    }

    constexpr auto clear() noexcept -> void { size_ = 0; }

    constexpr auto push_back(value_t const& value) noexcept -> void { emplace_back(value); }
    constexpr auto pop_back() noexcept -> void
    {
        assert(!empty() && "static_vector_t::pop(): underflow");
        --size_;
    }

    template <typename... args_t> constexpr auto emplace_back(args_t&&... args) -> decltype(auto)
    {
        assert(!full() && "static_vector_t::emplace_back(): overflow");
        return data_[size_++] = value_t{std::forward<args_t>(args)...};
    }

    // iterator pair
    constexpr auto erase(const_iterator first, const_iterator last) noexcept -> iterator
    {
        auto const offset = std::distance(cbegin(), first);
        auto const count = std::distance(first, last);
        auto mutable_position = begin() + offset;

        if (count > 0)
        {
            // shift left
            std::copy(last, cend(), mutable_position);

            size_ -= count;
        }

        return mutable_position;
    }

    // single element
    constexpr auto erase(const_iterator position) noexcept -> iterator { return erase(position, position + 1); }

    // iterator pair
    template <std::input_iterator iterator_t>
    constexpr auto insert(const_iterator position, iterator_t first, iterator_t last) -> iterator
    {
        auto const offset = std::distance(cbegin(), position);
        auto mutable_position = begin() + offset;

        if constexpr (std::forward_iterator<iterator_t>)
        {
            auto const count = std::distance(first, last);
            assert(size_ + count <= fixed_capacity && "static_vector_t::insert(): overflow");

            if (count > 0)
            {
                // shift right to make room
                std::copy_backward(mutable_position, end(), end() + count);

                // copy into gap
                std::copy(first, last, mutable_position);

                size_ += count;
            }
        }
        else
        {
            auto const partition = end();

            // append to back, checking capacity along the way
            for (; first != last; ++first)
            {
                assert(!full() && "static_vector_t::insert(): overflow");
                emplace(std::move(*first));
            }

            // rotate new partition before first
            std::ranges::rotate(mutable_position, partition, end());
        }

        return mutable_position;
    }

    // single element
    template <typename other_value_t> constexpr auto insert(const_iterator position, other_value_t&& value) -> iterator
    {
        assert(!full() && "static_vector_t::insert(): overflow");

        auto const offset = std::distance(cbegin(), position);
        auto mutable_position = begin() + offset;

        // shift right
        std::copy_backward(mutable_position, end(), end() + 1);

        // move
        *mutable_position = std::forward<other_value_t>(value);

        ++size_;

        return mutable_position;
    }

    // initializer list
    constexpr auto insert(const_iterator position, std::initializer_list<value_t> initializer_list) -> iterator
    {
        return insert(position, initializer_list.begin(), initializer_list.end());
    }

    // copy n
    constexpr auto insert(const_iterator position, size_type count, value_t const& value) -> iterator
    {
        assert(size_ + count <= fixed_capacity && "static_vector_t::insert(): overflow");

        auto const offset = std::distance(cbegin(), position);
        auto mutable_position = begin() + offset;

        if (count > 0)
        {
            // shift right
            std::copy_backward(mutable_position, end(), end() + count);

            // fill gap
            std::fill_n(mutable_position, count, value);

            size_ += count;
        }

        return mutable_position;
    }

    constexpr auto assign(size_type count, value_t const& value) noexcept -> void
    {
        clear();
        init(count, value);
    }

    template <std::input_iterator iterator_t> auto assign(iterator_t first, iterator_t last) noexcept -> void
    {
        clear();
        init(first, last);
    }

    void assign(std::initializer_list<value_t> initializer_list)
    {
        assign(std::begin(initializer_list), std::end(initializer_list));
    }

    auto insert_range(const_iterator pos, std::ranges::input_range auto&& range) -> iterator
    {
        return insert(pos, std::begin(range), std::end(range));
    }

    auto append_range(std::ranges::input_range auto&& range) -> iterator
    {
        for (; begin != end; ++begin)
        {
            assert(!full() && "static_vector_t: appended range exceeds capacity");
            push_back(*begin);
        }
    }

    auto append_range(std::ranges::forward_range auto&& range) -> iterator
    {
        auto const size = std::distance(begin, end);
        assert(std::cmp_less_equal(size, fixed_capacity) && "static_vector_t: constructed range exceeds capacity");
        std::copy(begin, end, data_.begin());
        size_ = size;
    }

    auto assign_range(std::ranges::input_range auto&& range) -> iterator
    {
        //
    }

private:
    auto init(size_type count, value_t const& value) noexcept -> void
    {
        assert(size_ + count < fixed_capacity && "static_vector_t: assigned count exceeds capacity");
        std::fill_n(end(), count, value);
    }

    template <std::input_iterator iterator_t> auto init(iterator_t first, iterator_t last) noexcept -> void
    {
        if constexpr (std::forward_iterator<iterator_t>)
        {
            auto const size = std::distance(first, last);
            assert(std::cmp_less_equal(size, fixed_capacity) && "static_vector_t: assigned range exceeds capacity");
            std::copy(first, last, data_.begin());
            size_ = size;
        }
        else
        {
            for (; first != last; ++first)
            {
                assert(!full() && "static_vector_t: assigned range exceeds capacity");

                // does this safely copy if the iterators are not move iterators?
                // same as in insert()
                emplace_back(std::move(*first));
            }
        }
    }

    template <typename other_value_t, std::size_t other_fixed_capacity>
    auto assign_other(static_vector_t<other_value_t, other_fixed_capacity>&& other) noexcept -> static_vector_t&
    {
        if constexpr (other_fixed_capacity <= fixed_capacity)
        {
            clear();
            std::copy(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()), data_.begin());
            size_ = other.size();
        }
        else
        {
            assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
        }
        return *this;
    }

    data_t data_;
    size_type size_ = 0;
};

} // namespace crv
