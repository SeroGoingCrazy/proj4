#include "DijkstraPathRouter.h"
#include <queue>
#include <vector>
#include <limits>
#include <algorithm>
#include <chrono>


struct Edge {
    CPathRouter::TVertexID dest;
    double weight;
};

struct CDijkstraPathRouter::SImplementation {
    std::vector<std::any> vertices; // stores tag for each vertex. Index serves as vertex ID
    std::vector<std::vector<Edge>> adjacencyList; // adjacency list: for each vertex
};

CDijkstraPathRouter::CDijkstraPathRouter()
    : DImplementation(std::make_unique<SImplementation>()) {
}

CDijkstraPathRouter::~CDijkstraPathRouter() = default;

// Returns the number of vertices in the path router
std::size_t CDijkstraPathRouter::VertexCount() const noexcept {
    return DImplementation->vertices.size();
}

// Adds a vertex with the tag provided. The tag can be of any type
CPathRouter::TVertexID CDijkstraPathRouter::AddVertex(std::any tag) noexcept {
    DImplementation->vertices.push_back(tag);
    DImplementation->adjacencyList.push_back(std::vector<Edge>());
    return DImplementation->vertices.size() - 1;
}

// Retrieves the tag associated with a given vertex ID.
// If the vertex ID is invalid, returns a default std::any.
std::any CDijkstraPathRouter::GetVertexTag(TVertexID id) const noexcept {
    if (id < DImplementation->vertices.size()) {
        return DImplementation->vertices[id];
    }
    return std::any();
}

// Adds an edge from vertex src to vertex dest with the given weight.
// If bidir is true, also adds the reverse edge from dest to src.
// Returns false if src or dest is invalid or if weight is negative.
bool CDijkstraPathRouter::AddEdge(TVertexID src, TVertexID dest, double weight, bool bidir) noexcept {
    if (src >= DImplementation->vertices.size() || dest >= DImplementation->vertices.size() || weight < 0) {
        return false;
    }
    DImplementation->adjacencyList[src].push_back({dest, weight});
    if (bidir) {
        DImplementation->adjacencyList[dest].push_back({src, weight});
    }
    return true;
}

bool CDijkstraPathRouter::Precompute(std::chrono::steady_clock::time_point deadline) noexcept {
    return true;
}

// Returns the path distance of the path from src to dest, and fills out path 
// with vertices. If no path exists NoPathExists is returned. 
double CDijkstraPathRouter::FindShortestPath(TVertexID src, TVertexID dest, std::vector<TVertexID>& path) noexcept {
    std::size_t n = DImplementation->vertices.size();
    if (src >= n || dest >= n) {
        return CPathRouter::NoPathExists;
    }

    std::vector<double> dist(n, std::numeric_limits<double>::max());
    std::vector<TVertexID> prev(n, CPathRouter::InvalidVertexID);

    using Pair = std::pair<double, TVertexID>;
    std::priority_queue<Pair, std::vector<Pair>, std::greater<Pair>> pq; // make the priority a min-heap, i.e. samllest element on top 

    dist[src] = 0.0;
    pq.push({0.0, src});
    while (!pq.empty()) {
        auto [d, u] = pq.top(); // dist to the vertex, vertex 
        pq.pop();

        if (d > dist[u]) continue;

        if (u == dest) break;
        
        // push neighbors into the pq 
        for (const auto& edge : DImplementation->adjacencyList[u]) {
            TVertexID v = edge.dest;
            double alt = d + edge.weight;
            if (alt < dist[v]) {
                dist[v] = alt;
                prev[v] = u;
                pq.push({alt, v});
            }
        }
    }
    // If can't reachdestination , return NoPathExists.
    if (dist[dest] == std::numeric_limits<double>::max()) {
        return CPathRouter::NoPathExists;
    }
    
    // Reconstruct path using the prev
    for (TVertexID at = dest; at != CPathRouter::InvalidVertexID; at = prev[at]) {
        path.push_back(at);
    }
    // reverse to correct order 
    std::reverse(path.begin(), path.end());
    
    return dist[dest];
}