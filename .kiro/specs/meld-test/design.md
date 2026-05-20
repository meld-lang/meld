# Design Document

## Overview

The Meld Testing Library is a unified testing framework that combines test execution, fluent assertions, comprehensive mocking, and behavior-driven development capabilities into a single cohesive API. The design draws inspiration from JUnit 6's annotation-driven test lifecycle, Mockito 5's intuitive mocking syntax, AssertJ 3's fluent assertion chains, and Cucumber's Given-When-Then BDD approach, while integrating seamlessly with Meld's unique language features.

The library follows a modular architecture with four core subsystems:
- **Test Execution Engine**: Manages test discovery, lifecycle, and execution
- **Fluent Assertion System**: Provides expressive, chainable validation methods  
- **Mock Framework**: Creates and manages test doubles with behavior verification
- **BDD Framework**: Enables behavior-driven development with Given-When-Then scenarios

## Architecture

The framework uses a layered architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────┐
│                 Test API Layer                      │
│  (Fluent interfaces, annotations, BDD DSL)         │
├─────────────────────────────────────────────────────┤
│                Execution Engine                     │
│  (Test discovery, lifecycle, BDD scenarios)        │
├─────────────────────────────────────────────────────┤
│              Core Services Layer                    │
│  (Assertions, mocks, step definitions, parser)     │
├─────────────────────────────────────────────────────┤
│               Runtime Support                       │
│  (Reflection, code generation, async handling)     │
└─────────────────────────────────────────────────────┘
```

The design emphasizes:
- **Fluent Interface Design**: Method chaining for natural test expression
- **Type Safety**: Compile-time validation of test constructs
- **Extensibility**: Plugin architecture for custom matchers and generators
- **Performance**: Minimal overhead during test execution
- **Integration**: Native support for Meld language features

## Components and Interfaces

### Test Execution Engine

**TestRunner Interface**
```meld
interface TestRunner {
    discover_tests(path: String) -> TestSuite
    execute_suite(suite: TestSuite) -> TestResults
    execute_test(test: TestCase) -> TestResult
}
```

**TestCase Interface**
```meld
interface TestCase {
    name: String
    setup_methods: List<Method>
    test_method: Method
    teardown_methods: List<Method>
    timeout: Duration?
    expected_exception: Type?
}
```

### Fluent Assertion System

**AssertionBuilder Interface**
```meld
interface AssertionBuilder<T> {
    is_equal_to(expected: T) -> AssertionBuilder<T>
    is_not_equal_to(unexpected: T) -> AssertionBuilder<T>
    satisfies(predicate: T -> Boolean) -> AssertionBuilder<T>
    matches(matcher: Matcher<T>) -> AssertionBuilder<T>
}
```

**CollectionAssertions Interface**
```meld
interface CollectionAssertions<T> extends AssertionBuilder<Collection<T>> {
    has_size(expected: Int) -> CollectionAssertions<T>
    contains(elements: T...) -> CollectionAssertions<T>
    contains_exactly(elements: T...) -> CollectionAssertions<T>
    is_sorted() -> CollectionAssertions<T>
}
```

### Mock Framework

**MockBuilder Interface**
```meld
interface MockBuilder<T> {
    create() -> T
    when_called(method: String) -> BehaviorBuilder<T>
    verify(invocations: Int) -> VerificationBuilder<T>
}
```

**BehaviorBuilder Interface**
```meld
interface BehaviorBuilder<T> {
    return_value(value: Any) -> MockBuilder<T>
    throw_exception(exception: Exception) -> MockBuilder<T>
    call_real_method() -> MockBuilder<T>
}
```

### BDD Framework

**ScenarioBuilder Interface**
```meld
interface ScenarioBuilder {
    given(step: String) -> GivenBuilder
    scenario(name: String) -> ScenarioBuilder
    feature(name: String) -> FeatureBuilder
}
```

**GivenBuilder Interface**
```meld
interface GivenBuilder {
    and_given(step: String) -> GivenBuilder
    when(step: String) -> WhenBuilder
}
```

**WhenBuilder Interface**
```meld
interface WhenBuilder {
    and_when(step: String) -> WhenBuilder
    then(step: String) -> ThenBuilder
}
```

**ThenBuilder Interface**
```meld
interface ThenBuilder {
    and_then(step: String) -> ThenBuilder
    execute() -> ScenarioResult
}
```

**StepDefinitionRegistry Interface**
```meld
interface StepDefinitionRegistry {
    discover_annotated_methods(class: Type) -> List<StepDefinition>
    register_step_definition(definition: StepDefinition) -> Void
    find_step(step_text: String) -> StepDefinition?
    get_all_patterns() -> List<String>
}
```

**Step Definition Annotations**
```meld
@given("I have {int} items in my cart")
fn setup_cart_with_items(count: Int) -> Void {
    // Implementation
}

@when("I add {int} more items")
fn add_items_to_cart(additional_count: Int) -> Void {
    // Implementation
}

@then("my cart should contain {int} items")
fn verify_cart_count(expected_count: Int) -> Void {
    MELD_ASSERT_THAT(cart.size()).is_equal_to(expected_count);
}

// Support for different parameter types
@given("I have a user named {string}")
fn setup_user(name: String) -> Void {
    // Implementation
}

@when("I set the price to {double}")
fn set_price(price: Double) -> Void {
    // Implementation
}

@then("the result should be {word}")
fn verify_result(result: String) -> Void {
    // Implementation
}

// Support for data tables
@given("the following users exist:")
fn setup_users(users: DataTable) -> Void {
    // users.asList() or users.asMap()
}

// Support for doc strings
@when("I submit the following JSON:")
fn submit_json(json_content: String) -> Void {
    // json_content contains the doc string
}
```

**GherkinParser Interface**
```meld
interface GherkinParser {
    parse_feature_file(path: String) -> Feature
    parse_scenario(text: String) -> Scenario
    validate_syntax(content: String) -> List<ParseError>
}

**StepDefinitionDiscovery Interface**
```meld
interface StepDefinitionDiscovery {
    scan_classes(packages: List<String>) -> List<StepDefinition>
    scan_class(class_type: Type) -> List<StepDefinition>
    validate_step_definitions(definitions: List<StepDefinition>) -> List<ValidationError>
}
```

## Data Models

### Test Metadata Model
```meld
struct TestMetadata {
    name: String
    description: String?
    tags: Set<String>
    timeout: Duration?
    expected_exception: Type?
    disabled: Boolean
    parameters: List<ParameterSet>?
}
```

### Test Result Model
```meld
struct TestResult {
    test_name: String
    status: TestStatus
    execution_time: Duration
    failure_reason: String?
    stack_trace: String?
    assertions_count: Int
}

enum TestStatus {
    PASSED
    FAILED
    SKIPPED
    ABORTED
}
```

### Mock Interaction Model
```meld
struct MockInteraction {
    method_name: String
    arguments: List<Any>
    return_value: Any?
    exception: Exception?
    invocation_time: Timestamp
}
```

### BDD Data Models

**Feature Model**
```meld
struct Feature {
    name: String
    description: String?
    scenarios: List<Scenario>
    background: List<Step>?
    tags: Set<String>
}
```

**Scenario Model**
```meld
struct Scenario {
    name: String
    description: String?
    given_steps: List<Step>
    when_steps: List<Step>
    then_steps: List<Step>
    tags: Set<String>
}
```

**Step Model**
```meld
struct Step {
    keyword: StepKeyword
    text: String
    cucumber_expression: String  // Converted from Gherkin to Cucumber expression
    extracted_parameters: List<Any>?
    data_table: DataTable?
    doc_string: String?
    line_number: Int
}

enum StepKeyword {
    GIVEN
    WHEN
    THEN
    AND
    BUT
}

// Cucumber expression parameter extraction
struct ParameterExtractor {
    fn extract_parameters(step_text: String, pattern: String) -> List<Any>
    fn convert_gherkin_to_cucumber_expression(gherkin_step: String) -> String
    fn match_pattern(step_text: String, cucumber_expression: String) -> Boolean
}
```

**StepDefinition Model**
```meld
struct StepDefinition {
    cucumber_expression: String  // e.g., "I have {int} items"
    compiled_pattern: Regex      // Compiled regex from cucumber expression
    method: Method
    parameter_types: List<ParameterType>
    step_type: StepKeyword
    source_location: String
    supports_data_table: Boolean
    supports_doc_string: Boolean
}

// Annotation types for step definitions
@annotation
struct given {
    pattern: String
}

@annotation  
struct when {
    pattern: String
}

@annotation
struct then {
    pattern: String
}

// Parameter type definitions for Cucumber expressions
enum ParameterType {
    INT("{int}")
    STRING("{string}")
    WORD("{word}")
    DOUBLE("{double}")
    FLOAT("{float}")
    BYTE("{byte}")
    SHORT("{short}")
    LONG("{long}")
    BIG_INTEGER("{biginteger}")
    BIG_DECIMAL("{bigdecimal}")
}

// Data structures for step parameters
struct DataTable {
    rows: List<List<String>>
    
    fn as_list() -> List<String>
    fn as_map() -> Map<String, String>
    fn as_list_of_maps() -> List<Map<String, String>>
}
```

**ScenarioResult Model**
```meld
struct ScenarioResult {
    scenario_name: String
    status: TestStatus
    step_results: List<StepResult>
    execution_time: Duration
    failure_step: String?
}

struct StepResult {
    step_text: String
    status: TestStatus
    execution_time: Duration
    error_message: String?
}
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

Property 1: Fluent interface chaining
*For any* assertion method call, the returned object should provide methods for further chaining validation operations
**Validates: Requirements 1.1**

Property 2: Complete assertion execution
*For any* chain of multiple assertions, all assertions should execute and collect failure information rather than short-circuiting on first failure
**Validates: Requirements 1.2**

Property 3: Detailed failure messages
*For any* failed assertion, the error message should contain both expected and actual values with clear formatting
**Validates: Requirements 1.3**

Property 4: Deep equality validation
*For any* complex nested object comparison, the equality check should validate all fields at all nesting levels
**Validates: Requirements 1.4**

Property 5: Collection assertion completeness
*For any* collection type, specialized assertion methods for size, contents, and ordering should be available and functional
**Validates: Requirements 1.5**

Property 6: Mock interface implementation
*For any* interface type, creating a mock should produce a test double that implements all methods of that interface
**Validates: Requirements 2.1**

Property 7: Mock behavior configuration
*For any* mock method, stubbing should support return values and exception throwing as configured
**Validates: Requirements 2.2**

Property 8: Mock interaction tracking
*For any* mock method call, the framework should accurately track method name, arguments, and invocation count
**Validates: Requirements 2.3**

Property 9: Argument matcher flexibility
*For any* argument matcher type (wildcard, custom predicate), the matching should work correctly during verification
**Validates: Requirements 2.4**

Property 10: Static method mocking
*For any* static method, mocking should support both stubbing behavior and verifying interactions
**Validates: Requirements 2.5**

Property 11: Test method discovery
*For any* class with annotated test methods, the framework should discover and execute all properly marked methods
**Validates: Requirements 3.1**

Property 12: Setup method execution order
*For any* test case with setup methods, setup should execute before the test method in the correct sequence
**Validates: Requirements 3.2**

Property 13: Cleanup method execution order
*For any* test case with cleanup methods, cleanup should execute after the test method in the correct sequence
**Validates: Requirements 3.3**

Property 14: Class-level lifecycle management
*For any* test suite with class-level setup/teardown, these methods should execute at appropriate times for the entire class
**Validates: Requirements 3.4**

Property 15: Test dependency handling
*For any* test with dependencies, execution order and conditional execution should respect the dependency constraints
**Validates: Requirements 3.5**

Property 16: Test result reporting completeness
*For any* test execution, the report should include pass/fail status, execution time, and failure details for each test
**Validates: Requirements 4.1**

Property 17: Failure context preservation
*For any* failed test, the report should include stack trace and contextual information about the failure location
**Validates: Requirements 4.2**

Property 18: Performance metrics collection
*For any* test suite execution, timing information and performance metrics should be captured and reported
**Validates: Requirements 4.3**

Property 19: Test filtering accuracy
*For any* applied filter criteria, only tests matching the filter should execute while others are skipped
**Validates: Requirements 4.4**

Property 20: Standard format compliance
*For any* test result output, the format should comply with standard testing result formats for IDE integration
**Validates: Requirements 4.5**

Property 21: Parameterized test execution
*For any* parameterized test with N parameter sets, the test method should execute exactly N times with correct parameters
**Validates: Requirements 5.2**

Property 22: Parameter failure reporting
*For any* failing parameterized test, the report should identify which specific parameter set caused the failure
**Validates: Requirements 5.3**

Property 23: Property test generation
*For any* property-based test specification, test cases should be automatically generated according to the property constraints
**Validates: Requirements 6.1**

Property 24: Constrained input generation
*For any* property test with input constraints, all generated inputs should satisfy the specified constraints
**Validates: Requirements 6.2**

Property 25: Shrinking effectiveness
*For any* failing property test, the framework should provide a minimal failing example through shrinking
**Validates: Requirements 6.3**

Property 26: Fluent verification chaining
*For any* mock verification operation, the result should support chaining additional verification calls
**Validates: Requirements 7.1**

Property 27: Mixed assertion chains
*For any* combination of assertions and mock verifications, they should work together in a single fluent chain
**Validates: Requirements 7.2**

Property 28: Custom matcher integration
*For any* custom matcher, it should integrate seamlessly with fluent assertion chains
**Validates: Requirements 7.3**

Property 29: Async assertion support
*For any* async operation assertion, the framework should handle async patterns correctly with proper awaiting
**Validates: Requirements 7.4**

Property 30: Meld pattern matching assertions
*For any* Meld pattern matching test, specialized assertions should correctly validate pattern match results
**Validates: Requirements 8.1**

Property 31: Null-safe assertion chains
*For any* nullable type assertion, the chain should handle null values safely without throwing unexpected exceptions
**Validates: Requirements 8.2**

Property 32: Meld collection integration
*For any* Meld collection type, assertions should work correctly with Meld-specific collection operations
**Validates: Requirements 8.3**

Property 33: Effect testing utilities
*For any* effect-based code test, the framework should provide utilities that correctly handle effect execution and validation
**Validates: Requirements 8.4**

Property 34: Meld async pattern support
*For any* Meld async operation test, the framework should support native Meld async/await patterns correctly
**Validates: Requirements 8.5**

Property 35: BDD fluent interface chaining
*For any* BDD scenario construction, the Given-When-Then interface should support proper method chaining for scenario building
**Validates: Requirements 9.1**

Property 36: Step definition mapping
*For any* registered step definition with a pattern, finding a matching step text should return the correct implementation
**Validates: Requirements 9.2**

Property 37: BDD execution order
*For any* BDD scenario, Given steps should execute before When steps, and When steps should execute before Then steps
**Validates: Requirements 9.3**

Property 38: BDD failure reporting
*For any* failing BDD scenario, the error report should identify the specific failing step with business-readable error messages
**Validates: Requirements 9.4**

Property 39: Feature organization
*For any* collection of BDD scenarios, they should be correctly grouped by features and maintain organizational structure during execution
**Validates: Requirements 9.5**

Property 40: Gherkin parsing correctness
*For any* valid Gherkin syntax, parsing should correctly identify all Feature, Scenario, and step elements
**Validates: Requirements 10.1**

Property 41: Feature file discovery
*For any* directory containing .feature files, all files should be automatically discovered and parsed correctly
**Validates: Requirements 10.2**

Property 42: Step mapping from files
*For any* step in a parsed feature file, it should be correctly mapped to registered step definitions when available
**Validates: Requirements 10.3**

Property 43: Undefined step reporting
*For any* step without a matching definition, the framework should report the undefined step with implementation templates
**Validates: Requirements 10.4**

Property 44: Gherkin syntax error reporting
*For any* feature file with syntax errors, parsing should provide clear error messages with accurate line numbers
**Validates: Requirements 10.5**

Property 45: BDD API integration
*For any* step definition implementation, the full assertion and mocking API should be accessible and functional
**Validates: Requirements 11.1**

Property 46: BDD mock integration
*For any* BDD scenario using mocks, mock creation and verification should work correctly across step boundaries
**Validates: Requirements 11.2**

Property 47: BDD lifecycle hooks
*For any* BDD scenario with lifecycle requirements, scenario-level and feature-level hooks should execute at correct times
**Validates: Requirements 11.3**

Property 48: Mixed testing approach support
*For any* test suite containing BDD scenarios and other test types, all test types should execute correctly together
**Validates: Requirements 11.4**

Property 49: BDD reporting integration
*For any* BDD scenario execution, results should integrate with standard test reporting formats alongside other test types
**Validates: Requirements 11.5**

## Error Handling

The framework implements comprehensive error handling across all subsystems:

**Assertion Failures**: Failed assertions collect detailed information including expected vs actual values, stack traces, and contextual information. Multiple assertion failures in a chain are aggregated rather than short-circuiting.

**Mock Configuration Errors**: Invalid mock configurations (e.g., stubbing final methods, conflicting behaviors) are detected at configuration time with clear error messages.

**Test Discovery Errors**: Problems during test discovery (e.g., invalid annotations, inaccessible methods) are reported with specific guidance for resolution.

**Runtime Exceptions**: Unexpected exceptions during test execution are caught, wrapped with test context, and reported with full stack traces.

**Resource Management**: Proper cleanup of test resources, mock objects, and temporary state is ensured even when tests fail or are interrupted.

**BDD-Specific Error Handling**:

**Gherkin Parsing Errors**: Invalid Gherkin syntax is detected with precise line number reporting and suggestions for correction.

**Step Definition Errors**: Missing or ambiguous step definitions are reported with clear error messages and generated implementation templates.

**Scenario Execution Errors**: Failed BDD steps are reported with business-readable error messages that identify the specific step and provide context about the failure.

**Feature File Errors**: Problems loading or discovering feature files are reported with file path information and resolution guidance.

## Testing Strategy

The Meld Test framework uses a comprehensive testing approach combining unit tests, property-based tests, and BDD scenarios:

**Unit Testing Approach**:
- Unit tests verify specific examples and edge cases for each component
- Integration tests validate interactions between assertion engine, mock system, and test runner
- Regression tests ensure compatibility with existing Meld language features
- Performance tests validate framework overhead and execution speed

**Property-Based Testing Approach**:
- Property tests verify universal behaviors across all valid inputs using Hypothesis for Python components
- Each correctness property from the design document is implemented as a property-based test
- Property tests run a minimum of 100 iterations to ensure comprehensive coverage
- Custom generators create realistic test scenarios for complex framework interactions

**BDD Testing Approach**:
- BDD scenarios test the framework's behavior-driven development capabilities
- Gherkin parser tests validate feature file parsing with various syntax combinations
- Step definition tests ensure proper mapping between natural language and executable code
- Integration tests verify BDD scenarios work correctly with existing assertion and mocking systems

**Testing Framework Selection**:
- **Python components**: Hypothesis for property-based testing, pytest for unit testing
- **C++ components**: Catch2 for unit testing, RapidCheck for property-based testing
- **BDD components**: Custom Gherkin parser tests, step definition validation tests
- **Integration testing**: Custom test harness that exercises the complete framework including BDD scenarios

**Property-Based Test Requirements**:
- Each property-based test must be tagged with: **Feature: meld-test, Property {number}: {property_text}**
- Property tests must reference their corresponding correctness property from this design document
- All property tests must run at least 100 iterations for statistical confidence
- Custom generators must produce realistic test data that exercises framework edge cases

The testing strategy ensures concrete correctness (unit tests), general behavioral correctness (property tests), and business-readable scenario validation (BDD tests) across the entire framework.