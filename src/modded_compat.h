#ifndef SHANDALAR_MODDED_COMPAT_H
#define SHANDALAR_MODDED_COMPAT_H

#ifdef __cplusplus
template<typename T> struct ModdedSafeBool
{
  T value;
  constexpr explicit ModdedSafeBool(T v): value(v) {}
  constexpr operator T() const { return value; }
  constexpr explicit operator bool() const { return static_cast<int>(value) != 0; }
  constexpr explicit operator int() const { return static_cast<int>(value); }
};

#define OP_BITWISE(T) \
inline constexpr T operator|(T lhs, T rhs) { return static_cast<T>(static_cast<int>(lhs) | static_cast<int>(rhs)); } \
inline T& operator|=(T& lhs, T rhs) { return lhs = lhs | rhs; } \
inline constexpr T operator^(T lhs, T rhs) { return static_cast<T>(static_cast<int>(lhs) ^ static_cast<int>(rhs)); } \
inline T& operator^=(T& lhs, T rhs) { return lhs = lhs ^ rhs; } \
inline constexpr ModdedSafeBool<T> operator&(T lhs, T rhs) { return ModdedSafeBool<T>(static_cast<T>(static_cast<int>(lhs) & static_cast<int>(rhs))); } \
inline T& operator&=(T& lhs, T rhs) { return lhs = static_cast<T>(static_cast<int>(lhs) & static_cast<int>(rhs)); } \
inline constexpr T operator~(T value) { return static_cast<T>(~static_cast<int>(value)); }
#endif

#endif
