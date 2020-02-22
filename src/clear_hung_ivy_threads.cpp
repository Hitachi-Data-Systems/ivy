//Copyright (c) 2016, 2017, 2018, 2019 Hitachi Vantara Corporation
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

#include <unistd.h>
#include <limits.h>

#include "ivytypes.h"
#include "ivyhelpers.h"

int main(int argc, char* argv[])
{

    char hostname[HOST_NAME_MAX + 1];
    hostname[HOST_NAME_MAX] = 0x00;
    if (0 != gethostname(hostname,HOST_NAME_MAX))
    {
        std::cout << "<Error> internal programming error - gethostname(hostname,HOST_NAME_MAX)) failed errno " << errno << " - " << strerror(errno) << "." << std::endl;
        return -1;
    }

    bool exclude_ivymaster {false};

    if (argc == 1)
    {
        exclude_ivymaster = false;
    }
    else
    {
        std::string argv1 {argv[1]};

        if (argv1 != "-exclude_ivymaster"s || argc > 2)
        {
            std::cout << argv[0] << " accepts only one optional command line option \"-exclude_master\"." << std::endl;
            return -1;
        }

        if (argv1 == "-exclude_ivymaster"s) { exclude_ivymaster = true; }
    }

	std::regex ps_ef_ssh_ivydriver_regex  ( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d ssh .+ (/[^ \t]+/)?ivydriver .+)" );
	std::regex ps_ef_ssh_ivy_cmddev_regex( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d ssh .+ (/[^ \t]+/)?ivy_cmddev .+)" );

	std::regex ps_ef_ivymaster_regex     ( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d (/[^ \t]+/)?ivy .+)" );
	std::regex ps_ef_ivydriver_regex      ( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d (/[^ \t]+/)?ivydriver .+)" );
	std::regex ps_ef_ivy_cmddev_regex    ( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d (/[^ \t]+/)?ivy_cmddev .+)" );

//#define development




#ifdef development
	std::string ps_ef_output(R"(/home/ivogelesang/ivy: ps -ef | grep ivy
501       5477     1  0 Apr30 ?        00:05:25 gedit /home/ivogelesang/ivy/src/MeasureController.h
501       5497     1  0 Apr30 ?        00:00:35 /usr/bin/gnome-terminal -x /bin/sh -c cd '/home/ivogelesang/ivy' && exec $SHELL
root     20576  5552  0 16:12 pts/1    00:00:00 ivy bork
root     20582 20576  0 16:12 pts/1    00:00:00 ssh -t -t root@sun159 ivydriver sun159
root     20583 20576  0 16:12 pts/1    00:00:00 ssh -t -t root@172.17.19.159 ivydriver 172.17.19.159
root     20592 20585  0 16:12 pts/3    00:00:00 ivydriver sun159
root     20593 20584  0 16:12 pts/2    00:00:00 ivydriver 172.17.19.159
root     20698 20576  0 16:12 pts/1    00:00:00 ssh -t -t root@172.17.19.159 ivy_cmddev /dev/sdf 410034 /scripts/ivyoutput/ivydriver_logs/log.ivydriver.172.17.19.159.ivy_cmddev.410034.txt
root     20703 20699  0 16:12 pts/4    00:00:00 ivy_cmddev /dev/sdf 410034 /scripts/ivyoutput/ivydriver_logs/log.ivydriver.172.17.19.159.ivy_cmddev.410034.txt
501      20735  5499  0 16:13 pts/0    00:00:00 grep ivy
root     10791 10789  0 18:52 pts/1    00:00:00 ivy_cmddev /dev/sdf 410115 /scripts/ivy/ivyoutput/ivydriver_logs/log.ivydriver.172.17.62.15.ivy_cmddev.410115.txt

and now later with fully qualified ivydriver name:
root     11534     1  0 Jun02 pts/0    00:00:00 ssh -t -t root@sun159 /usr/local/bin/ivydriver -log sun159
root     11539 11535  0 Jun02 pts/3    00:00:00 /usr/local/bin/ivydriver -log sun159
root     11673     1  0 Jun02 pts/0    00:00:00 ssh -t -t root@sun159 /usr/local/bin/ivydriver -log sun159
root     11678 11674  0 Jun02 pts/4    00:00:00 /usr/local/bin/ivydriver -log sun159
root     11777     1  0 Jun02 pts/0    00:00:00 ssh -t -t root@sun159 /usr/local/bin/ivydriver -log sun159
root     11782 11778  0 Jun02 pts/5    00:01:39 /usr/local/bin/ivydriver -log sun159
root     28457     1  0 Jun01 ?        00:00:00 ssh -t -t root@sun159 /usr/local/bin/ivydriver sun159
root     28462 28458  0 Jun01 pts/1    00:01:40 /usr/local/bin/ivydriver sun159
root     32464     1  0 Jun01 ?        00:00:00 ssh -t -t root@sun159 /usr/local/bin/ivydriver sun159
root     32469 32465  0 Jun01 pts/2    00:01:40 /usr/local/bin/ivydriver sun159

)");
#else
	std::string ps_ef_output = GetStdoutFromCommand(std::string("ps -ef | grep ivy"));
#endif
//	std::cout << ps_ef_output << std::endl;

	std::list<std::string> kill_pids;
	std::smatch pid_match;

	if (!exclude_ivymaster) // we might be running multiple ivy master instances on the same master host that is not a test host.
	{
		std::istringstream is(ps_ef_output);
		std::string psline;

		while (std::getline(is,psline))
		{
			if (	std::regex_match(psline, pid_match, ps_ef_ssh_ivydriver_regex)
			||	    std::regex_match(psline, pid_match, ps_ef_ssh_ivy_cmddev_regex)
			)
			{
				std::string pid = pid_match[1].str();
    	 	 	std::cout << "On " << hostname << ": " << psline << std::endl << "matched as an ssh into ivydriver or ivy_cmddev with pid \"" << pid << "\"." << std::endl << std::endl;
				kill_pids.push_back(pid);
			}
		}

		std::ostringstream kill_pids_command;
		if (kill_pids.size())
		{
#ifdef development
			kill_pids_command << "echo";
#else
			kill_pids_command << "kill -9";
#endif
			for (auto& pid: kill_pids) kill_pids_command << " " << pid;
			std::cout << "On " << hostname << ": " << "kill command for ssh pids \"" << kill_pids_command.str() << "\" output was: " << GetStdoutFromCommand(kill_pids_command.str()) << std::endl;
		}
		//else std::cout << "No ssh to ivydriver or to ivy_cmddev pids were found." << std::endl;

    	std::this_thread::sleep_for(std::chrono::milliseconds(250)); // to give enough time for the killed pids to disappear
	}


#ifdef development
#else
	ps_ef_output = GetStdoutFromCommand(std::string("ps -ef | grep ivy"));
#endif

	kill_pids.clear();

	{
		std::istringstream is(ps_ef_output);
		std::string psline;

		while (std::getline(is,psline))
		{
			if (	((!exclude_ivymaster) && std::regex_match(psline, pid_match, ps_ef_ivymaster_regex))
			||	    std::regex_match(psline, pid_match, ps_ef_ivydriver_regex)
			||	    std::regex_match(psline, pid_match, ps_ef_ivy_cmddev_regex)
			)
			{
				std::string pid = pid_match[1].str();
    			std::cout << "On " << hostname << ": " << psline << std::endl << "matched as an ivy or ivydriver with pid \"" << pid << "\"." << std::endl << std::endl;
				kill_pids.push_back(pid);
			}
		}

		std::ostringstream kill_pids_command;
		if (kill_pids.size())
		{
#ifdef development
			kill_pids_command << "echo";
#else
			kill_pids_command << "kill -9";
#endif
			for (auto& pid: kill_pids) kill_pids_command << " " << pid;
			std::cout << "On " << hostname << ": " << "kill command for ivy or ivydriver pids \"" << kill_pids_command.str() << "\" output was: " << GetStdoutFromCommand(kill_pids_command.str()) << std::endl;
		}
//		else
//		{
//			std::cout << "No ivymaster or ivydriver instances were found." << std::endl;
//		}
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	return 0;
}
