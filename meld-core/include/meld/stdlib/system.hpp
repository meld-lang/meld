#pragma once

#include "meld/kernel/primitives.hpp"
#include "effects.hpp"
#include <string>

namespace meld::stdlib {

// ============================================================================
// SYSTEM NAMESPACE
// Task 35.13: Implement System.out namespace
// Requirements: 41A.1, 41A.2, 41A.3, 41A.4, 41A.5, 41A.6, 41A.7
// ============================================================================

// System namespace provides standard library functions organized by category
// This follows the Java/C# convention of System.out for console output
namespace System {
    
    // out namespace provides console output functions
    // These are library functions that perform Console effects
    namespace out {
        
        // println - Print a message to stdout with newline
        // This is a library function that performs Console.println
        // The compiler will auto-infer @uses(Console) for any function calling this
        //
        // Usage:
        //   System::out::println("Hello, World!");
        //
        // Requirements implemented:
        // - 41A.1: Implement System.out.println as library function
        // - 41A.2: Perform Console.println effect
        // - 41A.3: Auto-infer @uses(Console) annotation
        inline void println(const std::string& message) {
            // Perform Console.println effect
            // This will search the handler stack for a Console handler
            // and invoke the println operation with the message
            perform("Console", "println", {
                kernel::Value(std::make_shared<kernel::String>(message))
            });
        }
        
        // print - Print a message to stdout without newline
        // This is a library function that performs Console.print
        // The compiler will auto-infer @uses(Console) for any function calling this
        //
        // Usage:
        //   System::out::print("Hello, ");
        //   System::out::print("World!");
        //
        // Requirements implemented:
        // - 41A.4: Implement System.out.print as library function
        // - 41A.5: Perform Console.print effect
        // - 41A.6: Auto-infer @uses(Console) annotation
        inline void print(const std::string& message) {
            // Perform Console.print effect
            // This will search the handler stack for a Console handler
            // and invoke the print operation with the message
            perform("Console", "print", {
                kernel::Value(std::make_shared<kernel::String>(message))
            });
        }
        
    } // namespace out
    
} // namespace System

} // namespace meld::stdlib
