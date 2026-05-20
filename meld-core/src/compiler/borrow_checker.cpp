#include "meld/compiler/borrow_checker.hpp"
#include "meld/compat/visit.hpp"
#include <algorithm>
#include <set>
#include <sstream>

namespace meld::compiler {

// LifetimeInference implementation
LifetimeInference::LifetimeInference() : next_lifetime_id_(1) {}

std::vector<LifetimeConstraint> LifetimeInference::infer_constraints(
    const parser::ast::function_definition& function) {
    
    std::vector<LifetimeConstraint> constraints;
    
    // Analyze function signature for lifetime relationships
    analyze_function_signature(function, constraints);
    
    // Analyze function body for additional constraints
    analyze_function_body(function.body.get(), constraints);
    
    return constraints;
}

std::expected<LifetimeAssignment, BorrowError> LifetimeInference::solve_constraints(
    const std::vector<LifetimeConstraint>& constraints) {
    
    LifetimeAssignment assignment;
    
    // Simple constraint solver - in a full implementation this would be more sophisticated
    // For now, we just record the constraints and create a basic assignment
    assignment.constraints = constraints;
    
    // Create assignments for all lifetimes mentioned in constraints
    std::set<size_t> lifetime_ids;
    for (const auto& constraint : constraints) {
        lifetime_ids.insert(constraint.shorter.id);
        lifetime_ids.insert(constraint.longer.id);
    }
    
    for (size_t id : lifetime_ids) {
        assignment.assignments[id] = LifetimeId(id);
    }
    
    return assignment;
}

LifetimeId LifetimeInference::create_lifetime(const std::string& name) {
    LifetimeId lifetime(next_lifetime_id_++, name);
    
    if (!name.empty()) {
        named_lifetimes_[name] = lifetime;
    }
    
    return lifetime;
}

void LifetimeInference::analyze_function_signature(
    const parser::ast::function_definition& function,
    std::vector<LifetimeConstraint>& constraints) {
    
    // For each parameter that's a reference type, create lifetime constraints
    for (const auto& param : function.parameters) {
        // In a full implementation, we'd check if the parameter type is a reference
        // and create appropriate lifetime constraints
        
        // For now, create a basic lifetime for each parameter
        LifetimeId param_lifetime = create_lifetime(param.name.name + "_lifetime");
        
        // If function has a return type that references parameters,
        // create constraints between parameter lifetimes and return lifetime
        if (function.has_return_type) {
            // This would require more sophisticated type analysis
            // For now, we'll skip this part
        }
    }
}

void LifetimeInference::analyze_function_body(
    const parser::ast::block_expression& body,
    std::vector<LifetimeConstraint>& constraints) {
    
    // Analyze each statement in the function body
    for (const auto& stmt : body.statements) {
        analyze_expression(stmt, constraints);
    }
}

void LifetimeInference::analyze_expression(
    const parser::ast::expression& expr,
    std::vector<LifetimeConstraint>& constraints) {
    
    // Visit each expression type and generate appropriate constraints
    meld::compat::visit([this, &constraints](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            // For val declarations, analyze the value expression
            analyze_expression(node.get().value, constraints);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            // For var declarations, analyze the value expression
            analyze_expression(node.get().value, constraints);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            // For binary operations, analyze both operands
            analyze_expression(node.get().left, constraints);
            analyze_expression(node.get().right, constraints);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            // For function calls, analyze all arguments
            for (const auto& arg : node.get().arguments) {
                analyze_expression(arg, constraints);
            }
        }
        // Add more expression types as needed
    }, expr);
}

// BorrowChecker implementation
BorrowChecker::BorrowChecker() 
    : lifetime_inference_(std::make_unique<LifetimeInference>())
    , current_function_(nullptr)
    , current_scope_depth_(0) {}

std::expected<void, BorrowError> BorrowChecker::check_function(
    const parser::ast::function_definition& function) {
    
    current_function_ = &function;
    
    // Clear previous state
    ownership_table_.clear();
    active_borrows_.clear();
    scope_stack_.clear();
    current_scope_depth_ = 0;
    
    // Initialize ownership for function parameters
    for (const auto& param : function.parameters) {
        OwnershipInfo info(true, false);  // Parameters are owned by default
        ownership_table_[param.name.name] = info;
    }
    
    // Infer lifetimes for the function
    auto constraints = lifetime_inference_->infer_constraints(function);
    auto lifetime_assignment = lifetime_inference_->solve_constraints(constraints);
    
    if (!lifetime_assignment) {
        return std::unexpected(lifetime_assignment.error());
    }
    
    // Enter function scope
    enter_scope();
    
    // Enhanced static analysis for Task 2.3
    
    // 1. Detect potential data races (Requirement 1.5)
    auto data_race_result = detect_data_races(function);
    if (!data_race_result) {
        exit_scope();
        return data_race_result;
    }
    
    // 2. Check the function body with enhanced analysis
    for (const auto& stmt : function.body.get().statements) {
        auto body_result = check_expression(stmt.get());
        if (!body_result) {
            exit_scope();
            return body_result;
        }
    }
    
    // 3. Perform use-after-move analysis on the entire function
    for (const auto& stmt : function.body.get().statements) {
        auto use_after_move_result = analyze_use_after_move(stmt.get());
        if (!use_after_move_result) {
            exit_scope();
            return use_after_move_result;
        }
    }
    
    // 4. Analyze lifetime relationships
    for (const auto& stmt : function.body.get().statements) {
        auto lifetime_result = analyze_lifetime_relationships(stmt.get());
        if (!lifetime_result) {
            exit_scope();
            return lifetime_result;
        }
    }
    
    // Exit function scope
    exit_scope();
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::check_expression(
    const parser::ast::expression& expr) {
    
    return meld::compat::visit<std::expected<void, BorrowError>>([this](const auto& node) -> std::expected<void, BorrowError> {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            return check_identifier_usage(node);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            return check_val_declaration(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::var_declaration>>) {
            return check_var_declaration(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            return check_function_call(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            return check_binary_operation(node.get());
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            // Check each statement in the block
            for (const auto& stmt : node.get().statements) {
                auto result = check_expression(stmt);
                if (!result) {
                    return result;
                }
            }
            return {};
        }
        else {
            // For other expression types, just return success for now
            // In a full implementation, we'd handle all expression types
            return {};
        }
    }, expr);
}

void BorrowChecker::track_ai_generated_borrows(
    const parser::ast::expression& node,
    const provenance::ProvenanceMetadata& provenance) {
    
    // Store provenance information for AI-generated code
    // This would be used to provide better error messages and tracking
    
    // For now, we'll just mark any ownership info we create with this provenance
    // In a full implementation, we'd traverse the AST and mark all relevant nodes
}

std::optional<OwnershipInfo> BorrowChecker::get_ownership_info(const std::string& symbol) const {
    auto it = ownership_table_.find(symbol);
    if (it != ownership_table_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void BorrowChecker::set_ownership_info(const std::string& symbol, const OwnershipInfo& info) {
    ownership_table_[symbol] = info;
}

bool BorrowChecker::is_borrowed(const std::string& symbol) const {
    auto it = active_borrows_.find(symbol);
    return it != active_borrows_.end() && !it->second.empty();
}

std::vector<BorrowInfo> BorrowChecker::get_active_borrows(const std::string& symbol) const {
    auto it = active_borrows_.find(symbol);
    if (it != active_borrows_.end()) {
        return it->second;
    }
    return {};
}

// Private helper methods
std::expected<void, BorrowError> BorrowChecker::check_val_declaration(
    const parser::ast::val_declaration& decl) {
    
    // Check the value expression first
    auto result = check_expression(decl.value);
    if (!result) {
        return result;
    }
    
    // Create ownership info for the new variable
    OwnershipInfo info(true, false);  // val declarations own their values
    ownership_table_[decl.name.name] = info;
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::check_var_declaration(
    const parser::ast::var_declaration& decl) {
    
    // Check the value expression first
    auto result = check_expression(decl.value);
    if (!result) {
        return result;
    }
    
    // Create ownership info for the new variable
    OwnershipInfo info(true, false);  // var declarations own their values
    ownership_table_[decl.name.name] = info;
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::check_function_call(
    const parser::ast::function_call& call) {
    
    // Check all arguments for ownership and borrowing violations
    for (size_t i = 0; i < call.arguments.size(); ++i) {
        const auto& arg = call.arguments[i];
        
        // First check the argument expression itself
        auto result = check_expression(arg);
        if (!result) {
            return result;
        }
        
        // Enhanced analysis: Check if this is a move or borrow operation
        auto maybe_id = meld::compat::visit<std::optional<parser::ast::identifier>>([](const auto& e) -> std::optional<parser::ast::identifier> {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, parser::ast::identifier>) {
                return e;
            }
            return std::nullopt;
        }, arg.get());
        if (maybe_id) {
            const auto& identifier = *maybe_id;
            // Check for special function calls that indicate ownership operations
            if (call.function_name.name == "move" || call.function_name.name == "move_value") {
                // This is a move operation
                auto move_result = move_value(identifier.name, identifier);
                if (!move_result) {
                    return move_result;
                }
            }
            else if (call.function_name.name == "borrow") {
                // This is an immutable borrow operation
                auto lifetime = lifetime_inference_->create_lifetime(identifier.name + "_borrow");
                auto borrow_result = borrow_value(identifier.name, BorrowType::Immutable, lifetime, identifier);
                if (!borrow_result) {
                    return borrow_result;
                }
            }
            else if (call.function_name.name == "borrow_mut") {
                // This is a mutable borrow operation
                auto lifetime = lifetime_inference_->create_lifetime(identifier.name + "_mut_borrow");
                auto borrow_result = borrow_value(identifier.name, BorrowType::Mutable, lifetime, identifier);
                if (!borrow_result) {
                    return borrow_result;
                }
            }
            else {
                // Regular function call - check for implicit moves or borrows
                // In a full implementation, we'd analyze the function signature
                // to determine if parameters are moved or borrowed
                
                // For now, assume non-Copy types are moved by default
                auto ownership_info = get_ownership_info(identifier.name);
                if (ownership_info && !ownership_info->is_copyable) {
                    // Check if this would be a move (non-Copy type passed by value)
                    if (can_move(identifier.name)) {
                        // Perform the move
                        auto move_result = move_value(identifier.name, identifier);
                        if (!move_result) {
                            return move_result;
                        }
                    } else {
                        // Cannot move - create error
                        return std::unexpected(BorrowError(
                            BorrowError::Type::MoveWhileBorrowed,
                            "Cannot move '" + identifier.name + "' into function call while it is borrowed",
                            identifier,
                            "Function parameter requires ownership but value is currently borrowed"
                        ));
                    }
                }
            }
        }
    }
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::check_binary_operation(
    const parser::ast::binary_operation& op) {
    
    // Check both operands
    auto left_result = check_expression(op.left);
    if (!left_result) {
        return left_result;
    }
    
    auto right_result = check_expression(op.right);
    if (!right_result) {
        return right_result;
    }
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::check_identifier_usage(
    const parser::ast::identifier& id) {
    
    // Check if the identifier exists in our ownership table
    auto ownership_info = get_ownership_info(id.name);
    if (!ownership_info) {
        // Variable not found - this would be caught by the type checker
        return {};
    }
    
    // Check if the variable has been moved from
    if (ownership_info->is_moved) {
        return std::unexpected(make_use_after_move_error(id.name, id));
    }
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::move_value(
    const std::string& symbol,
    const parser::ast::identifier& location) {
    
    if (!can_move(symbol)) {
        return std::unexpected(BorrowError(
            BorrowError::Type::MoveWhileBorrowed,
            "Cannot move '" + symbol + "' while it is borrowed",
            location
        ));
    }
    
    // Mark the variable as moved
    auto& info = ownership_table_[symbol];
    info.is_moved = true;
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::borrow_value(
    const std::string& symbol,
    BorrowType borrow_type,
    LifetimeId lifetime,
    const parser::ast::identifier& location) {
    
    // Enhanced analysis: Enforce exclusive mutable access (Requirement 1.3)
    if (borrow_type == BorrowType::Mutable) {
        auto exclusive_access_result = enforce_exclusive_mutable_access(symbol, location);
        if (!exclusive_access_result) {
            return exclusive_access_result;
        }
    }
    
    if (!can_borrow(symbol, borrow_type)) {
        return std::unexpected(make_borrow_conflict_error(symbol, borrow_type, location));
    }
    
    // Add the borrow to active borrows
    BorrowInfo borrow_info(lifetime, borrow_type, location, symbol);
    active_borrows_[symbol].push_back(borrow_info);
    
    return {};
}

bool BorrowChecker::can_move(const std::string& symbol) const {
    // Can't move if there are active borrows
    return !is_borrowed(symbol);
}

bool BorrowChecker::can_borrow(const std::string& symbol, BorrowType borrow_type) const {
    auto active_borrows = get_active_borrows(symbol);
    
    if (borrow_type == BorrowType::Mutable) {
        // Mutable borrow requires no other borrows
        return active_borrows.empty();
    } else {
        // Immutable borrow is allowed if there are no mutable borrows
        return std::none_of(active_borrows.begin(), active_borrows.end(),
            [](const BorrowInfo& borrow) {
                return borrow.borrow_type == BorrowType::Mutable;
            });
    }
}

BorrowError BorrowChecker::make_use_after_move_error(
    const std::string& symbol,
    const parser::ast::identifier& location) const {
    
    return BorrowError(
        BorrowError::Type::UseAfterMove,
        "Use of moved value '" + symbol + "'",
        location,
        "Value was moved and can no longer be used"
    );
}

BorrowError BorrowChecker::make_borrow_conflict_error(
    const std::string& symbol,
    BorrowType attempted_borrow,
    const parser::ast::identifier& location) const {
    
    std::string borrow_type_str = (attempted_borrow == BorrowType::Mutable) ? "mutable" : "immutable";
    
    return BorrowError(
        BorrowError::Type::MutableBorrowWhileImmutableExists,
        "Cannot create " + borrow_type_str + " borrow of '" + symbol + "'",
        location,
        "Conflicting borrow already exists"
    );
}

// Enhanced static analysis methods for Task 2.3

std::expected<void, BorrowError> BorrowChecker::analyze_use_after_move(
    const parser::ast::expression& expr) {
    
    return meld::compat::visit<std::expected<void, BorrowError>>([this](const auto& node) -> std::expected<void, BorrowError> {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, parser::ast::identifier>) {
            // Check if this identifier has been moved from
            auto ownership_info = get_ownership_info(node.name);
            if (ownership_info && ownership_info->is_moved) {
                return std::unexpected(BorrowError(
                    BorrowError::Type::UseAfterMove,
                    "Use of moved value '" + node.name + "'",
                    node,
                    "Value was previously moved and cannot be used again"
                ));
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            // Recursively check all arguments
            for (const auto& arg : node.get().arguments) {
                auto result = analyze_use_after_move(arg);
                if (!result) {
                    return result;
                }
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            // Check both operands
            auto left_result = analyze_use_after_move(node.get().left);
            if (!left_result) {
                return left_result;
            }
            return analyze_use_after_move(node.get().right);
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            // Check each statement in the block
            for (const auto& stmt : node.get().statements) {
                auto result = analyze_use_after_move(stmt);
                if (!result) {
                    return result;
                }
            }
        }
        
        return {};
    }, expr);
}

std::expected<void, BorrowError> BorrowChecker::enforce_exclusive_mutable_access(
    const std::string& symbol,
    const parser::ast::identifier& location) {
    
    auto active_borrows = get_active_borrows(symbol);
    
    // Check for existing mutable borrows
    auto mutable_borrow_count = std::count_if(active_borrows.begin(), active_borrows.end(),
        [](const BorrowInfo& borrow) {
            return borrow.borrow_type == BorrowType::Mutable;
        });
    
    if (mutable_borrow_count > 1) {
        return std::unexpected(BorrowError(
            BorrowError::Type::MultipleMutableBorrows,
            "Multiple mutable borrows of '" + symbol + "' detected",
            location,
            "Only one mutable borrow is allowed at a time"
        ));
    }
    
    // Check for mutable borrow while immutable borrows exist
    if (mutable_borrow_count > 0) {
        auto immutable_borrow_count = std::count_if(active_borrows.begin(), active_borrows.end(),
            [](const BorrowInfo& borrow) {
                return borrow.borrow_type == BorrowType::Immutable;
            });
        
        if (immutable_borrow_count > 0) {
            return std::unexpected(BorrowError(
                BorrowError::Type::MutableBorrowWhileImmutableExists,
                "Cannot create mutable borrow of '" + symbol + "' while immutable borrows exist",
                location,
                "Mutable and immutable borrows cannot coexist"
            ));
        }
    }
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::detect_data_races(
    const parser::ast::function_definition& function) {
    
    // Data race detection requires analyzing concurrent access patterns
    // This is a simplified implementation that checks for potential races
    
    std::unordered_map<std::string, std::vector<parser::ast::identifier>> concurrent_accesses;
    
    // Analyze function body for concurrent access patterns
    auto result = analyze_concurrent_access_patterns(function);
    if (!result) {
        return result;
    }
    
    // Check each variable that might be accessed concurrently
    for (const auto& [symbol, ownership_info] : ownership_table_) {
        // If a variable has mutable borrows and is accessed in multiple contexts,
        // it could lead to a data race
        auto active_borrows = get_active_borrows(symbol);
        
        auto mutable_borrows = std::count_if(active_borrows.begin(), active_borrows.end(),
            [](const BorrowInfo& borrow) {
                return borrow.borrow_type == BorrowType::Mutable;
            });
        
        if (mutable_borrows > 0 && active_borrows.size() > 1) {
            // Potential data race detected
            return std::unexpected(BorrowError(
                BorrowError::Type::MutableBorrowWhileImmutableExists,
                "Potential data race detected on '" + symbol + "'",
                active_borrows[0].source_location,
                "Multiple concurrent accesses with at least one mutable borrow"
            ));
        }
    }
    
    return {};
}

std::expected<void, BorrowError> BorrowChecker::analyze_lifetime_relationships(
    const parser::ast::expression& expr) {
    
    // Analyze lifetime relationships in the expression
    return meld::compat::visit<std::expected<void, BorrowError>>([this](const auto& node) -> std::expected<void, BorrowError> {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::val_declaration>>) {
            // For val declarations, ensure the value's lifetime is compatible
            auto result = analyze_lifetime_relationships(node.get().value);
            if (!result) {
                return result;
            }
            
            // Check if we're creating a reference that outlives its referent
            // This would require more sophisticated lifetime analysis
            // For now, we'll do basic checks
            
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            // Check lifetime relationships in function arguments
            for (const auto& arg : node.get().arguments) {
                auto result = analyze_lifetime_relationships(arg);
                if (!result) {
                    return result;
                }
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            // Check both operands
            auto left_result = analyze_lifetime_relationships(node.get().left);
            if (!left_result) {
                return left_result;
            }
            return analyze_lifetime_relationships(node.get().right);
        }
        
        return {};
    }, expr);
}

std::expected<void, BorrowError> BorrowChecker::analyze_concurrent_access_patterns(
    const parser::ast::function_definition& function) {
    
    // Analyze the function for patterns that might indicate concurrent access
    // This is a simplified implementation - a full implementation would need
    // to understand threading primitives, async/await, etc.
    
    // For now, we'll look for function calls that might indicate concurrency
    std::vector<std::string> concurrent_indicators = {
        "spawn", "async", "await", "thread", "parallel", "concurrent"
    };
    
    // Recursively analyze the function body statements
    for (const auto& stmt : function.body.get().statements) {
        auto result = analyze_expression_for_concurrency(stmt.get(), concurrent_indicators);
        if (!result) return result;
    }
    return {};
}

void BorrowChecker::enter_scope() {
    // Create a new scope for tracking borrows
    current_scope_depth_++;
    LifetimeId scope_lifetime = lifetime_inference_->create_lifetime("scope_" + std::to_string(current_scope_depth_));
    scope_stack_.push_back(scope_lifetime);
}

void BorrowChecker::exit_scope() {
    // Exit the current scope and clean up borrows that are no longer valid
    if (!scope_stack_.empty()) {
        LifetimeId exiting_scope = scope_stack_.back();
        scope_stack_.pop_back();
        current_scope_depth_--;
        
        // Clean up borrows that were created in this scope
        cleanup_expired_borrows(exiting_scope);
    }
}

void BorrowChecker::cleanup_expired_borrows(LifetimeId scope_lifetime) {
    // Remove borrows that have expired due to scope exit
    for (auto& [symbol, borrows] : active_borrows_) {
        borrows.erase(
            std::remove_if(borrows.begin(), borrows.end(),
                [scope_lifetime](const BorrowInfo& borrow) {
                    return borrow.lifetime.id == scope_lifetime.id;
                }),
            borrows.end()
        );
    }
}

// Helper method for concurrent access analysis
std::expected<void, BorrowError> BorrowChecker::analyze_expression_for_concurrency(
    const parser::ast::expression& expr,
    const std::vector<std::string>& concurrent_indicators) {
    
    return meld::compat::visit<std::expected<void, BorrowError>>([this, &concurrent_indicators](const auto& node) -> std::expected<void, BorrowError> {
        using T = std::decay_t<decltype(node)>;
        
        if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::function_call>>) {
            // Check if this function call indicates concurrency
            const auto& call = node.get();
            
            for (const auto& indicator : concurrent_indicators) {
                if (call.function_name.name.find(indicator) != std::string::npos) {
                    // This might be a concurrent operation
                    // Check all arguments for potential data races
                    for (const auto& arg : call.arguments) {
                        auto arg_id = meld::compat::visit<std::optional<parser::ast::identifier>>([](const auto& e) -> std::optional<parser::ast::identifier> {
                            using U = std::decay_t<decltype(e)>;
                            if constexpr (std::is_same_v<U, parser::ast::identifier>) {
                                return e;
                            }
                            return std::nullopt;
                        }, arg.get());
                        if (arg_id) {
                            // Check if this variable is borrowed mutably elsewhere
                            auto result = enforce_exclusive_mutable_access(arg_id->name, *arg_id);
                            if (!result) {
                                return result;
                            }
                        }
                    }
                }
            }
            
            // Recursively check arguments
            for (const auto& arg : call.arguments) {
                auto result = analyze_expression_for_concurrency(arg, concurrent_indicators);
                if (!result) {
                    return result;
                }
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::block_expression>>) {
            // Check each statement in the block
            for (const auto& stmt : node.get().statements) {
                auto result = analyze_expression_for_concurrency(stmt, concurrent_indicators);
                if (!result) {
                    return result;
                }
            }
        }
        else if constexpr (std::is_same_v<T, boost::spirit::x3::forward_ast<parser::ast::binary_operation>>) {
            // Check both operands
            auto left_result = analyze_expression_for_concurrency(node.get().left, concurrent_indicators);
            if (!left_result) {
                return left_result;
            }
            return analyze_expression_for_concurrency(node.get().right, concurrent_indicators);
        }
        
        return {};
    }, expr);
}

} // namespace meld::compiler