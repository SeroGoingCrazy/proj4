#include "BusSystem.h"
#include "CSVBusSystem.h"
#include "DSVReader.h"
#include "StreetMap.h"
#include <vector>
#include <map>

struct CCSVBusSystem:: SImplementation {
    // stop {stop_id,node_id 22043,2849810514}
    struct Stop: CBusSystem::SStop{
        TStopID DStopId;
        CStreetMap::TNodeID DNodeId; // stop occur in mutiple routes 

        Stop(TStopID stopId, CStreetMap::TNodeID nodeId)
            : DStopId(stopId), DNodeId(nodeId) {}
        
        // return stop_id
        TStopID ID() const noexcept override {
            return DStopId;
        }

        // return node_id
        CStreetMap::TNodeID NodeID() const noexcept override {
            return DNodeId;
        }
    };
    // route {route,stop_id}
    struct Route: CBusSystem::SRoute{
        std::string DRouteName;
        std::vector<TStopID> DStopIds;

        Route(const std::string& name, const std::vector<TStopID>& stopIds) 
            : DRouteName(name), DStopIds(stopIds) {}

        // return route name
        std::string Name() const noexcept override {
            return DRouteName;
        }

        // count stops in the route
        std::size_t StopCount() const noexcept override {
            return DStopIds.size();
        }

        // get stop in the route using specific index
        TStopID GetStopID(std::size_t index) const noexcept override {
            if (index >= DStopIds.size()) {
                return CBusSystem::InvalidStopID;  // max uint64_t
            }
            return DStopIds[index];
        }      
    };
    // CSVBusSystem Variables 
    std::map<TStopID, std::shared_ptr<Stop>> DStopsById;
    std::vector<std::shared_ptr<Stop>> DStopsOrdered;
    std::map<std::string, std::shared_ptr<Route>> DRoutesByName;
    std::vector<std::shared_ptr<Route>> DRoutesOrdered;
};

CCSVBusSystem::CCSVBusSystem(std::shared_ptr<CDSVReader> stopsrc, std::shared_ptr<CDSVReader> routesrc) {
    DImplementation = std::make_unique<SImplementation>();
    std::vector<std::string> row;

    // parse stop CSV, expect stop_id, node_id
    while (stopsrc->ReadRow(row)) {
        if (row.size() >= 2) {
            try {
                TStopID stopId = std::stoull(row[0]);
                CStreetMap::TNodeID nodeId = std::stoull(row[1]);
                auto stop = std::make_shared<SImplementation::Stop>(stopId, nodeId);
                DImplementation->DStopsById[stopId] = stop;
                DImplementation->DStopsOrdered.push_back(stop);
            } catch (const std::invalid_argument& e) {
                // Skip this row for header and invalid input
                continue;
            }
        }
    }

    // parse rout csvm expect rout_name, stop_id
    std::map<std::string, std::vector<TStopID>> stop_per_route;
    while (routesrc->ReadRow(row)) {
        if (row.size() >= 2) {
            try {
                std::string routeName = row[0];
                TStopID stopId = std::stoull(row[1]); 
                stop_per_route[routeName].push_back(stopId);
            }
            catch (const std::invalid_argument& e) { 
                continue; 
            }

        }
    }

    // create route objects via stop_per_route
    for (const auto& [name, stopIds] : stop_per_route) {
        auto route = std::make_shared<SImplementation::Route>(name, stopIds);
        DImplementation->DRoutesByName[name] = route;
        DImplementation->DRoutesOrdered.push_back(route);
    }
}

CCSVBusSystem::~CCSVBusSystem() = default;

// return the numbers of stops inside the DstopById
std::size_t CCSVBusSystem::StopCount() const noexcept {
    return DImplementation->DStopsById.size();
}

// return the numbers of routes inside the DRoutesByName
std::size_t CCSVBusSystem::RouteCount() const noexcept {
    return DImplementation->DRoutesByName.size();
}

// Returns the SStop specified by the index, nullptr is returned if index is
// greater than equal to StopCount()
std::shared_ptr<CBusSystem::SStop> CCSVBusSystem::StopByIndex(std::size_t index) const noexcept {
    if (index >= DImplementation->DStopsOrdered.size()) {
        return nullptr;
    }
    return DImplementation->DStopsOrdered[index];
}

// Returns the SStop specified by the stop id, nullptr is returned if id is
// not in the stops
std::shared_ptr<CBusSystem::SStop> CCSVBusSystem::StopByID(TStopID id) const noexcept {
    auto it = DImplementation->DStopsById.find(id);
    if (it != DImplementation->DStopsById.end()) {
        return it->second;
    }
    else {
        return nullptr;
    }
}

// Returns the SRoute specified by the index, nullptr is returned if index is
// greater than equal to RouteCount()
std::shared_ptr<CBusSystem::SRoute> CCSVBusSystem::RouteByIndex(std::size_t index) const noexcept {
    if (index >= DImplementation->DRoutesOrdered.size()) {
        return nullptr;
    }
    return DImplementation->DRoutesOrdered[index];
}

// Returns the SRoute specified by the name, nullptr is returned if name is
// not in the routes
std::shared_ptr<CBusSystem::SRoute> CCSVBusSystem::RouteByName(const std::string &name) const noexcept {
    auto it = DImplementation->DRoutesByName.find(name);
        if (it != DImplementation->DRoutesByName.end()) {
        return it->second;
    }
    else {
        return nullptr;
    }
}