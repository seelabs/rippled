//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef BEAST_CXX17_PMR_H_INCLUDED
#define BEAST_CXX17_PMR_H_INCLUDED

// typedefs for pmr classes. Use std::prm classes if available, fallback to
// boost versions if not available. as of Sept 2020, clang does not implement
// pmr.

#include <type_traits>

namespace ripple {
template <class, class = std::void_t<>>
struct is_pmr_enabled : std::false_type
{
};

// Classes signal support for pmr by definine the nested struct `allocator_type`
template <class T>
struct is_pmr_enabled<T, std::void_t<typename T::allocator_type>>
    : std::true_type
{
};
}  // namespace ripple

#ifdef _LIBCPP_VERSION

#include <boost/container/pmr/map.hpp>
#include <boost/container/pmr/memory_resource.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/string.hpp>
#include <boost/container/pmr/vector.hpp>

namespace ripple {
template <
    class Key,
    class T,
    class Compare = std::less<Key>,
    class Options = void>
using pmr_map = boost::container::pmr::map<Key, T, Compare, Options>;

using pmr_memory_resource = boost::container::pmr::memory_resource;

using pmr_monotonic_buffer_resource =
    boost::container::pmr::monotonic_buffer_resource;

template <class T>
using pmr_polymorphic_allocator =
    boost::container::pmr::polymorphic_allocator<T>;

using pmr_string = boost::container::pmr::string;

template <class T>
using pmr_vector = boost::container::pmr::vector<T>;

inline pmr_memory_resource*
pmr_get_default_resource()
{
    return boost::container::pmr::get_default_resource();
}

}  // namespace ripple

#else

#include <map>
#include <memory_resource>
#include <string>
#include <vector>

namespace ripple {
template <class Key, class T, class Compare = std::less<Key>>
using pmr_map = std::pmr::map<Key, T, Compare>;

using pmr_memory_resource = std::pmr::memory_resource;

using pmr_monotonic_buffer_resource = std::pmr::monotonic_buffer_resource;

template <class T>
using pmr_polymorphic_allocator = std::pmr::polymorphic_allocator<T>;

using pmr_string = std::pmr::string;

template <class T>
using pmr_vector = std::pmr::vector<T>;

inline pmr_memory_resource*
pmr_get_default_resource()
{
    return std::pmr::get_default_resource();
}

}  // namespace ripple

#endif

#endif
