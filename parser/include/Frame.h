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

#include "Arg.h"

#include "Block.h"

class Frame {

public:
	std::list<Arg> *p_arglist;
	size_t offset_after_args {0u};
	Block* p_code_Block {nullptr};
	unsigned int stack_offset {0u}; // offset from "stack"


	Frame(std::list<Arg>* p_args): p_arglist(p_args){};

	~Frame() { if (p_arglist) delete p_arglist; if (p_code_Block) delete p_code_Block; }

	void invoke(unsigned int /*frame_offset*/);
	void start_Block();
	const Type& genre() const { return p_arglist->front().arg_type; }
	const std::string& name() const { return p_arglist->front().arg_ID; }
};

std::ostream& operator<<(std::ostream&, const Frame&);
