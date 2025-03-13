#include "XMLWriter.h"
#include "XMLEntity.h"
#include "StringUtils.h"
#include <iostream>
#include <memory>
#include <vector>
#include <string>

struct CXMLWriter::SImplementation {
    std::shared_ptr<CDataSink> dsink;
    size_t IndentLevel = 0;

    struct PendingElement {
        SXMLEntity entity;
        bool flushed = false;      
        bool hasElementChild = false;
        bool hasTextChild = false;  
    };
    std::vector<PendingElement> pendingStack;

    SImplementation(std::shared_ptr<CDataSink> sink)
        : dsink(std::move(sink)) {}

    bool WriteRaw(const std::string &data) {
        std::vector<char> buffer(data.begin(), data.end());
        return dsink->Write(buffer);
    }

    static std::string EscapeXML(const std::string &input) {
        std::string output;
        output.reserve(input.size());
        for (char ch : input) {
            switch (ch) {
                case '&':  output.append("&amp;");  break;
                case '"':  output.append("&quot;"); break;
                case '\'': output.append("&apos;"); break;
                case '<':  output.append("&lt;");   break;
                case '>':  output.append("&gt;");   break;
                default:   output.push_back(ch);
            }
        }
        return output;
    }

    bool ShouldIndent() const {
        return false; // prevent extra indent
    }


    bool FlushPending() {
        if (!pendingStack.empty() && !pendingStack.back().flushed) {
            PendingElement &pe = pendingStack.back();
            std::string output;
            output += "<" + pe.entity.DNameData;
            for (const auto &attr : pe.entity.DAttributes) {
                output += " " + attr.first + "=\"" + EscapeXML(attr.second) + "\"";
            }
            output += ">";
            if (!WriteRaw(output)) {
                return false;
            }
            pe.flushed = true;
            IndentLevel++;
        }
        return true;
    }

    bool WriteStartElement(const SXMLEntity &entity) {
        if (!FlushPending()) {
            return false;
        }
        if (!pendingStack.empty()) {
            pendingStack.back().hasElementChild = true;
        }
        PendingElement pe;
        pe.entity = entity;
        pendingStack.push_back(pe);
        return true;
    }

    bool WriteCharData(const SXMLEntity &entity) {
        if (!FlushPending()) {
            return false;
        }
        if (!pendingStack.empty()) {
            pendingStack.back().hasTextChild = true;
        }
        std::string escaped = EscapeXML(entity.DNameData);
        return WriteRaw(escaped);
    }

    bool WriteCompleteElement(const SXMLEntity &entity) {
        if (!FlushPending()) {
            return false;
        }
        std::string output;
        if (!pendingStack.empty() && pendingStack.back().flushed && ShouldIndent()) {
            output += "\n" + std::string(IndentLevel, '\t');
        }
        output += "<" + entity.DNameData;
        for (const auto &attr : entity.DAttributes) {
            output += " " + attr.first + "=\"" + EscapeXML(attr.second) + "\"";
        }
        output += "/>";
        return WriteRaw(output);
    }

    bool WriteEndElement(const SXMLEntity &entity) {
        (void)entity;
        if (pendingStack.empty()) {
            return false;
        }
        PendingElement pe = pendingStack.back();
        pendingStack.pop_back();
        if (!pe.flushed) {
            std::string openTag;
            if (ShouldIndent()) {
                openTag += "\n" + std::string(IndentLevel, '\t');
            }
            openTag += "<" + pe.entity.DNameData;
            for (const auto &attr : pe.entity.DAttributes) {
                openTag += " " + attr.first + "=\"" + EscapeXML(attr.second) + "\"";
            }
            openTag += ">";
            if (!WriteRaw(openTag)) {
                return false;
            }
            pe.flushed = true;
            IndentLevel++;
            IndentLevel--;
        }
        std::string closeTag;
        if (pe.hasElementChild && ShouldIndent()) {
            closeTag += "\n" + std::string(IndentLevel, '\t');
        }
        closeTag += "</" + pe.entity.DNameData + ">";
        IndentLevel--;
        return WriteRaw(closeTag);
    }

    bool WriteEntity(const SXMLEntity &entity) {
        switch (entity.DType) {
            case SXMLEntity::EType::StartElement:
                return WriteStartElement(entity);
            case SXMLEntity::EType::EndElement:
                return WriteEndElement(entity);
            case SXMLEntity::EType::CharData:
                return WriteCharData(entity);
            case SXMLEntity::EType::CompleteElement:
                return WriteCompleteElement(entity);
            default:
                return false;
        }
    }

    bool Flush() {
        while (!pendingStack.empty()) {
            if (!WriteEndElement(SXMLEntity())) {
                return false;
            }
        }
        return true;
    }
};

CXMLWriter::CXMLWriter(std::shared_ptr<CDataSink> sink) :
    DImplementation(std::make_unique<SImplementation>(std::move(sink))) {}

CXMLWriter::~CXMLWriter() = default;

bool CXMLWriter::Flush() {
    return DImplementation->Flush();
}

bool CXMLWriter::WriteEntity(const SXMLEntity &entity) {
    return DImplementation->WriteEntity(entity);
}