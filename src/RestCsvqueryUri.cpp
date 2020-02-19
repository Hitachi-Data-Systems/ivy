#include "RestHandler.h"
#include "csvfile.h"

extern ivy_engine m_s;

void
RestCsvqueryUri::handle_post(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    std::string resultstr = "Not Supported";
    std::pair<bool, std::string> result {true, std::string()};

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestCsvqueryUri::handle_get(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    int rc = 0;
    std::string resultstr;
    std::string val;
    std::pair<bool, std::string> result {true, std::string()};
    bool get_rows {false};
    bool get_header_columns {false};
    bool get_columns_in_row {false};

    uri req_uri = request.absolute_uri();

    std::map<utility::string_t, utility::string_t>  qmap = req_uri.split_query(req_uri.query());

    for (auto& pear : qmap)
    {
        std::cout << "key: " << pear.first << ", value: " << pear.second << std::endl;
        if (pear.first == "rows" && pear.second == "true")
            get_rows = true;
        else if (pear.first == "header_columns" && pear.second == "true")
            get_header_columns = true;
        else if (pear.first == "columns_in_row" && pear.second == "true")
            get_columns_in_row = true;
    }

    if (get_rows || get_header_columns || get_columns_in_row)
    {
        csvfile csv(qmap["file_name"]);

        if (get_rows)
        {
            int row_count = csv.rows();
            val = std::to_string(row_count);
        }

	if (get_header_columns)
        {
            int header_columns = csv.header_columns();
            val = std::to_string(header_columns);
        }

	if (get_columns_in_row)
        {
            int columns_in_row = csv.columns_in_row(std::stoi(qmap["row"]));
            val = std::to_string(columns_in_row);
        }

        http_response response(status_codes::OK);
        make_response(response, val, result);
        request.reply(response);

        return;
    }

    char json_buf[1024];

    // extract json from request
    snprintf(json_buf, sizeof(json_buf), "%s", request.extract_string(true).get().c_str());

    std::cout << "JSON:\n" << json_buf << std::endl;

    rapidjson::Document document; 
    if (document.Parse(json_buf).HasParseError())
    {
        rc = 1;
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
        rc = 1;
        resultstr += get_schema_validation_error(&validator);
    }

    rapidjson::Value::MemberIterator filename = document.FindMember("filename");
    rapidjson::Value::MemberIterator row = document.FindMember("row");
    rapidjson::Value::MemberIterator col = document.FindMember("col");

    // Execute Ivy Engine command
    if (rc == 0)
    {
        csvfile csv(filename->value.GetString());
        if (document["col"].IsInt())
        {
            std::cout << "Col: " << col->value.GetInt() << std::endl;
            val = csv.cell_value(row->value.GetInt(), col->value.GetInt());
        } else
        {
            std::cout << "Col: " << col->value.GetString() << std::endl;
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
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    std::pair<bool, std::string> result {true, std::string()};
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    make_response(response, resultstr, result); 
    request.reply(response);
}

void
RestCsvqueryUri::handle_patch(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    std::pair<bool, std::string> result {true, std::string()};
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    make_response(response, resultstr, result); 
    request.reply(response);
}

void
RestCsvqueryUri::handle_delete(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    std::pair<bool, std::string> result {true, std::string()};
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    make_response(response, resultstr, result); 
    request.reply(response);
}
