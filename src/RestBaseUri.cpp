#include "RestHandler.h"
#include "ivy_engine.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"  
#include "rapidjson/schema.h"  
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

bool RestBaseUri::_schema_initialized = false;
rapidjson::SchemaDocument* RestBaseUri::_schema = nullptr;
std::mutex RestBaseUri::goStatementMutex;
std::map<std::string, ivy_engine*> _ivy_engine_instances;

std::string RestBaseUri::_active_session_token;
bool RestBaseUri::_session_state {false};
std::mutex RestBaseUri::_session_mutex;

void
RestBaseUri::initialize_ivy_schema()
{
    if (_schema_initialized)
        return;

    rapidjson::Document schema_doc;
    char buffer[4096];
    std::string schema_file {"/usr/local/bin/ivyschema.json"};
    FILE *fp = fopen(schema_file.c_str(), "r");
    if (!fp) {
        std::ostringstream err;
        err << "<Error> Schema file: " << schema_file << " not found" << std::endl;
        log(m_s.masterlogfile, err.str());
        throw std::runtime_error(err.str());
    }

    rapidjson::FileReadStream fs(fp, buffer, sizeof(buffer));
    schema_doc.ParseStream(fs);
    if (schema_doc.HasParseError()) {
        std::ostringstream err;
        err << "<Error> Schema file: " << schema_file << " is not a valid JSON." << std::endl;
        err << "<Error> Error(offset " << static_cast<unsigned>(schema_doc.GetErrorOffset()) << "): " << GetParseError_En(schema_doc.GetParseError()) << std::endl;
        log(m_s.masterlogfile, err.str());
        fclose(fp);
        throw std::runtime_error(err.str());
    }
    fclose(fp);

    _schema = new rapidjson::SchemaDocument(schema_doc);
    _schema_initialized = true;
}
