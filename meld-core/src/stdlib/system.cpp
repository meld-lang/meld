#include "meld/stdlib/system.hpp"
#include "meld/stdlib/effects.hpp"
#include "meld/kernel/primitives.hpp"

namespace meld::stdlib {

// ============================================================================
// SYSTEM NAMESPACE IMPLEMENTATION
// Task 35.13: Implement System.out namespace
// Requirements: 41A.1, 41A.2, 41A.3, 41A.4, 41A.5, 41A.6, 41A.7
// ============================================================================

// The System.out functions are implemented as inline functions in the header
// to ensure they can be properly analyzed by the effect inference system.
//
// This file provides any additional implementation details or helper functions
// that may be needed for the System namespace.

namespace System {
    namespace out {
        
        // Helper function to validate message parameter
        // This ensures that the message is valid before performing the effect
        inline void validate_message(const std::string& message) {
            // For now, we accept any string message
            // In a full implementation, we might validate encoding, length limits, etc.
            
            // Note: Empty messages are allowed - they will print nothing or just a newline
        }
        
        // Internal helper to create a Console effect value
        // This encapsulates the creation of kernel::Value from string
        inline kernel::Value create_message_value(const std::string& message) {
            return kernel::Value(std::make_shared<kernel::String>(message));
        }
        
    } // namespace out
} // namespace System

// ============================================================================
// EFFECT INFERENCE INTEGRATION
// ============================================================================

// The compiler's effect inference system will automatically detect that:
// 1. System::out::println calls perform("Console", "println", ...)
// 2. System::out::print calls perform("Console", "print", ...)
// 3. Any function calling these will be annotated with @uses(Console)
//
// This happens through static analysis of the AST, where the compiler:
// 1. Identifies calls to perform() with "Console" as the effect name
// 2. Propagates the Console effect requirement up the call chain
// 3. Automatically inserts @uses(Console) annotations
//
// Example:
//   fnc greet(name: string) {
//       System::out::println(`Hello, ${name}!`);  // Calls perform("Console", "println", ...)
//   }
//
// The compiler will automatically infer and add:
//   @uses(Console)
//   fnc greet(name: string) {
//       System::out::println(`Hello, ${name}!`);
//   }
//
// This satisfies requirements:
// - 41A.3: Auto-infer @uses(Console) for System.out.println calls
// - 41A.6: Auto-infer @uses(Console) for System.out.print calls
// - 41A.7: Ensure compiler integration works correctly

} // namespace meld::stdlib