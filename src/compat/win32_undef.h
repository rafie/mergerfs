/*
  Undefine Windows macros that conflict with mergerfs identifiers.
  Include this after <windows.h> in any compat header.
*/

#pragma once

#ifdef _WIN32

/* wingdi.h defines PASSTHROUGH as 19 — conflicts with mergerfs enums */
#ifdef PASSTHROUGH
#undef PASSTHROUGH
#endif

/* Other Windows macro conflicts */
#ifdef DELETE
#undef DELETE
#endif

#ifdef ERROR
#undef ERROR
#endif

#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

#ifdef RELATIVE
#undef RELATIVE
#endif

#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

#ifdef small
#undef small
#endif

#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

/* sal.h annotation macros — expand to empty, break struct field names */
#ifdef __reserved
#undef __reserved
#endif

#endif /* _WIN32 */
