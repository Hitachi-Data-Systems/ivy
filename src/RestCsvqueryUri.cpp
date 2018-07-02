#include "RestHandler.h"
#include "ivy_engine.h"
#include "csvfile.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"  
#include "rapidjson/schema.h"  
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

extern ivy_engine m_s;

void
RestCsvqueryUri::handle_post(http_request request)
{
    std::string resultstr = "Not Supported";
    std::pair<bool, std::string> result {true, std::string()};

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestCsvqueryUri::handle_get(http_request request)
{
    std::cout << "GET /ivy_engine/csvquery\n";
    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    char json[1024];

    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    printf("JSON:\n %s\n", json);

    rapidjson::Document document; 
    if (document.Parse(json).HasParseError())
    {
printf("Kumaran - Invalid Document");
        rc = 1;
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
printf("Invalid Document - Schema validation failed");
        rc = 1;
        resultstr += get_schema_validation_error(&validator);
    }

    rapidjson::Value::MemberIterator filename = document.FindMember("filename");
    rapidjson::Value::MemberIterator row = document.FindMember("row");
    rapidjson::Value::MemberIterator col = document.FindMember("col");

    std::string val;
    // Execute Ivy Engine command
    if (rc == 0)
    {
printf("File name1: %s\n", filename->value.GetString());

        csvfile csv(filename->value.GetString());
printf("File name2: %s\n", filename->value.GetString());
        if (document["col"].IsInt())
        {
            printf("Col: %d\n", col->value.GetInt());
            val = csv.cell_value(row->value.GetInt(), col->value.GetInt());
        } else
        {
            printf("Col: %s\n", col->value.GetString());
            val = csv.cell_value(row->value.GetInt(), col->value.GetString());
        }
    }

    // return value in response  = { "csv_val" = val };
    resultstr = val;
    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestCsvqueryUri::handle_put(http_request request)
{
    std::pair<bool, std::string> result {true, std::string()};
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    make_response(response, resultstr, result); 
    request.reply(response);
}

void
RestCsvqueryUri::handle_patch(http_request request)
{
    std::pair<bool, std::string> result {true, std::string()};
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    make_response(response, resultstr, result); 
    request.reply(response);
}

void
RestCsvqueryUri::handle_delete(http_request request)
{
    std::pair<bool, std::string> result {true, std::string()};
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    make_response(response, resultstr, result); 
    request.reply(response);
}
