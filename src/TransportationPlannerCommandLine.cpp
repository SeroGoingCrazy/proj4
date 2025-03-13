#include "TransportationPlannerCommandLine.h"
#include "FileDataFactory.h"
#include "StringDataSource.h"
#include "StringDataSink.h"
#include "GeographicUtils.h"
#include "KMLWriter.h"
#include "DSVWriter.h"
#include "DSVReader.h"
#include "FileDataSource.h"
#include "FileDataSink.h"
#include "StreetMap.h"
#include "BusSystem.h"
#include <sstream>
#include <string>
#include <limits>
#include <memory>
#include <chrono>

// Private implementation struct
struct CTransportationPlannerCommandLine::SImplementation {
    std::shared_ptr<CDataSource> cmdSource;
    std::shared_ptr<CDataSink> outSink;
    std::shared_ptr<CDataSink> errSink;
    std::shared_ptr<CDataFactory> resultFactory;
    std::shared_ptr<CTransportationPlanner> planner;
    std::vector<CTransportationPlanner::TTripStep> lastTripPath;  // Last fastest path
    std::vector<CTransportationPlanner::TNodeID> lastShortestPath;  // Last shortest path

    SImplementation(std::shared_ptr<CDataSource> src,
                    std::shared_ptr<CDataSink> out,
                    std::shared_ptr<CDataSink> err,
                    std::shared_ptr<CDataFactory> factory,
                    std::shared_ptr<CTransportationPlanner> plnr)
        : cmdSource(src), outSink(out), errSink(err), resultFactory(factory), planner(plnr) {
    }

    bool ReadLine(std::string& line) {
        line.clear();
        char ch;
        while (!cmdSource->End()) {
            if (!cmdSource->Get(ch)) break;
            if (ch == '\n') return true;
            line += ch;
        }
        return !line.empty();
    }

    bool WriteLine(std::shared_ptr<CDataSink> sink, const std::string& line) {
        for (char ch : line) {
            if (!sink->Put(ch)) return false;
        }
        return sink->Put('\n');
    }
    
    // Helper method to find a node by ID
    std::shared_ptr<CStreetMap::SNode> FindNodeByID(CStreetMap::TNodeID nodeID) {
        for (size_t i = 0; i < planner->NodeCount(); ++i) {
            auto node = planner->SortedNodeByIndex(i);
            if (node && node->ID() == nodeID) {
                return node;
            }
        }
        return nullptr;
    }

    bool SaveLastPathToFile(const std::string& filename) {
        if (lastTripPath.empty() && lastShortestPath.empty()) {
            WriteLine(errSink, "No path to save");
            return false;
        }

        // First create a CSV file with the path data
        auto csvSink = resultFactory->CreateSink(filename + ".csv");
        if (!csvSink) {
            WriteLine(errSink, "Failed to create file: " + filename + ".csv");
            return false;
        }

        auto csvWriter = std::make_shared<CDSVWriter>(csvSink, ',');
        
        // Write header
        csvWriter->WriteRow({"mode", "node_id"});
        
        // Write path data
        if (!lastTripPath.empty()) {
            // Write fastest path with modes
            for (const auto& step : lastTripPath) {
                std::string modeStr;
                switch (step.first) {
                    case CTransportationPlanner::ETransportationMode::Walk:
                        modeStr = "Walk";
                        break;
                    case CTransportationPlanner::ETransportationMode::Bike:
                        modeStr = "Bike";
                        break;
                    case CTransportationPlanner::ETransportationMode::Bus:
                        modeStr = "Bus";
                        break;
                }
                csvWriter->WriteRow({modeStr, std::to_string(step.second)});
            }
        } else if (!lastShortestPath.empty()) {
            // Write shortest path (all walking)
            for (const auto& nodeID : lastShortestPath) {
                csvWriter->WriteRow({"Walk", std::to_string(nodeID)});
            }
        }

        // Now create KML file
        std::string kmlFilename = filename + ".kml";
        auto kmlSink = resultFactory->CreateSink(kmlFilename);
        if (!kmlSink) {
            WriteLine(errSink, "Failed to create KML file: " + kmlFilename);
            return false;
        }

        // Get source and destination node info for KML name
        CStreetMap::TNodeID srcNodeID = 0;
        CStreetMap::TNodeID destNodeID = 0;
        
        if (!lastTripPath.empty()) {
            srcNodeID = lastTripPath.front().second;
            destNodeID = lastTripPath.back().second;
        } else if (!lastShortestPath.empty()) {
            srcNodeID = lastShortestPath.front();
            destNodeID = lastShortestPath.back();
        }

        std::string kmlName = std::to_string(srcNodeID) + " to " + std::to_string(destNodeID);
        std::string kmlDesc = !lastTripPath.empty() ? "Fastest path" : "Shortest path";

        // Colors for paths
        uint32_t walkColor = 0xff313131;   // gray
        uint32_t bikeColor = 0xffbe7443;   // orange/brown
        uint32_t busColor = 0xffa5a5a5;    // light gray
        uint32_t pointColor = 0xff8d5f24;  // brown
        int pathWidth = 4;

        // Create KML file
        CKMLWriter kmlWriter(kmlSink, kmlName, kmlDesc);
        
        // Create styles
        kmlWriter.CreatePointStyle("PointStyle", pointColor);
        kmlWriter.CreateLineStyle("WalkStyle", walkColor, pathWidth);
        kmlWriter.CreateLineStyle("BikeStyle", bikeColor, pathWidth);
        kmlWriter.CreateLineStyle("BusStyle", busColor, pathWidth);

        // Add points and paths
        if (!lastTripPath.empty()) {
            // Process multi-modal path
            std::vector<CStreetMap::TLocation> locationPath;
            std::string currentMode = "";
            
            for (size_t i = 0; i < lastTripPath.size(); ++i) {
                auto mode = lastTripPath[i].first;
                auto nodeID = lastTripPath[i].second;
                auto node = FindNodeByID(nodeID);
                
                if (!node) continue;

                std::string modeStr;
                switch (mode) {
                    case CTransportationPlanner::ETransportationMode::Walk:
                        modeStr = "Walk";
                        break;
                    case CTransportationPlanner::ETransportationMode::Bike:
                        modeStr = "Bike";
                        break;
                    case CTransportationPlanner::ETransportationMode::Bus:
                        modeStr = "Bus";
                        break;
                }

                if (currentMode != modeStr && !locationPath.empty()) {
                    // Write the previous segment
                    kmlWriter.CreatePath(currentMode, currentMode + "Style", locationPath);
                    locationPath.clear();
                }

                if (i == 0) {
                    // Add start point
                    std::string startDesc = "Start Point\nNode ID: " + std::to_string(nodeID) +
                                          "\nLatitude: " + std::to_string(node->Location().first) +
                                          "\nLongitude: " + std::to_string(node->Location().second);
                    kmlWriter.CreatePoint("Start Point", startDesc, "PointStyle", node->Location());
                } else if (i == lastTripPath.size() - 1) {
                    // Add end point
                    std::string endDesc = "End Point\nNode ID: " + std::to_string(nodeID) +
                                        "\nLatitude: " + std::to_string(node->Location().first) +
                                        "\nLongitude: " + std::to_string(node->Location().second);
                    kmlWriter.CreatePoint("End Point", endDesc, "PointStyle", node->Location());
                } else if (currentMode != modeStr) {
                    // Mode change point
                    std::string waypointDesc = modeStr + " Point\nNode ID: " + std::to_string(nodeID) +
                                           "\nLatitude: " + std::to_string(node->Location().first) +
                                           "\nLongitude: " + std::to_string(node->Location().second);
                    kmlWriter.CreatePoint(modeStr + " Point", waypointDesc, "PointStyle", node->Location());
                }

                currentMode = modeStr;
                locationPath.push_back(node->Location());
                
                // If it's the last item, write the segment
                if (i == lastTripPath.size() - 1 && !locationPath.empty()) {
                    kmlWriter.CreatePath(currentMode, currentMode + "Style", locationPath);
                }
            }
        } else if (!lastShortestPath.empty()) {
            // Handle walking-only path
            std::vector<CStreetMap::TLocation> pathPoints;
            
            for (size_t i = 0; i < lastShortestPath.size(); ++i) {
                auto nodeID = lastShortestPath[i];
                auto node = FindNodeByID(nodeID);
                
                if (!node) continue;
                
                if (i == 0) {
                    // Add start point
                    std::string startDesc = "Start Point\nNode ID: " + std::to_string(nodeID) +
                                          "\nLatitude: " + std::to_string(node->Location().first) +
                                          "\nLongitude: " + std::to_string(node->Location().second);
                    kmlWriter.CreatePoint("Start Point", startDesc, "PointStyle", node->Location());
                } else if (i == lastShortestPath.size() - 1) {
                    // Add end point
                    std::string endDesc = "End Point\nNode ID: " + std::to_string(nodeID) +
                                        "\nLatitude: " + std::to_string(node->Location().first) +
                                        "\nLongitude: " + std::to_string(node->Location().second);
                    kmlWriter.CreatePoint("End Point", endDesc, "PointStyle", node->Location());
                }
                
                pathPoints.push_back(node->Location());
            }
            
            if (!pathPoints.empty()) {
                kmlWriter.CreatePath("Walk", "WalkStyle", pathPoints);
            }
        }

        WriteLine(outSink, "Path saved to " + filename);
        return true;
    }

    bool ProcessCommands() {
        std::string line;
        while (ReadLine(line)) {
            std::istringstream iss(line);
            std::string command;
            iss >> command;

            if (command == "exit" || command == "quit") {
                return true;
            }
            else if (command == "help") {
                WriteLine(outSink, "help Display this help menu");
                WriteLine(outSink, "exit Exit the program");
                WriteLine(outSink, "count Output the number of nodes in the map");
                WriteLine(outSink, "node Syntax \"node [0, count)\"");
                WriteLine(outSink, "Will output node ID and Lat/Lon for node");
                WriteLine(outSink, "fastest Syntax \"fastest start end\"");
                WriteLine(outSink, "Calculates the time for fastest path from start to end");
                WriteLine(outSink, "shortest Syntax \"shortest start end\"");
                WriteLine(outSink, "Calculates the distance for the shortest path from start to end");
                WriteLine(outSink, "save Saves the last calculated path to file");
                WriteLine(outSink, "print Prints the steps for the last calculated path");
            }
            else if (command == "count") {
                std::ostringstream oss;
                oss << planner->NodeCount() << " nodes";
                WriteLine(outSink, oss.str());
            }
            else if (command == "node") {
                std::size_t index;
                if (iss >> index) {
                    if (index < planner->NodeCount()) {
                        auto node = planner->SortedNodeByIndex(index);
                        if (node) {
                            std::ostringstream oss;
                            auto loc = node->Location();
                            oss << "Node " << index << ": id = " << node->ID() << " is at "
                                << SGeographicUtils::ConvertLLToDMS(loc);
                            WriteLine(outSink, oss.str());
                        } else {
                            WriteLine(errSink, "Node not found at index " + std::to_string(index));
                        }
                    } else {
                        WriteLine(errSink, "Index out of range [0, " + std::to_string(planner->NodeCount()) + ")");
                    }
                } else {
                    WriteLine(errSink, "Usage: node [0, count)");
                }
            }
            else if (command == "shortest") {
                CTransportationPlanner::TNodeID src, dest;
                if (iss >> src >> dest) {
                    lastShortestPath.clear();
                    lastTripPath.clear();  // Clear fastest path too
                    double distance = planner->FindShortestPath(src, dest, lastShortestPath);
                    if (distance < std::numeric_limits<double>::max()) {
                        std::ostringstream oss;
                        oss << "Shortest path distance: " << distance << " miles";
                        WriteLine(outSink, oss.str());
                    } else {
                        WriteLine(errSink, "No path exists between " + std::to_string(src) + " and " + std::to_string(dest));
                    }
                } else {
                    WriteLine(errSink, "Usage: shortest start end");
                }
            }
            else if (command == "fastest") {
                CTransportationPlanner::TNodeID src, dest;
                if (iss >> src >> dest) {
                    lastTripPath.clear();
                    lastShortestPath.clear();  // Clear shortest path too
                    double time = planner->FindFastestPath(src, dest, lastTripPath);
                    if (time < std::numeric_limits<double>::max()) {
                        std::ostringstream oss;
                        oss << "Fastest path time: " << time << " hours";
                        WriteLine(outSink, oss.str());
                    } else {
                        WriteLine(errSink, "No path exists between " + std::to_string(src) + " and " + std::to_string(dest));
                    }
                } else {
                    WriteLine(errSink, "Usage: fastest start end");
                }
            }
            else if (command == "save") {
                std::string filename;
                if (iss >> filename) {
                    SaveLastPathToFile(filename);
                } else {
                    // If no filename provided, use default filename format
                    std::string defaultFilename;
                    if (!lastTripPath.empty()) {
                        defaultFilename = std::to_string(lastTripPath.front().second) + "_" 
                                        + std::to_string(lastTripPath.back().second);
                    } else if (!lastShortestPath.empty()) {
                        defaultFilename = std::to_string(lastShortestPath.front()) + "_" 
                                        + std::to_string(lastShortestPath.back());
                    } else {
                        WriteLine(errSink, "No path to save");
                        continue;
                    }
                    SaveLastPathToFile(defaultFilename);
                }
            }
            else if (command == "print") {
                if (!lastTripPath.empty()) {
                    std::vector<std::string> description;
                    if (planner->GetPathDescription(lastTripPath, description)) {
                        for (const auto& step : description) {
                            WriteLine(outSink, step);
                        }
                    } else {
                        WriteLine(errSink, "Failed to generate path description");
                    }
                } else if (!lastShortestPath.empty()) {
                    std::ostringstream oss;
                    oss << "Path: ";
                    for (size_t i = 0; i < lastShortestPath.size(); ++i) {
                        oss << lastShortestPath[i];
                        if (i < lastShortestPath.size() - 1) oss << " -> ";
                    }
                    WriteLine(outSink, oss.str());
                } else {
                    WriteLine(errSink, "No path computed yet to print");
                }
            }
            else {
                WriteLine(errSink, "Unknown command: " + command);
            }
        }
        return false;
    }
};

// Public interface implementations
CTransportationPlannerCommandLine::CTransportationPlannerCommandLine(
    std::shared_ptr<CDataSource> cmdsrc,
    std::shared_ptr<CDataSink> outsink,
    std::shared_ptr<CDataSink> errsink,
    std::shared_ptr<CDataFactory> results,
    std::shared_ptr<CTransportationPlanner> planner) {
    DImplementation = std::make_unique<SImplementation>(cmdsrc, outsink, errsink, results, planner);
}

CTransportationPlannerCommandLine::~CTransportationPlannerCommandLine() {
    // No explicit cleanup needed, unique_ptr handles it
}

bool CTransportationPlannerCommandLine::ProcessCommands() {
    return DImplementation->ProcessCommands();
}