#include "DSVWriter.h"
#include "DataSink.h"
#include <vector>
#include <string>

struct CDSVWriter::SImplementation {
    std::shared_ptr<CDataSink> Sink;
    char Delimiter;
    bool QuoteAll;

    SImplementation(std::shared_ptr<CDataSink> dsink, char del, bool quoteAll)
        : Sink(std::move(dsink)), Delimiter(del), QuoteAll(quoteAll) {}
};

CDSVWriter::CDSVWriter(std::shared_ptr<CDataSink> sink, char delimiter, bool quoteall)
    : DImplementation(std::make_unique<SImplementation>(std::move(sink), delimiter, quoteall)) {}

CDSVWriter::~CDSVWriter() = default;

bool CDSVWriter::WriteRow(const std::vector<std::string> &buf) {
    if (!DImplementation->Sink) {
        return false;
    }

    std::string row;
    row.reserve(128); // (optional) Pre-allocate for efficiency

    char d = DImplementation->Delimiter;
    bool forceQuote = DImplementation->QuoteAll;

    for (size_t i = 0; i < buf.size(); i++) {
        const std::string &field = buf[i];

        //if this field needs quotes:
        bool needQuotes = forceQuote
                          || (field.find(d)  != std::string::npos)
                          || (field.find('"') != std::string::npos)
                          || (field.find('\n') != std::string::npos)
                          || (field.find('\r') != std::string::npos);

        if (needQuotes) {
            row.push_back('"');

            for (char ch : field) {
                if (ch == '"') {
                    row += "\"\"";
                } else {
                    row.push_back(ch);
                }
            }

            row.push_back('"');
        } else {
            row += field;
        }

        if (i < buf.size() - 1) {
            row.push_back(d);
        }
    }

    // End this row with a newline
    row.push_back('\n');

    // Write to sink
    std::vector<char> buffer(row.begin(), row.end());
    if (!DImplementation->Sink->Write(buffer)) {
        return false;
    }

    return true;
}