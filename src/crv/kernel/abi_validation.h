// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief mechanisms to assert compatible abis
///
/// C++ code can't include Linux headers directly, so types are redeclared. This module is used to assert bitwise
/// compatibility.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#if defined __KERNEL__
#include <linux/stddef.h>
#else
#include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//
// compatibility
//

/// compatible alignof across languages
#if defined(__cplusplus)
#define CRV_ALIGNOF(type) alignof(type)
#else
#define CRV_ALIGNOF(type) _Alignof(type)
#endif

/// compatible static_assert across languages
#if defined(__cplusplus)
#define CRV_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define CRV_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

/// compatible typeof across languages
#define CRV_TYPEOF(expression) __typeof__(expression)

//
// validation
//

/// true if type is signed
#define CRV_IS_SIGNED_TYPE(type) (((type) - 1) < (type)1)

/// asserts types share size and alignment
#define CRV_SAME_LAYOUT(compat_type, ref_type)                                                                    \
    CRV_STATIC_ASSERT(                                                                                            \
        sizeof(compat_type) == sizeof(ref_type), #compat_type ": size does not match reference type " #ref_type); \
    CRV_STATIC_ASSERT(CRV_ALIGNOF(compat_type) == CRV_ALIGNOF(ref_type),                                          \
        #compat_type ": alignment does not match reference type " #ref_type)

/// references a member of a struct
#define CRV_MEMBER(type, member) (((type*)0)->member)

/// references type of a member of a struct
#define CRV_MEMBER_TYPEOF(type, member) CRV_TYPEOF(CRV_MEMBER(type, member))

/// true if type of member is signed
#define CRV_MEMBER_IS_SIGNED_TYPE(type, member) CRV_IS_SIGNED_TYPE(CRV_MEMBER_TYPEOF(type, member))

/// asserts member has specific size and offset
#define CRV_MEMBER_LAYOUT(type, member, size, offset)                                                         \
    CRV_STATIC_ASSERT(sizeof(CRV_MEMBER_TYPEOF(type, member)) == size, #type "::" #member ": size mismatch"); \
    CRV_STATIC_ASSERT(offsetof(type, member) == offset, #type "::" #member ": offset mismatch")

/// asserts member has specific size, offset, and signedness
#define CRV_MEMBER_LAYOUT_INTEGER(type, member, size, offset, signedness) \
    CRV_MEMBER_LAYOUT(type, member, size, offset);                        \
    CRV_STATIC_ASSERT(CRV_MEMBER_IS_SIGNED_TYPE(type, member) == signedness, #type "::" #member ": sign mismatch")

/// asserts same member size and offset in compat_type matches member size and offset in ref_type
#define CRV_MEMBER_SAME_LAYOUT(compat_type, ref_type, member)                                          \
    CRV_STATIC_ASSERT(sizeof(CRV_MEMBER(compat_type, member)) == sizeof(CRV_MEMBER(ref_type, member)), \
        #compat_type "::" #member ": size does not match reference type " #ref_type);                  \
    CRV_STATIC_ASSERT(offsetof(compat_type, member) == offsetof(ref_type, member),                     \
        #compat_type "::" #member ": offset does not match reference type " #ref_type)

/// asserts same member type and signedness
#define CRV_MEMBER_SAME_LAYOUT_INTEGER(compat_type, ref_type, member)                                                \
    CRV_MEMBER_SAME_LAYOUT(compat_type, ref_type, member);                                                           \
    CRV_STATIC_ASSERT(CRV_MEMBER_IS_SIGNED_TYPE(compat_type, member) == CRV_MEMBER_IS_SIGNED_TYPE(ref_type, member), \
        #compat_type "::" #member ": signedness does not match reference type " #ref_type)

#ifdef __cplusplus
} // extern "C" {
#endif
