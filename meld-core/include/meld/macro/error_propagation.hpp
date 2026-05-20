#pragma once

#include "meld/parser/ast.hpp"
#include "meld/types/result.hpp"
#include "meld/types/option.hpp"
#include <memory>
#include <string>

namespace meld::macro {

/**
 * Error Propagation Operator (? operator)
 * 
 * Implements Rust-style error propagation for Result and Option types.
 * 
 * For Result<T, E>:
 *   expr? expands to:
 *     match expr {
 *       Ok(value) => value
 *       Err(error) => return Err(error)
 *     }
 * 
 * For Option<T>:
 *   expr? expands to:
 *     match expr {
 *       Some(value) => value
 *       None => return None
 *     }
 * 
 * This allows for concise error handling without explicit match statements.
 */
class ErrorPropagationMacro {
public:
    /**
     * Expand the ? operator on a Result expression
     * 
     * @param expr The Result expression to unwrap
     * @return AST node representing the expanded code
     */
    static std::shared_ptr<parser::ASTNode> expand_result(
        std::shared_ptr<parser::Expression> expr
    );
    
    /**
     * Expand the ? operator on an Option expression
     * 
     * @param expr The Option expression to unwrap
     * @return AST node representing the expanded code
     */
    static std::shared_ptr<parser::ASTNode> expand_option(
        std::shared_ptr<parser::Expression> expr
    );
    
    /**
     * Determine if an expression is a Result or Option type
     * and expand accordingly
     * 
     * @param expr The expression to check and expand
     * @return AST node representing the expanded code
     */
    static std::shared_ptr<parser::ASTNode> expand(
        std::shared_ptr<parser::Expression> expr
    );
    
    /**
     * Register the ? operator with the macro system
     */
    static void register_operator();
    
private:
    /**
     * Create a match expression for Result unwrapping
     */
    static std::shared_ptr<parser::MatchExpression> create_result_match(
        std::shared_ptr<parser::Expression> expr
    );
    
    /**
     * Create a match expression for Option unwrapping
     */
    static std::shared_ptr<parser::MatchExpression> create_option_match(
        std::shared_ptr<parser::Expression> expr
    );
    
    /**
     * Create an early return statement
     */
    static std::shared_ptr<parser::ReturnStatement> create_early_return(
        std::shared_ptr<parser::Expression> error_expr
    );
};

/**
 * Try operator helper for use in expressions
 * 
 * This provides a functional interface for the ? operator
 * that can be used in contexts where postfix operators aren't available.
 * 
 * Usage:
 *   val result = try_unwrap(some_result_expr)
 */
template<typename T, typename E>
T try_unwrap(const types::Result<T, E>& result) {
    if (result.is_success()) {
        return result.value();
    }
    // In a full implementation, this would trigger an early return effect
    // For now, we use an assertion
    assert(false && "try_unwrap called on Error - should trigger early return");
    throw std::runtime_error("try_unwrap called on Error");
}

template<typename T>
T try_unwrap(const types::Option<T>& option) {
    if (option.is_some()) {
        return option.value();
    }
    // In a full implementation, this would trigger an early return effect
    assert(false && "try_unwrap called on None - should trigger early return");
    throw std::runtime_error("try_unwrap called on None");
}

/**
 * Attempt block for error recovery
 * 
 * Provides a structured way to handle errors with automatic propagation.
 * 
 * Usage:
 *   val result = attempt {
 *       val x = operation1()?
 *       val y = operation2()?
 *       Ok(x + y)
 *   } recover { error =>
 *       println(`Error: ${error}`)
 *       Err(error)
 *   }
 */
class AttemptBlock {
public:
    /**
     * Create an attempt block that captures error propagation
     */
    static std::shared_ptr<parser::ASTNode> create(
        std::shared_ptr<parser::BlockExpression> try_block,
        std::shared_ptr<parser::LambdaExpression> recover_handler
    );
    
    /**
     * Register the attempt/recover syntax with the parser
     */
    static void register_syntax();
};

} // namespace meld::macro
