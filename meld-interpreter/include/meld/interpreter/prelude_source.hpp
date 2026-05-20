#pragma once
#include <string_view>
namespace meld::interpreter {
inline constexpr std::string_view PRELUDE_SOURCE = R"MELD(
// ─── Conditionals ───────────────────────────────────────────────────
// when/then/else are native builtins dispatched through normal method dispatch.
// NOTE: In when chains, all conditions are evaluated eagerly. The .then()
// bodies are lazy (only called if matched), but conditions like .when(expr)
// evaluate expr even if an earlier branch already matched. Avoid side effects
// in chained conditions.

fnc ifTrue(cond: bool, body: any) -> any {
    rtn when(cond).then(body)
}

fnc ifFalse(cond: bool, body: any) -> any {
    rtn when(cond == false).then(body)
}

// ─── Collections ────────────────────────────────────────────────────
// forEach, map, filter, reduce, match are native builtins (iterative, no stack overflow).

fnc push(arr: list, item: any) -> list { rtn arr-push(arr, item) }

// ─── Assertions ─────────────────────────────────────────────────────

fnc assert(cond: bool, msg: string) -> () { when(cond == false).then({ panic(msg) }) }

// ─── Result / Option ────────────────────────────────────────────────

fnc Ok(v: any) -> Result { rtn Result { __type__ = "Ok", value = v, error = nil, is-ok = true } }
fnc Err(e: any) -> Result { rtn Result { __type__ = "Err", value = nil, error = e, is-ok = false } }
fnc Some(v: any) -> Option { rtn Option { __type__ = "Some", value = v, is-some = true } }
val None = Option { __type__ = "None", value = nil, is-some = false }
fnc unwrap(opt: any) -> any { rtn opt.value }

// ─── String utilities ───────────────────────────────────────────────

fnc contains(s: string, needle: string) -> bool { rtn str-find(s, needle) >= 0 }
fnc starts-with(s: string, prefix: string) -> bool { rtn substr(s, 0, len(prefix)) == prefix }
fnc ends-with(s: string, suffix: string) -> bool {
    val start = len(s) - len(suffix)
    rtn when(start < 0).then({ rtn false }).else({ rtn substr(s, start, len(suffix)) == suffix })
}
)MELD";
} // namespace meld::interpreter
