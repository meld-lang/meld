#include <gtest/gtest.h>
#include "../../include/meld/parser/parser.hpp"
#include "../../include/meld/compiler/compiler.hpp"
#include "../../include/meld/runtime/interpreter.hpp"
#include <sstream>
#include <vector>

namespace meld::stdlib::test {

// ============================================================================
// SYSTEM.OUT ANNOTATION-BASED TESTS
// Task 8.3: Test System.out with annotation-based effects
// Requirements: 7.1, 7.2, 7.3, 7.4, 7.5
// ============================================================================

class SystemOutAnnotationTest : public ::testing::Test {
protected:
    void SetUp() override {
        captured_output_.clear();
    }
    
    std::string runMeldCode(const std::string& code) {
        // Parse the code
        parser::Parser parser;
        auto ast = parser.parse(code);
        
        // Compile and run
        compiler::Compiler compiler;
        auto bytecode = compiler.compile(ast);
        
        runtime::Interpreter interpreter;
        interpreter.execute(bytecode);
        
        return getCapturedOutput();
    }
    
    std::string getCapturedOutput() {
        std::string result;
        for (const auto& line : captured_output_) {
            result += line;
        }
        captured_output_.clear();
        return result;
    }
    
    static std::vector<std::string> captured_output_;
};

std::vector<std::string> SystemOutAnnotationTest::captured_output_;

// ============================================================================
// TEST 1: System.out.println with handler
// Requirements: 7.1, 7.2, 7.3, 7.4
// ============================================================================

TEST_F(SystemOutAnnotationTest, PrintlnWithHandler) {
    std::string code = R"(
        // Import Console effect
        import Console from stdlib
        import System.out from stdlib
        
        // Test function that uses System.out.println
        @imposes(Console)
        fnc greet(name: string) {
            System.out.println(`Hello, ${name}!`)
        }
        
        // Main test
        fnc main() {
            val output = []
            
            // Handle Console effect to capture output
            handle(
                computation: { greet("Alice") }
            ) {
                Console {
                    fnc println(message: string) {
                        output.add(message)
                        resume()
                    }
                }
            }
            
            // Verify output was captured
            assert(output.length == 1)
            assert(output[0] == "Hello, Alice!")
        }
    )";
    
    EXPECT_NO_THROW(runMeldCode(code));
}

// ============================================================================
// TEST 2: System.out.print with handler
// Requirements: 7.1, 7.2, 7.3, 7.5
// ============================================================================

TEST_F(SystemOutAnnotationTest, PrintWithHandler) {
    std::string code = R"(
        // Import Console effect
        import Console from stdlib
        import System.out from stdlib
        
        // Test function that uses System.out.print
        @imposes(Console)
        fnc printParts(part1: string, part2: string) {
            System.out.print(part1)
            System.out.print(part2)
        }
        
        // Main test
        fnc main() {
            val output = []
            
            // Handle Console effect to capture output
            handle(
                computation: { printParts("Hello, ", "World!") }
            ) {
                Console {
                    fnc print(message: string) {
                        output.add(message)
                        resume()
                    }
                }
            }
            
            // Verify output was captured
            assert(output.length == 2)
            assert(output[0] == "Hello, ")
            assert(output[1] == "World!")
        }
    )";
    
    EXPECT_NO_THROW(runMeldCode(code));
}

// ============================================================================
// TEST 3: Mixed print and println with handler
// Requirements: 7.1, 7.2, 7.3, 7.4, 7.5
// ============================================================================

TEST_F(SystemOutAnnotationTest, MixedPrintAndPrintlnWithHandler) {
    std::string code = R"(
        // Import Console effect
        import Console from stdlib
        import System.out from stdlib
        
        // Test function that uses both print and println
        @imposes(Console)
        fnc displayMessage() {
            System.out.print("Start: ")
            System.out.println("First line")
            System.out.print("End")
        }
        
        // Main test
        fnc main() {
            val output = []
            
            // Handle Console effect to capture output
            handle(
                computation: { displayMessage() }
            ) {
                Console {
                    fnc print(message: string) {
                        output.add(`print: ${message}`)
                        resume()
                    }
                    fnc println(message: string) {
                        output.add(`println: ${message}`)
                        resume()
                    }
                }
            }
            
            // Verify output was captured correctly
            assert(output.length == 3)
            assert(output[0] == "print: Start: ")
            assert(output[1] == "println: First line")
            assert(output[2] == "print: End")
        }
    )";
    
    EXPECT_NO_THROW(runMeldCode(code));
}

// ============================================================================
// TEST 4: Verify @imposes annotation is required
// Requirements: 7.1, 7.2, 7.3
// ============================================================================

TEST_F(SystemOutAnnotationTest, ImposesAnnotationRequired) {
    std::string code = R"(
        // Import Console effect
        import Console from stdlib
        import System.out from stdlib
        
        // Function without @imposes annotation should fail compilation
        fnc greetWithoutAnnotation(name: string) {
            System.out.println(`Hello, ${name}!`)
        }
    )";
    
    // This should fail at compile time because the function
    // performs Console effect but doesn't declare it
    EXPECT_THROW(runMeldCode(code), std::runtime_error);
}

// ============================================================================
// TEST 5: Verify perform syntax is correct
// Requirements: 7.2, 7.3
// ============================================================================

TEST_F(SystemOutAnnotationTest, PerformSyntaxCorrect) {
    std::string code = R"(
        // Import Console effect
        import Console from stdlib
        
        // Test that System.out uses perform { Console.println(...) } syntax
        @imposes(Console)
        fnc testPerformSyntax() {
            val output = []
            
            // This should work because System.out.println uses
            // perform { Console.println(message) } internally
            handle(
                computation: { 
                    // Direct call to System.out.println
                    System.out.println("Test message")
                }
            ) {
                Console {
                    fnc println(message: string) {
                        output.add(message)
                        resume()
                    }
                }
            }
            
            assert(output.length == 1)
            assert(output[0] == "Test message")
        }
        
        fnc main() {
            testPerformSyntax()
        }
    )";
    
    EXPECT_NO_THROW(runMeldCode(code));
}

// ============================================================================
// TEST 6: Verify output capture works correctly
// Requirements: 7.4, 7.5
// ============================================================================

TEST_F(SystemOutAnnotationTest, OutputCaptureWorks) {
    std::string code = R"(
        // Import Console effect
        import Console from stdlib
        import System.out from stdlib
        
        // Test function that produces multiple outputs
        @imposes(Console)
        fnc multipleOutputs() {
            System.out.println("Line 1")
            System.out.println("Line 2")
            System.out.print("Part 1 ")
            System.out.print("Part 2")
            System.out.println("")
        }
        
        // Main test
        fnc main() {
            val captured = []
            
            // Handle Console effect to capture all output
            handle(
                computation: { multipleOutputs() }
            ) {
                Console {
                    fnc println(message: string) {
                        captured.add(`[println] ${message}`)
                        resume()
                    }
                    fnc print(message: string) {
                        captured.add(`[print] ${message}`)
                        resume()
                    }
                }
            }
            
            // Verify all output was captured in order
            assert(captured.length == 5)
            assert(captured[0] == "[println] Line 1")
            assert(captured[1] == "[println] Line 2")
            assert(captured[2] == "[print] Part 1 ")
            assert(captured[3] == "[print] Part 2")
            assert(captured[4] == "[println] ")
        }
    )";
    
    EXPECT_NO_THROW(runMeldCode(code));
}

// ============================================================================
// TEST 7: Verify nested handlers work correctly
// Requirements: 7.4, 7.5
// ============================================================================

TEST_F(SystemOutAnnotationTest, NestedHandlers) {
    std::string code = R"(
        // Import Console effect
        import Console from stdlib
        import System.out from stdlib
        
        // Test nested handler behavior
        @imposes(Console)
        fnc innerFunction() {
            System.out.println("Inner message")
        }
        
        @imposes(Console)
        fnc outerFunction() {
            System.out.println("Outer message")
            
            // Inner handler should take precedence
            handle(
                computation: { innerFunction() }
            ) {
                Console {
                    fnc println(message: string) {
                        System.out.println(`[Inner] ${message}`)
                        resume()
                    }
                }
            }
        }
        
        // Main test
        fnc main() {
            val captured = []
            
            // Outer handler
            handle(
                computation: { outerFunction() }
            ) {
                Console {
                    fnc println(message: string) {
                        captured.add(`[Outer] ${message}`)
                        resume()
                    }
                }
            }
            
            // Verify nested handling worked correctly
            assert(captured.length == 2)
            assert(captured[0] == "[Outer] Outer message")
            assert(captured[1] == "[Outer] [Inner] Inner message")
        }
    )";
    
    EXPECT_NO_THROW(runMeldCode(code));
}

} // namespace meld::stdlib::test
