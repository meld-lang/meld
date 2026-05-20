#pragma once

#include "shadow_provenance.hpp"
#include "history_api.hpp"
#include <string>
#include <vector>
#include <optional>

namespace meld {
namespace history {

/**
 * Represents a blueprint evolution step
 */
struct BlueprintEvolutionStep {
    int stepNumber;
    std::string blueprint;
    Timestamp timestamp;
    std::string reason;
    
    BlueprintEvolutionStep() : stepNumber(0) {}
    BlueprintEvolutionStep(int step, std::string bp, Timestamp ts, std::string r)
        : stepNumber(step), blueprint(std::move(bp)), timestamp(ts), reason(std::move(r)) {}
};

/**
 * Tracks the relationship between blueprints and implementations
 */
class BlueprintTracker {
public:
    explicit BlueprintTracker(HistoryAPI& historyAPI);
    
    // Record initial blueprint
    bool recordInitialBlueprint(const std::string& nodeId,
                                const std::string& blueprint);
    
    // Record blueprint evolution
    bool recordBlueprintEvolution(const std::string& nodeId,
                                  const std::string& newBlueprint,
                                  const std::string& reason);
    
    // Link blueprint to implementation
    bool linkBlueprintToImplementation(const std::string& nodeId,
                                       const std::string& implementationCode);
    
    // Get blueprint evolution history
    std::vector<BlueprintEvolutionStep> getBlueprintEvolution(const std::string& nodeId);
    
    // Check if implementation matches blueprint
    bool implementationMatchesBlueprint(const std::string& nodeId,
                                        const std::string& implementationCode);
    
    // Get blueprint-implementation relationship
    struct BlueprintImplementationRelationship {
        std::string nodeId;
        std::string originalBlueprint;
        std::string currentBlueprint;
        std::string currentImplementation;
        bool matches;
        std::vector<std::string> deviations;
    };
    
    std::optional<BlueprintImplementationRelationship> 
        getBlueprintImplementationRelationship(const std::string& nodeId);
    
    // Track blueprint changes
    bool trackBlueprintChange(const std::string& nodeId,
                              const std::string& oldBlueprint,
                              const std::string& newBlueprint,
                              const std::string& reason);
    
    // Get all functions with blueprints
    std::vector<std::string> getAllFunctionsWithBlueprints();
    
    // Detect blueprint-implementation mismatches
    std::vector<std::string> detectMismatches();
    
private:
    HistoryAPI& historyAPI_;
    
    // Helper methods
    std::vector<std::string> extractBlueprintRequirements(const std::string& blueprint);
    std::vector<std::string> extractImplementationFeatures(const std::string& code);
    std::vector<std::string> findDeviations(const std::vector<std::string>& requirements,
                                            const std::vector<std::string>& features);
};

} // namespace history
} // namespace meld
