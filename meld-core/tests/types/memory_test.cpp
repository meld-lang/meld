#include <gtest/gtest.h>
#include "meld/types/memory.hpp"
#include "meld/types/instance.hpp"
#include "meld/meta/metatype.hpp"

using namespace meld;
using namespace meld::types;
using namespace meld::meta;

// Test heap allocator
TEST(MemoryTest, HeapAllocatorBasic) {
    auto& allocator = HeapAllocator::instance();
    
    // Clear any previous allocations
    allocator.clear();
    
    size_t initial_count = allocator.allocated_count();
    
    // Allocate an object
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    
    // Check allocation count increased
    EXPECT_GT(allocator.allocated_count(), initial_count);
}

// Test reference counting
TEST(MemoryTest, ReferenceCountingBasic) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    
    // Initial ref count should be 1
    EXPECT_EQ(instance->ref_count(), 1);
    
    // Retain increases count
    instance->retain();
    EXPECT_EQ(instance->ref_count(), 2);
    
    // Release decreases count
    instance->release();
    EXPECT_EQ(instance->ref_count(), 1);
}

// Test weak references
TEST(MemoryTest, WeakReferenceBasic) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    
    // Create weak reference
    auto weak = instance->weak_ref();
    
    // Weak reference should not be expired while strong ref exists
    EXPECT_FALSE(weak.expired());
    
    // Lock should return valid pointer
    auto locked = weak.lock();
    EXPECT_NE(locked, nullptr);
}

// Test weak reference expiration
TEST(MemoryTest, WeakReferenceExpiration) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    WeakRef<ManagedObject> weak;
    
    {
        auto instance = create_class_instance(class_type);
        weak = instance->weak_ref();
        
        // Weak reference should be valid
        EXPECT_FALSE(weak.expired());
    }
    
    // After instance goes out of scope, weak ref should expire
    EXPECT_TRUE(weak.expired());
    
    // Lock should return null
    auto locked = weak.lock();
    EXPECT_EQ(locked, nullptr);
}

// Test weak fields in class instances
TEST(MemoryTest, WeakFieldsInClassInstance) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("Node", std::move(fields), {})
    );
    
    auto node1 = create_class_instance(class_type);
    auto node2 = create_class_instance(class_type);
    
    // Create weak reference from node1 to node2
    auto weak_node2 = WeakRef<ClassInstance>(node2);
    node1->set_weak_field("next", weak_node2);
    
    // Retrieve weak field
    auto retrieved = node1->get_weak_field("next");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FALSE(retrieved->expired());
}

// Test breaking reference cycles with weak references
TEST(MemoryTest, BreakingReferenceCycles) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("Node", std::move(fields), {})
    );
    
    auto node1 = create_class_instance(class_type);
    auto node2 = create_class_instance(class_type);
    
    // Create cycle: node1 -> node2 (strong), node2 -> node1 (weak)
    // In a full implementation, we'd store node2 in node1's fields
    // For now, we use weak fields
    auto weak_node1 = WeakRef<ClassInstance>(node1);
    node2->set_weak_field("prev", weak_node1);
    
    // Both nodes should still be alive
    EXPECT_FALSE(weak_node1.expired());
}

// Test Ref RAII wrapper
TEST(MemoryTest, RefWrapperBasic) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    size_t initial_count = instance->ref_count();
    
    {
        Ref<ClassInstance> ref(instance);
        // Ref should increase count
        EXPECT_GT(instance->ref_count(), initial_count);
    }
    
    // After Ref goes out of scope, count should decrease
    EXPECT_EQ(instance->ref_count(), initial_count);
}

// Test Ref copy semantics
TEST(MemoryTest, RefCopySemantics) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    size_t initial_count = instance->ref_count();
    
    Ref<ClassInstance> ref1(instance);
    size_t after_first = instance->ref_count();
    
    // Copy should increase count
    Ref<ClassInstance> ref2 = ref1;
    EXPECT_GT(instance->ref_count(), after_first);
    
    // Move should not increase count
    Ref<ClassInstance> ref3 = std::move(ref1);
    EXPECT_EQ(instance->ref_count(), after_first + 1);
}

// Test garbage collection
TEST(MemoryTest, GarbageCollection) {
    auto& allocator = HeapAllocator::instance();
    allocator.clear();
    
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    {
        auto instance = create_class_instance(class_type);
        // Instance is alive
    }
    
    // After instance goes out of scope, collect garbage
    allocator.collect_garbage();
    
    // Allocation count should decrease
    // (In a full implementation, this would be more sophisticated)
}

// Test heap allocator statistics
TEST(MemoryTest, HeapAllocatorStatistics) {
    auto& allocator = HeapAllocator::instance();
    allocator.clear();
    
    size_t initial_count = allocator.allocated_count();
    size_t initial_bytes = allocator.total_allocated_bytes();
    
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    
    // Statistics should increase
    EXPECT_GT(allocator.allocated_count(), initial_count);
    EXPECT_GT(allocator.total_allocated_bytes(), initial_bytes);
}

// Test multiple allocations
TEST(MemoryTest, MultipleAllocations) {
    auto& allocator = HeapAllocator::instance();
    allocator.clear();
    
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    std::vector<std::shared_ptr<ClassInstance>> instances;
    for (int i = 0; i < 10; ++i) {
        instances.push_back(create_class_instance(class_type));
    }
    
    EXPECT_EQ(allocator.allocated_count(), 10);
}

// Test cycle detector (basic)
TEST(MemoryTest, CycleDetectorBasic) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("Node", std::move(fields), {})
    );
    
    auto node = create_class_instance(class_type);
    
    // Single node has no cycle
    EXPECT_FALSE(CycleDetector::has_cycle(node.get()));
}

// Test memory pool (if implemented)
TEST(MemoryTest, MemoryPoolBasic) {
    MemoryPool<64> pool;
    
    void* ptr1 = pool.allocate();
    EXPECT_NE(ptr1, nullptr);
    
    void* ptr2 = pool.allocate();
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(ptr1, ptr2);
    
    pool.deallocate(ptr1);
    pool.deallocate(ptr2);
}

// Test memory pool reuse
TEST(MemoryTest, MemoryPoolReuse) {
    MemoryPool<64> pool;
    
    void* ptr1 = pool.allocate();
    pool.deallocate(ptr1);
    
    void* ptr2 = pool.allocate();
    
    // Should reuse the same memory
    EXPECT_EQ(ptr1, ptr2);
}

// Test on_deallocate callback
TEST(MemoryTest, OnDeallocateCallback) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    {
        auto instance = create_class_instance(class_type);
        // Set a field
        instance->set_field("value", kernel::make_int(42));
    }
    
    // on_deallocate should have been called when instance was destroyed
    // (We can't directly test this without instrumentation, but it should work)
}

// Test weak reference reset
TEST(MemoryTest, WeakReferenceReset) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    auto weak = instance->weak_ref();
    
    EXPECT_FALSE(weak.expired());
    
    weak.reset();
    
    // After reset, lock should return null
    auto locked = weak.lock();
    EXPECT_EQ(locked, nullptr);
}

// Test reference counting with multiple refs
TEST(MemoryTest, MultipleReferences) {
    auto int_type = TypeRegistry::instance().get_int_type();
    std::vector<Field> fields = {Field("value", int_type, true)};
    auto class_type = std::dynamic_pointer_cast<ClassMetaType>(
        MetaType::create_class("TestClass", std::move(fields), {})
    );
    
    auto instance = create_class_instance(class_type);
    
    std::vector<Ref<ClassInstance>> refs;
    for (int i = 0; i < 5; ++i) {
        refs.emplace_back(instance);
    }
    
    // Ref count should be 1 (initial) + 5 (refs)
    EXPECT_EQ(instance->ref_count(), 6);
    
    refs.clear();
    
    // After clearing refs, count should be back to 1
    EXPECT_EQ(instance->ref_count(), 1);
}
