#include "TransportationPlanner.h"
#include "TransportationPlannerConfig.h"
#include "DijkstraTransportationPlanner.h"
#include "BusSystemIndexer.h"
#include <vector>
#include <queue>
#include <unordered_map>
#include <limits>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <memory>
#include "GeographicUtils.h"


struct CDijkstraTransportationPlanner::SImplementation {
    std::shared_ptr<SConfiguration> the_config;
    std::vector<std::shared_ptr<CStreetMap::SNode>> sortedNodes;
    std::unordered_map<TNodeID, std::size_t> nodeIndexMap;
    std::vector<std::vector<std::pair<std::size_t, double>>> graphDriving;
    std::vector<std::vector<std::pair<std::size_t, double>>> graphWalking;
    std::vector<std::vector<std::pair<std::size_t, double>>> graphBiking;
    std::unique_ptr<CBusSystemIndexer> busIndexer;

    SImplementation(std::shared_ptr<SConfiguration> config) : the_config(config) {
        buildGraphs();
        busIndexer = std::make_unique<CBusSystemIndexer>(the_config->BusSystem());
    }

    void buildGraphs() {
        auto streetMap = the_config->StreetMap();
        std::size_t nCount = streetMap->NodeCount();
        for (std::size_t i = 0; i < nCount; i++) {
            auto node = streetMap->NodeByIndex(i);
            sortedNodes.push_back(node);
        }
        std::sort(sortedNodes.begin(), sortedNodes.end(),
                  [](const auto &a, const auto &b) { return a->ID() < b->ID(); });
        for (std::size_t i = 0; i < sortedNodes.size(); i++)
            nodeIndexMap[sortedNodes[i]->ID()] = i;

        graphDriving.resize(sortedNodes.size());
        graphWalking.resize(sortedNodes.size());
        graphBiking.resize(sortedNodes.size());

        std::size_t wCount = streetMap->WayCount();
        for (std::size_t i = 0; i < wCount; i++) {
            auto way = streetMap->WayByIndex(i);
            std::size_t numNodes = way->NodeCount();
            bool oneWay = false;
            if (way->HasAttribute("oneway") && way->GetAttribute("oneway") == "yes")
                oneWay = true;
            bool bicycleAllowed = true;
            if (way->HasAttribute("bicycle") && way->GetAttribute("bicycle") == "no")
                bicycleAllowed = false;
            double effectiveSpeed = the_config->DefaultSpeedLimit();
            if (way->HasAttribute("maxspeed")) {
                try {
                    effectiveSpeed = std::stod(way->GetAttribute("maxspeed"));
                }
                catch (...) {
                }
            }
            for (std::size_t j = 0; j + 1 < numNodes; j++) {
                TNodeID id1 = way->GetNodeID(j);
                TNodeID id2 = way->GetNodeID(j + 1);
                if (nodeIndexMap.find(id1) == nodeIndexMap.end() ||
                    nodeIndexMap.find(id2) == nodeIndexMap.end())
                    continue;
                std::size_t idx1 = nodeIndexMap[id1];
                std::size_t idx2 = nodeIndexMap[id2];
                double dist = SGeographicUtils::HaversineDistanceInMiles(sortedNodes[idx1]->Location(), sortedNodes[idx2]->Location());
                graphWalking[idx1].push_back({idx2, dist / the_config->WalkSpeed()});
                graphWalking[idx2].push_back({idx1, dist / the_config->WalkSpeed()});
                if (oneWay)
                    graphDriving[idx1].push_back({idx2, dist / effectiveSpeed});
                else {
                    graphDriving[idx1].push_back({idx2, dist / effectiveSpeed});
                    graphDriving[idx2].push_back({idx1, dist / effectiveSpeed});
                }
                if (bicycleAllowed) {
                    if (oneWay)
                        graphBiking[idx1].push_back({idx2, dist / the_config->BikeSpeed()});
                    else {
                        graphBiking[idx1].push_back({idx2, dist / the_config->BikeSpeed()});
                        graphBiking[idx2].push_back({idx1, dist / the_config->BikeSpeed()});
                    }
                }
            }
        }
    }

    double dijkstraDriving(TNodeID srcID, TNodeID destID, std::vector<std::size_t> &pathIndices) {
        if (nodeIndexMap.find(srcID) == nodeIndexMap.end() ||
            nodeIndexMap.find(destID) == nodeIndexMap.end())
            return std::numeric_limits<double>::max();
        std::size_t src = nodeIndexMap[srcID];
        std::size_t dest = nodeIndexMap[destID];
        std::vector<double> dist(sortedNodes.size(), std::numeric_limits<double>::max());
        std::vector<int> prev(sortedNodes.size(), -1);
        using QueueItem = std::pair<double, std::size_t>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;
        dist[src] = 0.0;
        pq.push({0.0, src});
        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();
            if (d > dist[u])
                continue;
            if (u == dest)
                break;
            for (auto &edge : graphDriving[u]) {
                std::size_t v = edge.first;
                double weight = edge.second;
                if (dist[u] + weight < dist[v]) {
                    dist[v] = dist[u] + weight;
                    prev[v] = u;
                    pq.push({dist[v], v});
                }
            }
        }
        if (dist[dest] == std::numeric_limits<double>::max())
            return dist[dest];
        std::vector<std::size_t> revPath;
        for (int at = static_cast<int>(dest); at != -1; at = prev[at])
            revPath.push_back(at);
        std::reverse(revPath.begin(), revPath.end());
        pathIndices = revPath;
        return dist[dest];
    }

    std::size_t NodeCount() const noexcept {
        return sortedNodes.size();
    }

    std::shared_ptr<CStreetMap::SNode> SortedNodeByIndex(std::size_t index) const noexcept {
        if (index < sortedNodes.size())
            return sortedNodes[index];
        return nullptr;
    }

    double FindShortestPath(TNodeID src, TNodeID dest, std::vector<TNodeID> &path) {
        std::vector<std::size_t> indices;
        double cost = dijkstraDriving(src, dest, indices);
        if (cost < std::numeric_limits<double>::max()) {
            path.clear();
            for (auto idx : indices)
                path.push_back(sortedNodes[idx]->ID());
        }
        return cost;
    }

    double FindFastestPath(TNodeID src, TNodeID dest, std::vector<TTripStep> &tripPath) {
        enum class Mode {
            Walk,
            Bike
        };
        const int modeCount = 2;

        auto stateToIndex = [modeCount](std::size_t node, Mode m) {
            return node * modeCount + static_cast<int>(m);
        };

        std::size_t n = sortedNodes.size();
        std::vector<double> dist(n * modeCount, std::numeric_limits<double>::max());
        std::vector<int> prev(n * modeCount, -1);
        std::vector<int> prevEdgeType(n * modeCount, -1);

        using QueueItem = std::pair<double, int>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;

        if (nodeIndexMap.find(src) == nodeIndexMap.end() ||
            nodeIndexMap.find(dest) == nodeIndexMap.end())
            return std::numeric_limits<double>::max();
        int startState = stateToIndex(nodeIndexMap[src], Mode::Walk);
        dist[startState] = 0.0;
        pq.push({0.0, startState});

        while (!pq.empty()) {
            auto [curCost, curStateIdx] = pq.top();
            pq.pop();
            if (curCost > dist[curStateIdx])
                continue;
            std::size_t curNode = curStateIdx / modeCount;
            Mode curMode = static_cast<Mode>(curStateIdx % modeCount);

            if (sortedNodes[curNode]->ID() == dest) {
                std::vector<int> statePath;
                int cur = curStateIdx;
                while (cur != -1) {
                    statePath.push_back(cur);
                    cur = prev[cur];
                }
                std::reverse(statePath.begin(), statePath.end());
                tripPath.clear();
                for (int state : statePath) {
                    std::size_t nodeIdx = state / modeCount;
                    ETransportationMode mode;
                    if (prevEdgeType[state] == 1)
                        mode = ETransportationMode::Bus;
                    else if ((state % modeCount) == static_cast<int>(Mode::Bike))
                        mode = ETransportationMode::Bike;
                    else
                        mode = ETransportationMode::Walk;
                    tripPath.push_back({mode, sortedNodes[nodeIdx]->ID()});
                }
                return curCost;
            }

            if (curMode == Mode::Walk) {
                for (auto &edge : graphWalking[curNode]) {
                    int nextState = stateToIndex(edge.first, Mode::Walk);
                    double newCost = curCost + edge.second;
                    if (newCost < dist[nextState]) {
                        dist[nextState] = newCost;
                        prev[nextState] = curStateIdx;
                        prevEdgeType[nextState] = 0;
                        pq.push({newCost, nextState});
                    }
                }
            }
            else if (curMode == Mode::Bike) {
                for (auto &edge : graphBiking[curNode]) {
                    int nextState = stateToIndex(edge.first, Mode::Bike);
                    double newCost = curCost + edge.second;
                    if (newCost < dist[nextState]) {
                        dist[nextState] = newCost;
                        prev[nextState] = curStateIdx;
                        prevEdgeType[nextState] = 0;
                        pq.push({newCost, nextState});
                    }
                }
            }

            int otherState = stateToIndex(curNode, (curMode == Mode::Walk ? Mode::Bike : Mode::Walk));
            if (curCost < dist[otherState]) {
                dist[otherState] = curCost;
                prev[otherState] = curStateIdx;
                prevEdgeType[otherState] = 0;
                pq.push({curCost, otherState});
            }

            if (curMode == Mode::Walk) {
                auto busStop = busIndexer->StopByNodeID(sortedNodes[curNode]->ID());
                if (busStop) {
                    for (std::size_t i = 0; i < the_config->BusSystem()->RouteCount(); i++) {
                        auto route = the_config->BusSystem()->RouteByIndex(i);
                        std::size_t stopCount = route->StopCount();
                        int currentIndexInRoute = -1;
                        for (std::size_t j = 0; j < stopCount; j++) {
                            if (route->GetStopID(j) == busStop->ID()) {
                                currentIndexInRoute = static_cast<int>(j);
                                break;
                            }
                        }
                        if (currentIndexInRoute != -1 && currentIndexInRoute < static_cast<int>(stopCount - 1)) {
                            double bestRemainingDist = std::numeric_limits<double>::max();
                            std::size_t bestAlightNode = 0;
                            double bestBusTime = 0.0;
                            for (std::size_t j = currentIndexInRoute + 1; j < stopCount; j++) {
                                auto alightStop = the_config->BusSystem()->StopByIndex(j);
                                if (!alightStop)
                                    continue;
                                TNodeID alightNodeID = alightStop->NodeID();
                                if (nodeIndexMap.find(alightNodeID) == nodeIndexMap.end())
                                    continue;
                                std::size_t alightNodeIdx = nodeIndexMap[alightNodeID];
                                double routeDistance = 0.0;
                                for (std::size_t k = currentIndexInRoute; k < j; k++) {
                                    auto stopA = the_config->BusSystem()->StopByIndex(k);
                                    auto stopB = the_config->BusSystem()->StopByIndex(k + 1);
                                    if (!stopA || !stopB)
                                        continue;
                                    TNodeID nodeA = stopA->NodeID();
                                    TNodeID nodeB = stopB->NodeID();
                                    if (nodeIndexMap.find(nodeA) == nodeIndexMap.end() ||
                                        nodeIndexMap.find(nodeB) == nodeIndexMap.end())
                                        continue;
                                    auto locA = sortedNodes[nodeIndexMap[nodeA]]->Location();
                                    auto locB = sortedNodes[nodeIndexMap[nodeB]]->Location();
                                    routeDistance += SGeographicUtils::HaversineDistanceInMiles(locA, locB);
                                }
                                double busTime = routeDistance / the_config->DefaultSpeedLimit();
                                auto destLoc = sortedNodes[nodeIndexMap[dest]]->Location();
                                auto alightLoc = sortedNodes[alightNodeIdx]->Location();
                                double remainingDist = SGeographicUtils::HaversineDistanceInMiles(destLoc, alightLoc);
                                if (remainingDist < bestRemainingDist) {
                                    bestRemainingDist = remainingDist;
                                    bestAlightNode = alightNodeIdx;
                                    bestBusTime = busTime;
                                }
                            }
                            if (bestRemainingDist < std::numeric_limits<double>::max()) {
                                double totalBusCost = the_config->BusStopTime() + bestBusTime;
                                int nextState = stateToIndex(bestAlightNode, Mode::Walk);
                                double newCost = curCost + totalBusCost;
                                if (newCost < dist[nextState]) {
                                    dist[nextState] = newCost;
                                    prev[nextState] = curStateIdx;
                                    prevEdgeType[nextState] = 1;
                                    pq.push({newCost, nextState});
                                }
                            }
                        }
                    }
                }
            }
        }
        return std::numeric_limits<double>::max();
    }

    bool GetPathDescription(const std::vector<TTripStep> &path, std::vector<std::string> &desc) const {
        return true; // Placeholder, needs full implementation
    }
};

// Public interface implementations
CDijkstraTransportationPlanner::CDijkstraTransportationPlanner(std::shared_ptr<SConfiguration> config) {
    DImplementation = std::make_unique<SImplementation>(config);
}

CDijkstraTransportationPlanner::~CDijkstraTransportationPlanner() {
    // No explicit cleanup needed, unique_ptr handles it
}

std::size_t CDijkstraTransportationPlanner::NodeCount() const noexcept {
    return DImplementation->NodeCount();
}

std::shared_ptr<CStreetMap::SNode> CDijkstraTransportationPlanner::SortedNodeByIndex(std::size_t index) const noexcept {
    return DImplementation->SortedNodeByIndex(index);
}

double CDijkstraTransportationPlanner::FindShortestPath(TNodeID src, TNodeID dest, std::vector<TNodeID> &path) {
    return DImplementation->FindShortestPath(src, dest, path);
}

double CDijkstraTransportationPlanner::FindFastestPath(TNodeID src, TNodeID dest, std::vector<TTripStep> &path) {
    return DImplementation->FindFastestPath(src, dest, path);
}

bool CDijkstraTransportationPlanner::GetPathDescription(const std::vector<TTripStep> &path, std::vector<std::string> &desc) const {
    return DImplementation->GetPathDescription(path, desc);
}