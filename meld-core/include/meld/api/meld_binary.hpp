#pragma once

/**
 * @file meld_binary.hpp
 * @brief MELD-B (Meld Binary) format for compact code representation
 * 
 * This header provides the MELD-B binary format that enables efficient AI context loading
 * with 10x+ compression compared to source text. The format stores complete ASG with
 * pre-computed indices for fast queries.
 */

#include "meld/api/semantic_graph.hpp"
#include "meld/api/semantic_graph_api.hpp"
#include "meld/api/holographic_view.hpp"
#include "meld/provenance/provenance.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>

namespace meld::api {

/**
 * @brief MELD-B file header structure
 */
struct MeldBinaryHeader {
    static constexpr uint32_t MAGIC_NUMBER = 0x4D4C4442; // "MLDB"
    static constexpr uint16_t VERSION_MAJOR = 2;
    static constexpr uint16_t VERSION_MINOR = 0; // Updated for v2.0 with flow definitions
    
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t compression_type; // 0 = none, 1 = zstd
    uint64_t uncompressed_size;
    uint64_t compressed_size;
    uint64_t symbol_table_offset;
    uint64_t type_table_offset;
    uint64_t cross_ref_table_offset;
    uint64_t effect_table_offset;
    uint64_t provenance_table_offset; // New: provenance metadata table
    uint64_t flow_table_offset; // New: flow definitions table
    uint64_t asg_data_offset;
    uint64_t metadata_offset;
    
    MeldBinaryHeader();
    bool isValid() const;
};

/**
 * @brief Symbol table entry for fast symbol lookup
 */
struct SymbolTableEntry {
    uint32_t symbol_id;
    uint32_t name_offset;
    uint32_t name_length;
    std::vector<NodeId> node_references;
    
    std::string getName(const std::vector<char>& string_pool) const;
};

/**
 * @brief Type table entry for type information
 */
struct TypeTableEntry {
    uint32_t type_id;
    uint32_t name_offset;
    uint32_t name_length;
    uint32_t definition_node_id;
    std::vector<NodeId> instance_references;
    
    std::string getName(const std::vector<char>& string_pool) const;
};

/**
 * @brief Cross-reference table entry for usage tracking
 */
struct CrossRefEntry {
    NodeId source_node;
    NodeId target_node;
    uint32_t reference_type; // 0 = usage, 1 = definition, 2 = modification
    uint32_t context_offset;
    uint32_t context_length;
    
    std::string getContext(const std::vector<char>& string_pool) const;
};

/**
 * @brief Effect table entry for effect tracking
 */
struct EffectTableEntry {
    NodeId function_node;
    std::vector<uint32_t> effect_name_offsets;
    std::vector<uint32_t> effect_name_lengths;
    
    std::vector<std::string> getEffectNames(const std::vector<char>& string_pool) const;
};

/**
 * @brief Provenance table entry for code origin tracking
 * 
 * Stores provenance metadata for AST nodes including origin type,
 * authorship information, and trust scores.
 */
struct ProvenanceTableEntry {
    NodeId node_id;
    uint8_t origin_type; // 0 = Human, 1 = Agent, 2 = Verified
    uint64_t creation_timestamp; // Unix timestamp in milliseconds
    double trust_score;
    double confidence_score; // For Agent origin
    
    // String pool offsets for optional fields
    uint32_t author_email_offset;
    uint32_t author_email_length;
    uint32_t commit_hash_offset;
    uint32_t commit_hash_length;
    uint32_t agent_model_offset;
    uint32_t agent_model_length;
    uint32_t blueprint_id_offset;
    uint32_t blueprint_id_length;
    uint32_t reviewer_email_offset;
    uint32_t reviewer_email_length;
    uint64_t verification_timestamp; // Unix timestamp in milliseconds, 0 if not verified
    
    // Helper methods to extract strings from string pool
    std::string getAuthorEmail(const std::vector<char>& string_pool) const;
    std::string getCommitHash(const std::vector<char>& string_pool) const;
    std::string getAgentModel(const std::vector<char>& string_pool) const;
    std::string getBlueprintId(const std::vector<char>& string_pool) const;
    std::string getReviewerEmail(const std::vector<char>& string_pool) const;
};

/**
 * @brief Flow transition entry for binary format
 * 
 * Represents a single transition in a flow state machine.
 */
struct FlowTransitionEntry {
    uint32_t event_name_offset;
    uint32_t event_name_length;
    uint32_t target_state_offset;
    uint32_t target_state_length;
    bool has_guard;
    uint32_t guard_condition_offset; // Offset to serialized guard expression
    uint32_t guard_condition_length;
    
    std::string getEventName(const std::vector<char>& string_pool) const;
    std::string getTargetState(const std::vector<char>& string_pool) const;
    std::string getGuardCondition(const std::vector<char>& string_pool) const;
};

/**
 * @brief Flow state entry for binary format
 * 
 * Represents a single state in a flow state machine.
 */
struct FlowStateEntry {
    uint32_t state_name_offset;
    uint32_t state_name_length;
    bool is_terminal;
    bool has_entry_action;
    bool has_exit_action;
    uint32_t entry_action_offset; // Offset to serialized entry action
    uint32_t entry_action_length;
    uint32_t exit_action_offset; // Offset to serialized exit action
    uint32_t exit_action_length;
    std::vector<FlowTransitionEntry> transitions;
    
    std::string getStateName(const std::vector<char>& string_pool) const;
    std::string getEntryAction(const std::vector<char>& string_pool) const;
    std::string getExitAction(const std::vector<char>& string_pool) const;
};

/**
 * @brief Flow table entry for flow definitions
 * 
 * Stores complete flow state machine definitions in binary format.
 */
struct FlowTableEntry {
    NodeId flow_node_id; // AST node ID for the flow definition
    uint32_t flow_name_offset;
    uint32_t flow_name_length;
    uint32_t initial_state_offset;
    uint32_t initial_state_length;
    std::vector<FlowStateEntry> states;
    
    std::string getFlowName(const std::vector<char>& string_pool) const;
    std::string getInitialState(const std::vector<char>& string_pool) const;
};

/**
 * @brief MELD-B binary context for efficient AI loading
 */
class MeldBinary {
public:
    MeldBinary() = default;
    
    // Loading and saving
    static std::unique_ptr<MeldBinary> load(const std::string& file_path);
    static std::unique_ptr<MeldBinary> fromSemanticGraph(std::shared_ptr<SemanticGraph> graph);
    static std::unique_ptr<MeldBinary> fromHologram(const ModuleHologram& hologram);
    
    void save(const std::string& file_path) const;
    
    // Query API (Requirement 40.7)
    std::vector<NodeId> query(const std::string& pattern) const;
    std::vector<NodeId> queryByType(NodeType type) const;
    std::vector<NodeId> queryByEffect(const std::string& effect_name) const;
    std::vector<UsageEdge> queryUsage(const std::string& symbol) const;
    
    // Provenance Query API (Requirement 45.12)
    std::vector<NodeId> queryByOrigin(uint8_t origin_type) const;
    std::vector<NodeId> queryByTrustLevel(double min_trust_level) const;
    std::vector<NodeId> queryByAuthor(const std::string& author_email) const;
    std::vector<NodeId> queryByAgentModel(const std::string& agent_model) const;
    std::vector<NodeId> queryVerifiedOnly() const;
    std::vector<ProvenanceTableEntry> getProvenanceForNodes(const std::vector<NodeId>& nodes) const;
    
    // Flow Query API (Requirement 40.2)
    std::vector<NodeId> queryFlowDefinitions() const;
    std::vector<FlowTableEntry> getFlowDefinitions() const;
    std::vector<NodeId> queryFlowsByName(const std::string& flow_name) const;
    std::vector<FlowStateEntry> getStatesForFlow(NodeId flow_node_id) const;
    std::vector<FlowTransitionEntry> getTransitionsForState(NodeId flow_node_id, const std::string& state_name) const;
    
    // Index access
    const std::vector<SymbolTableEntry>& getSymbolTable() const { return symbol_table_; }
    const std::vector<TypeTableEntry>& getTypeTable() const { return type_table_; }
    const std::vector<CrossRefEntry>& getCrossRefTable() const { return cross_ref_table_; }
    const std::vector<EffectTableEntry>& getEffectTable() const { return effect_table_; }
    const std::vector<ProvenanceTableEntry>& getProvenanceTable() const { return provenance_table_; }
    const std::vector<FlowTableEntry>& getFlowTable() const { return flow_table_; }
    const std::vector<char>& getStringPool() const { return string_pool_; }
    
    // ASG access
    std::shared_ptr<SemanticGraph> toSemanticGraph() const;
    
    // Statistics
    size_t getOriginalSize() const { return original_size_; }
    size_t getCompressedSize() const { return compressed_size_; }
    double getCompressionRatio() const;
    
    // Validation
    bool validate() const;
    
private:
    MeldBinaryHeader header_;
    std::vector<SymbolTableEntry> symbol_table_;
    std::vector<TypeTableEntry> type_table_;
    std::vector<CrossRefEntry> cross_ref_table_;
    std::vector<EffectTableEntry> effect_table_;
    std::vector<ProvenanceTableEntry> provenance_table_;
    std::vector<FlowTableEntry> flow_table_;
    std::vector<char> string_pool_;
    std::vector<uint8_t> asg_data_;
    std::vector<uint8_t> metadata_;
    size_t original_size_ = 0;
    size_t compressed_size_ = 0;
    
    // Serialization helpers
    void serializeHeader(std::vector<uint8_t>& buffer) const;
    void serializeSymbolTable(std::vector<uint8_t>& buffer) const;
    void serializeTypeTable(std::vector<uint8_t>& buffer) const;
    void serializeCrossRefTable(std::vector<uint8_t>& buffer) const;
    void serializeEffectTable(std::vector<uint8_t>& buffer) const;
    void serializeProvenanceTable(std::vector<uint8_t>& buffer) const;
    void serializeFlowTable(std::vector<uint8_t>& buffer) const;
    void serializeStringPool(std::vector<uint8_t>& buffer) const;
    void serializeASGData(std::vector<uint8_t>& buffer) const;
    void serializeMetadata(std::vector<uint8_t>& buffer) const;
    
    // Deserialization helpers
    void deserializeHeader(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeSymbolTable(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeTypeTable(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeCrossRefTable(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeEffectTable(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeProvenanceTable(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeFlowTable(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeStringPool(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeASGData(const std::vector<uint8_t>& buffer, size_t& offset);
    void deserializeMetadata(const std::vector<uint8_t>& buffer, size_t& offset);
    
    // Index building
    void buildIndicesFromSemanticGraph(std::shared_ptr<SemanticGraph> graph);
    void buildIndicesFromHologram(const ModuleHologram& hologram);
    
    // String pool management
    uint32_t addToStringPool(const std::string& str);
    std::string getFromStringPool(uint32_t offset, uint32_t length) const;
    
    // Compression
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed_data, size_t uncompressed_size) const;
};

/**
 * @brief Convenience functions for MELD-B operations
 */

// Create MELD-B from source code
inline std::unique_ptr<MeldBinary> compileToBinary(const std::string& source_code, 
                                                  const std::string& file_path = "") {
    auto graph = SemanticGraphAPI::parse(source_code, file_path);
    return MeldBinary::fromSemanticGraph(std::shared_ptr<SemanticGraph>(std::move(graph)));
}

// Create MELD-B from file
inline std::unique_ptr<MeldBinary> compileFileToBinary(const std::string& file_path) {
    auto graph = SemanticGraphAPI::parseFile(file_path);
    return MeldBinary::fromSemanticGraph(std::shared_ptr<SemanticGraph>(std::move(graph)));
}

// Create MELD-B from hologram
inline std::unique_ptr<MeldBinary> hologramToBinary(const ModuleHologram& hologram) {
    return MeldBinary::fromHologram(hologram);
}

} // namespace meld::api