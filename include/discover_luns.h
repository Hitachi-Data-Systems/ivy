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

class discovered_LUNs {

private:
	std::string showluns_output {};
	int next_line_starting_cursor {0};

	bool get_csv_line(int row, std::string& line, std::string logfilename);
	bool get_csv_field(int row, int col, std::string& field, std::string logfilename);

public:
	discovered_LUNs();
	discovered_LUNs(std::string showluns_output);

	void discover();

	bool isEmpty(){return showluns_output.length()==0;}

	void add_in(discovered_LUNs& dL);
        // if we already contain data, discard dL's header line and concatenate the remainder to the end of showluns_output.

	int get_column_by_header(std::string header, std::string logfilename);
        // returns -1 if no such column header

	bool get_attribute_value_by_header(std::string header, int lun_index, std::string& attribute_value, std::string logfilename);
		// returns true if and only if the column heading exists and the corresponding field in the designated csv line was successfully retrieved.

		// lun starts from 0, which corresponds to the 2nd line in the csvfile, the first line following the header line

	bool get_header_line(std::string& header_line);

	bool get_next_line(std::string& next_line);

	void set_showluns_output(std::string showluns_output){this->showluns_output = showluns_output;}

	const std::string get_showluns_output(){return showluns_output;}
};



