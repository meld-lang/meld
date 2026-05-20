#include <gtest/gtest.h>
#include "../../include/meld/compiler/ide_integration.hpp"
#include "../../include/meld/compiler/effect_checker.hpp"
#include "../../include/meld/parser/ast.hpp"

using namespace meld::compiler;
using namespace meld::parser::ast;

class IDEIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ide_integration = std::make_unique<IDEIntegration>();
        effect_checker = std::make_unique<EffectChecker>();
    }

    std::unique_ptr<IDEIntegration> ide_integration;
    std::unique_ptr<EffectChecker> effect_checker;
    
    // Helper to create a mock function definition
    function_definition create_mock_function(const std::string& name, int line = 10) {
        function_definition func;
        func.name.name = name;
        func.has_effects = false; // No manual annotation by default
        return func;
    }
};

// TASK 35.9: Test inlay hint generation for inferred effects
TEST_F(IDEIntegrationTest, GenerateEffectInlayHints) {
    // Create mock functions
    std::vector<function_definition> functions;
    functions.push_back(create_mock_function("saveUser", 10));
    functions.push_back(create_mock_function("calculateSum", 20));
    
    // Create mock inferred effects
    std::map<std::string, std::set<std::string>> inferred_effects;
    inferred_effects["saveUser"] = {"EffectIO", "EffectNetwork"};
    inferred_effects["calculateSum"] = {"EffectPure"};
    
    // Generate inlay hints
    auto hints = ide_integration->generate_effect_inlay_hints(functions, inferred_effects);
    
    // Should generate hints for functions without manual annotations
    EXPECT_EQ(hints.size(), 2);
    
    // Check first hint (saveUser)
    auto saveUser_hint = std::find_if(hints.begin(), hints.end(), 
        [](const InlayHint& hint) { 
            return hint.text.find("EffectIO") != std::string::npos; 
        });
    ASSERT_NE(saveUser_hint, hints.end());
    EXPECT_EQ(saveUser_hint->kind, InlayHintKind::Effect);
    EXPECT_TRUE(saveUser_hint->is_ghost_annotation);
    EXPECT_EQ(saveUser_hint->position.line, 9); // Line before function (10-1)
    
    // Check second hint (calculateSum - pure function)
    auto calculateSum_hint = std::find_if(hints.begin(), hints.end(),
        [](const InlayHint& hint) {
            return hint.text == "@uses()";
        });
    ASSERT_NE(calculateSum_hint, hints.end());
    EXPECT_EQ(calculateSum_hint->kind, InlayHintKind::Effect);
    EXPECT_TRUE(calculateSum_hint->is_ghost_annotation);
}

// TASK 35.9: Test @uses annotation text generation
TEST_F(IDEIntegrationTest, GenerateUsesAnnotationText) {
    // Test pure function
    std::set<std::string> pure_effects = {"EffectPure"};
    std::string pure_annotation = ide_integration->generate_uses_annotation_text(pure_effects);
    EXPECT_EQ(pure_annotation, "@uses()");
    
    // Test empty effects (also pure)
    std::set<std::string> empty_effects;
    std::string empty_annotation = ide_integration->generate_uses_annotation_text(empty_effects);
    EXPECT_EQ(empty_annotation, "@uses()");
    
    // Test single effect
    std::set<std::string> io_effects = {"EffectIO"};
    std::string io_annotation = ide_integration->generate_uses_annotation_text(io_effects);
    EXPECT_EQ(io_annotation, "@uses(EffectIO)");
    
    // Test multiple effects
    std::set<std::string> multiple_effects = {"EffectIO", "EffectNetwork"};
    std::string multiple_annotation = ide_integration->generate_uses_annotation_text(multiple_effects);
    EXPECT_EQ(multiple_annotation, "@uses(EffectIO, EffectNetwork)");
    
    // Test mixed effects (should exclude EffectPure)
    std::set<std::string> mixed_effects = {"EffectPure", "EffectIO"};
    std::string mixed_annotation = ide_integration->generate_uses_annotation_text(mixed_effects);
    EXPECT_EQ(mixed_annotation, "@uses(EffectIO)");
}

// TASK 35.9: Test effect annotation hint generation
TEST_F(IDEIntegrationTest, GenerateEffectAnnotationHint) {
    auto func = create_mock_function("testFunction", 15);
    std::set<std::string> effects = {"EffectIO", "EffectNetwork"};
    
    auto annotation = ide_integration->generate_effect_annotation_hint(func, effects);
    
    EXPECT_EQ(annotation.function_name, "testFunction");
    EXPECT_EQ(annotation.position.line, 14); // Line before function (15-1)
    EXPECT_EQ(annotation.position.column, 0);
    EXPECT_EQ(annotation.inferred_effects, effects);
    EXPECT_EQ(annotation.annotation_text, "@uses(EffectIO, EffectNetwork)");
    EXPECT_FALSE(annotation.is_manually_written); // No manual annotation
    EXPECT_FALSE(annotation.needs_update);
}

// TASK 35.9: Test document change handling
TEST_F(IDEIntegrationTest, OnDocumentChange) {
    std::string file_path = "test.meld";
    std::string content = "fnc saveUser() { ... }";
    
    std::vector<function_definition> functions;
    functions.push_back(create_mock_function("saveUser", 1));
    
    std::map<std::string, std::set<std::string>> inferred_effects;
    inferred_effects["saveUser"] = {"EffectIO"};
    
    // Update document
    ide_integration->on_document_change(file_path, content, functions, inferred_effects);
    
    // Check that hints were generated and cached
    auto hints = ide_integration->get_inlay_hints_for_file(file_path);
    EXPECT_EQ(hints.size(), 1);
    EXPECT_EQ(hints[0].text, "@uses(EffectIO)");
    
    auto annotations = ide_integration->get_effect_annotations_for_file(file_path);
    EXPECT_EQ(annotations.size(), 1);
    EXPECT_EQ(annotations[0].function_name, "saveUser");
}

// TASK 35.9: Test JSON export for LSP
TEST_F(IDEIntegrationTest, ExportInlayHintsAsJson) {
    std::vector<InlayHint> hints;
    
    InlayHint hint1(SourcePosition(10, 0), "@uses(EffectIO)", InlayHintKind::Effect, true);
    hint1.tooltip = "Inferred effects for function";
    hints.push_back(hint1);
    
    InlayHint hint2(SourcePosition(20, 0), "@uses()", InlayHintKind::Effect, true);
    hints.push_back(hint2);
    
    std::string json = ide_integration->export_inlay_hints_as_json(hints);
    
    // Check that JSON contains expected structure
    EXPECT_NE(json.find("\"position\""), std::string::npos);
    EXPECT_NE(json.find("\"line\":10"), std::string::npos);
    EXPECT_NE(json.find("\"line\":20"), std::string::npos);
    EXPECT_NE(json.find("\"label\":\"@uses(EffectIO)\""), std::string::npos);
    EXPECT_NE(json.find("\"label\":\"@uses()\""), std::string::npos);
    EXPECT_NE(json.find("\"isGhostAnnotation\":true"), std::string::npos);
}

// TASK 35.9: Test clearing hints for file
TEST_F(IDEIntegrationTest, ClearHintsForFile) {
    std::string file_path = "test.meld";
    
    // Add some hints
    std::vector<function_definition> functions;
    functions.push_back(create_mock_function("testFunc"));
    
    std::map<std::string, std::set<std::string>> inferred_effects;
    inferred_effects["testFunc"] = {"EffectIO"};
    
    ide_integration->on_document_change(file_path, "", functions, inferred_effects);
    
    // Verify hints exist
    EXPECT_FALSE(ide_integration->get_inlay_hints_for_file(file_path).empty());
    EXPECT_FALSE(ide_integration->get_effect_annotations_for_file(file_path).empty());
    
    // Clear hints
    ide_integration->clear_hints_for_file(file_path);
    
    // Verify hints are cleared
    EXPECT_TRUE(ide_integration->get_inlay_hints_for_file(file_path).empty());
    EXPECT_TRUE(ide_integration->get_effect_annotations_for_file(file_path).empty());
}

// TASK 35.9: Test incremental change handling
TEST_F(IDEIntegrationTest, OnIncrementalChange) {
    std::string file_path = "test.meld";
    
    // Set up initial state with some hints
    std::vector<function_definition> functions;
    functions.push_back(create_mock_function("func1", 5));
    functions.push_back(create_mock_function("func2", 15));
    functions.push_back(create_mock_function("func3", 25));
    
    std::map<std::string, std::set<std::string>> inferred_effects;
    inferred_effects["func1"] = {"EffectIO"};
    inferred_effects["func2"] = {"EffectNetwork"};
    inferred_effects["func3"] = {"EffectPure"};
    
    ide_integration->on_document_change(file_path, "", functions, inferred_effects);
    
    // Verify initial state
    auto initial_hints = ide_integration->get_inlay_hints_for_file(file_path);
    EXPECT_EQ(initial_hints.size(), 3);
    
    // Simulate incremental change affecting lines 10-20 (should affect func2)
    ide_integration->on_incremental_change(file_path, 10, 20, "new code\nmore code");
    
    // Check that hints are properly managed
    auto updated_hints = ide_integration->get_inlay_hints_for_file(file_path);
    
    // Should preserve hints for func1 (line 5) and func3 (line 25, adjusted)
    // func2 (line 15) should be removed as it's in the changed region
    EXPECT_LE(updated_hints.size(), 2); // At most 2 hints should remain
}

// TASK 35.9: Test ghost annotation refresh detection
TEST_F(IDEIntegrationTest, NeedsGhostAnnotationRefresh) {
    std::string file_path = "test.meld";
    std::string function_name = "testFunc";
    
    // Initial effects
    std::set<std::string> initial_effects = {"EffectIO"};
    
    // Should need refresh when no previous effects known
    EXPECT_TRUE(ide_integration->needs_ghost_annotation_refresh(file_path, function_name, initial_effects));
    
    // Set up initial state
    std::vector<function_definition> functions;
    functions.push_back(create_mock_function(function_name));
    
    std::map<std::string, std::set<std::string>> inferred_effects;
    inferred_effects[function_name] = initial_effects;
    
    ide_integration->on_document_change(file_path, "", functions, inferred_effects);
    
    // Should not need refresh with same effects
    EXPECT_FALSE(ide_integration->needs_ghost_annotation_refresh(file_path, function_name, initial_effects));
    
    // Should need refresh with different effects
    std::set<std::string> new_effects = {"EffectIO", "EffectNetwork"};
    EXPECT_TRUE(ide_integration->needs_ghost_annotation_refresh(file_path, function_name, new_effects));
}

// TASK 35.9: Test changed ghost annotations tracking
TEST_F(IDEIntegrationTest, GetChangedGhostAnnotations) {
    std::string file_path = "test.meld";
    
    // Set up initial state
    std::vector<function_definition> functions;
    functions.push_back(create_mock_function("func1"));
    functions.push_back(create_mock_function("func2"));
    
    std::map<std::string, std::set<std::string>> initial_effects;
    initial_effects["func1"] = {"EffectIO"};
    initial_effects["func2"] = {"EffectPure"};
    
    ide_integration->on_document_change(file_path, "", functions, initial_effects);
    
    // Update with changed effects for func1
    std::map<std::string, std::set<std::string>> updated_effects;
    updated_effects["func1"] = {"EffectIO", "EffectNetwork"}; // Changed
    updated_effects["func2"] = {"EffectPure"}; // Unchanged
    
    ide_integration->on_document_change(file_path, "", functions, updated_effects);
    
    // Get changed annotations
    auto changed_annotations = ide_integration->get_changed_ghost_annotations(file_path);
    
    // Should only include func1 (the one that changed)
    EXPECT_EQ(changed_annotations.size(), 1);
    EXPECT_EQ(changed_annotations[0].function_name, "func1");
    
    // Calling again should return empty (changes consumed)
    auto changed_again = ide_integration->get_changed_ghost_annotations(file_path);
    EXPECT_TRUE(changed_again.empty());
}

// ---------------------------------------------------------------------------
// TASK 9.2 (implicit-effect-calls): Ghost text reflects implicit effect calls
// Requirement 5.4: LSP ghost text displays inferred @uses reflecting implicit calls
// ---------------------------------------------------------------------------

// Verify that when EffectChecker infers effects from a function body containing
// implicit_effect_call nodes, the IDEIntegration ghost text includes those effects.
TEST_F(IDEIntegrationTest, GhostTextIncludesEffectsFromImplicitCalls) {
    // Build a function whose body is a single implicit_effect_call:
    //   fnc logMessage(msg: String) { Console.println(msg) }
    // The implicit call to Console.println should cause EffectChecker to infer "Console".
    implicit_effect_call implicit_call;
    implicit_call.effect_name.name = "Console";
    implicit_call.operation_name.name = "println";

    function_definition func = create_mock_function("logMessage", 5);
    block_expression body_block;
    body_block.statements.push_back(expression(implicit_call));
    func.body = body_block;

    // Use EffectChecker to infer effects (the same path the LSP server would use).
    auto inferred = effect_checker->infer_effects_for_functions({func});

    // The inferred set for logMessage must contain "Console".
    ASSERT_TRUE(inferred.find("logMessage") != inferred.end());
    EXPECT_TRUE(inferred["logMessage"].count("Console") > 0)
        << "Inferred effects should include 'Console' from implicit effect call";

    // Feed the inferred effects into IDEIntegration to generate ghost text hints.
    auto hints = ide_integration->generate_effect_inlay_hints({func}, inferred);

    // There should be exactly one ghost hint for logMessage.
    ASSERT_EQ(hints.size(), 1);
    EXPECT_EQ(hints[0].kind, InlayHintKind::Effect);
    EXPECT_TRUE(hints[0].is_ghost_annotation);

    // The annotation text must mention Console.
    EXPECT_NE(hints[0].text.find("Console"), std::string::npos)
        << "Ghost text annotation should include 'Console' from implicit effect call, got: "
        << hints[0].text;
}

// Verify ghost text with multiple implicit effect calls in one function body.
TEST_F(IDEIntegrationTest, GhostTextIncludesMultipleImplicitEffects) {
    // Build two implicit calls: Console.println and Time.now
    implicit_effect_call console_call;
    console_call.effect_name.name = "Console";
    console_call.operation_name.name = "println";

    implicit_effect_call time_call;
    time_call.effect_name.name = "Time";
    time_call.operation_name.name = "now";

    // Wrap both in a block expression so the function body contains both calls.
    block_expression block;
    block.statements.push_back(
        expression(console_call));
    block.statements.push_back(
        expression(time_call));

    function_definition func = create_mock_function("logWithTimestamp", 10);
    func.body = boost::spirit::x3::forward_ast<block_expression>(block);

    auto inferred = effect_checker->infer_effects_for_functions({func});

    ASSERT_TRUE(inferred.find("logWithTimestamp") != inferred.end());
    const auto& effects = inferred["logWithTimestamp"];
    EXPECT_TRUE(effects.count("Console") > 0)
        << "Should infer Console from implicit Console.println call";
    EXPECT_TRUE(effects.count("Time") > 0)
        << "Should infer Time from implicit Time.now call";

    auto hints = ide_integration->generate_effect_inlay_hints({func}, inferred);

    ASSERT_EQ(hints.size(), 1);
    EXPECT_TRUE(hints[0].is_ghost_annotation);
    EXPECT_NE(hints[0].text.find("Console"), std::string::npos)
        << "Ghost text should include Console, got: " << hints[0].text;
    EXPECT_NE(hints[0].text.find("Time"), std::string::npos)
        << "Ghost text should include Time, got: " << hints[0].text;
}

// Verify that a function with only explicit perform_expression calls AND implicit
// calls produces ghost text that includes effects from both.
TEST_F(IDEIntegrationTest, GhostTextIncludesMixedImplicitAndExplicitEffects) {
    // Implicit call: Console.println
    implicit_effect_call implicit_call;
    implicit_call.effect_name.name = "Console";
    implicit_call.operation_name.name = "println";

    // Explicit call: perform { Time.now() }
    perform_expression explicit_call;
    explicit_call.effect_name.name = "Time";
    explicit_call.operation_name.name = "now";
    explicit_call.is_deprecated = true;

    block_expression block;
    block.statements.push_back(
        expression(implicit_call));
    block.statements.push_back(
        expression(explicit_call));

    function_definition func = create_mock_function("mixedEffects", 20);
    func.body = boost::spirit::x3::forward_ast<block_expression>(block);

    auto inferred = effect_checker->infer_effects_for_functions({func});

    ASSERT_TRUE(inferred.find("mixedEffects") != inferred.end());
    const auto& effects = inferred["mixedEffects"];
    EXPECT_TRUE(effects.count("Console") > 0);
    EXPECT_TRUE(effects.count("Time") > 0);

    auto hints = ide_integration->generate_effect_inlay_hints({func}, inferred);

    ASSERT_EQ(hints.size(), 1);
    EXPECT_NE(hints[0].text.find("Console"), std::string::npos);
    EXPECT_NE(hints[0].text.find("Time"), std::string::npos);
}
