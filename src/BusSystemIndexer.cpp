#include "BusSystem.h"
#include "BusSystemIndexer.h"
#include <vector>
#include <unordered_map>
#include <algorithm>

struct CBusSystemIndexer::SImplementation {
    std::shared_ptr<CBusSystem> busSystem;
    std::vector<std::shared_ptr<CBusSystem::SStop>> sortedStops; // sort by stop
    std::vector<std::shared_ptr<CBusSystem::SRoute>> sortedRoutes; // sort by route name 
    std::unordered_map<CStreetMap::TNodeID, std::shared_ptr<CBusSystem::SStop>> nodeToStop; // Mapping {node ID: corresponding stop}

    SImplementation(std::shared_ptr<CBusSystem> bs) : busSystem(bs){
        std::size_t stopCount = busSystem->StopCount();
        sortedStops.reserve(stopCount);
        for (std::size_t i = 0; i < stopCount; i++) {   // populate sortedStops
            auto stop = busSystem->StopByIndex(i);
            if (stop) {
                sortedStops.push_back(stop);
            }
        }
        // sort stop by ID
        std::sort(sortedStops.begin(), sortedStops.end(), [](const auto &a, const auto &b) {
            return a->ID() < b->ID();
        });
        // build mapping 
        for (auto &stop : sortedStops) {
            nodeToStop[stop->NodeID()] =  stop;
        }
        
        std::size_t routeCount = busSystem->RouteCount();
        sortedStops.reserve(routeCount);
        for (std::size_t i = 0; i < routeCount; i++) {  // populate sortedRoutes
            auto route = busSystem->RouteByIndex(i);
            if (route) {
                sortedRoutes.push_back(route);
            }
        }
        // sort Routes by name 
        std::sort(sortedRoutes.begin(), sortedRoutes.end(), [](const auto &a){
            return a->Name() < b->Name();
        });
    }
};

CBusSystemIndexer::CBusSystemIndexer(std::shared_ptr<CBusSystem> bussystem): 
    DImplementation(std::make_unique<SImplementation>(bussystem)){}

CBusSystemIndexer::~CBusSystemIndexer() = default;

// Returns the number of stops in the CBusSystem being indexed 
std::size_t CBusSystemIndexer::StopCount() const noexcept{
    return DImplementation->sortedStops.size();
}

// Returns the number of routes in the CBusSystem being indexed
std::size_t CBusSystemIndexer::RouteCount() const noexcept {
    return DImplementation->sortedRoutes.size();
}

// Returns the SStop specified by the index where the stops are sorted by  
// their ID, nullptr is returned if index is greater than equal to  
// StopCount()
std::shared_ptr<CBusSystem::SStop> CBusSystemIndexer::SortedStopByIndex(std::size_t index) const noexcept{
    if (index < DImplementation->sortedStops.size()) {
        return DImplementation->sortedStops[index];
    }
    return nullptr;
}

// Returns the SRoute specified by the index where the routes are sorted by  
// their Name, nullptr is returned if index is greater than equal to  
// RouteCount()
std::shared_ptr<CBusSystem::SRoute> CBusSystemIndexer::SortedRouteByIndex(std::size_t index) const noexcept {
    if (index < DImplementation->sortedRoutes.size()) {
        return DImplementation->sortedRoutes[index];
    }
    return nullptr;
}

// Returns the SStop associated with the specified node ID, nullptr is  
// returned if no SStop associated with the node ID exists
std::shared_ptr<CBusSystem::SStop> CBusSystemIndexer::StopByNodeID(TNodeID id) const noexcept {
    auto it = DImplementation->nodeToStop.find(id);
    if (it != DImplementation->nodeToStop.end()) {
        return it->second;
    }
    return nullptr;
}

// Returns true if at least one route exists between the stops at the src and  
// dest node IDs. All routes that have a route segment between the stops at  
// the src and dest nodes will be placed in routes unordered set.
bool CBusSystemIndexer::RoutesByNodeIDs(TNodeID src, TNodeID dest, 
    std::unordered_set<std::shared_ptr<CBusSystem::SRoute>> &routes) const noexcept {
    routes.clear();
    auto srcStop = StopByNodeID(src);
    auto destStop = StopByNodeID(dest);

    // If either stop is not found, no routes exist
    if (!srcStop || !destStop) {
        return false;
    }

    auto srcID = srcStop->ID();
    auto destID = destStop->ID();

    for (const auto &route : DImplementation->sortedRoutes) {
        std::size_t count = route->StopCount();
        for (std::size_t i =0; i + 1 < count; ++i){
            auto curStopId = route->GetStopID(i);
            auto nextStopId = route->GetStopID(i+1);
            // if a route has segments that connects two stops, insert into the unordered list
            if((curStopId == srcID && nextStopId == destID) ||
                (curStopId == destID & nextStopId == srcID)){
                    routes.insert(route);
                    break;
                }
        }
    }
    return !routes.empty();
}

bool CBusSystemIndexer::RouteBetweenNodeIDs(TNodeID src, TNodeID dest) const noexcept {
    std::unordered_set<std::shared_ptr<CBusSystem::SRoute>> emptyRoutes;
    return RoutesByNodeIDs(src, dest, emptyRoutes);
}