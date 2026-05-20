#include "meld/types/anonymous_types.hpp"
#include <sstream>
#include <algorithm>

namespace meld::types {

// AnonymousObjectType implementation

AnonymousObjectType::AnonymousObjectType(const std::vector<AnonymousObjectField>& fields)
    : fields_(fields) {
    // Build field map for quick lookup
    for (const auto& field : fields_) {
        field_map_[field.name] = field.type;
    }
}

std::string AnonymousObjectType::name() const {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < fields_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << fields_[i].name << ": " << fields_[i].type->name();
    }
    oss << "}";
    return oss.str();
}

size_t AnonymousObjectType::size() const {
    // Sum of all field sizes
    size_t total = 0;
    for (const auto& field : fields_) {
        total += field.type->size();
    }
    return total;
}

std::optional<std::shared_ptr<meta::MetaType>> AnonymousObjectType::getFieldType(const std::string& name) const {
    auto it = field_map_.find(name);
    if (it != field_map_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool AnonymousObjectType::isStructurallyCompatibleWith(const AnonymousObjectType& other) const {
    // Check if all fields in 'other' exist in 'this' with compatible types
    for (const auto& other_field : other.fields_) {
        auto this_field_type = getFieldType(other_field.name);
        if (!this_field_type) {
            return false;  // Field doesn't exist
        }
        // For now, require exact type match
        // TODO: Implement proper type compatibility checking
        if (this_field_type.value()->name() != other_field.type->name()) {
            return false;
        }
    }
    return true;
}

// AnonymousMapType implementation

AnonymousMapType::AnonymousMapType(std::shared_ptr<meta::MetaType> value_type)
    : value_type_(value_type) {}

std::string AnonymousMapType::name() const {
    return "Map<String, " + value_type_->name() + ">";
}

size_t AnonymousMapType::size() const {
    return sizeof(std::unordered_map<std::string, void*>);
}

// AnonymousSetType implementation

AnonymousSetType::AnonymousSetType(std::shared_ptr<meta::MetaType> element_type)
    : element_type_(element_type) {}

std::string AnonymousSetType::name() const {
    return "Set<" + element_type_->name() + ">";
}

size_t AnonymousSetType::size() const {
    return sizeof(std::vector<void*>);
}

// AnonymousArrayType implementation

AnonymousArrayType::AnonymousArrayType(std::shared_ptr<meta::MetaType> element_type)
    : element_type_(element_type) {}

std::string AnonymousArrayType::name() const {
    return "Array<" + element_type_->name() + ">";
}

size_t AnonymousArrayType::size() const {
    return sizeof(std::vector<void*>);
}

// AnonymousObjectValue implementation

AnonymousObjectValue::AnonymousObjectValue(std::shared_ptr<AnonymousObjectType> type)
    : type_(type) {}

void AnonymousObjectValue::setField(const std::string& name, const std::variant<int, double, std::string, bool>& value) {
    // Verify field exists in type
    if (type_->getFieldType(name)) {
        fields_[name] = value;
    }
}

std::optional<std::variant<int, double, std::string, bool>> AnonymousObjectValue::getField(const std::string& name) const {
    auto it = fields_.find(name);
    if (it != fields_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// AnonymousMapValue implementation

AnonymousMapValue::AnonymousMapValue(std::shared_ptr<AnonymousMapType> type)
    : type_(type) {}

std::shared_ptr<AnonymousMapValue> AnonymousMapValue::empty(std::shared_ptr<meta::MetaType> value_type) {
    auto mapType = std::make_shared<AnonymousMapType>(value_type);
    return std::make_shared<AnonymousMapValue>(mapType);
}

void AnonymousMapValue::set(const std::string& key, const std::variant<int, double, std::string, bool>& value) {
    entries_[key] = value;
}

std::optional<std::variant<int, double, std::string, bool>> AnonymousMapValue::get(const std::string& key) const {
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> AnonymousMapValue::keys() const {
    std::vector<std::string> result;
    for (const auto& [key, _] : entries_) {
        result.push_back(key);
    }
    return result;
}

std::vector<std::variant<int, double, std::string, bool>> AnonymousMapValue::values() const {
    std::vector<std::variant<int, double, std::string, bool>> result;
    for (const auto& [_, value] : entries_) {
        result.push_back(value);
    }
    return result;
}

// AnonymousSetValue implementation

AnonymousSetValue::AnonymousSetValue(std::shared_ptr<AnonymousSetType> type)
    : type_(type) {}

std::shared_ptr<AnonymousSetValue> AnonymousSetValue::empty(std::shared_ptr<meta::MetaType> element_type) {
    auto setType = std::make_shared<AnonymousSetType>(element_type);
    return std::make_shared<AnonymousSetValue>(setType);
}

void AnonymousSetValue::add(const std::variant<int, double, std::string, bool>& element) {
    // Check if element already exists
    if (!contains(element)) {
        elements_.push_back(element);
    }
}

bool AnonymousSetValue::contains(const std::variant<int, double, std::string, bool>& element) const {
    return std::find(elements_.begin(), elements_.end(), element) != elements_.end();
}

std::shared_ptr<AnonymousSetValue> AnonymousSetValue::remove(const std::variant<int, double, std::string, bool>& element) const {
    auto new_set = std::make_shared<AnonymousSetValue>(type_);
    for (const auto& elem : elements_) {
        if (elem != element) {
            new_set->add(elem);
        }
    }
    return new_set;
}

// AnonymousArrayValue implementation

AnonymousArrayValue::AnonymousArrayValue(std::shared_ptr<AnonymousArrayType> type)
    : type_(type) {}

std::shared_ptr<AnonymousArrayValue> AnonymousArrayValue::empty(std::shared_ptr<meta::MetaType> element_type) {
    auto arrayType = std::make_shared<AnonymousArrayType>(element_type);
    return std::make_shared<AnonymousArrayValue>(arrayType);
}

void AnonymousArrayValue::push(const std::variant<int, double, std::string, bool>& element) {
    elements_.push_back(element);
}

std::optional<std::variant<int, double, std::string, bool>> AnonymousArrayValue::get(size_t index) const {
    if (index < elements_.size()) {
        return elements_[index];
    }
    return std::nullopt;
}

void AnonymousArrayValue::set(size_t index, const std::variant<int, double, std::string, bool>& element) {
    if (index < elements_.size()) {
        elements_[index] = element;
    }
}

std::optional<std::variant<int, double, std::string, bool>> AnonymousArrayValue::pop() {
    if (!elements_.empty()) {
        auto element = elements_.back();
        elements_.pop_back();
        return element;
    }
    return std::nullopt;
}

std::shared_ptr<AnonymousArrayValue> AnonymousArrayValue::slice(size_t start, size_t end) const {
    auto new_array = std::make_shared<AnonymousArrayValue>(type_);
    if (start < elements_.size() && end <= elements_.size() && start < end) {
        for (size_t i = start; i < end; ++i) {
            new_array->push(elements_[i]);
        }
    }
    return new_array;
}

void AnonymousArrayValue::insert(size_t index, const std::variant<int, double, std::string, bool>& element) {
    if (index <= elements_.size()) {
        elements_.insert(elements_.begin() + index, element);
    }
}

void AnonymousArrayValue::remove(size_t index) {
    if (index < elements_.size()) {
        elements_.erase(elements_.begin() + index);
    }
}

void AnonymousArrayValue::clear() {
    elements_.clear();
}

// AnonymousTupleType implementation

AnonymousTupleType::AnonymousTupleType(const std::vector<AnonymousTupleElement>& elements)
    : elements_(elements) {
    // Build name-to-index map for named elements
    for (const auto& element : elements_) {
        if (!element.name.empty()) {
            name_to_index_[element.name] = element.index;
        }
    }
}

std::string AnonymousTupleType::name() const {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < elements_.size(); ++i) {
        if (i > 0) oss << ", ";
        if (!elements_[i].name.empty()) {
            oss << elements_[i].name << ": ";
        }
        oss << elements_[i].type->name();
    }
    oss << "]";
    return oss.str();
}

size_t AnonymousTupleType::size() const {
    // Sum of all element sizes
    size_t total = 0;
    for (const auto& element : elements_) {
        total += element.type->size();
    }
    return total;
}

std::optional<std::shared_ptr<meta::MetaType>> AnonymousTupleType::getElementType(size_t index) const {
    if (index < elements_.size()) {
        return elements_[index].type;
    }
    return std::nullopt;
}

std::optional<std::shared_ptr<meta::MetaType>> AnonymousTupleType::getElementType(const std::string& name) const {
    auto it = name_to_index_.find(name);
    if (it != name_to_index_.end()) {
        return elements_[it->second].type;
    }
    return std::nullopt;
}

bool AnonymousTupleType::hasNamedElements() const {
    return std::any_of(elements_.begin(), elements_.end(),
                      [](const AnonymousTupleElement& elem) { return !elem.name.empty(); });
}

// AnonymousTupleValue implementation

AnonymousTupleValue::AnonymousTupleValue(std::shared_ptr<AnonymousTupleType> type)
    : type_(type), elements_(type->elementCount()) {
}

std::optional<std::variant<int, double, std::string, bool>> AnonymousTupleValue::get(size_t index) const {
    if (index < elements_.size()) {
        return elements_[index];
    }
    return std::nullopt;
}

void AnonymousTupleValue::set(size_t index, const std::variant<int, double, std::string, bool>& value) {
    if (index < elements_.size()) {
        elements_[index] = value;
    }
}

std::optional<std::variant<int, double, std::string, bool>> AnonymousTupleValue::get(const std::string& name) const {
    auto element_type = type_->getElementType(name);
    if (element_type) {
        // Find the index for this name
        const auto& elements = type_->elements();
        for (const auto& elem : elements) {
            if (elem.name == name) {
                return get(elem.index);
            }
        }
    }
    return std::nullopt;
}

void AnonymousTupleValue::set(const std::string& name, const std::variant<int, double, std::string, bool>& value) {
    auto element_type = type_->getElementType(name);
    if (element_type) {
        // Find the index for this name
        const auto& elements = type_->elements();
        for (const auto& elem : elements) {
            if (elem.name == name) {
                set(elem.index, value);
                return;
            }
        }
    }
}

std::shared_ptr<AnonymousTupleValue> AnonymousTupleValue::create(
    const std::vector<std::variant<int, double, std::string, bool>>& values,
    const std::vector<std::shared_ptr<meta::MetaType>>& types) {
    
    std::vector<AnonymousTupleElement> elements;
    for (size_t i = 0; i < values.size() && i < types.size(); ++i) {
        elements.emplace_back(i, types[i]);
    }
    
    auto tuple_type = std::make_shared<AnonymousTupleType>(elements);
    auto tuple_value = std::make_shared<AnonymousTupleValue>(tuple_type);
    
    for (size_t i = 0; i < values.size(); ++i) {
        tuple_value->set(i, values[i]);
    }
    
    return tuple_value;
}

std::shared_ptr<AnonymousTupleValue> AnonymousTupleValue::createNamed(
    const std::vector<std::pair<std::string, std::variant<int, double, std::string, bool>>>& named_values,
    const std::vector<std::shared_ptr<meta::MetaType>>& types) {
    
    std::vector<AnonymousTupleElement> elements;
    for (size_t i = 0; i < named_values.size() && i < types.size(); ++i) {
        elements.emplace_back(i, types[i], named_values[i].first);
    }
    
    auto tuple_type = std::make_shared<AnonymousTupleType>(elements);
    auto tuple_value = std::make_shared<AnonymousTupleValue>(tuple_type);
    
    for (size_t i = 0; i < named_values.size(); ++i) {
        tuple_value->set(i, named_values[i].second);
    }
    
    return tuple_value;
}

} // namespace meld::types
