#!/usr/bin/env python3
"""
Final validation script for meld-monorepo task 11.
Runs comprehensive validation tests for the complete workspace.
"""

import subprocess
import sys
import os
from pathlib import Path

def run_test_script(script_name, description):
    """Run a test script and return the result."""
    print(f"\n> {description}...")
    print("=" * 60)
    
    try:
        result = subprocess.run(
            [sys.executable, f"tools/{script_name}"],
            capture_output=True,
            text=True,
            cwd=Path.cwd()
        )
        
        if result.returncode == 0:
            print(f"+ {description}: PASSED")
            return True
        else:
            print(f"- {description}: FAILED")
            if result.stdout:
                print("STDOUT:", result.stdout[-500:])  # Last 500 chars
            if result.stderr:
                print("STDERR:", result.stderr[-500:])  # Last 500 chars
            return False
            
    except Exception as e:
        print(f"- {description}: ERROR - {e}")
        return False

def main():
    """Run all final validation tests."""
    print("Starting Final Validation for Meld Mono-repo")
    print("=" * 60)
    
    # Test configurations
    tests = [
        ("validate_workspace.py", "Workspace Configuration Validation"),
        ("test_workspace_properties.py", "Workspace Structure Properties"),
        ("test_functionality_preservation.py", "Functionality Preservation"),
        ("test_build_system_functionality.py", "Build System Functionality"),
        ("test_dependency_management.py", "Dependency Management"),
        ("test_incremental_build_efficiency.py", "Incremental Build Efficiency"),
        ("test_placeholder_package_viability.py", "Placeholder Package Viability"),
        ("test_documentation_preservation.py", "Documentation Preservation"),
        ("validate_dependencies.py", "Inter-package Dependencies"),
    ]
    
    results = []
    
    # Run all tests
    for script, description in tests:
        if os.path.exists(f"tools/{script}"):
            success = run_test_script(script, description)
            results.append((description, success))
        else:
            print(f"! {description}: SKIPPED (script not found)")
            results.append((description, None))
    
    # Summary
    print("\n" + "=" * 60)
    print("FINAL VALIDATION SUMMARY")
    print("=" * 60)
    
    passed = sum(1 for _, result in results if result is True)
    failed = sum(1 for _, result in results if result is False)
    skipped = sum(1 for _, result in results if result is None)
    
    for description, result in results:
        if result is True:
            print(f"+ {description}")
        elif result is False:
            print(f"- {description}")
        else:
            print(f"! {description} (SKIPPED)")
    
    print(f"\nResults: {passed} passed, {failed} failed, {skipped} skipped")
    
    if failed == 0:
        print("\nALL VALIDATIONS PASSED! Mono-repo transformation is complete.")
        return 0
    else:
        print(f"\n{failed} validation(s) failed. Review the issues above.")
        return 1

if __name__ == "__main__":
    sys.exit(main())