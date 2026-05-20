#!/usr/bin/env python3
"""
Basic functionality test for the Meld CLI core framework.
Tests the core components without requiring a full C++ build.
"""

def test_cli_core_structure():
    """Test that the CLI core structure is properly set up"""
    import os
    
    # Check that all required files exist
    required_files = [
        "BUILD.bazel",
        "README.md",
        "src/main.cpp",
        "src/cli_core.cpp",
        "src/command_dispatcher.cpp",
        "src/config_manager.cpp",
        "src/error_handler.cpp",
        "include/meld/cli/cli_core.hpp",
        "include/meld/cli/command_dispatcher.hpp",
        "include/meld/cli/command_handler.hpp",
        "include/meld/cli/config_manager.hpp",
        "include/meld/cli/error_handler.hpp",
        "tests/command_dispatcher_property_test.cpp",
        "tests/config_manager_property_test.cpp",
        "tests/config_hierarchy_property_test.cpp"
    ]
    
    missing_files = []
    for file_path in required_files:
        if not os.path.exists(file_path):
            missing_files.append(file_path)
    
    if missing_files:
        print(f"Missing files: {missing_files}")
        return False
    
    print("✓ All required CLI framework files exist")
    return True

def test_header_includes():
    """Test that header files have proper includes and structure"""
    import re
    
    headers = [
        "include/meld/cli/error_handler.hpp",
        "include/meld/cli/command_handler.hpp",
        "include/meld/cli/command_dispatcher.hpp",
        "include/meld/cli/config_manager.hpp",
        "include/meld/cli/cli_core.hpp"
    ]
    
    for header in headers:
        with open(header, 'r') as f:
            content = f.read()
            
        # Check for include guards
        if not content.startswith('#pragma once'):
            print(f"✗ {header} missing include guard")
            return False
            
        # Check for namespace
        if 'namespace meld::cli' not in content:
            print(f"✗ {header} missing proper namespace")
            return False
    
    print("✓ All header files have proper structure")
    return True

def test_source_file_structure():
    """Test that source files have proper includes and structure"""
    sources = [
        "src/main.cpp",
        "src/cli_core.cpp",
        "src/command_dispatcher.cpp",
        "src/config_manager.cpp",
        "src/error_handler.cpp"
    ]
    
    for source in sources:
        with open(source, 'r') as f:
            content = f.read()
            
        # Check for proper includes
        if '#include' not in content:
            print(f"✗ {source} missing includes")
            return False
            
        # Check for namespace usage
        if 'meld::cli' not in content:
            print(f"✗ {source} missing namespace usage")
            return False
    
    print("✓ All source files have proper structure")
    return True

def test_build_configuration():
    """Test that BUILD.bazel is properly configured"""
    with open("BUILD.bazel", 'r') as f:
        content = f.read()
    
    # Check for main binary target
    if 'cc_binary' not in content or 'name = "meld"' not in content:
        print("✗ BUILD.bazel missing main binary target")
        return False
    
    # Check for library targets
    required_targets = ['cli_core', 'command_dispatcher', 'config_manager', 'error_handler']
    for target in required_targets:
        if f'name = "{target}"' not in content:
            print(f"✗ BUILD.bazel missing {target} target")
            return False
    
    # Check for test targets
    test_targets = ['command_dispatcher_property_test', 'config_manager_property_test', 'config_hierarchy_property_test']
    for target in test_targets:
        if f'name = "{target}"' not in content:
            print(f"✗ BUILD.bazel missing {target} target")
            return False
    
    print("✓ BUILD.bazel properly configured")
    return True

def main():
    """Run all basic tests"""
    print("Running Meld CLI Basic Structure Tests")
    print("=" * 50)
    
    tests = [
        test_cli_core_structure,
        test_header_includes,
        test_source_file_structure,
        test_build_configuration
    ]
    
    passed = 0
    failed = 0
    
    for test in tests:
        try:
            if test():
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"✗ {test.__name__} failed: {e}")
            failed += 1
    
    print("=" * 50)
    print(f"Results: {passed} passed, {failed} failed")
    
    if failed == 0:
        print("All basic structure tests passed!")
        return 0
    else:
        print("Some tests failed.")
        return 1

if __name__ == "__main__":
    import sys
    sys.exit(main())