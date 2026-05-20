#pragma once

#include <map>
#include <vector>
#include <string>
#include <functional>
#include <any>
#include <algorithm>
#include <stdexcept>

namespace meld {
namespace stdlib {

/**
 * Base class for flow runtime support.
 * Provides the core functionality for state machine execution.
 */
template<typename StateEnum, typename EventEnum>
class FlowInstance {
public:
    using StateType = StateEnum;
    using EventType = EventEnum;
    using GuardFunction = std::function<bool()>;
    using ActionFunction = std::function<void()>;
    
    struct Transition {
        EventType event;
        StateType targetState;
        bool hasGuard = false;
        GuardFunction guard;
        
        Transition(EventType e, StateType target) 
            : event(e), targetState(target) {}
        
        Transition(EventType e, StateType target, GuardFunction g) 
            : event(e), targetState(target), hasGuard(true), guard(g) {}
    };
    
    struct StateDefinition {
        std::string name;
        std::vector<Transition> transitions;
        bool hasEntryAction = false;
        bool hasExitAction = false;
        ActionFunction entryAction;
        ActionFunction exitAction;
        bool isTerminal = false;
        
        StateDefinition() = default;
        StateDefinition(const std::string& n) : name(n) {}
    };

private:
    StateType currentState_;
    std::map<std::string, std::any> context_;
    std::map<StateType, StateDefinition> states_;
    StateType initialState_;

public:
    /**
     * Constructor that takes the initial state.
     */
    explicit FlowInstance(StateType initial) : currentState_(initial), initialState_(initial) {}
    
    /**
     * Triggers an event and attempts to transition to a new state.
     * 
     * @param event The event to trigger
     * @return true if a transition occurred, false if no valid transition was found
     */
    bool trigger(EventType event) {
        auto stateIt = states_.find(currentState_);
        if (stateIt == states_.end()) {
            return false;
        }
        
        const auto& currentStateDef = stateIt->second;
        
        // Find matching transition
        for (const auto& transition : currentStateDef.transitions) {
            if (transition.event == event) {
                // Check guard condition
                if (transition.hasGuard && !transition.guard()) {
                    continue; // Guard failed, try next transition
                }
                
                // Execute exit action
                if (currentStateDef.hasExitAction) {
                    currentStateDef.exitAction();
                }
                
                // Transition to new state
                currentState_ = transition.targetState;
                
                // Execute entry action
                auto newStateIt = states_.find(currentState_);
                if (newStateIt != states_.end()) {
                    const auto& newStateDef = newStateIt->second;
                    if (newStateDef.hasEntryAction) {
                        newStateDef.entryAction();
                    }
                }
                
                return true;
            }
        }
        
        return false; // No transition found
    }
    
    /**
     * Gets the current state.
     * 
     * @return The current state
     */
    StateType getCurrentState() const {
        return currentState_;
    }
    
    /**
     * Gets the name of the current state.
     * 
     * @return The name of the current state as a string
     */
    std::string getCurrentStateName() const {
        auto stateIt = states_.find(currentState_);
        if (stateIt != states_.end()) {
            return stateIt->second.name;
        }
        return "Unknown";
    }
    
    /**
     * Checks if the current state is a terminal state.
     * 
     * @return true if the current state is terminal, false otherwise
     */
    bool isTerminal() const {
        auto stateIt = states_.find(currentState_);
        if (stateIt != states_.end()) {
            return stateIt->second.isTerminal;
        }
        return false;
    }
    
    /**
     * Gets all events that can be triggered from the current state.
     * Only includes events whose guard conditions pass (if any).
     * 
     * @return A vector of available events
     */
    std::vector<EventType> getAvailableEvents() const {
        std::vector<EventType> events;
        
        auto stateIt = states_.find(currentState_);
        if (stateIt == states_.end()) {
            return events;
        }
        
        const auto& currentStateDef = stateIt->second;
        for (const auto& transition : currentStateDef.transitions) {
            // Only include events whose guards pass (if any)
            if (!transition.hasGuard || transition.guard()) {
                events.push_back(transition.event);
            }
        }
        
        return events;
    }
    
    /**
     * Checks if a specific event can be triggered from the current state.
     * 
     * @param event The event to check
     * @return true if the event can be triggered, false otherwise
     */
    bool canTrigger(EventType event) const {
        const auto& availableEvents = getAvailableEvents();
        return std::find(availableEvents.begin(), availableEvents.end(), event) != availableEvents.end();
    }
    
    /**
     * Sets a context value that can be accessed by guard conditions and actions.
     * 
     * @param key The context key
     * @param value The context value
     */
    void setContextValue(const std::string& key, const std::any& value) {
        context_[key] = value;
    }
    
    /**
     * Gets a context value by key.
     * 
     * @param key The context key
     * @return The context value cast to the specified type
     * @throws std::runtime_error if the key is not found
     */
    template<typename T>
    T getContextValue(const std::string& key) const {
        auto it = context_.find(key);
        if (it != context_.end()) {
            return std::any_cast<T>(it->second);
        }
        throw std::runtime_error("Context key not found: " + key);
    }
    
    /**
     * Checks if a context value exists.
     * 
     * @param key The context key
     * @return true if the key exists, false otherwise
     */
    bool hasContextValue(const std::string& key) const {
        return context_.find(key) != context_.end();
    }
    
    /**
     * Resets the flow to its initial state.
     */
    void reset() {
        // Execute exit action for current state if any
        auto stateIt = states_.find(currentState_);
        if (stateIt != states_.end()) {
            const auto& currentStateDef = stateIt->second;
            if (currentStateDef.hasExitAction) {
                currentStateDef.exitAction();
            }
        }
        
        // Reset to initial state
        currentState_ = initialState_;
        
        // Execute entry action for initial state if any
        stateIt = states_.find(currentState_);
        if (stateIt != states_.end()) {
            const auto& initialStateDef = stateIt->second;
            if (initialStateDef.hasEntryAction) {
                initialStateDef.entryAction();
            }
        }
        
        // Clear context
        context_.clear();
    }

protected:
    /**
     * Adds a state definition to the flow.
     * This is used by generated code to set up the state machine.
     * 
     * @param state The state enum value
     * @param definition The state definition
     */
    void addState(StateType state, const StateDefinition& definition) {
        states_[state] = definition;
    }
    
    /**
     * Gets the states map for use by generated code.
     * 
     * @return Reference to the states map
     */
    const std::map<StateType, StateDefinition>& getStates() const {
        return states_;
    }
};

/**
 * Base class for flow definitions.
 * Contains static methods for accessing flow metadata.
 */
template<typename StateEnum, typename EventEnum>
class FlowDefinition {
public:
    using StateType = StateEnum;
    using EventType = EventEnum;
    using StateDefinition = typename FlowInstance<StateEnum, EventEnum>::StateDefinition;
    
    /**
     * Gets the initial state for this flow.
     * Must be implemented by generated code.
     */
    static StateType getInitialState();
    
    /**
     * Gets the state definitions for this flow.
     * Must be implemented by generated code.
     */
    static const std::map<StateType, StateDefinition>& getStates();
};

} // namespace stdlib
} // namespace meld