// Module interface for smallstring.
//
// The header is the single source of truth; this unit only attaches its declarations to module
// `smallstring`. See the note at the top of include/smallstring.hpp for the ownership rule.
module;

// smallstring deliberately calls Assert(), not assert(), so the consumer can substitute its own
// assertion. Textually that was done by #define-ing Assert before including the header. Modules take
// that away: the header is an `import` at the consumer, and its macros are baked in *here*, when the
// interface is compiled. So the hook has to live here.
//
// Point SMALLSTRING_PRELUDE at a header that #define-s Assert (and includes whatever that needs).
// Without it, smallstring.hpp falls back to plain assert() as before.
#ifdef SMALLSTRING_PRELUDE
#include SMALLSTRING_PRELUDE
#endif

// std comes in as `import std.compat` below, NOT as textual libstdc++ headers here.
//
// Textual <stdexcept> in this fragment would bake libstdc++'s <string> declarations into the BMI as
// global-module entities. Any consumer that then reads <string> textually -- directly, or through
// <fmt/format.h> -- re-declares basic_string.tcc's explicit instantiations on top of them and clang
// rejects it: "explicit instantiation of 'getline' does not refer to a function template". It reaches
// consumers that never name smallstring, too, because a module that imports smallstring carries those
// declarations onward in its own BMI.
#include <sys/types.h>

// fmt is textual here, and stays textual even when the consumer imports it (STDB_USE_FMT_MODULE).
// That asymmetry with the shim in smallstring.hpp is deliberate, and it rests on one thing:
//
//   fmt's module must be built with FMT_ATTACH_TO_GLOBAL_MODULE.
//
// That macro detaches every fmt declaration from module `fmt` (fmt's own words: "you can mix TUs
// with either importing or #including the {fmt} API"). So the fmt::formatter this fragment sees and
// the fmt::formatter an importing consumer sees are the *same* global-module entity, and the
// fmt::formatter<basic_small_string> specialisation exported below is the one the consumer finds.
//
// It cannot be done the other way round. Reaching fmt by `import fmt;` here does not compile: the
// specialisation derives from fmt::formatter<std::string_view>, and through an import that base
// resolves to fmt's *primary* template -- "no member named 'parse'", deleted constructor. That is a
// property of fmt itself, not of smallstring; a TU that does nothing but `import fmt;` and name
// fmt::formatter<std::string_view> fails the same way, while the identical TU with a textual
// <fmt/format.h> compiles.
//
// Without FMT_ATTACH_TO_GLOBAL_MODULE the mix really is unsound -- the consumer's `import fmt;`
// meets the global-module fmt declarations baked into this BMI and clang rejects it, "declaration
// 'basic_appender' attached to named module 'fmt' cannot be attached to other modules" -- so say so
// here rather than let it surface as that error in someone else's translation unit.
#if defined(STDB_USE_FMT_MODULE) && !defined(FMT_ATTACH_TO_GLOBAL_MODULE)
#error "module smallstring needs fmt's module built with FMT_ATTACH_TO_GLOBAL_MODULE: it specialises fmt::formatter through a textual <fmt/format.h>, which only matches an imported fmt when fmt's declarations are attached to the global module."
#endif
#include <fmt/format.h>

export module smallstring;

import std.compat;

#define SMALLSTRING_MODULE_INTERFACE 1
export {
#include "smallstring.hpp"
}
#undef SMALLSTRING_MODULE_INTERFACE
