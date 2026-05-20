#include "meld/cli/guide_module.hpp"
#include <iostream>
#include <unordered_map>

namespace meld::cli {

const char* GuideModule::version() { return "0.1.0"; }

// ═══════════════════════════════════════════════════════════════════════════
// Embedded skill content — version-matched to this binary
// ═══════════════════════════════════════════════════════════════════════════

static const std::unordered_map<std::string, std::string> skill_content = {

{"syntax", R"(# Meld Syntax (v0.1.0)

## Keywords (only 5 reserved words)
  fnc  val  var  rtn  imp

## Critical Rules for Agents
- NO if/else/while/for/match keywords — use library functions
- NO return — use rtn
- NO fn/func/def/function — use fnc
- NO let/const — use val (immutable) / var (mutable)
- NO import — use imp
- NO => in lambdas — use ->
- NO semicolons

## Function Declaration
  fnc name(param: type, ...) -> ReturnType {
      rtn value
  }

## Variable Binding
  val x = 42          // immutable
  var y = 0           // mutable

## Struct Creation
  val p = Point { x = 3, y = 4 }

## Method Call (dot syntax)
  // fnc length(v: Vec2) -> float { ... }
  my_vec.length()     // calls length(my_vec)

## Lambda
  fnc(x: int) -> int { rtn x * 2 }

## Template String
  `Hello ${name}, result is ${2 + 2}`
)"},

{"control-flow", R"(# Meld Control Flow (v0.1.0)

## Conditional Expression
  when(condition).then({ rtn value_if_true }).else({ rtn value_if_false })

## Chained Conditions
  when(x > 100).then({ rtn "large" })
      .when(x > 10).then({ rtn "medium" })
      .else({ rtn "small" })

## Side-Effect Conditional
  when(flag).ifTrue({ println("yes") })
  when(flag).ifFalse({ println("no") })

## Iteration
  forEach(list, fnc(item: any) -> () { println(item) })

## Transform
  map(list, fnc(x: any) -> any { rtn x * 2 })
  filter(list, fnc(x: any) -> bool { rtn x > 0 })
  reduce(list, 0, fnc(acc: any, x: any) -> any { rtn acc + x })

## Pattern Matching
  match(value, [
      [pattern1, { rtn result1 }],
      [pattern2, { rtn result2 }]
  ])

## WRONG (do NOT use):
  if (x > 0) { ... }           // NOT VALID
  while (running) { ... }      // NOT VALID
  for (item in list) { ... }   // NOT VALID
)"},

{"errors", R"(# Meld Error Handling (v0.1.0)

## Result Type
  Ok(value)           // success
  Err(message)        // failure
  unwrap(result)      // extract (panics on Err)
  result.is-ok        // check success
  result.error        // get error message

## Option Type
  Some(value)         // present
  None                // absent
  unwrap(option)      // extract (panics on None)

## Error Propagation Pattern
  fnc process(input: string) -> Result {
      val parsed = parse(input)
      rtn when(parsed.is-ok == false).then({ rtn parsed }).else({
          rtn validate(unwrap(parsed))
      })
  }

## Assertions
  assert(condition, "message")
  panic("fatal error message")
)"},

{"stdlib", R"(# Meld Standard Library (v0.1.0)

## Output
  println(value)              // print + newline
  print(value)                // print (no newline)

## Collections
  len(collection)             // length of string or list
  push(list, item)            // new list with item appended
  forEach(list, fn)           // iterate
  map(list, fn)               // transform
  filter(list, fn)            // select
  reduce(list, init, fn)      // fold

## Strings
  len(s)                      // string length
  contains(s, substr)         // substring check
  starts-with(s, prefix)      // prefix check
  ends-with(s, suffix)        // suffix check
  replace(s, old, new)        // replace all
  split(s, delimiter)         // split to list
  substr(s, start, length)    // extract substring

## Type Introspection
  type-of(value)              // returns type name as string

## Effects
  Console.println(msg)        // print via effect system
)"},

{"diagnostics", R"(# Meld Diagnostics (v0.1.0)

## Running with JSON output
  meld run file.meld --json

## Diagnostic codes
  E001  File Read Error
  E002  Parse Error
  E003  Compilation Error
  E4001 Invalid View Access
  E4002 Use After Move
  E4010 Mutability Violation
  E4702 Unchecked Nil Access
  E4703 Missing Return Path
  E4706 Unhandled Error Propagation
  W4707 Force Unwrap
  W5001 Unused Mutable Binding

## Explain a code
  meld explain E002
  meld explain E002 --json

## Get fix plan
  meld fix --plan --json file.meld
)"},

{"workflow", R"(# Meld Agent Workflow (v0.1.0)

## Edit Loop
  1. Write .meld code (use fnc/val/var/rtn, NOT fn/let/return)
  2. Run: meld check file.meld
  3. Parse JSON diagnostics (E6001 = undeclared effect, E002 = parse error)
  4. If errors: meld fix --plan file.meld
  5. Apply fixes from the plan
  6. Repeat until "ok": true
  7. Execute: meld run file.meld

## Key Commands
  meld check file.meld            Check without executing (JSON default)
  meld run file.meld              Execute program
  meld explain <code>             Look up diagnostic code
  meld fix --plan file.meld       Get actionable fix plan
  meld guide all                  Full agent guidance

## Common Mistakes to Avoid
  - Using if/else/while/for (use when/then/else, forEach)
  - Using return (use rtn)
  - Using fn/func (use fnc)
  - Using let/const (use val/var)
  - Using => in lambdas (use ->)
  - Forgetting rtn in function body
  - Using import (use imp)
  - Calling effects without @uses annotation (E6001)

## Effect Declaration Pattern
  @uses(Console)
  fnc my-function() -> () {
      Console.println("declared and allowed")
  }

## Method Definition Pattern
  fnc method-name(self: MyType, arg: int) -> string {
      rtn self.field + arg
  }
  my_instance.method-name(42)
)"},

{"builds", R"(# Meld Build & Package (v0.1.0)

## Run a program
  meld run file.meld
  meld run file.meld --json       // structured output
  meld run file.meld --debug      // DAP debugger
  meld run -i                     // REPL

## Project structure
  project/
    meld.toml                     // manifest
    src/
      main.meld                   // entry point
      helpers.meld                // imp helpers

## Module imports
  imp module_name                 // loads module_name.meld from same dir

## Testing
  meld test file.meld
  meld test file.meld --json
)"}

};

// ═══════════════════════════════════════════════════════════════════════════
// GuideModule implementation
// ═══════════════════════════════════════════════════════════════════════════

GuideModule::GuideModule()
    : BaseCommandHandler("guide", "Version-matched language guidance") {}

CommandResult GuideModule::execute(const CommandArgs& args) {
    // JSON is the default output; --text for human-readable
    bool text = args.flags.count("text") > 0 || args.options.count("text") > 0;
    for (const auto& arg : args.positional) {
        if (arg == "--text") text = true;
    }
    bool json = !text;

    // meld guide (no args) — list topics
    if (args.positional.empty()) {
        print_topics();
        return CommandResult::Success;
    }

    // meld guide get <topic>
    std::string topic;
    bool get_mode = false;
    for (const auto& arg : args.positional) {
        if (arg == "get") { get_mode = true; continue; }
        if (arg.substr(0, 2) != "--") { topic = arg; }
    }

    if (!get_mode && !topic.empty()) {
        // Allow "meld guide syntax" as shorthand for "meld guide get syntax"
        topic = args.positional[0];
        if (topic.substr(0, 2) == "--") topic = "";
    }

    if (topic.empty()) {
        print_topics();
        return CommandResult::Success;
    }

    if (topic == "all" || topic == "--full") {
        print_all(json);
    } else {
        print_topic(topic, json);
    }
    return CommandResult::Success;
}

void GuideModule::print_topics() const {
    std::cout << "Meld Guide (v" << version() << ")\n\n";
    std::cout << "Available topics:\n";
    std::cout << "  syntax        Language syntax and keywords\n";
    std::cout << "  control-flow  Conditionals, iteration, matching\n";
    std::cout << "  errors        Result, Option, error handling\n";
    std::cout << "  stdlib        Built-in functions reference\n";
    std::cout << "  diagnostics   Diagnostic codes and tools\n";
    std::cout << "  workflow      Agent edit loop and commands\n";
    std::cout << "  builds        Running, testing, project structure\n";
    std::cout << "\nUsage:\n";
    std::cout << "  meld guide syntax               Get one topic (JSON)\n";
    std::cout << "  meld guide all                  Get all topics (JSON)\n";
    std::cout << "  meld guide syntax --text        Human-readable\n";
}

void GuideModule::print_topic(const std::string& topic, bool json) const {
    auto it = skill_content.find(topic);
    if (it == skill_content.end()) {
        std::cerr << "Unknown topic: " << topic << "\n";
        std::cerr << "Run 'meld skills' to see available topics.\n";
        return;
    }

    if (json) {
        std::cout << "{\n";
        std::cout << "  \"version\": \"" << version() << "\",\n";
        std::cout << "  \"topic\": \"" << topic << "\",\n";
        // Escape newlines for JSON
        std::string content = it->second;
        std::string escaped;
        for (char c : content) {
            if (c == '\n') escaped += "\\n";
            else if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else escaped += c;
        }
        std::cout << "  \"content\": \"" << escaped << "\"\n";
        std::cout << "}\n";
    } else {
        std::cout << it->second << "\n";
    }
}

void GuideModule::print_all(bool json) const {
    if (json) {
        std::cout << "{\n";
        std::cout << "  \"version\": \"" << version() << "\",\n";
        std::cout << "  \"topics\": {\n";
        bool first = true;
        for (const auto& [topic, content] : skill_content) {
            if (!first) std::cout << ",\n";
            first = false;
            std::string escaped;
            for (char c : content) {
                if (c == '\n') escaped += "\\n";
                else if (c == '"') escaped += "\\\"";
                else if (c == '\\') escaped += "\\\\";
                else escaped += c;
            }
            std::cout << "    \"" << topic << "\": \"" << escaped << "\"";
        }
        std::cout << "\n  }\n}\n";
    } else {
        std::cout << "=== Meld Guide (v" << version() << ") — Full Reference ===\n\n";
        // Print in logical order
        static const std::vector<std::string> order = {
            "syntax", "control-flow", "errors", "stdlib", "diagnostics", "workflow", "builds"
        };
        for (const auto& topic : order) {
            auto it = skill_content.find(topic);
            if (it != skill_content.end()) {
                std::cout << it->second << "\n";
            }
        }
    }
}

std::string GuideModule::get_help() const {
    return R"(Usage: meld guide [topic] [--text]

Emit version-matched language guidance directly from the CLI.
Output is JSON by default. Use --text for human-readable.

Topics:
  syntax        Language syntax and keywords
  control-flow  Conditionals, iteration, matching
  errors        Result, Option, error handling
  stdlib        Built-in functions reference
  diagnostics   Diagnostic codes and tools
  workflow      Agent edit loop and commands
  builds        Running, testing, project structure
  all           All topics combined

Examples:
  meld guide                      List topics
  meld guide syntax               JSON (default)
  meld guide syntax --text        Human-readable
  meld guide all                  Full reference as JSON)";
}

std::string GuideModule::get_usage() const {
    return "meld guide [topic] [--text]";
}

std::vector<std::string> GuideModule::get_completions(const std::string& partial) const {
    return {"get", "syntax", "control-flow", "errors", "stdlib",
            "diagnostics", "workflow", "builds", "all", "--text"};
}

bool GuideModule::validate_args(const CommandArgs& args, std::string& error_message) const {
    return true;
}

} // namespace meld::cli
