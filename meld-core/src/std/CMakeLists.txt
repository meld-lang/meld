# CMakeLists.txt for Meld Standard Library - Ownership System
# Task 2.1: Owned<T> and Borrowed<T> wrapper types implementation

cmake_minimum_required(VERSION 3.20)

# Add ownership system sources to the standard library
target_sources(meld_std_lib PRIVATE
    ownership.cpp
    ownership_intrinsics.cpp
)

# Add ownership system headers
target_include_directories(meld_std_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Link with required dependencies
target_link_libraries(meld_std_lib PRIVATE
    meld_kernel
    meld_compiler
    meld_provenance
)

# Compiler-specific settings for ownership system
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(meld_std_lib PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -fno-rtti  # Ownership system uses templates, not RTTI
    )
endif()

# Enable C++23 features for ownership system
target_compile_features(meld_std_lib PUBLIC cxx_std_23)

# Add ownership system tests
if(BUILD_TESTING)
    add_executable(ownership_demo
        ${CMAKE_CURRENT_SOURCE_DIR}/../../examples/ownership-demo.cpp
    )
    
    target_link_libraries(ownership_demo PRIVATE
        meld_std_lib
        meld_compiler
        meld_kernel
    )
    
    # Add the ownership demo as a test
    add_test(NAME ownership_demo_test COMMAND ownership_demo)
    
    # Add property test for ownership tracking
    find_program(PYTHON3_EXECUTABLE python3)
    if(PYTHON3_EXECUTABLE)
        add_test(
            NAME ownership_tracking_property_test
            COMMAND ${PYTHON3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/test_ownership_tracking_property.py
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        
        # Set test properties
        set_tests_properties(ownership_tracking_property_test PROPERTIES
            TIMEOUT 300  # 5 minute timeout for property test
            LABELS "property_test;ownership;rust_inspired"
        )
    endif()
endif()

# Install ownership system headers
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/meld/std/ownership.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/meld/std/ownership_intrinsics.hpp
    DESTINATION include/meld/std
)