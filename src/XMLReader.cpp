#include "XMLReader.h"
#include "XMLEntity.h"
#include "StringUtils.h"
#include <expat.h>
#include <memory>
#include <vector>
#include <stdexcept>

struct CXMLReader::SImplementation {
    std::shared_ptr<CDataSource> DSource;
    XML_Parser DParser;
    bool ifend;
    std::vector<SXMLEntity> enQue;
    std::string CharDataBuffer;

    SImplementation(std::shared_ptr<CDataSource> src) 
        : DSource(std::move(src)), ifend(false) {
        DParser = XML_ParserCreate(nullptr);
        if (!DParser) {
            throw std::runtime_error("Create XML parser failure");
        }
        XML_SetUserData(DParser, this);
        XML_SetElementHandler(DParser, StartElementHandler, EndElementHandler);
        XML_SetCharacterDataHandler(DParser, CharDataHandler);
    }

    ~SImplementation() {
        if (DParser) {
            XML_ParserFree(DParser);
        }
    }

    bool End() const {
        return ifend && enQue.empty();
    }

    bool ReadEntity(SXMLEntity &entity, bool skipdata) {
        // Parse more data if the queue is empty
        while (enQue.empty() && !ifend) {
            std::vector<char> Buffer(4096);
            if (!DSource->Read(Buffer, Buffer.size())) {
                ifend = true;
                if (XML_Parse(DParser, nullptr, 0, 1) == XML_STATUS_ERROR) {
                    throw std::runtime_error(XML_ErrorString(XML_GetErrorCode(DParser)));
                }
                FlushCharData(this);
                break;
            }
            
            size_t rsize = Buffer.size();
            if (XML_Parse(DParser, Buffer.data(), static_cast<int>(rsize), 0) == XML_STATUS_ERROR) {
                throw std::runtime_error(XML_ErrorString(XML_GetErrorCode(DParser)));
            }
            
            if (DSource->End()) {
                ifend = true;
            }
        }

        // Return the next entity (skip character data if requested)
        while (!enQue.empty()) {
            entity = std::move(enQue.front());
            enQue.erase(enQue.begin());
            
            if (skipdata && entity.DType == SXMLEntity::EType::CharData) {
                continue;
            }
            return true;
        }
        return false;
    }

    // Flush any accumulated character data into an entity.
    static void FlushCharData(SImplementation *impl) {
        if (!impl->CharDataBuffer.empty()) {
            std::string data = StringUtils::Strip(impl->CharDataBuffer);
            impl->CharDataBuffer.clear();
            
            if (!data.empty()) {
                SXMLEntity ent;
                ent.DType = SXMLEntity::EType::CharData;
                ent.DNameData = data;
                impl->enQue.push_back(std::move(ent));
            }
        }
    }

    // Callback for start element events.
    static void StartElementHandler(void *userData, const char *name, const char **attrs) {
        auto *impl = static_cast<SImplementation *>(userData);
        FlushCharData(impl);
        
        SXMLEntity ent;
        ent.DType = SXMLEntity::EType::StartElement;
        ent.DNameData = name;
        
        for (int i = 0; attrs && attrs[i]; i += 2) {
            ent.SetAttribute(attrs[i], attrs[i + 1]);
        }
        impl->enQue.push_back(std::move(ent));
    }

    // Callback for end element events.
    static void EndElementHandler(void *userData, const char *name) {
        auto *impl = static_cast<SImplementation *>(userData);
        FlushCharData(impl);
        
        SXMLEntity ent;
        ent.DType = SXMLEntity::EType::EndElement;
        ent.DNameData = name;
        impl->enQue.push_back(std::move(ent));
    }

    // Callback for character data events.
    static void CharDataHandler(void *userData, const char *text, int len) {
        auto *impl = static_cast<SImplementation *>(userData);
        impl->CharDataBuffer.append(text, len);
    }
};

CXMLReader::CXMLReader(std::shared_ptr<CDataSource> src) 
    : DImplementation(std::make_unique<SImplementation>(std::move(src))) {}

CXMLReader::~CXMLReader() = default;

bool CXMLReader::End() const {
    return DImplementation->End();
}

bool CXMLReader::ReadEntity(SXMLEntity &entity, bool skipdata) {
    return DImplementation->ReadEntity(entity, skipdata);
}