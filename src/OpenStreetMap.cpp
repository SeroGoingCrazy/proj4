#include "OpenStreetMap.h"
#include "StreetMap.h"
#include "XMLReader.h"  // Needed for CXMLReader definition in constructor
#include <vector>
#include <map>
#include <string>
#include <stdexcept>

// Define the private implementation struct for PIMPL idiom
struct COpenStreetMap::SImplementation {
    struct Node:CStreetMap::SNode {
        TNodeID DId; // store node's ID
        TLocation DLocation; // stores lattitude/longtitude 
        std::vector<std::pair<std::string, std::string>> DAttributes; // {"highway", "road"}

        // constuctor
        Node(TNodeID id, TLocation loc, const std::vector<std::pair<std::string, std::string>>& attrs = {})
            : DId(id), DLocation(loc), DAttributes(attrs) {}

        // Returns the node's ID
        TNodeID ID() const noexcept override {
            return DId;
        }

        // Returns the node's location (lat, lon)
        TLocation Location() const noexcept override {
            return DLocation;
        }

        // Returns the number of attributes
        std::size_t AttributeCount() const noexcept override {
            return DAttributes.size();
        }

        // Returns the key at the given index, or "" if invalid
        std::string GetAttributeKey(std::size_t index) const noexcept override {
            if (index >= DAttributes.size()) {
                return "";
            }
            return DAttributes[index].first;
        }

        // Checks if the key exists in attributes
        bool HasAttribute(const std::string& key) const noexcept override {
            for (const auto& attr : DAttributes) {
                if (attr.first == key) {
                    return true;
                }
            }
            return false;
        }

        // Returns the value for the key, or "" if not found
        std::string GetAttribute(const std::string& key) const noexcept override {
            for (const auto& attr : DAttributes) {
                if (attr.first == key) {
                    return attr.second;
                }
            }
            return "";
        }
    };

    // Way
    struct Way:CStreetMap::SWay{
        TWayID DId;  // Way's unique ID
        std::vector<TNodeID> DNodeIds;  // List of node IDs in the way
        std::vector<std::pair<std::string, std::string>> DAttributes;  // Key-value tags

        // Constructor to initialize the way
        Way(TWayID id, const std::vector<TNodeID>& nodeIds, 
            const std::vector<std::pair<std::string, std::string>>& attrs = {})
            : DId(id), DNodeIds(nodeIds), DAttributes(attrs) {}

        // Returns the way's ID
        TWayID ID() const noexcept override {
            return DId;
        }

        // Returns the number of nodes in the way
        std::size_t NodeCount() const noexcept override {
            return DNodeIds.size();
        }

        // Returns the node ID at the given index, or InvalidNodeID if invalid
        TNodeID GetNodeID(std::size_t index) const noexcept override {
            if (index >= DNodeIds.size()) {
                return CStreetMap::InvalidNodeID;  // Defined as max uint64_t
            }
            return DNodeIds[index];
        }

        // Returns the number of attributes
        std::size_t AttributeCount() const noexcept override {
            return DAttributes.size();
        }

        // Returns the key at the given index, or "" if invalid
        std::string GetAttributeKey(std::size_t index) const noexcept override {
            if (index >= DAttributes.size()) {
                return "";
            }
            return DAttributes[index].first;
        }

        // Checks if the key exists in attributes
        bool HasAttribute(const std::string& key) const noexcept override {
            for (const auto& attr : DAttributes) {
                if (attr.first == key) {
                    return true;
                }
            }
            return false;
        }

        // Returns the value for the key, or "" if not found
        std::string GetAttribute(const std::string& key) const noexcept override {
            for (const auto& attr : DAttributes) {
                if (attr.first == key) {
                    return attr.second;
                }
            }
            return "";
        }   
    };

    // COpenStreetMap variables
    std::map<TNodeID, std::shared_ptr<Node>> DNodesById;
    std::map<TWayID, std::shared_ptr<Way>> DWaysById;
    std::vector<std::shared_ptr<Node>> DNodesOrdered;
    std::vector<std::shared_ptr<Way>> DWaysOrdered;
};

// Constructor: Initializes the OpenStreetMap from an XML source
COpenStreetMap::COpenStreetMap(std::shared_ptr<CXMLReader> src) {
    DImplementation = std::make_unique<SImplementation>();
    SXMLEntity entity;

    // process entities when ReadEnity() returns true 
    while (src->ReadEntity(entity, true)) {
        if (entity.DType == SXMLEntity::EType::StartElement) {
            // handle Node case
            if (entity.DNameData == "node") {
                TNodeID id = std::stoull(entity.AttributeValue("id"));
                double lat = std::stod(entity.AttributeValue("lat"));
                double lon = std::stod(entity.AttributeValue("lon"));
                std::vector<std::pair<std::string, std::string>> attrs;

                // read the Node Elements (tag)
                while (src->ReadEntity(entity, true) && 
                       !(entity.DType == SXMLEntity::EType::EndElement && entity.DNameData == "node")) {
                    if (entity.DType == SXMLEntity::EType::StartElement && entity.DNameData == "tag") {
                        attrs.emplace_back(entity.AttributeValue("k"), entity.AttributeValue("v"));
                    }
                }

                auto node = std::make_shared<SImplementation::Node>(id, TLocation{lat, lon}, attrs);
                DImplementation->DNodesById[id] = node;
                DImplementation->DNodesOrdered.push_back(node);
            } 
            // handle Way case
            else if (entity.DNameData == "way") {
                TWayID id = std::stoull(entity.AttributeValue("id"));
                std::vector<TNodeID> nodeIds;
                std::vector<std::pair<std::string, std::string>> attrs; 
                // read the Way elements
                while (src->ReadEntity(entity, true) && 
                       !(entity.DType == SXMLEntity::EType::EndElement && entity.DNameData == "way")) {
                    if (entity.DType == SXMLEntity::EType::StartElement) {
                        if (entity.DNameData == "nd") {
                            nodeIds.push_back(std::stoull(entity.AttributeValue("ref")));
                        } else if (entity.DNameData == "tag") {
                            attrs.emplace_back(entity.AttributeValue("k"), entity.AttributeValue("v"));
                        }
                    }
                }

                auto way = std::make_shared<SImplementation::Way>(id, nodeIds, attrs);
                DImplementation->DWaysById[id] = way;
                DImplementation->DWaysOrdered.push_back(way);
            }  
        }
    }
}


// Destructor: Cleans up resources used by the OpenStreetMap
COpenStreetMap::~COpenStreetMap() = default;

// Returns the total number of nodes stored in the map, stored in DNodesById
std::size_t COpenStreetMap::NodeCount() const noexcept {
    return DImplementation->DNodesById.size(); 
}

// Returns the total number of ways stored in the map, stored in DWaysById
std::size_t COpenStreetMap::WayCount() const noexcept {
    return DImplementation->DWaysById.size(); 
}

// Retrieves a shared pointer to the node at the specified index; returns nullptr if index is invalid
std::shared_ptr<CStreetMap::SNode> COpenStreetMap::NodeByIndex(std::size_t index) const noexcept {
    if (index >= DImplementation->DNodesOrdered.size()) {
        return nullptr;
    }
    return DImplementation->DNodesOrdered[index];
}

// Retrieves a shared pointer to the node with the specified ID; returns nullptr if ID doesn't exist
std::shared_ptr<CStreetMap::SNode> COpenStreetMap::NodeByID(TNodeID id) const noexcept {
    auto it = DImplementation->DNodesById.find(id);
    if (it != DImplementation->DNodesById.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}
// Retrieves a shared pointer to the way at the specified index; returns nullptr if index is invalid
std::shared_ptr<CStreetMap::SWay> COpenStreetMap::WayByIndex(std::size_t index) const noexcept {
    if (index >= DImplementation->DWaysOrdered.size()) {
        return nullptr;
    }
    return DImplementation->DWaysOrdered[index];
}

// Retrieves a shared pointer to the way with the specified ID; returns nullptr if ID doesn't exist
std::shared_ptr<CStreetMap::SWay> COpenStreetMap::WayByID(TWayID id) const noexcept {
    auto it = DImplementation->DWaysById.find(id);
    if (it != DImplementation->DWaysById.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}