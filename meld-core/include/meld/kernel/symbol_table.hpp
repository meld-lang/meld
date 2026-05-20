#pragma once

#include "primitives.hpp"
#include <unordered_map>
#include <mutex>

namespace meld::kernel {

// Symbol table for interning symbols
class SymbolTable {
public:
    static SymbolTable& instance() {
        static SymbolTable table;
        return table;
    }
    
    std::shared_ptr<Symbol> intern(const std::string& name);
    std::shared_ptr<Symbol> gensym(const std::string& prefix = "G");
    
private:
    SymbolTable() = default;
    
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols_;
    std::mutex mutex_;
    size_t gensym_counter_ = 0;
};

} // namespace meld::kernel
