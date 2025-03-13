#include "DSVReader.h"
#include "DataSource.h"
#include <vector>
#include <string>

struct CDSVReader::SImplementation {
    std::shared_ptr<CDataSource> DataSource;
    char Delimiter;
    bool ifend;
    
    SImplementation(std::shared_ptr<CDataSource> src, char del)
        : DataSource(std::move(src)), Delimiter(del), ifend(false) {}
};

CDSVReader::CDSVReader(std::shared_ptr<CDataSource> src, char delimiter)
    : DImplementation(std::make_unique<SImplementation>(std::move(src), delimiter)) {}

CDSVReader::~CDSVReader() = default;

bool CDSVReader::End() const {
    return DImplementation->ifend;
}

bool CDSVReader::ReadRow(std::vector<std::string> &row){
    if (DImplementation->ifend) {
        return false;
    }

    row.clear();
    std::string cell;
    bool inQuotes = false;
    bool firstChar = true;

    while (true) {
        if (DImplementation->DataSource->End()) {
            DImplementation->ifend = true;
            if (!cell.empty() || !row.empty())
                row.push_back(cell);
            return !row.empty();
        }

        char ch;
        if (!DImplementation->DataSource->Get(ch)) {
            DImplementation->ifend = true;
            if (!cell.empty() || !row.empty())
                row.push_back(cell);
            return !row.empty();
        }

        if (firstChar) {
            if (ch == '"') {
                inQuotes = true;
                firstChar = false;
                continue;
            }
            firstChar = false;
        }

        if (inQuotes) {
            if (ch == '"') {
                char peekch;
                if (DImplementation->DataSource->Peek(peekch)) {
                    if (peekch == '"') {
                        cell.push_back('"');
                        DImplementation->DataSource->Get(ch);
                    } else {
                        inQuotes = false;
                    }
                } else {
                    inQuotes = false;
                }
            } else {
                cell.push_back(ch);
            }
        } else {
            if (ch == DImplementation->Delimiter) {
                row.push_back(cell);
                cell.clear();
                firstChar = true;
            } else if (ch == '\n' || ch == '\r') {
                row.push_back(cell);
                if (ch == '\r') {
                    char next;
                    if (DImplementation->DataSource->Peek(next) && next == '\n') {
                        DImplementation->DataSource->Get(next);
                    }
                }
                if (DImplementation->DataSource->End()) {
                    DImplementation->ifend = true;
                }
                return !row.empty();
            } else if (ch == '"'){ // two consecutive double quotes are changed into one 
                char peekch;
                if (DImplementation->DataSource->Peek(peekch)) {
                    if (peekch == '"') {
                        cell.push_back('"');
                        DImplementation->DataSource->Get(ch);
                    }
                }
            } else {
                cell.push_back(ch);
            }

    }
    }
}