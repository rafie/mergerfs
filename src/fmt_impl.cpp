/*
  Compile the vendored fmt library for linking.

  The fmt headers are used in header-only mode throughout mergerfs, but
  fmt::vprintln (and a few template instantiations) require a compiled
  translation unit.  This file wraps the vendored format.cpp so it
  participates in the normal src/*.cpp build.
*/

// Pull in the fmt implementation (defines FMT_FUNC symbols)
#include "fmt/format-inl.h"

FMT_BEGIN_NAMESPACE

#if FMT_USE_LOCALE
template FMT_API locale_ref::locale_ref(const std::locale& loc);
template FMT_API auto locale_ref::get<std::locale>() const -> std::locale;
#endif

namespace detail {

template FMT_API auto dragonbox::to_decimal(float x) noexcept
    -> dragonbox::decimal_fp<float>;
template FMT_API auto dragonbox::to_decimal(double x) noexcept
    -> dragonbox::decimal_fp<double>;

template FMT_API auto thousands_sep_impl(locale_ref)
    -> thousands_sep_result<char>;
template FMT_API auto decimal_point_impl(locale_ref) -> char;

template FMT_API void buffer<char>::append(const char*, const char*);

template FMT_API auto thousands_sep_impl(locale_ref)
    -> thousands_sep_result<wchar_t>;
template FMT_API auto decimal_point_impl(locale_ref) -> wchar_t;

template FMT_API void buffer<wchar_t>::append(const wchar_t*, const wchar_t*);

}  // namespace detail
FMT_END_NAMESPACE
