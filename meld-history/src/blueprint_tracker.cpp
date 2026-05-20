#include "meld/history/blueprint_tracker.hpp"
#include <sstream>
#include <algorithm>

namespace meld {
namespace history {

BlueprintTracker::BlueprintTracker(HistoryAPI& historyAPI)
    : historyAPI_(historyAPI) {}

bool BlueprintTracker::recordInitialBlueprint(const std::string& nodeId,
                                              const std::string& blueprint) {
    return historyAPI_.updateBlueprintHistory(nodeId, blueprint, blueprint);
}

bool BlueprintTracker::recordBlueprintEvolution(const std::string& nodeId,
                                                const std::string& newBlueprint,
                                                const std::string& reason) {
    auto history = historyAPI_.queryByNodeId(nodeId);
    if (!history) {
        return false;
    }
    
    std::string originalBlueprint = history->originalBlueprint.value_or("");
    
    // Update current blueprint
    return historyAPI_.updateBlueprintHistory(nodeId, originalBlueprint, newBlueprint);
}

bool BlueprintTracker::linkBlueprintToImplementation(const std::string& nodeId,
                                                     const std::string& implementationCode) {
    // This would typically store the implementation code in the history
    // For now, we'll just verify the link exists
    auto history = historyAPI_.queryByNodeId(nodeId);
    return history.has_value();
}

std::vector<BlueprintEvolutionStep> BlueprintTracker::getBlueprintEvolution(const std::string& nodeId) {
    std::vector<BlueprintEvolutionStep> steps;
    
    auto history = historyAPI_.queryByNodeId(nodeId);
    if (!history) {
        return steps;
    }
    
    // Add original blueprint as step 0
    if (history->originalBlueprint) {
        steps.emplace_back(0, *history->originalBlueprint, history->createdAt, "Initial blueprint");
    }
    
    // Add current blueprint if different
    if (history->currentBlueprint && 
        history->currentBlueprint != history->originalBlueprint) {
        steps.emplace_back(1, *history->currentBlueprint, history->updatedAt, "Blueprint updated");
    }
    
    return steps;
}

std::vector<std::string> BlueprintTracker::extractBlueprintRequirements(const std::string& blueprint) {
    std::vector<std::string> requirements;
    
    // Simple extraction: look for lines starting with "- " or "* "
    std::istringstream stream(blueprint);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.find("- ") == 0 || line.find("* ") == 0) {
            requirements.push_back(line.substr(2));
        }
    }
    
    return requirements;
}

std::vector<std::string> BlueprintTracker::extractImplementationFeatures(const std::string& code) {
    std::vector<std::string> features;
    
    // Simple extraction: look for function definitions, class definitions, etc.
    std::istringstream stream(code);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        
        if (line.find("fnc ") == 0 || line.find("class ") == 0 || 
            line.find("struct ") == 0 || line.find("trait ") == 0) {
            features.push_back(line);
        }
    }
    
    return features;
}

std::vector<std::string> BlueprintTracker::findDeviations(const std::vector<std::string>& requirements,
                                                          const std::vector<std::string>& features) {
    std::vector<std::string> deviations;
    
    // Check if each requirement is satisfied by some feature
    for (const auto& req : requirements) {
        bool found = false;
        for (const auto& feature : features) {
            // Simple substring match (in production, use more sophisticated matching)
            if (feature.find(req) != std::string::npos) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            deviations.push_back("Missing requirement: " + req);
        }
    }
    
    return deviations;
}

bool BlueprintTracker::implementationMatchesBlueprint(const std::string& nodeId,
                                                      const std::string& implementationCode) {
    auto history = historyAPI_.queryByNodeId(nodeId);
    if (!history || !history->currentBlueprint) {
        return false;
    }
    
    auto requirements = extractBlueprintRequirements(*history->currentBlueprint);
    auto features = extractImplementationFeatures(implementationCode);
    auto deviations = findDeviations(requirements, features);
    
    return deviations.empty();
}

std::optional<BlueprintTracker::BlueprintImplementationRelationship>
BlueprintTracker::getBlueprintImplementationRelationship(const std::string& nodeId) {
    auto history = historyAPI_.queryByNodeId(nodeId);
    if (!history) {
        return std::nullopt;
    }
    
    BlueprintImplementationRelationship relationship;
    relationship.nodeId = nodeId;
    relationship.originalBlueprint = history->originalBlueprint.value_or("");
    relationship.currentBlueprint = history->currentBlueprint.value_or("");
    
    // Get current implementation (from latest version or current code)
    if (!history->versions.empty()) {
        relationship.currentImplementation = history->versions.back().code;
    }
    
    // Check for deviations
    if (!relationship.currentBlueprint.empty() && !relationship.currentImplementation.empty()) {
        auto requirements = extractBlueprintRequirements(relationship.currentBlueprint);
        auto features = extractImplementationFeatures(relationship.currentImplementation);
        relationship.deviations = findDeviations(requirements, features);
        relationship.matches = relationship.deviations.empty();
    } else {
        relationship.matches = true;  // No blueprint or implementation to compare
    }
    
    return relationship;
}

bool BlueprintTracker::trackBlueprintChange(const std::string& nodeId,
                                            const std::string& oldBlueprint,
                                            const std::string& newBlueprint,
                                            const std::string& reason) {
    // Record the change as a refinement
    return historyAPI_.recordRefinement(nodeId, oldBlueprint, newBlueprint, reason);
}

std::vector<std::string> BlueprintTracker::getAllFunctionsWithBlueprints() {
    std::vector<std::string> functions;
    
    auto allEntries = historyAPI_.queryByFile("");
    
    for (const auto& entry : allEntries) {
        if (entry.originalBlueprint || entry.currentBlueprint) {
            functions.push_back(entry.functionName);
        }
    }
    
    return functions;
}

std::vector<std::string> BlueprintTracker::detectMismatches() {
    std::vector<std::string> mismatches;
    
    auto allEntries = historyAPI_.queryByFile("");
    
    for (const auto& entry : allEntries) {
        if (entry.currentBlueprint) {
            // Get current implementation
            std::string currentCode;
            if (!entry.versions.empty()) {
                currentCode = entry.versions.back().code;
            }
            
            if (!currentCode.empty()) {
                auto requirements = extractBlueprintRequirements(*entry.currentBlueprint);
                auto features = extractImplementationFeatures(currentCode);
                auto deviations = findDeviations(requirements, features);
                
                if (!deviations.empty()) {
                    mismatches.push_back(entry.functionName);
                }
            }
        }
    }
    
    return mismatches;
}

} // namespace history
} // namespace meld
