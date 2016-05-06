//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <regex>
#include <iostream>
#include <chrono>
#include <thread>

#include "ivytime.h"
#include "ivyhelpers.h"

int main(int argc, char* argv[])
{
	std::regex ps_ef_ssh_ivyslave_regex  ( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d ssh .+ ivyslave .+)" );
	std::regex ps_ef_ssh_ivy_cmddev_regex( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d ssh .+ ivy_cmddev .+)" );

	std::regex ps_ef_ivymaster_regex     ( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d ivy .+)" );
	std::regex ps_ef_ivyslave_regex      ( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d ivyslave .+)" );
	std::regex ps_ef_ivy_cmddev_regex    ( R"(\w+\s+(\d+)\s+\d+\s.+\d\d:\d\d:\d\d ivy_cmddev .+)" );

//#define development




#ifdef development
	std::string ps_ef_output(R"(/home/ivogelesang/ivy: ps -ef | grep ivy
501       5477     1  0 Apr30 ?        00:05:25 gedit /home/ivogelesang/ivy/src/DynamicFeedbackController.h
501       5497     1  0 Apr30 ?        00:00:35 /usr/bin/gnome-terminal -x /bin/sh -c cd '/home/ivogelesang/ivy' && exec $SHELL
root     20576  5552  0 16:12 pts/1    00:00:00 ivy bork
root     20582 20576  0 16:12 pts/1    00:00:00 ssh -t -t root@sun159 ivyslave sun159
root     20583 20576  0 16:12 pts/1    00:00:00 ssh -t -t root@172.17.19.159 ivyslave 172.17.19.159
root     20592 20585  0 16:12 pts/3    00:00:00 ivyslave sun159
root     20593 20584  0 16:12 pts/2    00:00:00 ivyslave 172.17.19.159
root     20698 20576  0 16:12 pts/1    00:00:00 ssh -t -t root@172.17.19.159 ivy_cmddev /dev/sdf 410034 /scripts/ivyoutput/ivyslave_logs/log.ivyslave.172.17.19.159.ivy_cmddev.410034.txt
root     20703 20699  0 16:12 pts/4    00:00:00 ivy_cmddev /dev/sdf 410034 /scripts/ivyoutput/ivyslave_logs/log.ivyslave.172.17.19.159.ivy_cmddev.410034.txt
501      20735  5499  0 16:13 pts/0    00:00:00 grep ivy
root     10791 10789  0 18:52 pts/1    00:00:00 ivy_cmddev /dev/sdf 410115 /scripts/ivy/ivyoutput/ivyslave_logs/log.ivyslave.172.17.62.15.ivy_cmddev.410115.txt
)");
#else
	std::string ps_ef_output = GetStdoutFromCommand(std::string("ps -ef | grep ivy"));
#endif
	std::cout << ps_ef_output << std::endl;

	std::list<std::string> kill_pids;
	std::smatch pid_match;

	{
		std::istringstream is(ps_ef_output);
		std::string psline;

		while (std::getline(is,psline))
		{
			if (	std::regex_match(psline, pid_match, ps_ef_ssh_ivyslave_regex)
			||	    std::regex_match(psline, pid_match, ps_ef_ssh_ivy_cmddev_regex)
			)
			{
				std::string pid = pid_match[1].str();
/*debug*/	 	 	std::cout << psline << std::endl << "matched as an ssh into ivyslave or ivy_cmddev with pid \"" << pid << "\"." << std::endl << std::endl;
				kill_pids.push_back(pid);
			}
		}

		std::ostringstream kill_pids_command;
		if (kill_pids.size())
		{
#ifdef development
			kill_pids_command << "echo";
#else
			kill_pids_command << "kill";
#endif
			for (auto& pid: kill_pids) kill_pids_command << " " << pid;
			std::cout << "kill command for ssh pids \"" << kill_pids_command.str() << "\" output was: " << GetStdoutFromCommand(kill_pids_command.str()) << std::endl;
		}
		else std::cout << "No ssh to ivyslave or to ivy_cmddev pids were found." << std::endl;
	}

	std::this_thread::sleep_for(std::chrono::seconds(1));

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
			if (	std::regex_match(psline, pid_match, ps_ef_ivymaster_regex)
			||	    std::regex_match(psline, pid_match, ps_ef_ivyslave_regex)
			||	    std::regex_match(psline, pid_match, ps_ef_ivy_cmddev_regex)
			)
			{
				std::string pid = pid_match[1].str();
/*debug*/			std::cout << psline << std::endl << "matched as an ivymaster or ivyslave with pid \"" << pid << "\"." << std::endl << std::endl;
				kill_pids.push_back(pid);
			}
		}

		std::ostringstream kill_pids_command;
		if (kill_pids.size())
		{
#ifdef development
			kill_pids_command << "echo";
#else
			kill_pids_command << "kill";
#endif
			for (auto& pid: kill_pids) kill_pids_command << " " << pid;
			std::cout << "kill command for ivymaster or ivyslave pids \"" << kill_pids_command.str() << "\" output was: " << GetStdoutFromCommand(kill_pids_command.str()) << std::endl;
		}
		else
		{
			std::cout << "No ivymaster or ivyslave instances were found." << std::endl;
		}
	}

	std::this_thread::sleep_for(std::chrono::seconds(2));

	return 0;
}
