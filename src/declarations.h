//
// Created by Alex on 18.06.2025.
//

#ifndef DECLARATIONS_H
#define DECLARATIONS_H

#include <cstdint>
#include <type_traits>

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using i128 = __int128_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using u128 = __uint128_t;

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
using isize = decltype(sizeof(nullptr));     // std::ptrdiff_t
using usize = decltype(sizeof(nullptr));     // std::size_t
#else
using isize = std::int32_t;
using usize = std::uint32_t;
#endif

static_assert(sizeof(char)  == 1, "Invalid char size");
using char8  = char;
using char16 = char16_t;
using char32 = char32_t;

using f32  = float;
using f64  = double;
using f128 = __float128;

template<typename T>
using makeUnsigned = std::make_unsigned_t<T>;

template<typename T>
using makeSigned = std::make_signed_t<T>;

static_assert(sizeof(i8)   == 1,   "Invalid i8 size");
static_assert(sizeof(u8)   == 1,   "Invalid u8 size");
static_assert(sizeof(i16)  == 2,   "Invalid i16 size");
static_assert(sizeof(u16)  == 2,   "Invalid u16 size");
static_assert(sizeof(i32)  == 4,   "Invalid i32 size");
static_assert(sizeof(u32)  == 4,   "Invalid u32 size");
static_assert(sizeof(i64)  == 8,   "Invalid i64 size");
static_assert(sizeof(u64)  == 8,   "Invalid u64 size");
static_assert(sizeof(u128) == 16,  "Invalid u128 size");
static_assert(sizeof(i128) == 16,  "Invalid i128 size");
static_assert(sizeof(f32)  == 4,   "Invalid f32 size");
static_assert(sizeof(f64)  == 8,   "Invalid f64 size");
static_assert(sizeof(f128) == 16,  "Invalid f128 size");

#define val auto const
#define var auto

#define fun auto

#endif //DECLARATIONS_H
