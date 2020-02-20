//Copyright (c) 2016-2020 Hitachi Vantara Corporation
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in compliance with the License. You may obtain
//   a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
//   License for the specific language governing permissions and limitations
//   under the License.
//
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include "RestHandler.h"

extern ivy_engine m_s;

void
RestWorkloadsUri::handle_post(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    std::cout << request.method() << " : " << _listener.uri().path() << std::endl;

    //Routing to sub uri - Example: /v1/objects/servers/{id}/hbas
    //std::vector<std::string> baseuritokens;
    std::vector<std::string> suburitokens;
    std::istringstream  req_uri(request.absolute_uri().path());

    if (request.absolute_uri().path().size() != _listener.uri().path().size())
    {
        std::ostringstream o;
        std::string s;
        // tokenize and extract fields such as an id of an instance or objects under
        while(std::getline(req_uri, s, '/'))
            suburitokens.push_back(s);

        for (auto elem : suburitokens)
            o << elem << "+";

        std::cout << request.method() << " Sub URI : " << o.str() << std::endl;

    }

    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};
    http_response response(status_codes::OK);

    char json[1024];
    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    std::cout << "JSON:\n" << json << std::endl;

    rapidjson::Document document;
    if (document.Parse(json).HasParseError())
    {
        rc = 1;
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
        rc = 1;
        resultstr += get_schema_validation_error(&validator);
    }

    rapidjson::Value::MemberIterator workload = document.FindMember("name");
    rapidjson::Value::MemberIterator iosequencer = document.FindMember("iosequencer");
    rapidjson::Value::MemberIterator parameters = document.FindMember("parameters");
    rapidjson::Value::MemberIterator select = document.FindMember("select");
    std::ostringstream params;

    std::string select_str;
    if (select != document.MemberEnd())
    {
        const Value& select = document["select"];
        json_to_select_str(select, select_str);
    }

    if (parameters != document.MemberEnd() && parameters->value.IsString())
    {
        params << parameters->value.GetString();
    } else
    {
        rapidjson::Value::MemberIterator IOPS = document.FindMember("IOPS");
        rapidjson::Value::MemberIterator fractionRead = document.FindMember("fractionRead");
        rapidjson::Value::MemberIterator RangeStart = document.FindMember("RangeStart");
        rapidjson::Value::MemberIterator RangeEnd = document.FindMember("RangeEnd");
        rapidjson::Value::MemberIterator blocksize = document.FindMember("blocksize");
        rapidjson::Value::MemberIterator maxtags = document.FindMember("maxtags");

        if (fractionRead != document.MemberEnd())
            params << "fractionRead=" << (fractionRead->value.IsInt() ? fractionRead->value.GetInt() :
                                          fractionRead->value.GetDouble());
        if (RangeStart != document.MemberEnd())
            params << "RangeStart=" << (RangeStart->value.IsInt() ? RangeStart->value.GetInt() :
                                                         RangeStart->value.GetDouble());
        if (RangeEnd != document.MemberEnd())
            params << "RangeEnd=" << ((RangeEnd->value.IsInt() ? RangeEnd->value.GetInt() :
                                                        RangeEnd->value.GetDouble()));
        if (blocksize != document.MemberEnd())
            params << "%, blocksize=" << blocksize->value.GetInt();
        if (IOPS != document.MemberEnd())
        {
            params << "KiB, IOPS=" << IOPS->value.GetString();
        } else {
            // missing mandatory field
#if 0
            resultstr += "IOPS field is missing";
            make_response(response, resultstr, result);
            request.reply(response);
            return;
#endif
        }
        if (maxtags != document.MemberEnd())
            params << ", maxtags=" << maxtags->value.GetInt();
    }

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        m_s.createWorkload(workload->value.GetString(), select_str.c_str(), iosequencer->value.GetString(), params.str());
    }

    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestWorkloadsUri::handle_get(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestWorkloadsUri::handle_put(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    char json[1024];
    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    std::cout << "JSON:\n" << json << std::endl;

    rapidjson::Document document;
    if (document.Parse(json).HasParseError())
    {
        rc = 1;
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
        rc = 1;
        resultstr += get_schema_validation_error(&validator);
    }

    rapidjson::Value::MemberIterator iosequencer_template = document.FindMember("iosequencer");
    rapidjson::Value::MemberIterator parameters = document.FindMember("parameters");
    std::ostringstream params;

    rapidjson::Value::MemberIterator IOPS = document.FindMember("IOPS");
    rapidjson::Value::MemberIterator fractionRead = document.FindMember("fractionRead");
    rapidjson::Value::MemberIterator volfstart = document.FindMember("RangeStart");
    rapidjson::Value::MemberIterator volfend = document.FindMember("RangeEnd");
    rapidjson::Value::MemberIterator blocksize = document.FindMember("blocksize");
    rapidjson::Value::MemberIterator maxtags = document.FindMember("maxtags");
    http_response response(status_codes::OK);

    if (parameters != document.MemberEnd() && parameters->value.IsString())
    {
        params << parameters->value.GetString();
    } else
    {
        if (fractionRead != document.MemberEnd())
        {
            params << "fractionRead=" << (fractionRead->value.IsInt() ? fractionRead->value.GetInt() :
                                          fractionRead->value.GetDouble());
        }
        if (volfstart != document.MemberEnd())
        {
            params << "RangeStart=" << (volfstart->value.IsInt() ? volfstart->value.GetInt() :
                                                         volfstart->value.GetDouble());
        }
        if (volfend != document.MemberEnd())
        {
            params << "RangeEnd=" << ((volfend->value.IsInt() ? volfend->value.GetInt() :
                                                        volfend->value.GetDouble()));
        }
        if (blocksize != document.MemberEnd())
        {
            params << "%, blocksize=" << blocksize->value.GetInt();
        }
        if (IOPS != document.MemberEnd())
        {
            params << "KiB, IOPS=" << IOPS->value.GetString();
        } else {
            // missing mandatory field
            resultstr += "IOPS field is missing";
            make_response(response, resultstr, result);
            request.reply(response);
            return;
        }
        if (maxtags != document.MemberEnd())
        {
            params << ", maxtags=" << maxtags->value.GetInt();
        }
    }

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        m_s.set_iosequencer_template(iosequencer_template->value.GetString(), params.str());
    }

    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestWorkloadsUri::handle_patch(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestWorkloadsUri::handle_delete(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    char json[1024];
    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    std::cout << "JSON:\n" << json << std::endl;

    rapidjson::Document document;
    if (document.Parse(json).HasParseError())
    {
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
        resultstr += get_schema_validation_error(&validator);
    }

    rapidjson::Value::MemberIterator workload = document.FindMember("name");
    rapidjson::Value::MemberIterator select = document.FindMember("select");

    std::string select_str;
    if (select != document.MemberEnd())
    {
        const Value& select = document["select"];
        json_to_select_str(select, select_str);
    }

    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        result = m_s.deleteWorkload(workload->value.GetString(),
                                 select_str);
    }
    make_response(response, resultstr, result);
    request.reply(response);
}
