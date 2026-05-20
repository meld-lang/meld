#include "meld/api/meld_binary.hpp"
#include "meld/api/semantic_graph_api.hpp"
#include "meld/parser/ast.hpp"
#include "meld/provenance/provenance.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace meld::api {

// MeldBinaryHeader implementation
MeldBinaryHeader::MeldBinaryHeader() 
    : magic(MAGIC_NUMBER)
    , version_major(VERSION_MAJOR)
    , version_minor(VERSION_MINOR)
    , compression_type(0)
    , uncompressed_size(0)
    , compressed_size(0)
    , symbol_table_offset(0)
    , type_table_offset(0)
    , cross_ref_table_offset(0)
    , effect_table_offset(0)
    , provenance_table_offset(0)
    , flow_table_offset(0)
    , asg_data_offset(0)
    , metadata_offset(0) {
}

bool MeldBinaryHeader::isValid() const {
    return magic == MAGIC_NUMBER && 
           version_major == VERSION_MAJOR && 
           version_minor == VERSION_MINOR;
}

// SymbolTableEntry implementation
std::string SymbolTableEntry::getName(const std::vector<char>& string_pool) const {
    if (name_offset + name_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + name_offset, name_length);
}

// TypeTableEntry implementation
std::string TypeTableEntry::getName(const std::vector<char>& string_pool) const {
    if (name_offset + name_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + name_offset, name_length);
}

// CrossRefEntry implementation
std::string CrossRefEntry::getContext(const std::vector<char>& string_pool) const {
    if (context_offset + context_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + context_offset, context_length);
}

// EffectTableEntry implementation
std::vector<std::string> EffectTableEntry::getEffectNames(const std::vector<char>& string_pool) const {
    std::vector<std::string> names;
    for (size_t i = 0; i < effect_name_offsets.size(); ++i) {
        uint32_t offset = effect_name_offsets[i];
        uint32_t length = effect_name_lengths[i];
        if (offset + length <= string_pool.size()) {
            names.emplace_back(string_pool.data() + offset, length);
        }
    }
    return names;
}

// ProvenanceTableEntry implementation
std::string ProvenanceTableEntry::getAuthorEmail(const std::vector<char>& string_pool) const {
    if (author_email_length == 0 || author_email_offset + author_email_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + author_email_offset, author_email_length);
}

std::string ProvenanceTableEntry::getCommitHash(const std::vector<char>& string_pool) const {
    if (commit_hash_length == 0 || commit_hash_offset + commit_hash_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + commit_hash_offset, commit_hash_length);
}

std::string ProvenanceTableEntry::getAgentModel(const std::vector<char>& string_pool) const {
    if (agent_model_length == 0 || agent_model_offset + agent_model_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + agent_model_offset, agent_model_length);
}

std::string ProvenanceTableEntry::getBlueprintId(const std::vector<char>& string_pool) const {
    if (blueprint_id_length == 0 || blueprint_id_offset + blueprint_id_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + blueprint_id_offset, blueprint_id_length);
}

std::string ProvenanceTableEntry::getReviewerEmail(const std::vector<char>& string_pool) const {
    if (reviewer_email_length == 0 || reviewer_email_offset + reviewer_email_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + reviewer_email_offset, reviewer_email_length);
}

// FlowTransitionEntry implementation
std::string FlowTransitionEntry::getEventName(const std::vector<char>& string_pool) const {
    if (event_name_length == 0 || event_name_offset + event_name_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + event_name_offset, event_name_length);
}

std::string FlowTransitionEntry::getTargetState(const std::vector<char>& string_pool) const {
    if (target_state_length == 0 || target_state_offset + target_state_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + target_state_offset, target_state_length);
}

std::string FlowTransitionEntry::getGuardCondition(const std::vector<char>& string_pool) const {
    if (!has_guard || guard_condition_length == 0 || guard_condition_offset + guard_condition_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + guard_condition_offset, guard_condition_length);
}

// FlowStateEntry implementation
std::string FlowStateEntry::getStateName(const std::vector<char>& string_pool) const {
    if (state_name_length == 0 || state_name_offset + state_name_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + state_name_offset, state_name_length);
}

std::string FlowStateEntry::getEntryAction(const std::vector<char>& string_pool) const {
    if (!has_entry_action || entry_action_length == 0 || entry_action_offset + entry_action_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + entry_action_offset, entry_action_length);
}

std::string FlowStateEntry::getExitAction(const std::vector<char>& string_pool) const {
    if (!has_exit_action || exit_action_length == 0 || exit_action_offset + exit_action_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + exit_action_offset, exit_action_length);
}

// FlowTableEntry implementation
std::string FlowTableEntry::getFlowName(const std::vector<char>& string_pool) const {
    if (flow_name_length == 0 || flow_name_offset + flow_name_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + flow_name_offset, flow_name_length);
}

std::string FlowTableEntry::getInitialState(const std::vector<char>& string_pool) const {
    if (initial_state_length == 0 || initial_state_offset + initial_state_length > string_pool.size()) {
        return "";
    }
    return std::string(string_pool.data() + initial_state_offset, initial_state_length);
}

// MeldBinary implementation
std::unique_ptr<MeldBinary> MeldBinary::load(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    
    // Read entire file
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(file_size);
    file.read(reinterpret_cast<char*>(buffer.data()), file_size);
    
    auto binary = std::make_unique<MeldBinary>();
    size_t offset = 0;
    
    try {
        binary->deserializeHeader(buffer, offset);
        if (!binary->header_.isValid()) {
            return nullptr;
        }
        
        binary->deserializeSymbolTable(buffer, offset);
        binary->deserializeTypeTable(buffer, offset);
        binary->deserializeCrossRefTable(buffer, offset);
        binary->deserializeEffectTable(buffer, offset);
        binary->deserializeProvenanceTable(buffer, offset);
        binary->deserializeFlowTable(buffer, offset);
        binary->deserializeStringPool(buffer, offset);
        binary->deserializeASGData(buffer, offset);
        binary->deserializeMetadata(buffer, offset);
        
        return binary;
    } catch (const std::exception&) {
        return nullptr;
    }
}

std::unique_ptr<MeldBinary> MeldBinary::fromSemanticGraph(std::shared_ptr<SemanticGraph> graph) {
    auto binary = std::make_unique<MeldBinary>();
    binary->buildIndicesFromSemanticGraph(graph);
    return binary;
}

std::unique_ptr<MeldBinary> MeldBinary::fromHologram(const ModuleHologram& hologram) {
    auto binary = std::make_unique<MeldBinary>();
    binary->buildIndicesFromHologram(hologram);
    return binary;
}

void MeldBinary::save(const std::string& file_path) const {
    std::vector<uint8_t> buffer;
    
    // Serialize all sections
    serializeHeader(buffer);
    serializeSymbolTable(buffer);
    serializeTypeTable(buffer);
    serializeCrossRefTable(buffer);
    serializeEffectTable(buffer);
    serializeProvenanceTable(buffer);
    serializeFlowTable(buffer);
    serializeStringPool(buffer);
    serializeASGData(buffer);
    serializeMetadata(buffer);
    
    // Write to file
    std::ofstream file(file_path, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }
}

std::vector<NodeId> MeldBinary::query(const std::string& pattern) const {
    std::vector<NodeId> results;
    
    // Simple pattern matching - in a real implementation, this would be more sophisticated
    for (const auto& entry : symbol_table_) {
        std::string name = entry.getName(string_pool_);
        if (name.find(pattern) != std::string::npos) {
            results.insert(results.end(), entry.node_references.begin(), entry.node_references.end());
        }
    }
    
    return results;
}

std::vector<NodeId> MeldBinary::queryByType(NodeType type) const {
    // This would require storing type information in the binary format
    // For now, return empty vector
    return {};
}

std::vector<NodeId> MeldBinary::queryByEffect(const std::string& effect_name) const {
    std::vector<NodeId> results;
    
    for (const auto& entry : effect_table_) {
        auto effect_names = entry.getEffectNames(string_pool_);
        if (std::find(effect_names.begin(), effect_names.end(), effect_name) != effect_names.end()) {
            results.push_back(entry.function_node);
        }
    }
    
    return results;
}

std::vector<UsageEdge> MeldBinary::queryUsage(const std::string& symbol) const {
    std::vector<UsageEdge> results;
    
    // Find symbol in symbol table
    for (const auto& entry : symbol_table_) {
        std::string name = entry.getName(string_pool_);
        if (name == symbol) {
            // Find cross-references for this symbol
            for (const auto& cross_ref : cross_ref_table_) {
                for (NodeId node_id : entry.node_references) {
                    if (cross_ref.source_node == node_id || cross_ref.target_node == node_id) {
                        DataFlowEdge::FlowType flow_type;
                        switch (cross_ref.reference_type) {
                            case 0: flow_type = DataFlowEdge::FlowType::USE; break;
                            case 1: flow_type = DataFlowEdge::FlowType::DEFINITION; break;
                            case 2: flow_type = DataFlowEdge::FlowType::MODIFICATION; break;
                            default: flow_type = DataFlowEdge::FlowType::USE; break;
                        }
                        
                        SourceLocation location; // Would need to be stored in binary format
                        std::string context = cross_ref.getContext(string_pool_);
                        
                        results.emplace_back(cross_ref.source_node, 0, flow_type, location, context);
                    }
                }
            }
        }
    }
    
    return results;
}

// Provenance Query API (Requirement 45.12)
std::vector<NodeId> MeldBinary::queryByOrigin(uint8_t origin_type) const {
    std::vector<NodeId> results;
    
    for (const auto& entry : provenance_table_) {
        if (entry.origin_type == origin_type) {
            results.push_back(entry.node_id);
        }
    }
    
    return results;
}

std::vector<NodeId> MeldBinary::queryByTrustLevel(double min_trust_level) const {
    std::vector<NodeId> results;
    
    for (const auto& entry : provenance_table_) {
        if (entry.trust_score >= min_trust_level) {
            results.push_back(entry.node_id);
        }
    }
    
    return results;
}

std::vector<NodeId> MeldBinary::queryByAuthor(const std::string& author_email) const {
    std::vector<NodeId> results;
    
    for (const auto& entry : provenance_table_) {
        if (entry.origin_type == 0 && // Human origin
            entry.getAuthorEmail(string_pool_) == author_email) {
            results.push_back(entry.node_id);
        }
    }
    
    return results;
}

std::vector<NodeId> MeldBinary::queryByAgentModel(const std::string& agent_model) const {
    std::vector<NodeId> results;
    
    for (const auto& entry : provenance_table_) {
        if (entry.origin_type == 1 && // Agent origin
            entry.getAgentModel(string_pool_) == agent_model) {
            results.push_back(entry.node_id);
        }
    }
    
    return results;
}

std::vector<NodeId> MeldBinary::queryVerifiedOnly() const {
    std::vector<NodeId> results;
    
    for (const auto& entry : provenance_table_) {
        if (entry.origin_type == 2 || // Verified origin
            (entry.verification_timestamp > 0)) { // Or has verification timestamp
            results.push_back(entry.node_id);
        }
    }
    
    return results;
}

std::vector<ProvenanceTableEntry> MeldBinary::getProvenanceForNodes(const std::vector<NodeId>& nodes) const {
    std::vector<ProvenanceTableEntry> results;
    
    for (NodeId node_id : nodes) {
        for (const auto& entry : provenance_table_) {
            if (entry.node_id == node_id) {
                results.push_back(entry);
                break;
            }
        }
    }
    
    return results;
}

// Flow Query API (Requirement 40.2)
std::vector<NodeId> MeldBinary::queryFlowDefinitions() const {
    std::vector<NodeId> results;
    
    for (const auto& entry : flow_table_) {
        results.push_back(entry.flow_node_id);
    }
    
    return results;
}

std::vector<FlowTableEntry> MeldBinary::getFlowDefinitions() const {
    return flow_table_;
}

std::vector<NodeId> MeldBinary::queryFlowsByName(const std::string& flow_name) const {
    std::vector<NodeId> results;
    
    for (const auto& entry : flow_table_) {
        if (entry.getFlowName(string_pool_) == flow_name) {
            results.push_back(entry.flow_node_id);
        }
    }
    
    return results;
}

std::vector<FlowStateEntry> MeldBinary::getStatesForFlow(NodeId flow_node_id) const {
    for (const auto& entry : flow_table_) {
        if (entry.flow_node_id == flow_node_id) {
            return entry.states;
        }
    }
    
    return {};
}

std::vector<FlowTransitionEntry> MeldBinary::getTransitionsForState(NodeId flow_node_id, const std::string& state_name) const {
    for (const auto& flow_entry : flow_table_) {
        if (flow_entry.flow_node_id == flow_node_id) {
            for (const auto& state_entry : flow_entry.states) {
                if (state_entry.getStateName(string_pool_) == state_name) {
                    return state_entry.transitions;
                }
            }
        }
    }
    
    return {};
}

std::shared_ptr<SemanticGraph> MeldBinary::toSemanticGraph() const {
    auto graph = std::make_shared<SemanticGraph>();
    
    // Reconstruct nodes from the binary data
    // This is a simplified reconstruction - in a full implementation,
    // we would deserialize the complete ASG structure
    
    // Add nodes based on symbol table entries
    for (const auto& symbol_entry : symbol_table_) {
        std::string symbol_name = symbol_entry.getName(string_pool_);
        
        // Create a symbol node for each entry
        kernel::Value symbol_node = kernel::createSymbol(symbol_name);
        
        // Find and attach provenance metadata if available
        for (const auto& provenance_entry : provenance_table_) {
            // Check if any of the symbol's node references match this provenance entry
            for (NodeId node_id : symbol_entry.node_references) {
                if (provenance_entry.node_id == node_id) {
                    // Reconstruct provenance metadata
                    provenance::ProvenanceMetadata metadata;
                    
                    // Set origin type
                    switch (provenance_entry.origin_type) {
                        case 0: metadata.origin = provenance::OriginType::Human; break;
                        case 1: metadata.origin = provenance::OriginType::Agent; break;
                        case 2: metadata.origin = provenance::OriginType::Verified; break;
                    }
                    
                    // Set timestamps
                    metadata.creation_timestamp = std::chrono::system_clock::time_point(
                        std::chrono::milliseconds(provenance_entry.creation_timestamp)
                    );
                    
                    if (provenance_entry.verification_timestamp > 0) {
                        metadata.verification_timestamp = std::chrono::system_clock::time_point(
                            std::chrono::milliseconds(provenance_entry.verification_timestamp)
                        );
                    }
                    
                    // Set trust and confidence scores
                    metadata.trust_score = provenance_entry.trust_score;
                    if (provenance_entry.confidence_score > 0) {
                        metadata.confidence_score = provenance_entry.confidence_score;
                    }
                    
                    // Set optional string fields
                    if (provenance_entry.author_email_length > 0) {
                        metadata.author_email = provenance_entry.getAuthorEmail(string_pool_);
                    }
                    if (provenance_entry.commit_hash_length > 0) {
                        metadata.commit_hash = provenance_entry.getCommitHash(string_pool_);
                    }
                    if (provenance_entry.agent_model_length > 0) {
                        metadata.agent_model = provenance_entry.getAgentModel(string_pool_);
                    }
                    if (provenance_entry.blueprint_id_length > 0) {
                        metadata.blueprint_id = provenance_entry.getBlueprintId(string_pool_);
                    }
                    if (provenance_entry.reviewer_email_length > 0) {
                        metadata.reviewer_email = provenance_entry.getReviewerEmail(string_pool_);
                    }
                    
                    // Attach provenance to the node
                    symbol_node = provenance::Provenance::attachProvenance(symbol_node, metadata);
                    break;
                }
            }
        }
        
        parser::ast::identifier id_node;
        id_node.name = symbol_name;
        parser::ast::expression dummy_expr(id_node);
        SourceLocation loc(symbol_name, 0, 0, 0);
        graph->addNode(NodeType::VARIABLE_DECLARATION, dummy_expr, loc);
    }
    
    return graph;
}

double MeldBinary::getCompressionRatio() const {
    if (original_size_ == 0) return 0.0;
    return static_cast<double>(compressed_size_) / static_cast<double>(original_size_);
}

bool MeldBinary::validate() const {
    return header_.isValid();
}

// Serialization helpers
void MeldBinary::serializeHeader(std::vector<uint8_t>& buffer) const {
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(MeldBinaryHeader));
    std::memcpy(buffer.data() + start_pos, &header_, sizeof(MeldBinaryHeader));
}

void MeldBinary::serializeSymbolTable(std::vector<uint8_t>& buffer) const {
    // Serialize symbol table entries
    uint32_t count = static_cast<uint32_t>(symbol_table_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &count, sizeof(uint32_t));
    
    for (const auto& entry : symbol_table_) {
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(SymbolTableEntry) - sizeof(std::vector<NodeId>));
        std::memcpy(buffer.data() + start_pos, &entry, sizeof(SymbolTableEntry) - sizeof(std::vector<NodeId>));
        
        // Serialize node references
        uint32_t ref_count = static_cast<uint32_t>(entry.node_references.size());
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(uint32_t));
        std::memcpy(buffer.data() + start_pos, &ref_count, sizeof(uint32_t));
        
        if (ref_count > 0) {
            start_pos = buffer.size();
            buffer.resize(start_pos + ref_count * sizeof(NodeId));
            std::memcpy(buffer.data() + start_pos, entry.node_references.data(), ref_count * sizeof(NodeId));
        }
    }
}

void MeldBinary::serializeTypeTable(std::vector<uint8_t>& buffer) const {
    // Similar to symbol table serialization
    uint32_t count = static_cast<uint32_t>(type_table_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &count, sizeof(uint32_t));
    
    for (const auto& entry : type_table_) {
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(TypeTableEntry) - sizeof(std::vector<NodeId>));
        std::memcpy(buffer.data() + start_pos, &entry, sizeof(TypeTableEntry) - sizeof(std::vector<NodeId>));
        
        uint32_t ref_count = static_cast<uint32_t>(entry.instance_references.size());
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(uint32_t));
        std::memcpy(buffer.data() + start_pos, &ref_count, sizeof(uint32_t));
        
        if (ref_count > 0) {
            start_pos = buffer.size();
            buffer.resize(start_pos + ref_count * sizeof(NodeId));
            std::memcpy(buffer.data() + start_pos, entry.instance_references.data(), ref_count * sizeof(NodeId));
        }
    }
}

void MeldBinary::serializeCrossRefTable(std::vector<uint8_t>& buffer) const {
    uint32_t count = static_cast<uint32_t>(cross_ref_table_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &count, sizeof(uint32_t));
    
    for (const auto& entry : cross_ref_table_) {
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(CrossRefEntry));
        std::memcpy(buffer.data() + start_pos, &entry, sizeof(CrossRefEntry));
    }
}

void MeldBinary::serializeEffectTable(std::vector<uint8_t>& buffer) const {
    uint32_t count = static_cast<uint32_t>(effect_table_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &count, sizeof(uint32_t));
    
    for (const auto& entry : effect_table_) {
        // Serialize function node
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(NodeId));
        std::memcpy(buffer.data() + start_pos, &entry.function_node, sizeof(NodeId));
        
        // Serialize effect name arrays
        uint32_t effect_count = static_cast<uint32_t>(entry.effect_name_offsets.size());
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(uint32_t));
        std::memcpy(buffer.data() + start_pos, &effect_count, sizeof(uint32_t));
        
        if (effect_count > 0) {
            start_pos = buffer.size();
            buffer.resize(start_pos + effect_count * sizeof(uint32_t));
            std::memcpy(buffer.data() + start_pos, entry.effect_name_offsets.data(), effect_count * sizeof(uint32_t));
            
            start_pos = buffer.size();
            buffer.resize(start_pos + effect_count * sizeof(uint32_t));
            std::memcpy(buffer.data() + start_pos, entry.effect_name_lengths.data(), effect_count * sizeof(uint32_t));
        }
    }
}

void MeldBinary::serializeProvenanceTable(std::vector<uint8_t>& buffer) const {
    uint32_t count = static_cast<uint32_t>(provenance_table_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &count, sizeof(uint32_t));
    
    for (const auto& entry : provenance_table_) {
        // Serialize the fixed-size portion of the entry
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(ProvenanceTableEntry));
        std::memcpy(buffer.data() + start_pos, &entry, sizeof(ProvenanceTableEntry));
    }
}

void MeldBinary::serializeStringPool(std::vector<uint8_t>& buffer) const {
    uint32_t size = static_cast<uint32_t>(string_pool_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &size, sizeof(uint32_t));
    
    if (size > 0) {
        start_pos = buffer.size();
        buffer.resize(start_pos + size);
        std::memcpy(buffer.data() + start_pos, string_pool_.data(), size);
    }
}

void MeldBinary::serializeASGData(std::vector<uint8_t>& buffer) const {
    uint32_t size = static_cast<uint32_t>(asg_data_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &size, sizeof(uint32_t));
    
    if (size > 0) {
        start_pos = buffer.size();
        buffer.resize(start_pos + size);
        std::memcpy(buffer.data() + start_pos, asg_data_.data(), size);
    }
}

void MeldBinary::serializeMetadata(std::vector<uint8_t>& buffer) const {
    uint32_t size = static_cast<uint32_t>(metadata_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &size, sizeof(uint32_t));
    
    if (size > 0) {
        start_pos = buffer.size();
        buffer.resize(start_pos + size);
        std::memcpy(buffer.data() + start_pos, metadata_.data(), size);
    }
}

// Deserialization helpers (simplified implementations)
void MeldBinary::deserializeHeader(const std::vector<uint8_t>& buffer, size_t& offset) {
    if (offset + sizeof(MeldBinaryHeader) > buffer.size()) {
        throw std::runtime_error("Buffer too small for header");
    }
    std::memcpy(&header_, buffer.data() + offset, sizeof(MeldBinaryHeader));
    offset += sizeof(MeldBinaryHeader);
}

void MeldBinary::deserializeSymbolTable(const std::vector<uint8_t>& buffer, size_t& offset) {
    if (offset + sizeof(uint32_t) > buffer.size()) {
        throw std::runtime_error("Buffer too small for symbol table count");
    }
    
    uint32_t count;
    std::memcpy(&count, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    symbol_table_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (offset + sizeof(SymbolTableEntry) - sizeof(std::vector<NodeId>) > buffer.size()) {
            throw std::runtime_error("Buffer too small for symbol table entry");
        }
        
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnontrivial-memcall"
        std::memcpy(&symbol_table_[i], buffer.data() + offset, sizeof(SymbolTableEntry) - sizeof(std::vector<NodeId>));
#pragma clang diagnostic pop
        offset += sizeof(SymbolTableEntry) - sizeof(std::vector<NodeId>);
        
        uint32_t ref_count;
        std::memcpy(&ref_count, buffer.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (ref_count > 0) {
            symbol_table_[i].node_references.resize(ref_count);
            std::memcpy(symbol_table_[i].node_references.data(), buffer.data() + offset, ref_count * sizeof(NodeId));
            offset += ref_count * sizeof(NodeId);
        }
    }
}

void MeldBinary::deserializeTypeTable(const std::vector<uint8_t>& buffer, size_t& offset) {
    // Similar to symbol table deserialization
    uint32_t count;
    std::memcpy(&count, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    type_table_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnontrivial-memcall"
        std::memcpy(&type_table_[i], buffer.data() + offset, sizeof(TypeTableEntry) - sizeof(std::vector<NodeId>));
#pragma clang diagnostic pop
        offset += sizeof(TypeTableEntry) - sizeof(std::vector<NodeId>);
        
        uint32_t ref_count;
        std::memcpy(&ref_count, buffer.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (ref_count > 0) {
            type_table_[i].instance_references.resize(ref_count);
            std::memcpy(type_table_[i].instance_references.data(), buffer.data() + offset, ref_count * sizeof(NodeId));
            offset += ref_count * sizeof(NodeId);
        }
    }
}

void MeldBinary::deserializeCrossRefTable(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t count;
    std::memcpy(&count, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    cross_ref_table_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&cross_ref_table_[i], buffer.data() + offset, sizeof(CrossRefEntry));
        offset += sizeof(CrossRefEntry);
    }
}

void MeldBinary::deserializeEffectTable(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t count;
    std::memcpy(&count, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    effect_table_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        std::memcpy(&effect_table_[i].function_node, buffer.data() + offset, sizeof(NodeId));
        offset += sizeof(NodeId);
        
        uint32_t effect_count;
        std::memcpy(&effect_count, buffer.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        if (effect_count > 0) {
            effect_table_[i].effect_name_offsets.resize(effect_count);
            std::memcpy(effect_table_[i].effect_name_offsets.data(), buffer.data() + offset, effect_count * sizeof(uint32_t));
            offset += effect_count * sizeof(uint32_t);
            
            effect_table_[i].effect_name_lengths.resize(effect_count);
            std::memcpy(effect_table_[i].effect_name_lengths.data(), buffer.data() + offset, effect_count * sizeof(uint32_t));
            offset += effect_count * sizeof(uint32_t);
        }
    }
}

void MeldBinary::deserializeProvenanceTable(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t count;
    std::memcpy(&count, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    provenance_table_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (offset + sizeof(ProvenanceTableEntry) > buffer.size()) {
            throw std::runtime_error("Buffer too small for provenance table entry");
        }
        std::memcpy(&provenance_table_[i], buffer.data() + offset, sizeof(ProvenanceTableEntry));
        offset += sizeof(ProvenanceTableEntry);
    }
}

void MeldBinary::serializeFlowTable(std::vector<uint8_t>& buffer) const {
    uint32_t count = static_cast<uint32_t>(flow_table_.size());
    size_t start_pos = buffer.size();
    buffer.resize(start_pos + sizeof(uint32_t));
    std::memcpy(buffer.data() + start_pos, &count, sizeof(uint32_t));
    
    for (const auto& flow_entry : flow_table_) {
        // Serialize flow node ID
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(NodeId));
        std::memcpy(buffer.data() + start_pos, &flow_entry.flow_node_id, sizeof(NodeId));
        
        // Serialize flow name offset and length
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(uint32_t) * 2);
        std::memcpy(buffer.data() + start_pos, &flow_entry.flow_name_offset, sizeof(uint32_t));
        std::memcpy(buffer.data() + start_pos + sizeof(uint32_t), &flow_entry.flow_name_length, sizeof(uint32_t));
        
        // Serialize initial state offset and length
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(uint32_t) * 2);
        std::memcpy(buffer.data() + start_pos, &flow_entry.initial_state_offset, sizeof(uint32_t));
        std::memcpy(buffer.data() + start_pos + sizeof(uint32_t), &flow_entry.initial_state_length, sizeof(uint32_t));
        
        // Serialize states count
        uint32_t states_count = static_cast<uint32_t>(flow_entry.states.size());
        start_pos = buffer.size();
        buffer.resize(start_pos + sizeof(uint32_t));
        std::memcpy(buffer.data() + start_pos, &states_count, sizeof(uint32_t));
        
        // Serialize each state
        for (const auto& state_entry : flow_entry.states) {
            // Serialize state name offset and length
            start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(uint32_t) * 2);
            std::memcpy(buffer.data() + start_pos, &state_entry.state_name_offset, sizeof(uint32_t));
            std::memcpy(buffer.data() + start_pos + sizeof(uint32_t), &state_entry.state_name_length, sizeof(uint32_t));
            
            // Serialize state flags
            start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(bool) * 3);
            std::memcpy(buffer.data() + start_pos, &state_entry.is_terminal, sizeof(bool));
            std::memcpy(buffer.data() + start_pos + sizeof(bool), &state_entry.has_entry_action, sizeof(bool));
            std::memcpy(buffer.data() + start_pos + sizeof(bool) * 2, &state_entry.has_exit_action, sizeof(bool));
            
            // Serialize entry action offset and length
            start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(uint32_t) * 2);
            std::memcpy(buffer.data() + start_pos, &state_entry.entry_action_offset, sizeof(uint32_t));
            std::memcpy(buffer.data() + start_pos + sizeof(uint32_t), &state_entry.entry_action_length, sizeof(uint32_t));
            
            // Serialize exit action offset and length
            start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(uint32_t) * 2);
            std::memcpy(buffer.data() + start_pos, &state_entry.exit_action_offset, sizeof(uint32_t));
            std::memcpy(buffer.data() + start_pos + sizeof(uint32_t), &state_entry.exit_action_length, sizeof(uint32_t));
            
            // Serialize transitions count
            uint32_t transitions_count = static_cast<uint32_t>(state_entry.transitions.size());
            start_pos = buffer.size();
            buffer.resize(start_pos + sizeof(uint32_t));
            std::memcpy(buffer.data() + start_pos, &transitions_count, sizeof(uint32_t));
            
            // Serialize each transition
            for (const auto& transition_entry : state_entry.transitions) {
                // Serialize event name offset and length
                start_pos = buffer.size();
                buffer.resize(start_pos + sizeof(uint32_t) * 2);
                std::memcpy(buffer.data() + start_pos, &transition_entry.event_name_offset, sizeof(uint32_t));
                std::memcpy(buffer.data() + start_pos + sizeof(uint32_t), &transition_entry.event_name_length, sizeof(uint32_t));
                
                // Serialize target state offset and length
                start_pos = buffer.size();
                buffer.resize(start_pos + sizeof(uint32_t) * 2);
                std::memcpy(buffer.data() + start_pos, &transition_entry.target_state_offset, sizeof(uint32_t));
                std::memcpy(buffer.data() + start_pos + sizeof(uint32_t), &transition_entry.target_state_length, sizeof(uint32_t));
                
                // Serialize guard flag
                start_pos = buffer.size();
                buffer.resize(start_pos + sizeof(bool));
                std::memcpy(buffer.data() + start_pos, &transition_entry.has_guard, sizeof(bool));
                
                // Serialize guard condition offset and length
                start_pos = buffer.size();
                buffer.resize(start_pos + sizeof(uint32_t) * 2);
                std::memcpy(buffer.data() + start_pos, &transition_entry.guard_condition_offset, sizeof(uint32_t));
                std::memcpy(buffer.data() + start_pos + sizeof(uint32_t), &transition_entry.guard_condition_length, sizeof(uint32_t));
            }
        }
    }
}

void MeldBinary::deserializeFlowTable(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t count;
    std::memcpy(&count, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    flow_table_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        // Deserialize flow node ID
        if (offset + sizeof(NodeId) > buffer.size()) {
            throw std::runtime_error("Buffer too small for flow node ID");
        }
        std::memcpy(&flow_table_[i].flow_node_id, buffer.data() + offset, sizeof(NodeId));
        offset += sizeof(NodeId);
        
        // Deserialize flow name offset and length
        if (offset + sizeof(uint32_t) * 2 > buffer.size()) {
            throw std::runtime_error("Buffer too small for flow name");
        }
        std::memcpy(&flow_table_[i].flow_name_offset, buffer.data() + offset, sizeof(uint32_t));
        std::memcpy(&flow_table_[i].flow_name_length, buffer.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
        offset += sizeof(uint32_t) * 2;
        
        // Deserialize initial state offset and length
        if (offset + sizeof(uint32_t) * 2 > buffer.size()) {
            throw std::runtime_error("Buffer too small for initial state");
        }
        std::memcpy(&flow_table_[i].initial_state_offset, buffer.data() + offset, sizeof(uint32_t));
        std::memcpy(&flow_table_[i].initial_state_length, buffer.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
        offset += sizeof(uint32_t) * 2;
        
        // Deserialize states count
        uint32_t states_count;
        if (offset + sizeof(uint32_t) > buffer.size()) {
            throw std::runtime_error("Buffer too small for states count");
        }
        std::memcpy(&states_count, buffer.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        
        flow_table_[i].states.resize(states_count);
        
        // Deserialize each state
        for (uint32_t j = 0; j < states_count; ++j) {
            auto& state_entry = flow_table_[i].states[j];
            
            // Deserialize state name offset and length
            if (offset + sizeof(uint32_t) * 2 > buffer.size()) {
                throw std::runtime_error("Buffer too small for state name");
            }
            std::memcpy(&state_entry.state_name_offset, buffer.data() + offset, sizeof(uint32_t));
            std::memcpy(&state_entry.state_name_length, buffer.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
            offset += sizeof(uint32_t) * 2;
            
            // Deserialize state flags
            if (offset + sizeof(bool) * 3 > buffer.size()) {
                throw std::runtime_error("Buffer too small for state flags");
            }
            std::memcpy(&state_entry.is_terminal, buffer.data() + offset, sizeof(bool));
            std::memcpy(&state_entry.has_entry_action, buffer.data() + offset + sizeof(bool), sizeof(bool));
            std::memcpy(&state_entry.has_exit_action, buffer.data() + offset + sizeof(bool) * 2, sizeof(bool));
            offset += sizeof(bool) * 3;
            
            // Deserialize entry action offset and length
            if (offset + sizeof(uint32_t) * 2 > buffer.size()) {
                throw std::runtime_error("Buffer too small for entry action");
            }
            std::memcpy(&state_entry.entry_action_offset, buffer.data() + offset, sizeof(uint32_t));
            std::memcpy(&state_entry.entry_action_length, buffer.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
            offset += sizeof(uint32_t) * 2;
            
            // Deserialize exit action offset and length
            if (offset + sizeof(uint32_t) * 2 > buffer.size()) {
                throw std::runtime_error("Buffer too small for exit action");
            }
            std::memcpy(&state_entry.exit_action_offset, buffer.data() + offset, sizeof(uint32_t));
            std::memcpy(&state_entry.exit_action_length, buffer.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
            offset += sizeof(uint32_t) * 2;
            
            // Deserialize transitions count
            uint32_t transitions_count;
            if (offset + sizeof(uint32_t) > buffer.size()) {
                throw std::runtime_error("Buffer too small for transitions count");
            }
            std::memcpy(&transitions_count, buffer.data() + offset, sizeof(uint32_t));
            offset += sizeof(uint32_t);
            
            state_entry.transitions.resize(transitions_count);
            
            // Deserialize each transition
            for (uint32_t k = 0; k < transitions_count; ++k) {
                auto& transition_entry = state_entry.transitions[k];
                
                // Deserialize event name offset and length
                if (offset + sizeof(uint32_t) * 2 > buffer.size()) {
                    throw std::runtime_error("Buffer too small for event name");
                }
                std::memcpy(&transition_entry.event_name_offset, buffer.data() + offset, sizeof(uint32_t));
                std::memcpy(&transition_entry.event_name_length, buffer.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
                offset += sizeof(uint32_t) * 2;
                
                // Deserialize target state offset and length
                if (offset + sizeof(uint32_t) * 2 > buffer.size()) {
                    throw std::runtime_error("Buffer too small for target state");
                }
                std::memcpy(&transition_entry.target_state_offset, buffer.data() + offset, sizeof(uint32_t));
                std::memcpy(&transition_entry.target_state_length, buffer.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
                offset += sizeof(uint32_t) * 2;
                
                // Deserialize guard flag
                if (offset + sizeof(bool) > buffer.size()) {
                    throw std::runtime_error("Buffer too small for guard flag");
                }
                std::memcpy(&transition_entry.has_guard, buffer.data() + offset, sizeof(bool));
                offset += sizeof(bool);
                
                // Deserialize guard condition offset and length
                if (offset + sizeof(uint32_t) * 2 > buffer.size()) {
                    throw std::runtime_error("Buffer too small for guard condition");
                }
                std::memcpy(&transition_entry.guard_condition_offset, buffer.data() + offset, sizeof(uint32_t));
                std::memcpy(&transition_entry.guard_condition_length, buffer.data() + offset + sizeof(uint32_t), sizeof(uint32_t));
                offset += sizeof(uint32_t) * 2;
            }
        }
    }
}

void MeldBinary::deserializeStringPool(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t size;
    std::memcpy(&size, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    if (size > 0) {
        string_pool_.resize(size);
        std::memcpy(string_pool_.data(), buffer.data() + offset, size);
        offset += size;
    }
}

void MeldBinary::deserializeASGData(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t size;
    std::memcpy(&size, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    if (size > 0) {
        asg_data_.resize(size);
        std::memcpy(asg_data_.data(), buffer.data() + offset, size);
        offset += size;
    }
}

void MeldBinary::deserializeMetadata(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t size;
    std::memcpy(&size, buffer.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    
    if (size > 0) {
        metadata_.resize(size);
        std::memcpy(metadata_.data(), buffer.data() + offset, size);
        offset += size;
    }
}

// Index building
void MeldBinary::buildIndicesFromSemanticGraph(std::shared_ptr<SemanticGraph> graph) {
    // Build symbol table from graph index
    const auto& graph_index = graph->getIndex();
    
    // Extract provenance metadata from all nodes in the semantic graph
    auto all_nodes = graph->getAllNodes();
    for (const auto& graph_node : all_nodes) {
        // Get the AST node from the graph node
        auto ast_node = graph_node->getASTNode();
        
        // Check if AST node has provenance metadata
        // TODO: Provenance extraction from AST nodes requires Value-based API
        (void)ast_node;
        
        // Extract flow definitions from AST nodes
        auto flow_def_fwd = boost::get<boost::spirit::x3::forward_ast<meld::parser::ast::flow_definition>>(&ast_node);
        if (flow_def_fwd) {
            const auto* flow_def = &flow_def_fwd->get();
            FlowTableEntry flow_entry;
            flow_entry.flow_node_id = graph_node->getId();
            
            // Add flow name to string pool
            flow_entry.flow_name_offset = addToStringPool(flow_def->name.name);
            flow_entry.flow_name_length = static_cast<uint32_t>(flow_def->name.name.length());
            
            // Add initial state to string pool
            flow_entry.initial_state_offset = addToStringPool(flow_def->initial_state.name);
            flow_entry.initial_state_length = static_cast<uint32_t>(flow_def->initial_state.name.length());
            
            // Process each state in the flow definition
            for (const auto& state_ast : flow_def->states) {
                FlowStateEntry state_entry;
                
                // Add state name to string pool
                state_entry.state_name_offset = addToStringPool(state_ast.name.name);
                state_entry.state_name_length = static_cast<uint32_t>(state_ast.name.name.length());
                
                // Set state flags
                state_entry.is_terminal = state_ast.is_terminal;
                state_entry.has_entry_action = state_ast.has_entry_action;
                state_entry.has_exit_action = state_ast.has_exit_action;
                
                // Add entry action to string pool if present
                if (state_ast.has_entry_action && state_ast.has_entry_action) {
                    // Convert entry action expression to string representation
                    std::string entry_action_str = "/* entry action */"; // Simplified - would need proper AST-to-string conversion
                    state_entry.entry_action_offset = addToStringPool(entry_action_str);
                    state_entry.entry_action_length = static_cast<uint32_t>(entry_action_str.length());
                } else {
                    state_entry.entry_action_offset = 0;
                    state_entry.entry_action_length = 0;
                }
                
                // Add exit action to string pool if present
                if (state_ast.has_exit_action && state_ast.has_exit_action) {
                    // Convert exit action expression to string representation
                    std::string exit_action_str = "/* exit action */"; // Simplified - would need proper AST-to-string conversion
                    state_entry.exit_action_offset = addToStringPool(exit_action_str);
                    state_entry.exit_action_length = static_cast<uint32_t>(exit_action_str.length());
                } else {
                    state_entry.exit_action_offset = 0;
                    state_entry.exit_action_length = 0;
                }
                
                // Process each transition in the state
                for (const auto& transition_ast : state_ast.transitions) {
                    FlowTransitionEntry transition_entry;
                    
                    // Add event name to string pool
                    transition_entry.event_name_offset = addToStringPool(transition_ast.event_name.name);
                    transition_entry.event_name_length = static_cast<uint32_t>(transition_ast.event_name.name.length());
                    
                    // Add target state to string pool
                    transition_entry.target_state_offset = addToStringPool(transition_ast.target_state.name);
                    transition_entry.target_state_length = static_cast<uint32_t>(transition_ast.target_state.name.length());
                    
                    // Set guard flag and condition
                    transition_entry.has_guard = transition_ast.has_guard;
                    if (transition_ast.has_guard && transition_ast.has_guard) {
                        // Convert guard expression to string representation
                        std::string guard_str = "/* guard condition */"; // Simplified - would need proper AST-to-string conversion
                        transition_entry.guard_condition_offset = addToStringPool(guard_str);
                        transition_entry.guard_condition_length = static_cast<uint32_t>(guard_str.length());
                    } else {
                        transition_entry.guard_condition_offset = 0;
                        transition_entry.guard_condition_length = 0;
                    }
                    
                    state_entry.transitions.push_back(transition_entry);
                }
                
                flow_entry.states.push_back(state_entry);
            }
            
            flow_table_.push_back(flow_entry);
        }
    }
    
    // This is a simplified implementation - in practice, we'd need to iterate
    // through all symbols and build proper indices
    
    // Calculate more realistic size estimates
    size_t estimated_original_size = 0;
    
    // Estimate original size based on symbol names and provenance data
    for (const auto& symbol_entry : symbol_table_) {
        estimated_original_size += symbol_entry.getName(string_pool_).length() * 2; // Rough estimate for source code
    }
    
    for (const auto& provenance_entry : provenance_table_) {
        // Add size for provenance metadata in source form (comments, annotations)
        estimated_original_size += 100; // Rough estimate per provenance annotation
        if (provenance_entry.author_email_length > 0) {
            estimated_original_size += provenance_entry.author_email_length;
        }
        if (provenance_entry.agent_model_length > 0) {
            estimated_original_size += provenance_entry.agent_model_length;
        }
    }
    
    // Add size estimate for flow definitions
    for (const auto& flow_entry : flow_table_) {
        estimated_original_size += flow_entry.getFlowName(string_pool_).length() * 10; // Rough estimate for flow source code
        for (const auto& state_entry : flow_entry.states) {
            estimated_original_size += state_entry.getStateName(string_pool_).length() * 5; // Rough estimate per state
            for (const auto& transition_entry : state_entry.transitions) {
                estimated_original_size += transition_entry.getEventName(string_pool_).length() * 3; // Rough estimate per transition
            }
        }
    }
    
    // Ensure minimum size
    if (estimated_original_size < 1000) {
        estimated_original_size = 1000;
    }
    
    original_size_ = estimated_original_size;
    
    // Calculate compressed size based on actual binary data
    size_t binary_size = sizeof(MeldBinaryHeader);
    binary_size += symbol_table_.size() * sizeof(SymbolTableEntry);
    binary_size += type_table_.size() * sizeof(TypeTableEntry);
    binary_size += cross_ref_table_.size() * sizeof(CrossRefEntry);
    binary_size += effect_table_.size() * sizeof(EffectTableEntry);
    binary_size += provenance_table_.size() * sizeof(ProvenanceTableEntry);
    binary_size += flow_table_.size() * sizeof(FlowTableEntry);
    binary_size += string_pool_.size();
    binary_size += asg_data_.size();
    binary_size += metadata_.size();
    
    compressed_size_ = binary_size;
}

void MeldBinary::buildIndicesFromHologram(const ModuleHologram& hologram) {
    // Build indices from hologram data
    
    // Add function signatures to symbol table
    uint32_t symbol_id = 0;
    for (const auto& func_sig : hologram.getFunctionSignatures()) {
        SymbolTableEntry entry;
        entry.symbol_id = symbol_id++;
        entry.name_offset = addToStringPool(func_sig.name);
        entry.name_length = static_cast<uint32_t>(func_sig.name.length());
        // Note: node_references would be empty for hologram since we don't have full ASG
        symbol_table_.push_back(entry);
    }
    
    // Add type definitions to type table
    uint32_t type_id = 0;
    for (const auto& type_def : hologram.getTypeDefinitions()) {
        TypeTableEntry entry;
        entry.type_id = type_id++;
        entry.name_offset = addToStringPool(type_def.name);
        entry.name_length = static_cast<uint32_t>(type_def.name.length());
        entry.definition_node_id = type_id; // Use type_id as a simple node identifier
        type_table_.push_back(entry);
    }
    
    // Calculate compression statistics
    original_size_ = hologram.getOriginalTokenCount() * 4; // Rough estimate: 4 bytes per token
    compressed_size_ = hologram.getHologramTokenCount() * 4; // Compressed size
}

// String pool management
uint32_t MeldBinary::addToStringPool(const std::string& str) {
    uint32_t offset = static_cast<uint32_t>(string_pool_.size());
    string_pool_.insert(string_pool_.end(), str.begin(), str.end());
    return offset;
}

std::string MeldBinary::getFromStringPool(uint32_t offset, uint32_t length) const {
    if (offset + length > string_pool_.size()) {
        return "";
    }
    return std::string(string_pool_.data() + offset, length);
}

// Compression (simplified - would use zstd in real implementation)
std::vector<uint8_t> MeldBinary::compress(const std::vector<uint8_t>& data) const {
    // Simple run-length encoding for demonstration
    // In a real implementation, this would use zstd compression
    std::vector<uint8_t> compressed;
    
    if (data.empty()) {
        return compressed;
    }
    
    uint8_t current_byte = data[0];
    uint8_t count = 1;
    
    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i] == current_byte && count < 255) {
            count++;
        } else {
            compressed.push_back(count);
            compressed.push_back(current_byte);
            current_byte = data[i];
            count = 1;
        }
    }
    
    // Add the last run
    compressed.push_back(count);
    compressed.push_back(current_byte);
    
    return compressed;
}

std::vector<uint8_t> MeldBinary::decompress(const std::vector<uint8_t>& compressed_data, size_t uncompressed_size) const {
    // Simple run-length decoding for demonstration
    // In a real implementation, this would use zstd decompression
    std::vector<uint8_t> decompressed;
    decompressed.reserve(uncompressed_size);
    
    for (size_t i = 0; i < compressed_data.size(); i += 2) {
        if (i + 1 < compressed_data.size()) {
            uint8_t count = compressed_data[i];
            uint8_t byte_value = compressed_data[i + 1];
            
            for (uint8_t j = 0; j < count; ++j) {
                decompressed.push_back(byte_value);
            }
        }
    }
    
    return decompressed;
}

} // namespace meld::api