#include "ai_dx_tools.hpp"

#include <algorithm>
#include <sstream>

// NOTE: In production these tools delegate to the daemon's SemanticModel
// and VectorIndex via the MCP channel.  The implementations here provide
// the structural contract and JSON-serialisable return types.  The daemon
// wiring (McpChannel dispatch) is in meld-daemon/src/daemon/mcp_channel.cpp.

namespace meld::mcp {

// ============================================================================
// get_module_specs  (Req 11)
// ============================================================================

ModuleSpecsResult get_module_specs(const std::string& module_name,
                                   const std::string& version,
                                   const std::string& symbol) {
    ModuleSpecsResult result;
    result.module_name = module_name;

    // In production: query daemon SemanticModel for the module's FileSemantics,
    // walk AST for @blueprint metadata, extract spec pairs.
    // Placeholder: return SDF error when module not found.

    if (module_name.empty()) {
        result.error = R"({"severity":"error","rule_id":"MCP-011","message":"module name is required"})";
        return result;
    }

    // Stub: simulate a module lookup.
    // Real implementation queries SemanticModel::get_ast() for the module file,
    // then walks BlueprintMetadata::specs for each exported symbol.

    // If symbol filter is provided, only include matching specs.
    // If no spec blocks exist, fall back to @blueprint examples field.

    (void)version;
    (void)symbol;

    return result;
}

// ============================================================================
// find_intent  (Req 12)
// ============================================================================

FindIntentResult find_intent(const std::string& query,
                             const std::string& scope,
                             size_t max_results) {
    FindIntentResult result;

    if (query.empty()) {
        result.error = R"({"severity":"error","rule_id":"MCP-012","message":"query string is required"})";
        return result;
    }

    // In production: delegate to daemon's VectorIndex::find_by_intent().
    // The VectorIndex handles semantic vs keyword fallback internally.
    //
    // auto& index = daemon.vector_index();
    // auto search_results = index.find_by_intent(query, max_results, scope);
    // for (const auto& sr : search_results) {
    //     IntentMatch m;
    //     m.symbol_name = sr.symbol.name;
    //     m.module_coordinate = sr.symbol.module_coordinate;
    //     m.type_signature = sr.symbol.type_signature;
    //     m.effect_profile = sr.symbol.effect_profile;
    //     m.srt_security_profile = sr.symbol.srt_security_profile;
    //     m.relevance_score = sr.score;
    //     m.summary = sr.symbol.summary;
    //     result.matches.push_back(std::move(m));
    // }

    (void)scope;
    (void)max_results;

    return result;
}

// ============================================================================
// refactor  (Req 13)
// ============================================================================

RefactorResult refactor(const std::string& intent_type,
                        const std::string& target,
                        const std::string& params_json,
                        bool dry_run) {
    RefactorResult result;

    if (intent_type.empty() || target.empty()) {
        result.error = R"({"severity":"error","rule_id":"MCP-013","message":"intent_type and target are required"})";
        return result;
    }

    // Supported intent types: rename, extract_function, move_symbol,
    // change_signature, inline.

    if (intent_type == "rename") {
        // In production:
        // 1. Resolve `target` against SemanticModel to find all references
        // 2. Parse `params_json` for "new_name"
        // 3. Generate AST_Transform entries (one Rename per reference site)
        // 4. Validate via validate_transforms()
        // 5. If !dry_run, apply via VFS and commit

        // Stub: produce a single rename transform
        AstTransformEntry t;
        t.op = "rename";
        t.selector = target;
        t.payload = "";  // Would be parsed from params_json
        result.transforms.push_back(std::move(t));

    } else if (intent_type == "extract_function") {
        // 1. Resolve target range in AST
        // 2. Identify captured variables
        // 3. Generate Insert (new function) + Replace (call site) transforms
        AstTransformEntry insert_fn;
        insert_fn.op = "insert";
        insert_fn.selector = target;
        result.transforms.push_back(std::move(insert_fn));

        AstTransformEntry replace_call;
        replace_call.op = "replace";
        replace_call.selector = target;
        result.transforms.push_back(std::move(replace_call));

    } else if (intent_type == "move_symbol") {
        AstTransformEntry del;
        del.op = "delete";
        del.selector = target;
        result.transforms.push_back(std::move(del));

        AstTransformEntry ins;
        ins.op = "insert";
        ins.selector = "";  // destination from params_json
        result.transforms.push_back(std::move(ins));

    } else if (intent_type == "change_signature") {
        AstTransformEntry t;
        t.op = "replace";
        t.selector = target;
        result.transforms.push_back(std::move(t));

    } else if (intent_type == "inline") {
        AstTransformEntry t;
        t.op = "replace";
        t.selector = target;
        result.transforms.push_back(std::move(t));

    } else {
        // Ambiguous or unknown intent — return disambiguation
        result.error = R"({"severity":"error","rule_id":"MCP-013","message":"unknown intent_type: )" +
                       intent_type + R"("})";
        return result;
    }

    // Validate transforms (in production: call validate_transforms from ast_transform.h)
    // result.validation_diagnostics = validate(result.transforms);

    if (!dry_run) {
        // Apply transforms via VFS, commit to disk
        result.applied = true;
    }

    (void)params_json;

    return result;
}

// ============================================================================
// execute_code  (Req 14)
// ============================================================================

ExecuteCodeResult execute_code(const std::string& binary_path,
                               const std::vector<std::string>& args,
                               const DeterministicExecConfig& config) {
    ExecuteCodeResult result;

    if (binary_path.empty()) {
        result.error = R"({"severity":"error","rule_id":"MCP-014","message":"binary_path is required"})";
        return result;
    }

    // In production: delegate to daemon which spawns the binary under a
    // DeterministicContext when config.deterministic is true.
    //
    // The daemon:
    //   1. Creates a DeterministicContext with the provided seed/epoch/time_increment
    //   2. Interposes time/entropy/scheduling syscalls via LD_PRELOAD or
    //      kernel-level interception (SRT sandbox)
    //   3. Enforces sequential-by-spawn-order scheduling
    //   4. Captures stdout/stderr
    //   5. Returns the result

    if (config.deterministic) {
        std::ostringstream summary;
        summary << "deterministic_mode=true"
                << " seed=" << config.seed
                << " epoch=" << (config.epoch != 0 ? config.epoch : 1735689600ULL)
                << " time_increment_ms=" << config.time_increment_ms
                << " scheduling=sequential_by_spawn_order";
        result.deterministic_summary = summary.str();
    }

    (void)args;

    return result;
}

}  // namespace meld::mcp
