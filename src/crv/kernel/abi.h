// SPDX-License-Identifier: GPL-2.0+ OR MIT

/// \file
/// \brief common linux c++ abi
///
/// C++ code can't include Linux headers directly, so types are redeclared here compatibly, then asserted in abi.c for
/// compatibility.
///
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

typedef __INT8_TYPE__ crv_s8_t;
typedef __INT16_TYPE__ crv_s16_t;
typedef __INT32_TYPE__ crv_s32_t;
typedef __INT64_TYPE__ crv_s64_t;

typedef __UINT8_TYPE__ crv_u8_t;
typedef __UINT16_TYPE__ crv_u16_t;
typedef __UINT32_TYPE__ crv_u32_t;
typedef __UINT64_TYPE__ crv_u64_t;

#if !defined __KERNEL__
#define __user
#endif

/// compatible static_assert across languages
#if defined(__cplusplus)
#define CRV_STATIC_ASSERT(condition, message) static_assert(condition, message)
#else
#define CRV_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#endif

/// compatible typeof across languages
#define CRV_TYPEOF(expression) __typeof__(expression)

/// true if type is signed
#define CRV_IS_SIGNED_TYPE(type) (((type) - 1) < (type)1)

/// references a member of a struct
#define CRV_MEMBER(type, member) (((type*)0)->member)

/// references type of a member of a struct
#define CRV_MEMBER_TYPE(type, member) CRV_TYPEOF(CRV_MEMBER(type, member))

/// true if type of member is signed
#define CRV_MEMBER_SIGNED(type, member) CRV_IS_SIGNED_TYPE(CRV_MEMBER_TYPE(type, member))

/// asserts member has specific size and offset
#define CRV_MEMBER_LAYOUT(type, member, size, offset)                                                       \
    CRV_STATIC_ASSERT(sizeof(CRV_MEMBER_TYPE(type, member)) == size, #type "::" #member ": size mismatch"); \
    CRV_STATIC_ASSERT(offsetof(type, member) == offset, #type "::" #member ": offset mismatch");

/// aserts member has specific size, offset, and signedness
#define CRV_MEMBER_LAYOUT_INTEGER(type, member, size, offset, signedness) \
    CRV_MEMBER_LAYOUT(type, member, size, offset);                        \
    CRV_STATIC_ASSERT(CRV_MEMBER_SIGNED(type, member) == signedness, #type "::" #member ": sign mismatch")

/// asserts types share size and alignment
#define CRV_SAME_LAYOUT(compat_type, ref_type)                                                                    \
    CRV_STATIC_ASSERT(                                                                                            \
        sizeof(compat_type) == sizeof(ref_type), #compat_type ": size does not match reference type " #ref_type); \
    CRV_STATIC_ASSERT(_Alignof(compat_type) == _Alignof(ref_type),                                                \
        #compat_type ": alignment does not match reference type " #ref_type)

/// asserts member size and offset in compat_type matches member size and offset in ref_type
#define CRV_MEMBER_SAME_LAYOUT(compat_type, ref_type, member)                                          \
    CRV_STATIC_ASSERT(sizeof(CRV_MEMBER(compat_type, member)) == sizeof(CRV_MEMBER(ref_type, member)), \
        #compat_type "::" #member ": size does not match reference type " #ref_type);                  \
    CRV_STATIC_ASSERT(offsetof(compat_type, member) == offsetof(ref_type, member),                     \
        #compat_type "::" #member ": offset does not match reference type " #ref_type)

/// asserts member type and signedness
#define CRV_MEMBER_SAME_LAYOUT_INTEGER(compat_type, ref_type, member)                                \
    CRV_MEMBER_SAME_LAYOUT(compat_type, ref_type, member);                                           \
    CRV_STATIC_ASSERT(CRV_MEMBER_SIGNED(compat_type, member) == CRV_MEMBER_SIGNED(ref_type, member), \
        #compat_type "::" #member ": signedness does not match reference type " #ref_type)
