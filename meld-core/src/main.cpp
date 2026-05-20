#include "meld/kernel/primitives.hpp"
#include "meld/kernel/operations.hpp"
#include "meld/kernel/symbol_table.hpp"
#include "meld/parser/parser.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <print>
#include <format>
#include <ranges>
#include <array>

using namespace meld;

// C++23: Use std::print instead of std::cout
void print_banner() {
    std::println("=== Meld Programming Language ===");
    std::println("Version 0.1.0 (C++23)");
    std::println("");
}

// C++23: Use std::print and std::format
void demo_kernel() {
    std::println("1. Kernel Primitives Demo");
    std::println("-------------------------");
    
    // Create symbols
    auto sym1 = kernel::SymbolTable::instance().intern("hello");
    auto sym2 = kernel::SymbolTable::instance().intern("hello");
    std::println("Symbol: {}", sym1->to_string());
    std::println("Interned (same ptr): {}", sym1 == sym2);
    
    // Create primitives
    auto num = std::make_shared<kernel::Integer>(42);
    auto str = std::make_shared<kernel::String>("world");
    auto bool_val = kernel::Boolean::true_value();
    std::println("Integer: {}", num->to_string());
    std::println("String: {}", str->to_string());
    std::println("Boolean: {}", bool_val->to_string());
    
    // C++23: Use ranges for list construction
    auto values = std::views::iota(1, 4) 
        | std::views::transform([](int i) { 
            return kernel::Value(std::make_shared<kernel::Integer>(i)); 
          });
    auto list = kernel::list(values);
    std::println("List: {}", list.to_string());
    
    // Create AST
    auto val_decl = kernel::make_val_decl("x", kernel::make_int_literal(42));
    std::println("AST (val x = 42): {}", val_decl.to_string());
    
    // Optional with std::expected
    auto some_val = kernel::Optional<kernel::Value>::some(
        kernel::Value(std::make_shared<kernel::Integer>(42))
    );
    auto none_val = kernel::Optional<kernel::Value>::none();
    std::println("Optional Some: {}", some_val->to_string());
    std::println("Optional None: {}", none_val->to_string());
    
    // C++23: Demonstrate std::expected
    auto result = some_val->get();
    if (result) {
        std::println("✓ Got value from Some");
    }
    
    auto none_result = none_val->get();
    if (!none_result) {
        std::println("✓ Cannot get value from None: {}", none_result.error());
    }
    
    std::println("");
}

// C++23: Use std::print and ranges
void demo_parser() {
    std::println("2. Parser Demo");
    std::println("--------------");
    
    parser::Parser p;
    parser::ast::expression result;
    
    // C++23: Use array with structured bindings
    constexpr std::array test_cases = {
        "42",
        "\"hello world\"",
        "true",
        "myVariable",
        "val x = 42",
        "var y = \"test\"",
        "add(1, 2)",
        "[1, 2, 3]"
    };
    
    // C++23: Use ranges
    for (const auto& test : test_cases) {
        if (p.parse_expression(test, result)) {
            std::println("✓ Parsed: {}", test);
        } else {
            std::println("✗ Failed: {}", test);
            std::println("  Error: {}", p.error_message());
        }
    }
    
    std::println("");
}

int main(int argc, char* argv[]) {
    print_banner();
    
    if (argc > 1) {
        // Parse file
        std::string filename = argv[1];
        std::ifstream file(filename);
        
        if (!file) {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return 1;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        parser::Parser p;
        std::vector<parser::ast::expression> expressions;
        
        if (p.parse_file(content, expressions)) {
            std::cout << "Successfully parsed " << expressions.size() << " expressions" << std::endl;
        } else {
            std::cerr << "Parse error: " << p.error_message() << std::endl;
            return 1;
        }
    } else {
        // Run demos
        demo_kernel();
        demo_parser();
        
        std::cout << "Usage: meld <filename.meld>" << std::endl;
        std::cout << "       Run without arguments to see demos" << std::endl;
    }
    
    return 0;
}
