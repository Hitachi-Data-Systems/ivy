//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
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
#pragma once

// It was a surprise to me that std::getline() considers a line
// complete when the source blocks.  On a pipe or a socket connection
// you never know if something you send in one piece will arrive in
// multiple pieces.

#include "ivytypes.h"
#include "ivytime.h"
#include "ivydefines.h"
#include "ivydriver.h"

class pipe_line_reader
{
public:
// variables

    int fd {0};  // default is to read from stdin

    char buffer[sizeof(uint32_t) + IVYMAXMSGSIZE];

	int
		next_char_from_slave_buf {0},
		bytes_last_read_from_slave {0};

	bool last_read_from_pipe=true;

// methods

    pipe_line_reader(){};
    ~pipe_line_reader(){};

    void set_fd(int f) { fd = f; }

    std::string get_reassembled_line  // reassembles fragmented lines
    (
        ivytime timeout,
        std::string description // Used when there is some sort of failure to construct an error message written to the log file.
    );

    std::string real_get_line_from_pipe
    (
        ivytime timeout,
        std::string description // Used when there is some sort of failure to construct an error message written to the log file.
    );

    bool read_from_pipe(ivytime timeout);
			// Used by real_get_line_from_pipe() when there are no more bytes left in "from_slave_buf" to serve up.
};
