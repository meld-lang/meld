#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <variant>
#include <optional>
#include "meld/meta/metatype.hpp"

namespace meld::types {

// Forward declarations
class AnonymousObjectType;
class AnonymousMapType;
class AnonymousSetType;
class AnonymousArrayType;
class AnonymousTupleType;

// Anonymous object field descriptor
struct AnonymousObjectField {
    std::string name;
    std::shared_ptr<meta::MetaType> type;
    
    AnonymousObjectField(const std::string& n, std::shared_ptr<meta::MetaType> t)
        : name(n), type(t) {}
};

// Anonymous object type - structural type with named fields of potentially different types
// Example: {status: Status, checked: Bool}
class AnonymousObjectType : public meta::MetaType {
public:
    AnonymousObjectType(const std::vector<AnonymousObjectField>& fields);
    
    std::string name() const override;
    size_t size() const override;
    bool is_value_type() const override { return false; }
    
    const std::vector<AnonymousObjectField>& fields() const { return fields_; }
    std::optional<std::shared_ptr<meta::MetaType>> getFieldType(const std::string& name) const;
    
    // Structural typing: check if this type is compatible with another
    bool isStructurallyCompatibleWith(const AnonymousObjectType& other) const;
    
private:
    std::vector<AnonymousObjectField> fields_;
    std::unordered_map<std::string, std::shared_ptr<meta::MetaType>> field_map_;
};

// Anonymous map type - Map<String, T> with homogeneous value types
// Example: {active: true, checked: true, enabled: false} -> Map<String, Bool>
class AnonymousMapType : public meta::MetaType {
public:
    AnonymousMapType(std::shared_ptr<meta::MetaType> value_type);
    
    std::string name() const override;
    size_t size() const override;
    bool is_value_type() const override { return false; }
    
    std::shared_ptr<meta::MetaType> valueType() const { return value_type_; }
    
private:
    std::shared_ptr<meta::MetaType> value_type_;
};

// Anonymous set type - Set<T> with homogeneous element types
// Example: {"No", "Yes", "Maybe"} -> Set<String>
class AnonymousSetType : public meta::MetaType {
public:
    AnonymousSetType(std::shared_ptr<meta::MetaType> element_type);
    
    std::string name() const override;
    size_t size() const override;
    bool is_value_type() const override { return false; }
    
    std::shared_ptr<meta::MetaType> elementType() const { return element_type_; }
    
private:
    std::shared_ptr<meta::MetaType> element_type_;
};

// Anonymous array type - Array<T> with homogeneous element types
// Example: [1, 2, 3, 4] -> Array<Int>
class AnonymousArrayType : public meta::MetaType {
public:
    AnonymousArrayType(std::shared_ptr<meta::MetaType> element_type);
    
    std::string name() const override;
    size_t size() const override;
    bool is_value_type() const override { return false; }
    
    std::shared_ptr<meta::MetaType> elementType() const { return element_type_; }
    
private:
    std::shared_ptr<meta::MetaType> element_type_;
};

// Tuple element descriptor for anonymous tuples
struct AnonymousTupleElement {
    std::string name;  // Empty for unnamed elements
    std::shared_ptr<meta::MetaType> type;
    size_t index;
    
    AnonymousTupleElement(size_t idx, std::shared_ptr<meta::MetaType> t, const std::string& n = "")
        : name(n), type(t), index(idx) {}
};

// Anonymous tuple type - Tuple with heterogeneous element types
// Example: [1, "hello", true] -> Tuple<Int, String, Bool>
// Example: [x: 10, y: 20] -> NamedTuple<x: Int, y: Int>
class AnonymousTupleType : public meta::MetaType {
public:
    AnonymousTupleType(const std::vector<AnonymousTupleElement>& elements);
    
    std::string name() const override;
    size_t size() const override;
    bool is_value_type() const override { return true; }  // Tuples are value types
    
    const std::vector<AnonymousTupleElement>& elements() const { return elements_; }
    std::optional<std::shared_ptr<meta::MetaType>> getElementType(size_t index) const;
    std::optional<std::shared_ptr<meta::MetaType>> getElementType(const std::string& name) const;
    
    bool hasNamedElements() const;
    size_t elementCount() const { return elements_.size(); }
    
private:
    std::vector<AnonymousTupleElement> elements_;
    std::unordered_map<std::string, size_t> name_to_index_;
};

// Runtime value types for anonymous collections

// Anonymous object value - holds field values
class AnonymousObjectValue {
public:
    AnonymousObjectValue(std::shared_ptr<AnonymousObjectType> type);
    
    void setField(const std::string& name, const std::variant<int, double, std::string, bool>& value);
    std::optional<std::variant<int, double, std::string, bool>> getField(const std::string& name) const;
    
    std::shared_ptr<AnonymousObjectType> type() const { return type_; }
    
private:
    std::shared_ptr<AnonymousObjectType> type_;
    std::unordered_map<std::string, std::variant<int, double, std::string, bool>> fields_;
};

// Anonymous map value - holds key-value pairs
class AnonymousMapValue {
public:
    AnonymousMapValue(std::shared_ptr<AnonymousMapType> type);
    
    void set(const std::string& key, const std::variant<int, double, std::string, bool>& value);
    std::optional<std::variant<int, double, std::string, bool>> get(const std::string& key) const;
    std::vector<std::string> keys() const;
    std::vector<std::variant<int, double, std::string, bool>> values() const;
    bool isEmpty() const { return entries_.empty(); }
    
    std::shared_ptr<AnonymousMapType> type() const { return type_; }
    
    // Static factory for empty map
    static std::shared_ptr<AnonymousMapValue> empty(std::shared_ptr<meta::MetaType> value_type);
    
private:
    std::shared_ptr<AnonymousMapType> type_;
    std::unordered_map<std::string, std::variant<int, double, std::string, bool>> entries_;
};

// Anonymous set value - holds unique elements
class AnonymousSetValue {
public:
    AnonymousSetValue(std::shared_ptr<AnonymousSetType> type);
    
    void add(const std::variant<int, double, std::string, bool>& element);
    bool contains(const std::variant<int, double, std::string, bool>& element) const;
    std::shared_ptr<AnonymousSetValue> remove(const std::variant<int, double, std::string, bool>& element) const;
    size_t size() const { return elements_.size(); }
    bool isEmpty() const { return elements_.empty(); }
    
    std::shared_ptr<AnonymousSetType> type() const { return type_; }
    
    // Static factory for empty set
    static std::shared_ptr<AnonymousSetValue> empty(std::shared_ptr<meta::MetaType> element_type);
    
private:
    std::shared_ptr<AnonymousSetType> type_;
    std::vector<std::variant<int, double, std::string, bool>> elements_;
};

// Anonymous array value - holds ordered elements
class AnonymousArrayValue {
public:
    AnonymousArrayValue(std::shared_ptr<AnonymousArrayType> type);
    
    void push(const std::variant<int, double, std::string, bool>& element);
    std::optional<std::variant<int, double, std::string, bool>> get(size_t index) const;
    void set(size_t index, const std::variant<int, double, std::string, bool>& element);
    std::optional<std::variant<int, double, std::string, bool>> pop();
    size_t size() const { return elements_.size(); }
    bool isEmpty() const { return elements_.empty(); }
    
    // Array operations
    std::shared_ptr<AnonymousArrayValue> slice(size_t start, size_t end) const;
    void insert(size_t index, const std::variant<int, double, std::string, bool>& element);
    void remove(size_t index);
    void clear();
    
    std::shared_ptr<AnonymousArrayType> type() const { return type_; }
    
    // Static factory for empty array
    static std::shared_ptr<AnonymousArrayValue> empty(std::shared_ptr<meta::MetaType> element_type);
    
private:
    std::shared_ptr<AnonymousArrayType> type_;
    std::vector<std::variant<int, double, std::string, bool>> elements_;
};

// Anonymous tuple value - holds heterogeneous elements
class AnonymousTupleValue {
public:
    AnonymousTupleValue(std::shared_ptr<AnonymousTupleType> type);
    
    // Access by index
    std::optional<std::variant<int, double, std::string, bool>> get(size_t index) const;
    void set(size_t index, const std::variant<int, double, std::string, bool>& value);
    
    // Access by name (for named tuples)
    std::optional<std::variant<int, double, std::string, bool>> get(const std::string& name) const;
    void set(const std::string& name, const std::variant<int, double, std::string, bool>& value);
    
    size_t size() const { return elements_.size(); }
    bool isEmpty() const { return elements_.empty(); }
    
    std::shared_ptr<AnonymousTupleType> type() const { return type_; }
    
    // Static factory methods
    static std::shared_ptr<AnonymousTupleValue> create(
        const std::vector<std::variant<int, double, std::string, bool>>& values,
        const std::vector<std::shared_ptr<meta::MetaType>>& types);
    
    static std::shared_ptr<AnonymousTupleValue> createNamed(
        const std::vector<std::pair<std::string, std::variant<int, double, std::string, bool>>>& named_values,
        const std::vector<std::shared_ptr<meta::MetaType>>& types);
    
private:
    std::shared_ptr<AnonymousTupleType> type_;
    std::vector<std::variant<int, double, std::string, bool>> elements_;
};

} // namespace meld::types
