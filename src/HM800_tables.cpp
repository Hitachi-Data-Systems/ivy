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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#include "HM800_tables.h"



std::map
<
    std::string, // sub_model HM800S / HM800M / HM800H
    std::map
    <
        std::string, // MPU "0", "1", "2", "3"
        std::vector
        <
            std::pair
            <
                std::string, // MPU#    like MP#00
                std::string  // MP_core like MP10-00
            >
        >
    >
>

HM800_MPU_map
{
	{
        "HM800S",
        {
            {
                "0",
                {
                    { "MP#00", "MP10-00" },
                    { "MP#01", "MP10-01" }
                }
            },

            {
                "1",
                {
                    { "MP#02", "MP11-00" },
                    { "MP#03", "MP11-01" }
                }
            },

            {
                "2",
                {
                    { "MP#04", "MP20-00" },
                    { "MP#05", "MP20-01" }
                }
            },

            {
                "3",
                {
                    { "MP#06", "MP21-00" },
                    { "MP#07", "MP21-01" }
                }
            }
        }

    },
	{
        "HM800M",
        {
            {
                "0",
                {
                    { "MP#00", "MP10-00" },
                    { "MP#01", "MP10-01" },
                    { "MP#02", "MP10-02" },
                    { "MP#03", "MP10-03" }
                }
            },

            {
                "1",
                {
                    { "MP#04", "MP11-00" },
                    { "MP#05", "MP11-01" },
                    { "MP#06", "MP11-02" },
                    { "MP#07", "MP11-03" }
                }
            },

            {
                "2",
                {
                    { "MP#08", "MP20-00" },
                    { "MP#09", "MP20-01" },
                    { "MP#0A", "MP20-02" },
                    { "MP#0B", "MP20-03" }
                }
            },

            {
                "3",
                {
                    { "MP#0C", "MP21-00" },
                    { "MP#0D", "MP21-01" },
                    { "MP#0E", "MP21-02" },
                    { "MP#0F", "MP21-03" }
                }
            }
        }
    }
    ,
   	{
        "HM800H",
        {
            {
                "0",
                {
                    { "MP#00", "MP10-00" },
                    { "MP#01", "MP10-01" },
                    { "MP#02", "MP10-02" },
                    { "MP#03", "MP10-03" },
                    { "MP#04", "MP10-04" },
                    { "MP#05", "MP10-05" },
                    { "MP#06", "MP10-06" },
                    { "MP#07", "MP10-07" }
                }
            },

            {
                "1",
                {
                    { "MP#08", "MP11-00" },
                    { "MP#09", "MP11-01" },
                    { "MP#0A", "MP11-02" },
                    { "MP#0B", "MP11-03" },
                    { "MP#0C", "MP11-04" },
                    { "MP#0D", "MP11-05" },
                    { "MP#0E", "MP11-06" },
                    { "MP#0F", "MP11-07" }
                }
            },

            {
                "2",
                {
                    { "MP#10", "MP20-00" },
                    { "MP#11", "MP20-01" },
                    { "MP#12", "MP20-02" },
                    { "MP#13", "MP20-03" },
                    { "MP#14", "MP20-04" },
                    { "MP#15", "MP20-05" },
                    { "MP#16", "MP20-06" },
                    { "MP#17", "MP20-07" }
                }
            },

            {
                "3",
                {
                    { "MP#18", "MP21-00" },
                    { "MP#19", "MP21-01" },
                    { "MP#1A", "MP21-02" },
                    { "MP#1B", "MP21-03" },
                    { "MP#1C", "MP21-04" },
                    { "MP#1D", "MP21-05" },
                    { "MP#1E", "MP21-06" },
                    { "MP#1F", "MP21-07" }
                }
            }
        }
    }
	,
	{
		"HM850H",
		{
			{
			    "0",
			    {
    				 { "MP#00", "MP10-00" }
    				,{ "MP#01", "MP10-01" }
    				,{ "MP#02", "MP10-02" }
    				,{ "MP#03", "MP10-03" }
    				,{ "MP#04", "MP10-04" }
    				,{ "MP#05", "MP10-05" }
    				,{ "MP#06", "MP10-06" }
    				,{ "MP#07", "MP10-07" }
    				,{ "MP#08", "MP10-08" }
    				,{ "MP#09", "MP10-09" }
    				,{ "MP#0A", "MP10-0A" }
    				,{ "MP#0B", "MP10-0B" }
    				,{ "MP#0C", "MP10-0C" }
    				,{ "MP#0D", "MP10-0D" }
    				,{ "MP#0E", "MP10-0E" }
    				,{ "MP#0F", "MP10-0F" }
    				,{ "MP#10", "MP10-10" }
    				,{ "MP#11", "MP10-11" }
    				,{ "MP#12", "MP10-12" }
    				,{ "MP#12", "MP10-12" }
    				,{ "MP#13", "MP10-13" }
			    }
            },
            {
                "1",
                {
    				 { "MP#20", "MP20-00" }
    				,{ "MP#21", "MP20-01" }
    				,{ "MP#22", "MP20-02" }
    				,{ "MP#23", "MP20-03" }
    				,{ "MP#24", "MP20-04" }
    				,{ "MP#25", "MP20-05" }
    				,{ "MP#26", "MP20-06" }
    				,{ "MP#27", "MP20-07" }
    				,{ "MP#28", "MP20-08" }
    				,{ "MP#29", "MP20-09" }
    				,{ "MP#2A", "MP20-0A" }
    				,{ "MP#2B", "MP20-0B" }
    				,{ "MP#2C", "MP20-0C" }
    				,{ "MP#2D", "MP20-0D" }
    				,{ "MP#2E", "MP20-0E" }
    				,{ "MP#2F", "MP20-0F" }
    				,{ "MP#30", "MP20-10" }
    				,{ "MP#31", "MP20-11" }
    				,{ "MP#32", "MP20-12" }
    				,{ "MP#33", "MP20-13" }
                }
            }
        }
    }
	,
	{
		"HM850M",
		{
			{
			    "0",
			    {
    				 { "MP#00", "MP10-00" }
    				,{ "MP#01", "MP10-01" }
    				,{ "MP#02", "MP10-02" }
    				,{ "MP#03", "MP10-03" }
    				,{ "MP#04", "MP10-04" }
    				,{ "MP#05", "MP10-05" }
    				,{ "MP#06", "MP10-06" }
    				,{ "MP#07", "MP10-07" }
    				,{ "MP#08", "MP10-08" }
    				,{ "MP#09", "MP10-09" }
    				,{ "MP#0A", "MP10-0A" }
    				,{ "MP#0B", "MP10-0B" }
			    }
            },
            {
                "1",
                {
    				 { "MP#20", "MP20-00" }
    				,{ "MP#21", "MP20-01" }
    				,{ "MP#22", "MP20-02" }
    				,{ "MP#23", "MP20-03" }
    				,{ "MP#24", "MP20-04" }
    				,{ "MP#25", "MP20-05" }
    				,{ "MP#26", "MP20-06" }
    				,{ "MP#27", "MP20-07" }
    				,{ "MP#28", "MP20-08" }
    				,{ "MP#29", "MP20-09" }
    				,{ "MP#2A", "MP20-0A" }
    				,{ "MP#2B", "MP20-0B" }
                }
            }
        }
    }
    ,
	{
		"HM850S2",
		{
			{
			    "0",
			    {
    				 { "MP#00", "MP10-00" }
    				,{ "MP#01", "MP10-01" }
    				,{ "MP#02", "MP10-02" }
    				,{ "MP#03", "MP10-03" }
    				,{ "MP#04", "MP10-04" }
    				,{ "MP#05", "MP10-05" }
    				,{ "MP#06", "MP10-06" }
    				,{ "MP#07", "MP10-07" }
    				,{ "MP#08", "MP10-08" }
    				,{ "MP#09", "MP10-09" }
    				,{ "MP#0A", "MP10-0A" }
    				,{ "MP#0B", "MP10-0B" }
			    }
            },
            {
                "1",
                {
    				 { "MP#20", "MP20-00" }
    				,{ "MP#21", "MP20-01" }
    				,{ "MP#22", "MP20-02" }
    				,{ "MP#23", "MP20-03" }
    				,{ "MP#24", "MP20-04" }
    				,{ "MP#25", "MP20-05" }
    				,{ "MP#26", "MP20-06" }
    				,{ "MP#27", "MP20-07" }
    				,{ "MP#28", "MP20-08" }
    				,{ "MP#29", "MP20-09" }
    				,{ "MP#2A", "MP20-0A" }
    				,{ "MP#2B", "MP20-0B" }
                }
            }
        }
    }
    ,
	{
		"HM850S1",
		{
			{
			    "0",
			    {
    				 { "MP#00", "MP10-00" }
    				,{ "MP#01", "MP10-01" }
    				,{ "MP#02", "MP10-02" }
    				,{ "MP#03", "MP10-03" }
    				,{ "MP#04", "MP10-04" }
    				,{ "MP#05", "MP10-05" }
			    }
            },
            {
                "1",
                {
    				 { "MP#08", "MP20-00" }
    				,{ "MP#09", "MP20-01" }
    				,{ "MP#0A", "MP20-02" }
    				,{ "MP#0B", "MP20-03" }
    				,{ "MP#0C", "MP20-04" }
    				,{ "MP#0D", "MP20-05" }
                }
            }
        }
    }
    ,

	{
		"HM850S0",
		{
			{
			    "0",
			    {
    				 { "MP#00", "MP10-00" }
    				,{ "MP#01", "MP10-01" }
    				,{ "MP#02", "MP10-02" }
    				,{ "MP#03", "MP10-03" }
    				,{ "MP#04", "MP10-04" }
    				,{ "MP#05", "MP10-05" }
			    }
            },
            {
                "1",
                {
    				 { "MP#08", "MP20-00" }
    				,{ "MP#09", "MP20-01" }
    				,{ "MP#0A", "MP20-02" }
    				,{ "MP#0B", "MP20-03" }
    				,{ "MP#0C", "MP20-04" }
    				,{ "MP#0D", "MP20-05" }
                }
            }
        }
    }
	,
	{
		"HM850XS",
		{
			{
			    "0",
			    {
    				 { "MP#00", "MP10-00" }
    				,{ "MP#01", "MP10-01" }
			    }
            },
            {
                "1",
                {
    				 { "MP#04", "MP20-00" }
    				,{ "MP#05", "MP20-01" }
                }
            }
        }
    }
};
