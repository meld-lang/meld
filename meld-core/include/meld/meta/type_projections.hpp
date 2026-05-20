#pragma once

#include "metatype.hpp"
#include <set>
#include <algorithm>

namespace meld::meta {

// Type projection utilities for manipulating structural types
// These utilities work on StructMetaType and ClassMetaType

// Omit<T, K> - Remove specified properties from a type
// Returns a new type with all properties except those named in keys
class OmitProjection {
public:
    static std::expected<std::shared_ptr<MetaType>, std::string>
    apply(std::shared_ptr<MetaType> source_type, const std::set<std::string>& keys_to_omit);
    
private:
    static std::expected<std::shared_ptr<StructMetaType>, std::string>
    apply_to_struct(std::shared_ptr<StructMetaType> source, const std::set<std::string>& keys);
    
    static std::expected<std::shared_ptr<ClassMetaType>, std::string>
    apply_to_class(std::shared_ptr<ClassMetaType> source, const std::set<std::string>& keys);
};

// Pick<T, K> - Select only specified properties from a type
// Returns a new type with only the properties named in keys
class PickProjection {
public:
    static std::expected<std::shared_ptr<MetaType>, std::string>
    apply(std::shared_ptr<MetaType> source_type, const std::set<std::string>& keys_to_pick);
    
private:
    static std::expected<std::shared_ptr<StructMetaType>, std::string>
    apply_to_struct(std::shared_ptr<StructMetaType> source, const std::set<std::string>& keys);
    
    static std::expected<std::shared_ptr<ClassMetaType>, std::string>
    apply_to_class(std::shared_ptr<ClassMetaType> source, const std::set<std::string>& keys);
};

// Partial<T> - Make all properties optional (nullable)
// Returns a new type where all properties are wrapped in optional (T | Null)
class PartialProjection {
public:
    static std::expected<std::shared_ptr<MetaType>, std::string>
    apply(std::shared_ptr<MetaType> source_type);
    
private:
    static std::expected<std::shared_ptr<StructMetaType>, std::string>
    apply_to_struct(std::shared_ptr<StructMetaType> source);
    
    static std::expected<std::shared_ptr<ClassMetaType>, std::string>
    apply_to_class(std::shared_ptr<ClassMetaType> source);
};

// Required<T> - Make all properties required (non-nullable)
// Returns a new type where all optional properties become required
class RequiredProjection {
public:
    static std::expected<std::shared_ptr<MetaType>, std::string>
    apply(std::shared_ptr<MetaType> source_type);
    
private:
    static std::expected<std::shared_ptr<StructMetaType>, std::string>
    apply_to_struct(std::shared_ptr<StructMetaType> source);
    
    static std::expected<std::shared_ptr<ClassMetaType>, std::string>
    apply_to_class(std::shared_ptr<ClassMetaType> source);
};

// Readonly<T> - Make all properties immutable
// Returns a new type where all mutable properties become immutable
class ReadonlyProjection {
public:
    static std::expected<std::shared_ptr<MetaType>, std::string>
    apply(std::shared_ptr<MetaType> source_type);
    
private:
    static std::expected<std::shared_ptr<StructMetaType>, std::string>
    apply_to_struct(std::shared_ptr<StructMetaType> source);
    
    static std::expected<std::shared_ptr<ClassMetaType>, std::string>
    apply_to_class(std::shared_ptr<ClassMetaType> source);
};

// Helper functions for type projection operations
namespace projection_helpers {
    // Check if a type is nullable (union with Null)
    bool is_nullable(const MetaType& type);
    
    // Get the inner type from a nullable type
    std::expected<std::shared_ptr<MetaType>, std::string>
    get_inner_type(std::shared_ptr<MetaType> nullable_type);
    
    // Make a type nullable (wrap in union with Null)
    std::shared_ptr<MetaType> make_nullable(std::shared_ptr<MetaType> type);
    
    // Make a type non-nullable (unwrap from union with Null)
    std::shared_ptr<MetaType> make_required(std::shared_ptr<MetaType> type);
}

} // namespace meld::meta
