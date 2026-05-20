#include "meld/stdlib/flow_runtime.hpp"
#include <iostream>

namespace meld {
namespace stdlib {

// This file contains any non-template implementations for flow runtime support.
// Most functionality is implemented in the header file as templates.

// Helper functions for debugging and logging flow state transitions
namespace flow_debug {

void logStateTransition(const std::string& flowName, 
                       const std::string& fromState, 
                       const std::string& toState, 
                       const std::string& event) {
    std::cout << "[Flow:" << flowName << "] " 
              << fromState << " --(" << event << ")--> " << toState << std::endl;
}

void logGuardEvaluation(const std::string& flowName,
                       const std::string& state,
                       const std::string& event,
                       bool guardResult) {
    std::cout << "[Flow:" << flowName << "] Guard for " << state 
              << " on " << event << ": " << (guardResult ? "PASS" : "FAIL") << std::endl;
}

void logActionExecution(const std::string& flowName,
                       const std::string& state,
                       const std::string& actionType) {
    std::cout << "[Flow:" << flowName << "] Executing " << actionType 
              << " action for state " << state << std::endl;
}

} // namespace flow_debug

} // namespace stdlib
} // namespace meld