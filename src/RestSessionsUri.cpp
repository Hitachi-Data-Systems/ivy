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

#include "ivytypes.h"
#include "RestHandler.h"

extern ivy_engine m_s;

void
RestSessionsUri::handle_post(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    std::cout << request.method() << " : " << request.headers()["Cookie"] << std::endl;
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

    // if there is an active session then return "Locked"
#if 0
    if (rc == 0 && _session_state)
    {
        http_response response(status_codes::Locked);
        resultstr += "Can not establish new session, currently running: ";
        resultstr += _active_session_token;

        std::cout << request.method() << " : " << request.absolute_uri().path() <<
                                                               resultstr<< std::endl;
        make_response(response, resultstr, result);
        request.reply(response);

        return;
    }
#endif

    // Establish new session
    rapidjson::Value::MemberIterator api_token = document.FindMember("api_token");

    {
        std::unique_lock<std::mutex> u_lk(_session_mutex);

        _active_session_token = api_token->value.GetString();
        resultstr += _active_session_token;
        _session_state = true;
    }

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestSessionsUri::handle_get(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestSessionsUri::handle_put(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestSessionsUri::handle_patch(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestSessionsUri::handle_delete(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    {
        std::unique_lock<std::mutex> u_lk(_session_mutex);

        _active_session_token = "";
        _session_state = false;

        RestHandler::shutdown();
    }

    http_response response(status_codes::OK);
    std::string resultstr("Done");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}
