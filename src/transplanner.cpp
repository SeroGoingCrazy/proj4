#include "OpenStreetMap.h"
#include "CSVBusSystem.h"
#include "TransportationPlannerConfig.h"
#include "DijkstraTransportationPlanner.h"
#include "TransportationPlannerCommandLine.h"
#include "FileDataFactory.h"
#include "StandardDataSource.h"
#include "StandardDataSink.h"
#include "StandardErrorDataSink.h"
#include "DSVReader.h"
#include "XMLReader.h"
#include <iostream>
#include <string>
#include <memory>
#include <unordered_map>

void PrintUsage(const std::string& programName) {
    std::cerr << "Usage: " << programName << " street_map.osm stops.csv routes.csv" << std::endl;
    std::cerr << "  street_map.osm: OpenStreetMap XML file with street map data" << std::endl;
    std::cerr << "  stops.csv: CSV file with bus stop data" << std::endl;
    std::cerr << "  routes.csv: CSV file with bus route data" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 4) {
        PrintUsage(argv[0]);
        return 1;
    }

    std::string osmFilename = argv[1];
    std::string stopsFilename = argv[2];
    std::string routesFilename = argv[3];

    try {
        // Create the file data factory for input/output
        auto fileFactory = std::make_shared<CFileDataFactory>("./");
        
        // Load the street map
        auto osmSource = fileFactory->CreateSource(osmFilename);
        if (!osmSource) {
            std::cerr << "Failed to open OSM file: " << osmFilename << std::endl;
            return 1;
        }
        auto xmlReader = std::make_shared<CXMLReader>(osmSource);
        auto streetMap = std::make_shared<COpenStreetMap>(xmlReader);
        
        // Load the bus system data
        auto stopsSource = fileFactory->CreateSource(stopsFilename);
        auto routesSource = fileFactory->CreateSource(routesFilename);
        if (!stopsSource || !routesSource) {
            std::cerr << "Failed to open bus data files" << std::endl;
            return 1;
        }
        auto stopsReader = std::make_shared<CDSVReader>(stopsSource, ',');
        auto routesReader = std::make_shared<CDSVReader>(routesSource, ',');
        auto busSystem = std::make_shared<CCSVBusSystem>(stopsReader, routesReader);
        
        // Create transportation planner configuration
        // Default values:
        // - Walk speed: 3.0 mph
        // - Bike speed: 8.0 mph
        // - Default speed limit: 25.0 mph
        // - Bus stop time: 30.0 seconds (0.0083 hours)
        // - Precompute time: 30 seconds
        auto config = std::make_shared<STransportationPlannerConfig>(streetMap, busSystem);
        
        // Create transportation planner
        auto planner = std::make_shared<CDijkstraTransportationPlanner>(config);
        
        // Create I/O for command line
        auto cmdSource = std::make_shared<CStandardDataSource>();
        auto outSink = std::make_shared<CStandardDataSink>();
        auto errSink = std::make_shared<CStandardErrorDataSink>();
        
        // Create command line interface
        CTransportationPlannerCommandLine commandLine(cmdSource, outSink, errSink, fileFactory, planner);
        
        // Process commands
        return commandLine.ProcessCommands() ? 0 : 1;
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}