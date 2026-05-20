#include "meld/ide/flow_visualizer.hpp"
#include "meld/parser/ast.hpp"
#include <gtest/gtest.h>

using namespace meld;
using namespace meld::ide;
using namespace meld::parser;

class FlowVisualizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test flow
        testFlow.name.name = "TestFlow";
        testFlow.initial_state.name = "Start";
        
        // Create states
        ast::flow_state startState;
        startState.name.name = "Start";
        startState.has_entry_action = false;
        startState.has_exit_action = false;
        
        ast::flow_state processState;
        processState.name.name = "Processing";
        processState.has_entry_action = true;
        processState.has_exit_action = false;
        
        ast::flow_state endState;
        endState.name.name = "End";
        endState.has_entry_action = false;
        endState.has_exit_action = true;
        
        // Create transitions
        ast::flow_transition startToProcess;
        startToProcess.event_name.name = "Begin";
        startToProcess.target_state.name = "Processing";
        startToProcess.has_guard = false;
        startState.transitions.push_back(startToProcess);
        
        ast::flow_transition processToEnd;
        processToEnd.event_name.name = "Complete";
        processToEnd.target_state.name = "End";
        processToEnd.has_guard = true;
        processState.transitions.push_back(processToEnd);
        
        testFlow.states = {startState, processState, endState};
    }
    
    ast::flow_definition testFlow;
};

TEST_F(FlowVisualizerTest, CanVisualizeValidFlow) {
    EXPECT_TRUE(FlowVisualizer::canVisualize(testFlow));
}

TEST_F(FlowVisualizerTest, CannotVisualizeEmptyFlow) {
    ast::flow_definition emptyFlow;
    emptyFlow.name.name = "Empty";
    emptyFlow.initial_state.name = "Start";
    // No states
    
    EXPECT_FALSE(FlowVisualizer::canVisualize(emptyFlow));
}

TEST_F(FlowVisualizerTest, CannotVisualizeFlowWithMissingInitialState) {
    ast::flow_definition invalidFlow = testFlow;
    invalidFlow.initial_state.name = "NonExistent";
    
    EXPECT_FALSE(FlowVisualizer::canVisualize(invalidFlow));
}

TEST_F(FlowVisualizerTest, CreateVisualizationGeneratesCorrectNodes) {
    auto chart = FlowVisualizer::createVisualization(testFlow);
    
    EXPECT_EQ(chart.title, "TestFlow");
    EXPECT_EQ(chart.nodes.size(), 3);
    
    // Check node types
    bool hasInitial = false, hasTerminal = false, hasRegular = false;
    for (const auto& node : chart.nodes) {
        if (node.type == "initial") hasInitial = true;
        else if (node.type == "terminal") hasTerminal = true;
        else if (node.type == "state") hasRegular = true;
    }
    
    EXPECT_TRUE(hasInitial);
    EXPECT_TRUE(hasTerminal);
    EXPECT_TRUE(hasRegular);
}

TEST_F(FlowVisualizerTest, CreateVisualizationGeneratesCorrectEdges) {
    auto chart = FlowVisualizer::createVisualization(testFlow);
    
    EXPECT_EQ(chart.edges.size(), 2);
    
    // Check edge labels
    bool hasBeginEdge = false, hasCompleteEdge = false;
    for (const auto& edge : chart.edges) {
        if (edge.label == "Begin") hasBeginEdge = true;
        else if (edge.label == "Complete") hasCompleteEdge = true;
    }
    
    EXPECT_TRUE(hasBeginEdge);
    EXPECT_TRUE(hasCompleteEdge);
}

TEST_F(FlowVisualizerTest, ExportToMermaidFormat) {
    std::string mermaid = FlowVisualizer::exportFlow(testFlow, ExportFormat::MERMAID);
    
    EXPECT_FALSE(mermaid.empty());
    EXPECT_NE(mermaid.find("flowchart TD"), std::string::npos);
    EXPECT_NE(mermaid.find("TestFlow"), std::string::npos);
    EXPECT_NE(mermaid.find("Begin"), std::string::npos);
    EXPECT_NE(mermaid.find("Complete"), std::string::npos);
}

TEST_F(FlowVisualizerTest, ExportToDotFormat) {
    std::string dot = FlowVisualizer::exportFlow(testFlow, ExportFormat::DOT);
    
    EXPECT_FALSE(dot.empty());
    EXPECT_NE(dot.find("digraph"), std::string::npos);
    EXPECT_NE(dot.find("TestFlow"), std::string::npos);
    EXPECT_NE(dot.find("Begin"), std::string::npos);
    EXPECT_NE(dot.find("Complete"), std::string::npos);
}

TEST_F(FlowVisualizerTest, ExportToJsonFormat) {
    std::string json = FlowVisualizer::exportFlow(testFlow, ExportFormat::JSON);
    
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("\"title\""), std::string::npos);
    EXPECT_NE(json.find("\"TestFlow\""), std::string::npos);
    EXPECT_NE(json.find("\"nodes\""), std::string::npos);
    EXPECT_NE(json.find("\"edges\""), std::string::npos);
}

TEST_F(FlowVisualizerTest, ExportToSvgFormat) {
    std::string svg = FlowVisualizer::exportFlow(testFlow, ExportFormat::SVG);
    
    EXPECT_FALSE(svg.empty());
    EXPECT_NE(svg.find("<svg"), std::string::npos);
    EXPECT_NE(svg.find("TestFlow"), std::string::npos);
    EXPECT_NE(svg.find("<rect"), std::string::npos);
    EXPECT_NE(svg.find("<line"), std::string::npos);
}

TEST_F(FlowVisualizerTest, ExportToPlantUmlFormat) {
    std::string plantuml = FlowVisualizer::exportFlow(testFlow, ExportFormat::PLANTUML);
    
    EXPECT_FALSE(plantuml.empty());
    EXPECT_NE(plantuml.find("@startuml"), std::string::npos);
    EXPECT_NE(plantuml.find("@enduml"), std::string::npos);
    EXPECT_NE(plantuml.find("TestFlow"), std::string::npos);
}

TEST_F(FlowVisualizerTest, GetVisualizationMetadata) {
    auto metadata = FlowVisualizer::getVisualizationMetadata(testFlow);
    
    EXPECT_EQ(metadata["name"], "TestFlow");
    EXPECT_EQ(metadata["initial_state"], "Start");
    EXPECT_EQ(metadata["state_count"], "3");
    EXPECT_EQ(metadata["transition_count"], "2");
    EXPECT_EQ(metadata["has_entry_actions"], "true");
    EXPECT_EQ(metadata["has_exit_actions"], "true");
}

TEST_F(FlowVisualizerTest, IdeIntegrationGeneratesVisualization) {
    std::string ideViz = FlowIdeIntegration::generateIdeVisualization(testFlow);
    
    EXPECT_FALSE(ideViz.empty());
    EXPECT_NE(ideViz.find("meld_flow_visualization"), std::string::npos);
    EXPECT_NE(ideViz.find("interactive"), std::string::npos);
    EXPECT_NE(ideViz.find("navigation"), std::string::npos);
}

TEST_F(FlowVisualizerTest, IdeIntegrationCreatesHoverInfo) {
    const auto& startState = testFlow.states[0];
    std::string hoverInfo = FlowIdeIntegration::createHoverInfo(startState);
    
    EXPECT_FALSE(hoverInfo.empty());
    EXPECT_NE(hoverInfo.find("State: Start"), std::string::npos);
    EXPECT_NE(hoverInfo.find("Transitions:"), std::string::npos);
}

TEST_F(FlowVisualizerTest, IdeIntegrationGeneratesNavigationLinks) {
    auto navLinks = FlowIdeIntegration::generateNavigationLinks(testFlow);
    
    EXPECT_FALSE(navLinks.empty());
    EXPECT_NE(navLinks.find("flow_definition"), navLinks.end());
    EXPECT_NE(navLinks.find("state_Start"), navLinks.end());
    EXPECT_NE(navLinks.find("state_Processing"), navLinks.end());
    EXPECT_NE(navLinks.find("state_End"), navLinks.end());
}

TEST_F(FlowVisualizerTest, IdeIntegrationValidatesFlow) {
    auto issues = FlowIdeIntegration::validateForIde(testFlow);
    
    // Should have no issues for a simple, valid flow
    EXPECT_TRUE(issues.empty());
}

TEST_F(FlowVisualizerTest, IdeIntegrationDetectsLargeFlow) {
    ast::flow_definition largeFlow = testFlow;
    
    // Add many states to trigger warning
    for (int i = 0; i < 25; ++i) {
        ast::flow_state state;
        state.name.name = "State" + std::to_string(i);
        largeFlow.states.push_back(state);
    }
    
    auto issues = FlowIdeIntegration::validateForIde(largeFlow);
    
    EXPECT_FALSE(issues.empty());
    EXPECT_NE(issues[0].find("many states"), std::string::npos);
}

TEST_F(FlowVisualizerTest, IdeIntegrationCreatesInteractiveVisualization) {
    std::string interactive = FlowIdeIntegration::createInteractiveVisualization(testFlow);
    
    EXPECT_FALSE(interactive.empty());
    EXPECT_NE(interactive.find("interactive_chart"), std::string::npos);
    EXPECT_NE(interactive.find("interactions"), std::string::npos);
    EXPECT_NE(interactive.find("rendering"), std::string::npos);
}

// Test edge cases and error conditions

TEST_F(FlowVisualizerTest, HandlesFlowWithNoTransitions) {
    ast::flow_definition noTransitionsFlow;
    noTransitionsFlow.name.name = "NoTransitions";
    noTransitionsFlow.initial_state.name = "Isolated";
    
    ast::flow_state isolatedState;
    isolatedState.name.name = "Isolated";
    // No transitions
    
    noTransitionsFlow.states = {isolatedState};
    
    EXPECT_TRUE(FlowVisualizer::canVisualize(noTransitionsFlow));
    
    auto chart = FlowVisualizer::createVisualization(noTransitionsFlow);
    EXPECT_EQ(chart.nodes.size(), 1);
    EXPECT_EQ(chart.edges.size(), 0);
}

TEST_F(FlowVisualizerTest, HandlesSpecialCharactersInNames) {
    ast::flow_definition specialFlow = testFlow;
    specialFlow.name.name = "Special-Flow_With.Chars";
    specialFlow.states[0].name.name = "Start-State";
    
    EXPECT_TRUE(FlowVisualizer::canVisualize(specialFlow));
    
    std::string json = FlowVisualizer::exportFlow(specialFlow, ExportFormat::JSON);
    EXPECT_NE(json.find("Special-Flow_With.Chars"), std::string::npos);
    
    std::string mermaid = FlowVisualizer::exportFlow(specialFlow, ExportFormat::MERMAID);
    EXPECT_NE(mermaid.find("Start-State"), std::string::npos);
}