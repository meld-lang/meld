#include "meld/stdlib/flow_runtime.hpp"
#include <gtest/gtest.h>
#include <string>

// Test enums
enum class TestState {
    Start,
    Processing,
    Complete,
    Error
};

enum class TestEvent {
    Begin,
    Finish,
    Fail,
    Reset
};

// Test FlowInstance
class TestFlowInstance : public meld::stdlib::FlowInstance<TestState, TestEvent> {
public:
    TestFlowInstance() : FlowInstance(TestState::Start) {
        setupStates();
    }
    
private:
    void setupStates() {
        // Start state
        StateDefinition startState("Start");
        startState.transitions.emplace_back(TestEvent::Begin, TestState::Processing);
        startState.hasEntryAction = true;
        startState.entryAction = []() {
            // Entry action for Start state
        };
        addState(TestState::Start, startState);
        
        // Processing state
        StateDefinition processingState("Processing");
        processingState.transitions.emplace_back(TestEvent::Finish, TestState::Complete);
        processingState.transitions.emplace_back(TestEvent::Fail, TestState::Error);
        processingState.hasEntryAction = true;
        processingState.entryAction = []() {
            // Entry action for Processing state
        };
        addState(TestState::Processing, processingState);
        
        // Complete state (terminal)
        StateDefinition completeState("Complete");
        completeState.isTerminal = true;
        completeState.hasEntryAction = true;
        completeState.entryAction = []() {
            // Entry action for Complete state
        };
        addState(TestState::Complete, completeState);
        
        // Error state
        StateDefinition errorState("Error");
        errorState.transitions.emplace_back(TestEvent::Reset, TestState::Start);
        errorState.hasEntryAction = true;
        errorState.entryAction = []() {
            // Entry action for Error state
        };
        addState(TestState::Error, errorState);
    }
};

// Test fixture
class FlowRuntimeTest : public ::testing::Test {
protected:
    TestFlowInstance flow;
};

TEST_F(FlowRuntimeTest, InitialState) {
    EXPECT_EQ(flow.getCurrentState(), TestState::Start);
    EXPECT_EQ(flow.getCurrentStateName(), "Start");
    EXPECT_FALSE(flow.isTerminal());
}

TEST_F(FlowRuntimeTest, ValidTransition) {
    bool result = flow.trigger(TestEvent::Begin);
    EXPECT_TRUE(result);
    EXPECT_EQ(flow.getCurrentState(), TestState::Processing);
    EXPECT_EQ(flow.getCurrentStateName(), "Processing");
}

TEST_F(FlowRuntimeTest, InvalidTransition) {
    flow.trigger(TestEvent::Begin); // Start -> Processing
    bool result = flow.trigger(TestEvent::Begin); // Invalid from Processing
    EXPECT_FALSE(result);
    EXPECT_EQ(flow.getCurrentState(), TestState::Processing); // Should remain unchanged
}

TEST_F(FlowRuntimeTest, AvailableEvents) {
    flow.trigger(TestEvent::Begin); // Start -> Processing
    
    auto availableEvents = flow.getAvailableEvents();
    EXPECT_EQ(availableEvents.size(), 2); // Finish and Fail
    EXPECT_TRUE(flow.canTrigger(TestEvent::Finish));
    EXPECT_TRUE(flow.canTrigger(TestEvent::Fail));
    EXPECT_FALSE(flow.canTrigger(TestEvent::Begin));
}

TEST_F(FlowRuntimeTest, TerminalState) {
    flow.trigger(TestEvent::Begin);  // Start -> Processing
    flow.trigger(TestEvent::Finish); // Processing -> Complete
    
    EXPECT_EQ(flow.getCurrentState(), TestState::Complete);
    EXPECT_TRUE(flow.isTerminal());
}

TEST_F(FlowRuntimeTest, ContextValues) {
    flow.setContextValue("test_key", std::string("test_value"));
    EXPECT_TRUE(flow.hasContextValue("test_key"));
    EXPECT_EQ(flow.getContextValue<std::string>("test_key"), "test_value");
    EXPECT_FALSE(flow.hasContextValue("nonexistent_key"));
}

TEST_F(FlowRuntimeTest, ContextValueException) {
    EXPECT_THROW(flow.getContextValue<std::string>("nonexistent_key"), std::runtime_error);
}

TEST_F(FlowRuntimeTest, ResetFunctionality) {
    flow.trigger(TestEvent::Begin); // Start -> Processing
    flow.setContextValue("test_key", std::string("test_value"));
    
    flow.reset();
    
    EXPECT_EQ(flow.getCurrentState(), TestState::Start);
    EXPECT_FALSE(flow.hasContextValue("test_key")); // Context should be cleared
}

TEST_F(FlowRuntimeTest, ErrorHandlingPath) {
    flow.trigger(TestEvent::Begin); // Start -> Processing
    flow.trigger(TestEvent::Fail);  // Processing -> Error
    EXPECT_EQ(flow.getCurrentState(), TestState::Error);
    
    flow.trigger(TestEvent::Reset); // Error -> Start
    EXPECT_EQ(flow.getCurrentState(), TestState::Start);
}

TEST_F(FlowRuntimeTest, MultipleTransitions) {
    // Test a complete flow through multiple states
    EXPECT_EQ(flow.getCurrentState(), TestState::Start);
    
    flow.trigger(TestEvent::Begin);
    EXPECT_EQ(flow.getCurrentState(), TestState::Processing);
    
    flow.trigger(TestEvent::Finish);
    EXPECT_EQ(flow.getCurrentState(), TestState::Complete);
    EXPECT_TRUE(flow.isTerminal());
}

// Test with guard conditions
enum class GuardedState {
    Idle,
    Active
};

enum class GuardedEvent {
    Activate
};

class GuardedFlowInstance : public meld::stdlib::FlowInstance<GuardedState, GuardedEvent> {
public:
    bool guardCondition = false;
    
    GuardedFlowInstance() : FlowInstance(GuardedState::Idle) {
        StateDefinition idleState("Idle");
        idleState.transitions.emplace_back(
            GuardedEvent::Activate, 
            GuardedState::Active,
            [this]() { return guardCondition; }
        );
        addState(GuardedState::Idle, idleState);
        
        StateDefinition activeState("Active");
        activeState.isTerminal = true;
        addState(GuardedState::Active, activeState);
    }
};

TEST(FlowRuntimeGuardTest, GuardConditionFails) {
    GuardedFlowInstance flow;
    flow.guardCondition = false;
    
    bool result = flow.trigger(GuardedEvent::Activate);
    EXPECT_FALSE(result);
    EXPECT_EQ(flow.getCurrentState(), GuardedState::Idle);
}

TEST(FlowRuntimeGuardTest, GuardConditionPasses) {
    GuardedFlowInstance flow;
    flow.guardCondition = true;
    
    bool result = flow.trigger(GuardedEvent::Activate);
    EXPECT_TRUE(result);
    EXPECT_EQ(flow.getCurrentState(), GuardedState::Active);
}

// Test with entry/exit actions
class ActionFlowInstance : public meld::stdlib::FlowInstance<TestState, TestEvent> {
public:
    int entryCount = 0;
    int exitCount = 0;
    
    ActionFlowInstance() : FlowInstance(TestState::Start) {
        StateDefinition startState("Start");
        startState.transitions.emplace_back(TestEvent::Begin, TestState::Processing);
        startState.hasExitAction = true;
        startState.exitAction = [this]() {
            exitCount++;
        };
        addState(TestState::Start, startState);
        
        StateDefinition processingState("Processing");
        processingState.hasEntryAction = true;
        processingState.entryAction = [this]() {
            entryCount++;
        };
        processingState.isTerminal = true;
        addState(TestState::Processing, processingState);
    }
};

TEST(FlowRuntimeActionTest, EntryExitActions) {
    ActionFlowInstance flow;
    
    EXPECT_EQ(flow.entryCount, 0);
    EXPECT_EQ(flow.exitCount, 0);
    
    flow.trigger(TestEvent::Begin);
    
    EXPECT_EQ(flow.exitCount, 1); // Exit action from Start
    EXPECT_EQ(flow.entryCount, 1); // Entry action to Processing
}