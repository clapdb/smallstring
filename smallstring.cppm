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

// fmt is textual, here and everywhere else in smallstring -- the header we are about to read
// specialises fmt::formatter, and it is the one heavy header that has to stay in this fragment. The
// note at the top of include/smallstring.hpp explains why it is never an `import fmt;`, and asserts
// the FMT_ATTACH_TO_GLOBAL_MODULE that lets a textual read here meet an imported fmt elsewhere.
#include <fmt/format.h>

export module smallstring;

import std.compat;

#define SMALLSTRING_MODULE_INTERFACE 1
export {
#include "smallstring.hpp"
}
#undef SMALLSTRING_MODULE_INTERFACE
