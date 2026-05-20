#include "meld/cli/explain_module.hpp"
#include <iostream>
#include <algorithm>

namespace meld::cli {

// ═══════════════════════════════════════════════════════════════════════════
// DiagnosticRegistry — singleton with all known codes
// ═══════════════════════════════════════════════════════════════════════════

const DiagnosticRegistry& DiagnosticRegistry::instance() {
    static DiagnosticRegistry registry;
    return registry;
}

const DiagnosticEntry* DiagnosticRegistry::lookup(const std::string& code) const {
    auto it = entries_.find(code);
    return it != entries_.end() ? &it->second : nullptr;
}

std::vector<const DiagnosticEntry*> DiagnosticRegistry::all() const {
    std::vector<const DiagnosticEntry*> result;
    result.reserve(entries_.size());
    for (const auto& [_, entry] : entries_) {
        result.push_back(&entry);
    }
    std::sort(result.begin(), result.end(),
              [](const auto* a, const auto* b) { return a->code < b->code; });
    return result;
}

std::vector<const DiagnosticEntry*> DiagnosticRegistry::by_category(const std::string& category) const {
    std::vector<const DiagnosticEntry*> result;
    for (const auto& [_, entry] : entries_) {
        if (entry.category == category) {
            result.push_back(&entry);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto* a, const auto* b) { return a->code < b->code; });
    return result;
}

void DiagnosticRegistry::register_entry(DiagnosticEntry entry) {
    auto code = entry.code;
    entries_.emplace(std::move(code), std::move(entry));
}

DiagnosticRegistry::DiagnosticRegistry() {
    // ─── Top-level compiler errors (E001–E003) ───────────────────────────

    register_entry({
        "E001", "File Read Error",
        "The compiler could not read the specified source file.",
        {"File does not exist", "Permission denied", "Path is a directory"},
        "Verify the file path exists and is readable.",
        "meld run nonexistent.meld",
        "meld run src/main.meld",
        "error"
    });

    register_entry({
        "E002", "Parse Error",
        "The source code contains a syntax error that prevents parsing.",
        {
            "Missing closing brace or parenthesis",
            "Using keywords from other languages (if, else, while, return, fn, let)",
            "Missing rtn in function body",
            "Invalid operator or expression"
        },
        "Check syntax against Meld conventions: fnc (not fn), rtn (not return), "
        "val/var (not let/const), when().then().else() (not if/else).",
        "fnc add(a: int, b: int) -> int {\n    return a + b\n}",
        "fnc add(a: int, b: int) -> int {\n    rtn a + b\n}",
        "error"
    });

    register_entry({
        "E003", "Compilation Error",
        "A general compilation error occurred during semantic analysis.",
        {"Type mismatch", "Undefined symbol", "Arity mismatch"},
        "Check the specific error message for details. Run with --json for structured output.",
        "", "",
        "error"
    });

    // ─── Ownership/View errors (E4001–E4012) ─────────────────────────────

    register_entry({
        "E4001", "Invalid View Access",
        "A view (borrowed reference) was used in a context that requires ownership.",
        {
            "Storing a view into a container that outlives the borrow",
            "Returning a view from a function where the source is local",
            "Passing a view where an owned value is expected"
        },
        "Convert the view to an owned value, or restructure to keep the borrow within scope.",
        "fnc bad() -> View[Data] {\n    val local = Data { x = 1 }\n    rtn view(local)  // local dies here\n}",
        "fnc good() -> Data {\n    rtn Data { x = 1 }  // return owned value\n}",
        "error"
    });

    register_entry({
        "E4002", "Use After Move",
        "A value was used after being moved to another owner.",
        {
            "Passing a value to a function that takes ownership, then using it again",
            "Moving a value into a container, then accessing the original binding"
        },
        "Clone the value before the move, or restructure to avoid the second use.",
        "val x = Data { n = 1 }\nconsume(x)\nprintln(x.n)  // ERROR: x was moved",
        "val x = Data { n = 1 }\nval y = clone(x)\nconsume(y)\nprintln(x.n)  // OK: x still valid",
        "error"
    });

    register_entry({
        "E4003", "Container Constraint Violation",
        "A value placed in a container violates the container's ownership constraints.",
        {
            "Inserting a view into an owning container",
            "Mixing owned and borrowed values in a homogeneous collection"
        },
        "Ensure all values in the container match its ownership model.",
        "", "",
        "error"
    });

    register_entry({
        "E4010", "Mutability Violation",
        "Attempted to mutate a value declared as immutable (val).",
        {
            "Assigning to a val binding",
            "Mutating a field on a val struct",
            "Passing a val to a function that requires var"
        },
        "Change val to var if mutation is intended, or create a new value instead.",
        "val x = 5\nx = 10  // ERROR: x is immutable",
        "var x = 5\nx = 10  // OK: x is mutable",
        "error"
    });

    register_entry({
        "E4011", "Immutable Field Mutation",
        "Attempted to mutate a field on a struct bound with val.",
        {
            "Direct field assignment on a val binding",
            "Calling a mutating method on a val binding"
        },
        "Bind the struct with var, or create a new struct with the desired field value.",
        "val p = Point { x = 1, y = 2 }\np.x = 5  // ERROR",
        "var p = Point { x = 1, y = 2 }\np.x = 5  // OK",
        "error"
    });

    register_entry({
        "E4012", "Mutable Borrow Conflict",
        "Multiple mutable borrows of the same value exist simultaneously.",
        {"Taking two &var references to the same binding in overlapping scopes"},
        "Restructure to ensure only one mutable borrow exists at a time.",
        "", "",
        "error"
    });

    // ─── Safety/chaining errors (E4701–E4706) ────────────────────────────

    register_entry({
        "E4701", "Unsafe Chain Operation",
        "A chaining operation was used on a value that may be nil without a nil check.",
        {"Calling .method() on an Optional without unwrapping"},
        "Use the ?. operator or unwrap with a guard clause.",
        "val x = find(\"key\")\nx.process()  // x might be None",
        "val x = find(\"key\")\nwhen(x.is-some).ifTrue({ unwrap(x).process() })",
        "error"
    });

    register_entry({
        "E4702", "Unchecked Nil Access",
        "A value that may be nil was accessed without a nil check.",
        {
            "Using unwrap() without checking is-ok/is-some first",
            "Force-unwrapping a Result without error handling",
            "Chaining on an Optional without guard"
        },
        "Check for nil/error before accessing the value.",
        "val r = parse(input)\nprintln(unwrap(r))  // may panic",
        "val r = parse(input)\nwhen(r.is-ok).ifTrue({ println(unwrap(r)) })\n    .ifFalse({ println(\"error: \" + r.error) })",
        "error"
    });

    register_entry({
        "E4703", "Missing Return Path",
        "Not all code paths return a value in a function with a non-void return type.",
        {"Conditional without else branch", "Match without exhaustive cases"},
        "Ensure every code path ends with rtn.",
        "fnc abs(n: int) -> int {\n    when(n >= 0).then({ rtn n })\n    // missing else!\n}",
        "fnc abs(n: int) -> int {\n    rtn when(n >= 0).then({ rtn n }).else({ rtn -n })\n}",
        "error"
    });

    register_entry({
        "E4704", "Unsafe Return",
        "A function returns a value that may be invalid or dangling.",
        {"Returning a view to a local variable", "Returning uninitialized data"},
        "Return an owned value or ensure the referenced data outlives the caller.",
        "", "",
        "error"
    });

    register_entry({
        "E4706", "Unhandled Error Propagation",
        "A fallible function's error is not handled or propagated.",
        {
            "Calling a function that returns Result without checking the result",
            "Ignoring the error case in a Result chain"
        },
        "Handle the error with when(r.is-ok) or propagate it to the caller.",
        "val data = read-file(path)  // Result ignored",
        "val data = read-file(path)\nrtn when(data.is-ok == false).then({ rtn data }).else({ ... })",
        "error"
    });

    // ─── Mutability checker (E5001–E5004) ────────────────────────────────

    register_entry({
        "E5001", "Mutation of Immutable Parameter",
        "A function parameter declared as immutable was mutated inside the function body.",
        {"Assigning to a parameter", "Calling a mutating method on a parameter"},
        "Declare the parameter as var, or create a local copy.",
        "fnc inc(x: int) -> int {\n    x = x + 1  // ERROR: x is immutable param\n    rtn x\n}",
        "fnc inc(x: int) -> int {\n    var local = x\n    local = local + 1\n    rtn local\n}",
        "error"
    });

    register_entry({
        "E5002", "Parameter Mutability Mismatch",
        "A value was passed to a parameter expecting a different mutability qualifier.",
        {"Passing a val where var is required", "Passing a mutable ref where const ref is expected"},
        "Match the mutability of the argument to the parameter declaration.",
        "", "",
        "error"
    });

    register_entry({
        "E5003", "Trait Method Mutability Violation",
        "A trait implementation's method has different mutability than the trait declares.",
        {"Trait declares immutable self, implementation mutates self"},
        "Match the mutability annotation of the trait method signature.",
        "", "",
        "error"
    });

    register_entry({
        "E5004", "Missing Trait Method",
        "A type claims to implement a trait but is missing a required method.",
        {"Forgot to implement a method", "Method has wrong signature"},
        "Implement all required methods with matching signatures.",
        "", "",
        "error"
    });

    // ─── Warnings (W001–W003, W4001–W5002) ──────────────────────────────

    register_entry({
        "W002", "MELD-B Generation Failed",
        "The binary artifact (.meld-b) could not be generated, but compilation succeeded.",
        {"Disk full", "Permission error on output directory"},
        "Check output directory permissions and available disk space.",
        "", "",
        "warning"
    });

    register_entry({
        "W003", "Semantic Graph Build Failed",
        "The semantic graph for IDE features could not be built.",
        {"Complex or unsupported AST patterns", "Internal analyzer limitation"},
        "This is non-fatal. The program will still run correctly.",
        "", "",
        "warning"
    });

    register_entry({
        "W4001", "Ownership Cycle Detected",
        "A cycle in ownership references was detected, which may cause memory leaks.",
        {"Struct A owns Struct B which owns Struct A", "Self-referential data structures"},
        "Break the cycle with a weak reference or restructure the data model.",
        "", "",
        "warning"
    });

    register_entry({
        "W4002", "Unnecessary Clone",
        "A value was cloned but the original is never used again.",
        {"Cloning before a move where the move alone would suffice"},
        "Remove the clone — the value can be moved directly.",
        "", "",
        "warning"
    });

    register_entry({
        "W4707", "Force Unwrap",
        "A force-unwrap (!!) was used, which will panic at runtime if the value is nil.",
        {"Using !! on a Result or Option without prior nil check"},
        "Prefer explicit error handling with when().then().else().",
        "val x = find(key)!!  // panics if None",
        "val x = find(key)\nval value = when(x.is-some).then({ rtn unwrap(x) }).else({ rtn default })",
        "warning"
    });

    register_entry({
        "W5001", "Unused Mutable Binding",
        "A variable declared with var is never mutated.",
        {"Declared var but only read the value"},
        "Change var to val if mutation is not needed.",
        "var x = 42\nprintln(x)  // x is never reassigned",
        "val x = 42\nprintln(x)",
        "warning"
    });

    register_entry({
        "W5002", "Parameter Could Be Immutable",
        "A var parameter is never mutated inside the function body.",
        {"Declared parameter as mutable but only read it"},
        "Remove the var qualifier from the parameter.",
        "", "",
        "warning"
    });

    // ─── Info (I001, I4010–I4011) ────────────────────────────────────────

    register_entry({
        "I001", "Binary Generated",
        "A MELD-B binary artifact was successfully generated.",
        {},
        "",
        "", "",
        "info"
    });

    register_entry({
        "I4010", "Migration Hint: Handle Syntax",
        "Code uses the old effect handler syntax. The new handle block syntax is available.",
        {"Using legacy perform/resume pattern"},
        "Migrate to the new handle { ... } block syntax.",
        "perform Console.println(msg)",
        "handle Console {\n    fnc println(msg: string) -> () { ... }\n}",
        "info"
    });

    register_entry({
        "I4011", "Migration Hint: Effect Declaration",
        "Code uses the old effect declaration syntax.",
        {"Using legacy @effect annotation"},
        "Migrate to the new effect keyword syntax.",
        "@effect Console",
        "effect Console {\n    fnc println(msg: string) -> ()\n}",
        "info"
    });

    register_entry({
        "E6001", "Undeclared Effect Usage",
        "A function calls an effect that is not listed in its @uses annotation. "
        "Meld is pure-by-default: functions without @uses cannot perform any effects.",
        {
            "Function has no @uses annotation but calls an effect",
            "Function has @uses(Console) but calls FileSystem",
            "Forgot to add the effect to the @uses list"
        },
        "Add @uses(EffectName) before the function declaration.",
        "fnc greet(name: string) -> () {\n    Console.println(\"Hello\")\n}",
        "@uses(Console)\nfnc greet(name: string) -> () {\n    Console.println(\"Hello\")\n}",
        "error"
    });
}

// ═══════════════════════════════════════════════════════════════════════════
// ExplainModule
// ═══════════════════════════════════════════════════════════════════════════

ExplainModule::ExplainModule()
    : BaseCommandHandler("explain", "Explain a diagnostic code") {}

CommandResult ExplainModule::execute(const CommandArgs& args) {
    // JSON is the default output; --text for human-readable
    bool text = args.flags.count("text") > 0 || args.options.count("text") > 0;
    bool list = args.flags.count("list") > 0 || args.options.count("list") > 0;

    // Also check positional args for flags (belt and suspenders)
    for (const auto& arg : args.positional) {
        if (arg == "--text") text = true;
        if (arg == "--list") list = true;
    }
    bool json = !text;

    if (list) {
        print_list(json);
        return CommandResult::Success;
    }

    // Find the code (first positional that doesn't start with --)
    std::string code;
    for (const auto& arg : args.positional) {
        if (arg.substr(0, 2) != "--") {
            code = arg;
            break;
        }
    }

    if (code.empty()) {
        std::cerr << "Usage: meld explain <diagnostic-code>\n"
                  << "       meld explain --list\n"
                  << "Example: meld explain E001\n";
        return CommandResult::InvalidArguments;
    }

    const auto* entry = DiagnosticRegistry::instance().lookup(code);

    if (!entry) {
        std::cerr << "Unknown diagnostic code: " << code << "\n";
        std::cerr << "Run 'meld explain --list' to see all known codes.\n";
        return CommandResult::NotFound;
    }

    if (json) {
        print_json(*entry);
    } else {
        print_human(*entry);
    }
    return CommandResult::Success;
}

void ExplainModule::print_human(const DiagnosticEntry& entry) const {
    std::cout << entry.code << ": " << entry.title << "\n\n";
    std::cout << "  " << entry.description << "\n\n";

    if (!entry.causes.empty()) {
        std::cout << "  Common causes:\n";
        for (const auto& cause : entry.causes) {
            std::cout << "    - " << cause << "\n";
        }
        std::cout << "\n";
    }

    if (!entry.fix_pattern.empty()) {
        std::cout << "  Fix:\n    " << entry.fix_pattern << "\n\n";
    }

    if (!entry.example_bad.empty()) {
        std::cout << "  Before:\n";
        std::cout << "    " << entry.example_bad << "\n\n";
    }

    if (!entry.example_good.empty()) {
        std::cout << "  After:\n";
        std::cout << "    " << entry.example_good << "\n";
    }
}

void ExplainModule::print_json(const DiagnosticEntry& entry) const {
    std::cout << "{\n";
    std::cout << "  \"code\": \"" << entry.code << "\",\n";
    std::cout << "  \"title\": \"" << entry.title << "\",\n";
    std::cout << "  \"category\": \"" << entry.category << "\",\n";
    std::cout << "  \"description\": \"" << entry.description << "\",\n";

    std::cout << "  \"causes\": [";
    for (size_t i = 0; i < entry.causes.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << "\"" << entry.causes[i] << "\"";
    }
    std::cout << "],\n";

    std::cout << "  \"fix_pattern\": \"" << entry.fix_pattern << "\",\n";
    std::cout << "  \"example_bad\": \"" << entry.example_bad << "\",\n";
    std::cout << "  \"example_good\": \"" << entry.example_good << "\"\n";
    std::cout << "}\n";
}

void ExplainModule::print_list(bool json) const {
    auto entries = DiagnosticRegistry::instance().all();

    if (json) {
        std::cout << "[\n";
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i > 0) std::cout << ",\n";
            std::cout << "  {\"code\": \"" << entries[i]->code
                      << "\", \"title\": \"" << entries[i]->title
                      << "\", \"category\": \"" << entries[i]->category << "\"}";
        }
        std::cout << "\n]\n";
    } else {
        std::cout << "Known diagnostic codes:\n\n";
        std::string last_category;
        for (const auto* entry : entries) {
            if (entry->category != last_category) {
                if (!last_category.empty()) std::cout << "\n";
                std::string header = entry->category;
                header[0] = static_cast<char>(std::toupper(header[0]));
                std::cout << header << "s:\n";
                last_category = entry->category;
            }
            std::cout << "  " << entry->code << "  " << entry->title << "\n";
        }
    }
}

std::string ExplainModule::get_help() const {
    return R"(Usage: meld explain <diagnostic-code> [--text]
       meld explain --list [--text]

Look up a diagnostic code and get a rich, actionable explanation.
Output is JSON by default (for AI agents). Use --text for human-readable.

Arguments:
  <code>     Diagnostic code (e.g., E001, E4001, W4707)

Flags:
  --text     Output as human-readable plain text
  --list     List all known diagnostic codes

Examples:
  meld explain E001              JSON explanation (default)
  meld explain E4001 --text      Human-readable explanation
  meld explain --list            All codes as JSON array
  meld explain --list --text     All codes as plain text)";
}

std::string ExplainModule::get_usage() const {
    return "meld explain <code> [--text]";
}

std::vector<std::string> ExplainModule::get_completions(const std::string& partial) const {
    std::vector<std::string> completions;
    auto entries = DiagnosticRegistry::instance().all();
    for (const auto* entry : entries) {
        if (entry->code.find(partial) == 0) {
            completions.push_back(entry->code);
        }
    }
    completions.push_back("--list");
    completions.push_back("--text");
    return completions;
}

bool ExplainModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    bool has_list = args.flags.count("list") > 0 || args.options.count("list") > 0;
    if (args.positional.empty() && !has_list) {
        error_message = "Expected a diagnostic code or --list flag";
        return false;
    }
    return true;
}

} // namespace meld::cli
