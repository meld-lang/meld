#include "meld/kernel/symbol_table.hpp"

namespace meld::kernel {

std::shared_ptr<Symbol> SymbolTable::intern(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = symbols_.find(name);
    if (it != symbols_.end()) {
        return it->second;
    }
    
    auto symbol = std::make_shared<Symbol>(name);
    symbols_[name] = symbol;
    return symbol;
}

std::shared_ptr<Symbol> SymbolTable::gensym(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string name = prefix + "__" + std::to_string(gensym_counter_++);
    auto symbol = std::make_shared<Symbol>(name);
    // Don't add to symbols_ table - gensyms are unique
    return symbol;
}

} // namespace meld::kernel
