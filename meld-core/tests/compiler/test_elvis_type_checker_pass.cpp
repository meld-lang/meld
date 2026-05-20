/// @file test_elvis_type_checker_pass.cpp
/// @brief Unit tests for the Elvis Operator Type Checker Pass
///
/// Tests that the pass correctly:
///   - Detects `?:` on optional values (no error)
///   - Detects `?:` on nullable types (no error)
///   - Rejects `?:` on Result[T, E] types (E4708)
///   - Rejects `opr ?:` definitions (E4702)
///   - Allows `?:` on plain non-optional types (type inference resolves later)
///   - Handles multiple `?:` in same program
///   - Handles `?:` inside function bodies
///   - Propagates source file in diagnostics
///   - Verifies type helper static methods
///   - Verifies forbidden symbols set contains only "?: