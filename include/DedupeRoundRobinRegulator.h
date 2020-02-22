//Copyright (c) 2020 Hitachi Vantara Corporation
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
//Authors: Stephen Morgan <stephen.morgan@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include "ivytypes.h"

class Eyeo;

class DedupeRoundRobinRegulator
{
    public:
    
		DedupeRoundRobinRegulator(uint64_t my_coverage_blocks, uint64_t my_block_size, ivy_float my_dedupe_ratio, uint64_t my_dedupe_unit_bytes, LUN *pLUN);
		virtual ~DedupeRoundRobinRegulator();
		uint64_t get_seed(Eyeo *p_eyeo, uint64_t offset);

    protected:

    private:
    
		uint64_t LUN_number;
};
