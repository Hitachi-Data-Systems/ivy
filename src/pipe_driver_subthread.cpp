//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
#include <errno.h>
#include <fcntl.h>
#include <functional>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <list>
#include <locale>
#include <malloc.h>
#include <poll.h>
#include <regex>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <map>
#include <limits.h> // LLONG_MAX, LLONG_MIN
#include <algorithm>
#include <stdexcept>
#include <syscall.h>

using namespace std;

// invoked "ivymaster scriptname.ivyscript"

#include "ivyhelpers.h"
#include "ivytime.h"
// SHOULD GET AROUND TO GIVING IVYTIME THE ABILITY TO PARSE AT LEAST YYYY-MM-DD HH:MM:SS AND YYYY-MM-DD HH:MM:SS.NNNNNNNNN
// ... except that regexes seem to be broken and are known to be broken in this version of libstdc++

#include "ivydefines.h"
#include "RunningStat.h"
#include "discover_luns.h"
#include "LDEVset.h"
#include "IogeneratorInput.h"
#include "IogeneratorInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "ivybuilddate.h"
#include "ivylinuxcpubusy.h"
#include "Subinterval_CPU.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "LUN.h"
#include "Select.h"
#include "LUNpointerList.h"
#include "GatherData.h"
#include "Subsystem.h"
#include "pipe_driver_subthread.h"
#include "WorkloadTracker.h"
#include "WorkloadTrackers.h"
#include "SubintervalRollup.h"
#include "SequenceOfSubintervalRollup.h"
#include "AttributeNameCombo.h"
#include "RollupInstance.h"
#include "RollupType.h"
#include "RollupSet.h"
#include "master_stuff.h"


#define PIPE_READ 0
#define PIPE_WRITE 1

extern bool routine_logging;

extern bool trace_evaluate;

extern std::regex identifier_regex;
std::regex number_regex( R"((([0-9]+(\.[0-9]*)?)|(\.[0-9]+))%?)" );  // 5 or .5 or 5.5 - may have trailing % sign

void pipe_driver_subthread::send_and_get_OK(std::string s, const ivytime timeout)
{
    send_string(s);

    std::string r = get_line_from_pipe(timeout, std::string("send_and_get_OK()"));
    trim(r);
    if (0 != std::string("<OK>").compare(r))
    {
        std::ostringstream o;
        o << "For host " << ivyscript_hostname << ", sent command \"" << s << "\" to ivyslave, and was expecting ivyslave to respond with <OK> but instead got \"" << r << "\".";
        throw std::runtime_error(o.str());
    }
    return;
}

void pipe_driver_subthread::send_and_get_OK(std::string s)
{
    send_and_get_OK(s, m_s.subintervalLength);
    return;
}

std::string pipe_driver_subthread::send_and_clip_response_upto(std::string s, std::string pattern, const ivytime timeout)
{
    // sends the string s, then gets a response string from the slave.

    // Then we scan through the response looking for the pattern substring. e.g. [LUNHEADER] or [LUN]

    // If the pattern is found, the remaining portion of the response from the slave is returned.


    // NOTE: probably when I wrote this I had forgotten that send_string() eats the echo of what it sends.
    //       So probably we are matching on the pattern right at the beginning.  Not going to stop now to figure this out and simplify.


    send_string(s);

    std::string r = get_line_from_pipe(timeout, std::string("send_and_clip_response_upto(\"") + s + std::string("\", \"") + pattern + std::string("\")"));

    trim(r);

    if ( r.length() < pattern.length() )
    {
        std::ostringstream o;
        o << "For host " << ivyscript_hostname << ", sent command \"" << s << "\" to ivyslave, and was expecting the ivyslave response to contain \"" << pattern << "\" but instead got \"" << r << "\".";
        throw std::runtime_error(o.str());
    }

    for (int cursor=0; (pattern.length()+cursor) <= r.length(); cursor++)
    {
        if (pattern==r.substr(cursor,pattern.length()))
        {
            if ((pattern.length()+cursor)==r.length())
                return "";
            else
                return r.substr(cursor+pattern.length(),r.length()-(cursor+pattern.length()));
        }
    }
    {
        std::ostringstream o;
        o << "For host " << ivyscript_hostname << ", sent command \"" << s << "\" to ivyslave, and was expecting the ivyslave response to contain \"" << pattern << "\" but instead got \"" << r << "\".";
        throw std::runtime_error(o.str());
    }
    return ""; //paranoia in case compiler complains
}

void pipe_driver_subthread::send_string(std::string s)
{
    rtrim(s);
    s+='\n';  // put a line end on it if it didn't already have one

    if (s.length() > IVYMAXMSGSIZE)
    {
        std::ostringstream o;
        o << "For host " << ivyscript_hostname << ", write() for string to slave - oversized string of length " << s.length() << " when max is " << IVYMAXMSGSIZE << ". " << std::endl;
        throw std::runtime_error(o.str());
    }
    memcpy(to_slave_buf,s.c_str(),s.length());

    int rc = write(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE],to_slave_buf,s.length());

    if (-1 == rc)
    {
        ostringstream o;
        o << "For host " << ivyscript_hostname << ", write() for string into pipe to ivyslave failed errno = " << errno << " - " << strerror(errno) << std::endl;
        throw std::runtime_error(o.str());
    }

    if ( s.length() != ((unsigned int) rc) )
    {
        ostringstream o;
        o << "For host " << ivyscript_hostname << ", write() for string into pipe to ivyslave only wrote " << rc << " bytes out of " << s.length() << "." << std::endl;
        throw std::runtime_error(o.str());
    }

    if (routine_logging) log(logfilename, format_utterance("Master",s));

    // With ssh, we hear back an echo of what we typed.  So we read from the pipe to eat the echo.
    std::string echo = get_line_from_pipe(m_s.subintervalLength, std::string("reading echo of what we just said."), /*reading_echo_line*/ true);

// /*debug*/ log(logfilename, std::string("Echo line was \"")+echo+std::string("\"\n"));

    rtrim(s);
    rtrim(echo);
    if (0 != s.compare(echo))
    {
        throw std::runtime_error("Echo line didn't match what we sent.");
    }

    return;
}

bool parseWorkloadIDfromIstream(std::string& callers_error_message, std::istream& i, WorkloadID& wid)
{
    std::string w;

    char c;

    i >> c;
    if (i.fail() || c != '<')
    {
        callers_error_message = "WorkloadID field did not start with \'<\'.";
        return false;
    }

    while (true)
    {
        i >> c;
        if (i.fail())
        {
            callers_error_message = "WorkloadID field terminating \'>\' not found.";
            return false;
        }
        if ('>' == c) break;
        w.push_back(c);
    }

    if (!wid.set(w))
    {
        std::ostringstream o;
        o << "WorkloadID field \"" << w << "\" did not parse into three sections separated by \'+\' signs.";
        callers_error_message = o.str();
        return false;
    }

    if (m_s.workloadTrackers.workloadTrackerPointers.end() == m_s.workloadTrackers.workloadTrackerPointers.find(wid.workloadID))
    {
        std::ostringstream o;
        o << "No such workload ID \"" << w << "\".";
        callers_error_message = o.str();
        return false;
    }

    return true;
}

bool pipe_driver_subthread::read_from_pipe(ivytime timeout)
{
    // Used by get_line_from_pipe() when there are no more bytes left in "from_slave_buf" to serve up.

    ivytime start, now, remaining, waitthistime;
    int rc;
    fd_set read_fd_set;

    start.setToNow();

    now.setToNow();
    if (now >= (start+timeout)) return false;

    remaining=(start+timeout)-now;

    while(true)
    {
        // This loop keeps running until the child pid exits, we time out, or we have something ready to read from the pipe

        if (-1==fcntl(slave_to_pipe_driver_subthread_pipe[PIPE_READ],F_GETFD)) // check to make sure fd is valid
        {
            ostringstream logmsg;
            logmsg << "For host " << ivyscript_hostname << ", pipe_driver_subthread::read_from_pipe() - fcntl() for pipe fd failed. errno = " << errno << " - " << strerror(errno);
            log(logfilename,logmsg.str());
            return false;
        }

        int pid_status;
        pid_t result = waitpid(ssh_pid, &pid_status, WNOHANG);
//*debug*/{std::ostringstream o; o<< std::endl << "pipe_driver_subthread::read_from_pipe() for " << ivyscript_hostname << " ssh_pid = " << ssh_pid << " result of checking pid with WNOHANG is " << result << " pid_status = " << pid_status << std::endl  << std::endl; log(logfilename,o.str());}

        if (-1==result)
        {
            ostringstream logmsg;
            logmsg << "For host " << ivyscript_hostname << ", pipe_driver_subthread::read_from_pipe() - waitpid(ssh_pid,,WNOHANG) failed. errno = " << errno << " - " << strerror(errno);
            log(logfilename,logmsg.str());
            return false;
        }

        if (0 != pid_status && WIFEXITED(pid_status))
        {
            ostringstream logmsg;
            logmsg << "For host " << ivyscript_hostname << ", pipe_driver_subthread::read_from_pipe() - child has exited.";
            log(logfilename,logmsg.str());
            return false;
        }

        if (remaining>ivytime(1))
            waitthistime=ivytime(1);   // wait a second at a time so we can check if the child pid exited.
        else
            waitthistime=remaining;

        FD_ZERO(&read_fd_set);
        FD_SET(slave_to_pipe_driver_subthread_pipe[PIPE_READ],&read_fd_set);
        rc=pselect(1+slave_to_pipe_driver_subthread_pipe[PIPE_READ],&read_fd_set,NULL,NULL,&(waitthistime.t),NULL);
        if (-1==rc)
        {
            ostringstream logmsg;
            logmsg << "For host " << ivyscript_hostname << ", pipe_driver_subthread::read_from_pipe() - pselect() failed. errno = " << errno << " - " << strerror(errno);
            log(logfilename,logmsg.str());
            return false;
        }
        else if (0==rc)
        {
            // timeout expired
            now.setToNow();
            if (now >= (start+timeout))
            {
                ostringstream logmsg;
                logmsg << "For host " << ivyscript_hostname << ", timeout (" << timeout.format_as_duration_HMMSSns()
                << ") reading from pipe to remotely executing ivyslave instance.  "
                << "start=" << start.format_as_datetime_with_ns() << ", now=" << now.format_as_datetime_with_ns()
                    << "  " << __FILE__ << " line " << __LINE__;
                log(logfilename,logmsg.str());
                return false;
            }
        }
        else if (0<rc) break;
    }

    // we have data ready to receive from the child

    rc = read( slave_to_pipe_driver_subthread_pipe[PIPE_READ], (void*) &from_slave_buf, IVYMAXMSGSIZE );
    if (-1==rc)
    {
        ostringstream logmsg;
        logmsg << "For host " << ivyscript_hostname << ", read() from pipe failed errno = " << errno << " - " << strerror(errno);
        log(logfilename,logmsg.str());
        return false;
    }
    else if (0==rc)
    {
        ostringstream logmsg;
        logmsg << "For host " << ivyscript_hostname << ", read() from pipe got end of file.";
        log(logfilename,logmsg.str());
        return false;
    }

    next_char_from_slave_buf=0;
    bytes_last_read_from_slave=rc;

    return true;

    // keeping these comments in case we need to use them in future

    //     struct pollfd {
    //        int   fd;         /* file descriptor */
    //        short events;     /* requested events */
    //        short revents;    /* returned events */
    //    };

    ///* Event types that can be polled for.  These bits may be set in `events'
    //   to indicate the interesting event types; they will appear in `revents'
    //   to indicate the status of the file descriptor.  */
    //#define POLLIN          0x001           /* There is data to read.  */
    //#define POLLPRI         0x002           /* There is urgent data to read.  */
    //#define POLLOUT         0x004           /* Writing now will not block.  */
    //
    //#if defined __USE_XOPEN || defined __USE_XOPEN2K8
    ///* These values are defined in XPG4.2.  */
    //# define POLLRDNORM     0x040           /* Normal data may be read.  */
    //# define POLLRDBAND     0x080           /* Priority data may be read.  */
    //# define POLLWRNORM     0x100           /* Writing now will not block.  */
    //# define POLLWRBAND     0x200           /* Priority data may be written.  */
    //#endif
    //
    //#ifdef __USE_GNU
    ///* These are extensions for Linux.  */
    //# define POLLMSG        0x400
    //# define POLLREMOVE     0x1000
    //# define POLLRDHUP      0x2000
    /* POLLRDHUP - (since Linux 2.6.17) Stream socket peer closed connection, or shut down writing half of connection.
    The _GNU_SOURCE feature test macro must be defined (before including any header files) in order to obtain this definition. */
    //#endif
    //
    ///* Event types always implicitly polled for.  These bits need not be set in
    //   `events', but they will appear in `revents' to indicate the status of
    //   the file descriptor.  */
    //#define POLLERR         0x008           /* Error condition.  */
    //#define POLLHUP         0x010           /* Hung up.  */
    //#define POLLNVAL        0x020           /* Invalid polling request.  */

}


std::string pipe_driver_subthread::real_get_line_from_pipe
(
    ivytime timeout,
    std::string description, // Used when there is some sort of failure to construct an error message written to the log file.
    bool reading_echo_line
)
{
    std::string s;

    if ( next_char_from_slave_buf >= bytes_last_read_from_slave && !last_read_from_pipe )
    {
        std::ostringstream o;
        o << "For host " << ivyscript_hostname << ", pipe_driver_subthread::get_line_from_pipe(timeout=" << timeout.format_as_duration_HMMSSns() << ") - no characters available in buffer and read_from_pipe() failed - " << description;
        throw std::runtime_error(o.str());
    }

    if ( next_char_from_slave_buf >= bytes_last_read_from_slave )
    {
        last_read_from_pipe=read_from_pipe(timeout);
        if (!last_read_from_pipe)
        {
            std::ostringstream o;
            o << "For host " << ivyscript_hostname << ", pipe_driver_subthread::get_line_from_pipe(timeout=" << timeout.format_as_duration_HMMSSns() << ") - no characters available in buffer and read_from_pipe() failed - " << description;
            throw std::runtime_error(o.str());
        }
    }

    // We have bytes in the buffer to serve up and from here on we can only return true.

    while(true)
    {
        // we move characters one at a time from the read buffer, stopping when we see a '\n'.

        if (next_char_from_slave_buf>=bytes_last_read_from_slave)
        {
            // if no more characters in buffer
            last_read_from_pipe=read_from_pipe(timeout);
            if (!last_read_from_pipe) break;
        }
        char c;
        c = from_slave_buf[next_char_from_slave_buf++];
        if ('\n' == c) break;
        s.push_back(c);
    }

//{std::ostringstream o; o << "==debug== pipe_driver_subthread::real_get_line_from_pipe("
//<< "timeout="<<timeout.format_as_duration_HMMSSns()
//<< ",description=\"" << description << "\""
//<< ",reading_echo_line=" << ( (reading_echo_line) ? "true" : "false" )
//<< ") - about to decide to log or not."; log(logfilename,o.str()); }
//

    if ( trace_evaluate || (routine_logging && !reading_echo_line)) log(logfilename, format_utterance(" Slave",s));

//{std::ostringstream o; o << "==debug== pipe_driver_subthread::real_get_line_from_pipe("
//<< "timeout="<<timeout.format_as_duration_HMMSSns()
//<< ",description=\"" << description << "\""
//<< ",reading_echo_line=" << ( (reading_echo_line) ? "true" : "false" )
//<< ") - after logging or not."; log(logfilename,o.str()); }
//

    return s;
}

std::string pipe_driver_subthread::get_line_from_pipe
(
    ivytime timeout,
    std::string description, // Used when there is some sort of failure to construct an error message written to the log file.
    bool reading_echo_line
)
{
    // This is a wrapper around the "real" get_line_from_pipe() that checks for when what ivyslave says starts with "<Error>".

    // If what ivyslave says starts with <Error>, in order to be able to catch multi-line error messages,
    // then we keep additional reading lines from ivyslave if they are ready within .25 second.

    // On seeing <Error>, after echoing the host it came from and the message itself to the console and the master log and the host log,
    // the pipe_driver subthread needs to signal ivymaster that an error has been logged and that it (ivymaster main thread) needs to kill all subtasks and exit.

    // If what ivyslave says starts with <Warning> then we echo the line to the console and the log with the host it came from but
    // then just loop back and read another line transparently to the caller.

    std::string s;
    while (true)  // we only ever loop back to the beginning after processing a <Warning> line
    {
        s = real_get_line_from_pipe(timeout,description,reading_echo_line);
        if (startsWith(s, std::string("<Warning>")))
        {
            std::ostringstream o;
            o << std::endl << "From " << ivyscript_hostname << ": " << s << std::endl << std::endl;
            log (logfilename, o.str());
            log (m_s.masterlogfile, o.str());
            std::cout << o.str();
            continue;
        }

        if (startsWith(s, std::string("<Error>"))) throw std::runtime_error(s);

        return s;
    }
}



void pipe_driver_subthread::kill_ssh_and_harvest()
{
    kill (ssh_pid,SIGTERM);
    harvest_pid();
    return;
}

void pipe_driver_subthread::orderSlaveToDie()
{
    std::string response;
    std::string result {"ivyslave exited upon command\n"};
    try
    {
        response = send_and_clip_response_upto("[Die, Earthling!]\n", "[what?]", ivytime(5));
    }
    catch (std::runtime_error& reex)
    {
        std::ostringstream o;
        o << "[Die, Earthlin!] command failed saying \"" << reex.what() << "\"." << std::endl;
        result = o.str();
        kill (ssh_pid,SIGTERM);
    }

    // give other end some time to kill its iogenerator & RMLIB subthreads

    ivytime one_second(1,0);
    nanosleep(&(one_second.t),NULL);

    harvest_pid();
    if (routine_logging) log(logfilename, std::string());
    return;
}

void pipe_driver_subthread::harvest_pid()
{
    close(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE]);
    close(slave_to_pipe_driver_subthread_pipe[PIPE_READ]);

    // harvest the child's pid so it won't be ... a zombie!
    int status;
    do
    {
        int rc  = waitpid(ssh_pid, &status, WUNTRACED | WCONTINUED);
        if (rc == -1)
        {
            std::ostringstream o;
            o << "\nwaitpid() to harvest ssh shell pid failed, errno = " << errno << " = " << strerror(errno) << std::endl;
            log(logfilename, o.str());
            break;
        }
        if (WIFEXITED(status))
        {
            std::ostringstream o;
            o << "ssh pid exited, status=" << WEXITSTATUS(status) << std::endl;
            log(logfilename, o.str());
        }
        else if (WIFSIGNALED(status))
        {
            std::ostringstream o;
            o << "killed by signal " << WTERMSIG(status) << std::endl;
            log(logfilename, o.str());
        }
        else if (WIFSTOPPED(status))
        {
            std::ostringstream o;
            o << "stopped by signal " << WSTOPSIG(status) << std::endl;
            log(logfilename, o.str());
        }
        else if (WIFCONTINUED(status))
        {
            log(logfilename, "waitpid() for ssh child pid is looping due to \"continued\" status.");
        }
    }
    while (!WIFEXITED(status) && !WIFSIGNALED(status));
    return;
}

void pipe_driver_subthread::threadRun()
{

    pipe_driver_pid = getpid();

    linux_gettid_thread_id = syscall(SYS_gettid);


    // fork

    // if child - adjust piping for child
    //          - do an execl("ssh", "userid@ivyscript_hostname", (char*)0)
    //          - this point, the world vapourizes and becomes "ssh root@ivyscript_hostname" attached by pipes to the parent

    // if parent - adjust piping for parent and wait for child to say something.
    //           - judge whether we are logged OK or not somehow.  Maybe we need a successful login prompt reference string that you could scrape.
    //           - send a command line to fire up ivyslave
    //           - wait (with timeout) for slave to emit an error message or report ready for duty.
    //           - send slave the starting / ending times.
    //           - send slave the LUN names
    //           - tell slave to start the test
    //           - if test completes successfully, receive minmaxcum objects from the slave for host overall and for each LUN.


    if (pCmdDevLUN)
    {
        std::string LDEV = pCmdDevLUN->attribute_value("LDEV");
        if (5==LDEV.length())
        {
            LDEV.erase(2,1); // remove colon
        }
        std::ostringstream lfn;
        lfn << logfolder << "/log.ivymaster.cmd_dev."
            << "serial_number_" << LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(p_Hitachi_RAID_subsystem->serial_number)
            << "+LDEV_" << LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(LDEV)
            << "+host_" << LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(pCmdDevLUN->attribute_value("ivyscript_hostname"))
            << "+LUN" << LUN::convert_to_lower_case_and_convert_nonalphameric_to_underscore(pCmdDevLUN->attribute_value("LUN Name"))
            << ".txt";
        logfilename=lfn.str();
    }
    else
    {
        logfilename=logfolder+std::string("/log.ivymaster.")+ivyscript_hostname+std::string(".txt");
    }

    remove(logfilename.c_str()); // erase log file from last time in this output subfolder

    //if (routine_logging) std::cout<< "pipe_driver_subthread inherited routine_logging = " << (routine_logging ? "true" : "false" ) << std:: endl;

    if (routine_logging)
    {
        ostringstream logmsg;
        logmsg << "pipe_driver_subthread startup for " << ivyscript_hostname;
        if (pCmdDevLUN)
        {
            logmsg << " to run ivy_cmddev";
        }
        else
        {
            logmsg << " to run ivyslave";
        }
        logmsg <<   ".  ivy build date was " << IVYBUILDDATE << "." << std::endl;
        log(logfilename, logmsg.str());
    }
    else
    {
        log(logfilename,"For logging of routine (non-error) events, use the ivy -log command line option, like \"ivy -log a.ivyscript\".\n\n");
    }

    if (pipe(pipe_driver_subthread_to_slave_pipe) < 0)
    {
        ostringstream logmsg;
        logmsg << "pipe(pipe_driver_subthread_to_slave_pipe) failed - rc = " << errno << " " << strerror(errno) << std::endl;
        log(logfilename, logmsg.str());
        return;
    }
    if (pipe(slave_to_pipe_driver_subthread_pipe) < 0)
    {
        close(pipe_driver_subthread_to_slave_pipe[PIPE_READ]);
        close(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE]);
        ostringstream logmsg;
        logmsg << "pipe(slave_to_pipe_driver_subthread_pipe) failed - rc = " << errno << " " << strerror(errno) << std::endl;
        log(logfilename, logmsg.str());
        return;
    }

    log(logfilename, "About to fork to create ssh subthread.\n");

    ssh_pid = fork();
    if (0 == ssh_pid)
    {
        // child

        // redirect stdin
        if (dup2(pipe_driver_subthread_to_slave_pipe[PIPE_READ], STDIN_FILENO) == -1)
        {
            ostringstream logmsg;
            logmsg << "dup2(pipe_driver_subthread_to_slave_pipe[PIPE_READ], STDIN_FILENO) failed - rc = " << errno << " " << strerror(errno) << std::endl;
            log(logfilename, logmsg.str());
            return;
        }

        // redirect stdout
        if (dup2(slave_to_pipe_driver_subthread_pipe[PIPE_WRITE], STDOUT_FILENO) == -1)
        {
            ostringstream logmsg;
            logmsg << "dup2(slave_to_pipe_driver_subthread_pipe[PIPE_WRITE], STDOUT_FILENO) failed - rc = " << errno << " " << strerror(errno) << std::endl;
            log(logfilename, logmsg.str());
            return;
        }

        // redirect stderr
        if (dup2(slave_to_pipe_driver_subthread_pipe[PIPE_WRITE], STDERR_FILENO) == -1)
        {
            ostringstream logmsg;
            logmsg << "dup2(slave_to_pipe_driver_subthread_pipe[PIPE_WRITE], STDERR_FILENO) failed - rc = " << errno << " " << strerror(errno) << std::endl;
            log(logfilename, logmsg.str());
            return;
        }

        // all these are for use by parent only
        close(pipe_driver_subthread_to_slave_pipe[PIPE_READ]);
        close(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE]);
        close(slave_to_pipe_driver_subthread_pipe[PIPE_READ]);
        close(slave_to_pipe_driver_subthread_pipe[PIPE_WRITE]);

        // run child process image
        log(logfilename, "Child pid about to issue execl for ssh.\n");


        login = SLAVEUSERID + std::string("@") + ivyscript_hostname;

        std::string cmd, arg, serial, remote_logfilename;

        if (pCmdDevLUN)
        {
            cmd = IVY_CMDDEV_EXECUTABLE;
            arg = pCmdDevLUN->attribute_value("LUN_name");
            serial = pCmdDevLUN->attribute_value("serial_number");
            {
                ostringstream fn;
                fn << IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER << "/log.ivyslave." << ivyscript_hostname << ".ivy_cmddev." << p_Hitachi_RAID_subsystem->serial_number << ".txt";
                remote_logfilename = fn.str();
            }
            if (routine_logging)
            {
                execl("/usr/bin/ssh","ssh","-t","-t", login.c_str(), cmd.c_str(), "-log", arg.c_str(), serial.c_str(), remote_logfilename.c_str(), (char*)NULL);
            }
            else
            {
                execl("/usr/bin/ssh","ssh","-t","-t", login.c_str(), cmd.c_str(),         arg.c_str(), serial.c_str(), remote_logfilename.c_str(), (char*)NULL);
            }
        }
        else
        {
            cmd = IVYSLAVE_EXECUTABLE;
            arg = ivyscript_hostname;
            {
                ostringstream execl_cmd;
                execl_cmd << "/usr/bin/ssh ssh -t -t " << login << ' ' << cmd << ' ';
                if (routine_logging) execl_cmd << "-log ";
                execl_cmd << arg << std::endl;
                log(logfilename, execl_cmd.str());
            }
            if (routine_logging)
            {
                execl("/usr/bin/ssh","ssh","-t","-t",login.c_str(),cmd.c_str(),"-log", arg.c_str(),(char*)NULL);
            }
            else
            {
                execl("/usr/bin/ssh","ssh","-t","-t",login.c_str(),cmd.c_str(),        arg.c_str(),(char*)NULL);
            }
        }

        log(logfilename, std::string("execl() for ssh failed\n"));
        std::cerr << "child\'s execl() of ssh " << login << " failed." << std::endl;

        return;
    }
    else if (ssh_pid > 0)
    {
        // parent

        /*debug*/
        {
            std::ostringstream o;
            o << "pid for ssh to " << ivyscript_hostname << " is " << ssh_pid << std::endl;
            log(logfilename,o.str());
        }

        // close unused file descriptors, these are for child only
        close(pipe_driver_subthread_to_slave_pipe[PIPE_READ]);
        close(slave_to_pipe_driver_subthread_pipe[PIPE_WRITE]);

        // ready for interaction with the remote slave
        int flags = fcntl(slave_to_pipe_driver_subthread_pipe[PIPE_READ], F_GETFL, 0);
        if (fcntl(slave_to_pipe_driver_subthread_pipe[PIPE_READ], F_SETFL, flags | O_NONBLOCK))
        {
            ostringstream logmsg;
            logmsg << "failed setting master pipe read fd from slave stdout/stderr to O_NON_BLOCK" << std::endl;
            log(logfilename, logmsg.str());
            close(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE]);
            close(slave_to_pipe_driver_subthread_pipe[PIPE_READ]);
            {
                std::lock_guard<std::mutex> lk_guard(master_slave_lk);
                startupComplete=true;
                startupSuccessful=false;
                commandErrorMessage = logmsg.str();
                dead=true;
            }
            master_slave_cv.notify_all();

            return;
        }

        std::string prompt;

        try
        {
            prompt = get_line_from_pipe(ivytime(MAXWAITFORINITALPROMPT), "get \"Hello, Whirrled!\" prompt from child");
        }
        catch (std::runtime_error& reex)
        {
            std::ostringstream o;
            o << "Initial read from pipe to ssh subthread failed saying \"" << reex.what() << "\"." << std::endl;
            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
            kill_ssh_and_harvest();
            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
            master_slave_cv.notify_all();
            return;
        }

        std::string tcgetattr_error_msg("tcgetattr: Invalid argument");

        if ( 0 == tcgetattr_error_msg.compare(prompt))
        {
            try
            {
                prompt = get_line_from_pipe(ivytime(MAXWAITFORINITALPROMPT), "get \"Hello, Whirrled!\" prompt from child");
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "Initial read from pipe to ssh subthread failed saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                kill_ssh_and_harvest();
                commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                master_slave_cv.notify_all();
                return;
            }
        }

        trim(prompt); // removes leading / trailing whitespace

        std::string hellowhirrled("Hello, whirrled! from ");

        log(logfilename, std::string("initial prompt from remote was \"")+prompt+std::string("\"."));

        unsigned int i;
        for (i=0; i<=(prompt.size()-hellowhirrled.size()); i++)
        {
            if ( i == (prompt.size()-hellowhirrled.size()) )
            {
                std::ostringstream o;
                o << "Remote ";
                if (pCmdDevLUN) o << "ivy_cmddev";
                else            o << "ivyslave";
                o << " startup failure, saying \"" << prompt << "\"." << std::endl;

                log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                kill_ssh_and_harvest();
                commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                master_slave_cv.notify_all();
                return;
            }
            if (0 == prompt.substr(i,hellowhirrled.size()).compare(hellowhirrled))
                break;
        }
        hostname=prompt.substr( i+hellowhirrled.size(), prompt.size()-(i+hellowhirrled.size()) );

        trim(hostname);

        log(logfilename, std::string("remote slave said its hostname is \"")+hostname+std::string("\"."));

        // Retrieve list of luns visible on the slave host
        if (!pCmdDevLUN)
        {
            try
            {
                lun_csv_header = send_and_clip_response_upto("send LUN header\n", "[LUNheader]", ivytime(5));
            }
            catch (std::runtime_error& reex)
            {
                std::ostringstream o;
                o << "\"send LUN header\" command sent to ivyslave failed saying \"" << reex.what() << "\"." << std::endl;
                log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                kill_ssh_and_harvest();
                commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                master_slave_cv.notify_all();
                return;
            }
            lun_csv_header.push_back('\n');

            while (true)
            {
                std::string response;
                try
                {
                    response = send_and_clip_response_upto("send LUN\n", "[LUN]", ivytime(5));
                }
                catch (std::runtime_error& reex)
                {
                    std::ostringstream o;
                    o << "\"send LUN\" command sent to ivyslave failed saying \"" << reex.what() << "\"." << std::endl;
                    log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                    kill_ssh_and_harvest();
                    commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                    master_slave_cv.notify_all();
                    return;
                }

                trim(response);

                if (std::string("<eof>") == response) break;

                lun_csv_body << response << std::endl;

                LUN* p_LUN = new LUN();
                if (!p_LUN->loadcsvline(lun_csv_header, response, logfilename))
                {
                    // we already just printed an error message inside loadcsvline()
                    delete p_LUN;
                    std::ostringstream o;
                    trim(lun_csv_header); trim(response);
                    o << "Failed loading LUN:" << std::endl
                        << "LUN header - \"" << lun_csv_header << "\"" << std::endl
                        << "LUN        - \"" << response << "\"" << std::endl
                    ;
                    log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                    kill_ssh_and_harvest();
                    commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                    master_slave_cv.notify_all();
                    return;
                }
                thisHostLUNs.LUNpointers.push_back(p_LUN);
                HostSampleLUN.copyOntoMe(p_LUN);
                // If multiple LUN-lister csv file formats, this captures
                // one instance of each column header title value in any LUN seen.
            }
        }

        {
            std::lock_guard<std::mutex> lk_guard(master_slave_lk);
            startupComplete=true;
            startupSuccessful=true;
        }
        master_slave_cv.notify_all();

        // process commands from ivymaster

        bool processingCommands {true};
        ivytime subintervalStart;

        while (processingCommands)
        {
            {
                std::unique_lock<std::mutex> s_lk(master_slave_lk);
                while (!command)
                {
                    master_slave_cv.wait(s_lk);
                }

                command=false;
                commandComplete=false;
                commandSuccess=false;

                // what kind of command did we get?

                if (0==std::string("die").compare(commandString))
                {
                    orderSlaveToDie();
                    dead=true;
                    s_lk.unlock();
                    master_slave_cv.notify_all();
                    return;
                }

                if (pCmdDevLUN)
                {
                    // process commands to interact with a remote ivy_cmddev CLI

                    ivytime start;
                    start.setToNow();

                    if (0==std::string("get config").compare(commandString))
                    {

                        ivytime gatherStart;
                        gatherStart.setToNow();

                        try
                        {
                            send_and_get_OK("get config");

                            try
                            {
                                process_ivy_cmddev_response(p_Hitachi_RAID_subsystem->configGatherData, gatherStart);
                            }
                            catch (std::invalid_argument& iaex)
                            {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get config\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                            catch (std::runtime_error& reex)
                            {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get config\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "\"get config\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            master_slave_cv.notify_all();
                            return;
                        }
                        commandComplete=true;
                        commandSuccess=true;
                        ivytime end;
                        end.setToNow();
                        /* ivytime */ duration = end-start;

                        // Post process config gather

                        auto element_it = p_Hitachi_RAID_subsystem->configGatherData.data.find("LDEV");
                        if (element_it == p_Hitachi_RAID_subsystem->configGatherData.data.end())
                        {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - post-processing configuration gather data, gathered configuration did not have the LDEV element type" << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                        }
                        for (auto& LDEV_pear : element_it->second)
                        {
                            // for every LDEV in the subsystem config, if it's a Pool Vol, insert it in the list of pool vols

                            const std::string& subsystem_LDEV = LDEV_pear.first;
//                            std::map
//                            <
//                                std::string, // Metric, e.g. "drive_type"
//                                metric_value
//                            >& subsystem_LDEV_metrics = LDEV_pear.second;

                            //{std::ostringstream o; o << "Looking for pool vols, examining " << subsystem_LDEV << std::endl;log(logfilename,o.str());}

                            auto metric_it = LDEV_pear.second.find("Pool_Vol");

                            if (metric_it != LDEV_pear.second.end())
                            {
                                //{std::ostringstream o; o << "it's a pool vol. " << std::endl; log(logfilename,o.str());}
                                const std::string& pool_ID = LDEV_pear.second["Pool_ID"].string_value();
                                std::pair
                                <
                                    const std::string, // Configuration element instance, e.g. 00:FF
                                    std::map
                                    <
                                        std::string, // Metric, e.g. "drive_type"
                                        metric_value
                                    >
                                >*p;

                                p = & LDEV_pear;

                                if (routine_logging)
                                {
                                    std::ostringstream o;
                                    o << "Inserting Pool Vol " << subsystem_LDEV << " into pool_vols[pool ID \"" << pool_ID << "\"]." << std::endl;
                                    log(logfilename,o.str());
                                }

                                p_Hitachi_RAID_subsystem->pool_vols[pool_ID].insert(p);
                            }
                            //else
                            //{
                            //    {std::ostringstream o; o << "it's not a pool vol. " << std::endl; log(logfilename,o.str());}
                            //}
                        }

                        if (routine_logging)
                        {
                            std::ostringstream o;
                            o << "pool_vols.size() = " << p_Hitachi_RAID_subsystem->pool_vols.size() << std::endl;
                            for (auto& pear : p_Hitachi_RAID_subsystem->pool_vols)
                            {
                                const std::string& poolid = pear.first;
                                std::set     // Set of pointers to pool volume instances in configGatherData
                                <
                                    std::pair
                                    <
                                        const std::string, // Configuration element instance, e.g. 00:FF
                                        std::map
                                        <
                                            std::string, // Metric, e.g. "total I/O count"
                                            metric_value
                                        >
                                    >*
                                >& pointers = pear.second;

                                o << "Pool ID \"" << poolid << "\", Pool Vols = " << std::endl;

                                for (auto& p : pointers)
                                {
                                    o << "Pool ID \"" << poolid << "\", Pool Vol = " << p->first << ", Drive_type = " << p->second["Drive_type"].string_value() << std::endl;
                                }
                            }
                            log(logfilename,o.str());
                        }

                        // Until such time as we print the GatherData csv files, we log to confirm at least the gather is working

                        if (routine_logging)
                        {
                            std::ostringstream o;
                            o << "Completed config gather for subsystem serial_number=" << p_Hitachi_RAID_subsystem->serial_number << ", gather time = " << duration.format_as_duration_HMMSSns() << std::endl;
                            //o << p_Hitachi_RAID_subsystem->configGatherData;
                            log(logfilename, o.str());
                        }

                        s_lk.unlock();
                        master_slave_cv.notify_all();

                        continue;

                        // "get config" - end
                    }
                    else if (0==std::string("gather").compare(commandString))
                    {

                        if (routine_logging)
                        {
                            std::ostringstream o;
                            o << "\"gather\" command received.  There are data from " << p_Hitachi_RAID_subsystem->gathers.size() << " previous gathers." << std::endl;
                            log(logfilename,o.str());
                        }

                        // add a new GatherData on the end of the Subsystem's chain.
                        GatherData gd;
                        p_Hitachi_RAID_subsystem->gathers.push_back(gd);


                        // run whatever ivy_cmddev commands we need to, and parse the output pointing at the current GatherData object.
                        GatherData& currentGD = p_Hitachi_RAID_subsystem->gathers.back();
                        ivytime gatherStart;

                        gatherStart.setToNow();

                        try
                        {
                            send_and_get_OK("get CLPRdetail");

                            try
                            {
                                process_ivy_cmddev_response(currentGD, gatherStart);
                            }
                            catch (std::invalid_argument& iaex)
                            {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get CLPRdetail\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                            catch (std::runtime_error& reex)
                            {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get CLPRdetail\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "\"get CLPRdetail\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            master_slave_cv.notify_all();
                            return;
                        }

                        gatherStart.setToNow();
                        try
                        {
                            send_and_get_OK("get MP_busy");
                            try
                            {
                                process_ivy_cmddev_response(currentGD, gatherStart);
                            }
                            catch (std::invalid_argument& iaex)
                            {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get MP_busy\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                            catch (std::runtime_error& reex)
                            {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get MP_busy\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "\"get MP_busy\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            master_slave_cv.notify_all();
                            return;
                        }


                        gatherStart.setToNow();
                        try
                        {
                            send_and_get_OK("get LDEVIO");
                            try
                            {
                                process_ivy_cmddev_response(currentGD, gatherStart);
                            }
                            catch (std::invalid_argument& iaex)
                            {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get LDEVIO\"), std::invalid_argument saying \"" << iaex.what() << "\"." << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                            catch (std::runtime_error& reex)
                            {
                                std::ostringstream o;
                                o << "pipe_driver_subthread for remote ivy_cmddev CLI - failed parsing output from command (\"get LDEVIO\"), std::runtime_error saying \"" << reex.what() << "\"." << std::endl;
                                log(logfilename,o.str());
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "\"get LDEVIO\" command sent to ivy_cmddev failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            master_slave_cv.notify_all();
                            return;
                        }


                        // Post-processing after gather to fill in derived metrics.


                        if (p_Hitachi_RAID_subsystem->gathers.size() > 1)
                        {
                            // post processing for the end of a subinterval (not for the t=0 gather)

                            GatherData& lastGD = p_Hitachi_RAID_subsystem->gathers[p_Hitachi_RAID_subsystem->gathers.size() - 2 ];

                            // compute & store MP % busy from MP counter values

                            auto this_element_it = currentGD.data.find(std::string("MP_core"));
                            auto last_element_it =    lastGD.data.find(std::string("MP_core"));

                            if (this_element_it != currentGD.data.end() && last_element_it != lastGD.data.end()) // processing MP_core from 2nd gather on
                            {
                                std::map<uint64_t, RunningStat<ivy_float, ivy_int>> mpu_busy;

                                for (auto this_instance_it = this_element_it->second.begin(); this_instance_it != this_element_it->second.end(); this_instance_it++)
                                {
                                    // for each MP_core instance

                                    const std::string& location = this_instance_it->first;

                                    auto last_instance_it = last_element_it->second.find(location);

                                    if (last_instance_it == last_element_it->second.end() )
                                    {
                                        throw std::runtime_error(std::string("pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter - corresponding MP_core not found in previous gather for current gather location =\"") + location + std::string("\"."));
                                    }

                                    auto this_elapsed_it = this_instance_it->second.find(std::string("elapsed_counter"));
                                    auto last_elapsed_it = last_instance_it->second.find(std::string("elapsed_counter"));
                                    auto this_busy_it    = this_instance_it->second.find(std::string("busy_counter"));
                                    auto last_busy_it    = last_instance_it->second.find(std::string("busy_counter"));

                                    if (this_elapsed_it == this_instance_it->second.end())
                                    {
                                        std::ostringstream o;
                                        o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter -"
                                            << "this gather instance " << this_instance_it->first << " does not have has metric_value elapsed_counter." << std::endl;
                                        log(logfilename,o.str());
                                        throw std::runtime_error(o.str());
                                    }

                                    if (last_elapsed_it == last_instance_it->second.end())
                                    {
                                        std::ostringstream o;
                                        o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter -"
                                            << "last gather instance " << last_instance_it->first << " does not have has metric_value elapsed_counter." << std::endl;
                                        log(logfilename,o.str());
                                        throw std::runtime_error(o.str());
                                    }

                                    if (this_busy_it == this_instance_it->second.end())
                                    {
                                        std::ostringstream o;
                                        o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter -"
                                            << "this gather instance " << this_instance_it->first << " does not have has metric_value busy_counter." << std::endl;
                                        log(logfilename,o.str());
                                        throw std::runtime_error(o.str());
                                    }

                                    if (last_busy_it == last_instance_it->second.end())
                                    {
                                        std::ostringstream o;
                                        o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter -"
                                            << "last gather instance " << last_instance_it->first << " does not have has metric_value busy_counter." << std::endl;
                                        log(logfilename,o.str());
                                        throw std::runtime_error(o.str());
                                    }

                                    uint64_t this_elapsed = this_elapsed_it->second.uint64_t_value("this_elapsed_it->second"); // throws std::invalid_argument
                                    uint64_t last_elapsed = last_elapsed_it->second.uint64_t_value("last_elapsed_it->second"); // throws std::invalid_argument
                                    uint64_t this_busy    = this_busy_it   ->second.uint64_t_value("this_busy_it   ->second"); // throws std::invalid_argument
                                    uint64_t last_busy    = last_busy_it   ->second.uint64_t_value("last_busy_it   ->second"); // throws std::invalid_argument

                                    std::ostringstream os;
                                    if (this_elapsed<= last_elapsed || this_busy < last_busy)
                                    {
                                        std::ostringstream o;
                                        o 	<< "pipe_driver_subthread.cpp: Computation of subsystem MP % busy as delta busy counter divided by delta elapsed counter - "
                                            << "elapsed_counter this gather = " << this_elapsed << ", elapsed_counter last gather = " << last_elapsed
                                            << ", busy_counter this gather = " << this_busy << ", busy_counter last gather = " << last_busy << std::endl
                                            << "Elapsed counter did not increase, or busy counter decreased";
                                        log(logfilename,o.str());
                                        throw std::runtime_error(o.str());
                                    }
                                    std::ostringstream o;
                                    ivy_float busy_percent = 100. * ( ((ivy_float) this_busy) - ((ivy_float) last_busy) ) / ( ((ivy_float) this_elapsed) - ((ivy_float) last_elapsed) );
                                    o << std::fixed <<  std::setprecision(3) << busy_percent  << "%";
                                    auto& metric_value = this_instance_it->second["busy_percent"];
                                    metric_value.start = this_elapsed_it->second.start;
                                    metric_value.complete = this_elapsed_it->second.complete;
                                    metric_value.value = o.str();
                                    currentGD.data["MP_busy"]["all"][location] = metric_value;

                                    auto this_MPU_it = this_instance_it->second.find(std::string("MPU"));
                                    if (this_MPU_it != this_instance_it->second.end())
                                    {
                                        uint64_t mpu_number = this_MPU_it->second.uint64_t_value("this_MPU_it->second"); // throws std::invalid_argument
                                        mpu_busy[mpu_number].push(busy_percent);
                                    }
                                } // for each location MP10-00
                                for (auto& pear : mpu_busy)
                                {
                                    std::ostringstream m;
                                    m << pear.first; // MPU number
                                    auto& mv = currentGD.data["MPU"][m.str()];
                                    {
                                        std::ostringstream v;
                                        v <<                                       pear.second.count();
                                        mv["count"].value = v.str();
                                    }
                                    {
                                        std::ostringstream v;
                                        v << std::fixed << std::setprecision(3) << pear.second.avg() << "%";
                                        mv["avg_busy"].value = v.str();
                                    }
                                    {
                                        std::ostringstream v;
                                        v << std::fixed << std::setprecision(3) << pear.second.min() << "%";
                                        mv["min_busy"].value = v.str();
                                    }
                                    {
                                        std::ostringstream v;
                                        v << std::fixed << std::setprecision(3) << pear.second.max() << "%";
                                        mv["max_busy"].value = v.str();
                                    }
                                }
                            } //  processing MP_core from 2nd gather on


                            // post-process LDEV I/O data only starts from third subinterval, subinterval #1
                            // because the t=0 gather is asynchronous from "Go!", so at the end of subinterval #0
                            // there is no valid delta-DKC time yet.  The calculations work starting the 3rd subinterval.


                            // Note that the suffix "_count" at the end of a metric name is significant.

                            // Metric names ending in _count are [usually cumulative] counters that require some
                            // further processing before they are "consumeable".

                            // So in the code below, we create whatever derived metrics we want from the raw collected data, including the counts.

                            // Metrics whose name ends in _count are ignored when printing csv file titles and data lines.

                            // Even though the _count metrics don't print in by-step subsystem performance csv files, they are retained in the data structures
                            // so that you can easily interpret the raw counter values over any time period.

                            // This makes it easy for a DynamicFeedbackController to get an average value over a subinterval sequence by looking at the values
                            // of raw cumulative counters at the beginning and end of the sequence and dividing the delta by the elapsed time.

                            // Note that the way ivy_cmddevice operates, cumulative counter values are monotonically increasing
                            // from the time that the ivy_cmddev executable starts.  This is because ivy_cmddev extends every
                            // RMLIB unsigned 32 bit cumulative counter value that is subject to rollovers to a 64-bit unsigned value
                            // using as the upper 32 bits the number of rollovers that ivy_cmddev has seen for this particular counter.

                            m_s.rollups.not_participating.push_back(subsystem_summary_data());

                            void rollup_Hitachi_RAID_data(const std::string&, Hitachi_RAID_subsystem*, GatherData& currentGD, GatherData& lastGD);

                            if (p_Hitachi_RAID_subsystem->gathers.size() > 2) rollup_Hitachi_RAID_data(logfilename, p_Hitachi_RAID_subsystem, currentGD, lastGD);

                            ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


                            // Filter & post subsystem performance data by RollupInstance

                            //- As each gather is post-processed for a Subsystem, after we have posted derived metrics like MPU busy:
                            //	- if gather is not for t=0
                            //		- for each RollupType, for each RollupInstance
                            //			- add a subsystem_summary_data object for this subinterval
                            //			- for each summary element type ("MP_core", "CLPR", ...) master_stuff::subsystem_summary_metrics
                            //				- for each instance of the element, e.g. MP_core="MP10-00"
                            //					- if the subsystem serial number and attribute name match the RollupInstance
                            //						- for each metric
                            //							- post values -.




                        } // if (p_Hitachi_RAID_subsystem->gathers.size() > 1)
                        else
                        {
                            // post-processing for the t=0 gather

                            // Weirdness here is that some configuration information is collected with performance data so we copy these over to the config gather
                            // so that from subinterval 1 on the DFC will see these metrics in the config data.

                            GatherData& gd = p_Hitachi_RAID_subsystem->gathers[0];
                            auto element = gd.data.find("subsystem");
                            if (element != gd.data.end())
                            {
                                auto instance = element->second.find("subsystem");
                                if (instance != element->second.end())
                                {
                                    // It's possible that we might record "subsystem=subsystem" performance data (like the old cache path busy metric that was average over whole subsystem).

                                    // Therefore each subsystem=subsystem metric that is a constant config metric is individually copied over.

                                    auto metric = instance->second.find("max_MP_io_buffers");
                                    if (metric != instance->second.end())
                                    {
                                        p_Hitachi_RAID_subsystem->configGatherData.data[element->first][instance->first][metric->first] = metric->second;
                                    }
                                    metric = instance->second.find("total_LDEV_capacity_512_sectors");
                                    if (metric != instance->second.end())
                                    {
                                        p_Hitachi_RAID_subsystem->configGatherData.data[element->first][instance->first][metric->first] = metric->second;
                                    }
                                }
                            }

                        }

                        commandComplete=true;
                        commandSuccess=true;
                        commandErrorMessage.clear();
                        ivytime end;
                        end.setToNow();
                        /* ivytime */ duration = end-start;

                        // Until such time as we print the GatherData csv files, we log to confirm at least the gather is working

                        if (routine_logging)
                        {
                            std::ostringstream o;
                            o << "Completed gather for subsystem serial_number=" << p_Hitachi_RAID_subsystem->serial_number << ", gather time = " << duration.format_as_duration_HMMSSns() << std::endl;
                            //o << currentGD;
                            log(logfilename, o.str());
                        }

                        s_lk.unlock();
                        master_slave_cv.notify_all();

                        continue;
                    }
                    else
                    {
                        // if we do not recognize the command
                        std::ostringstream o;
                        o << "ivy_cmddev pipe_driver_subthread failed to recognize ivymaster command \"" << commandString << "\"." << std::endl;
                        std::cout << o.str();
                        log(logfilename,o.str());
                        kill_ssh_and_harvest();
                        commandComplete=true;
                        commandSuccess=false;
                        commandErrorMessage = o.str();
                        dead=true;
                        s_lk.unlock();
                        master_slave_cv.notify_all();
                        return;
                    }

                    // end processing a command for a remote ivy_cmddev
                }
                else
                // process commands to interact with a remote ivyslave instance
                {
                    if (0==std::string("[CreateWorkload]").compare(commandString))
                    {
                        ostringstream utterance;
                        utterance << "[CreateWorkload]" << commandWorkloadID << "[Parameters]" << commandIogeneratorParameters << std::endl;

                        try
                        {
                            send_and_get_OK(utterance.str());
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "[CreateWorkload] at remote end failed failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }
                        commandFinish.setToNow();
                        commandSuccess=true;

                    }
                    else if (0==std::string("[EditWorkload]").compare(commandString))
                    {

                        ostringstream utterance;
                        utterance << "[EditWorkload]" << commandListOfWorkloadIDs.toString() << "[Parameters]" << commandIogeneratorParameters << std::endl;

                        try
                        {
                            send_and_get_OK(utterance.str());
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "[EditWorkload] at remote end failed failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }
                        commandFinish.setToNow();
                        commandSuccess=true;
                    }
                    else if (0==std::string("[DeleteWorkload]").compare(commandString))
                    {

                        ostringstream utterance;
                        utterance << "[DeleteWorkload]" << commandWorkloadID << std::endl;
                        try
                        {
                            send_and_get_OK(utterance.str());
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "[DeleteWorkload] at remote end failed failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }
                        commandFinish.setToNow();
                        commandSuccess=true;
                    }
                    else if (0==std::string("Catch in flight I/Os").compare(commandString))
                    {

                        ostringstream utterance;
                        utterance << "Catch in flight I/Os" << std::endl;
                        try
                        {
                            send_and_get_OK(utterance.str(),ivytime(5*m_s.subinterval_seconds));
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "\"Catch in flight I/Os\" at remote end failed failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }
                        commandFinish.setToNow();
                        commandSuccess=true;
                    }
                    else if
                    (
                        startsWith(commandString,std::string("Go!"))
                        // In "Go!<5,0>"  the <5,0> is the subinterval length as an ivytime::toString() representation - <seconds,nanoseconds>
                        || 0==std::string("continue").compare(commandString)
                        || 0==std::string("cooldown").compare(commandString)
                        || 0==std::string("stop").compare(commandString)

                        // No matter which one of these commands we got, we send it and then we gather the subinterval result lines.
                    )
                    {
                        std::string hostCPUline;
                        //*debug*/{std::ostringstream o; o << "About to send out command to ivyslave - \"" << commandString << "\" with timeout in seconds = " << (m_s.subintervalLength.getlongdoubleseconds()+5) << std::endl; std::cout << o.str(); log (logfilename, o.str());}

                        // Here we separate out sending the Go!/continue/cooldown/stop for which we get an OK.
                        // Originally, after sending the Go!/continue/cooldown/stop, we simply waited for the [CPU] line to appear.
                        // Now we get an OK after Go!/continue/cooldown/stop command has been posted to all the test
                        // host's workload threads.  This is so that we can print a message showing how much time
                        // we had in hand, i.e. how much margin we have in terms of the length of the subinterval in seconds.

                        try
                        {
                            send_and_get_OK(commandString);
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "Sending \"" << commandString << "\" to ivyslave failed failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }

                        ivytime allworkloadthreadsposted;
                        allworkloadthreadsposted.setToNow();

                        if (allworkloadthreadsposted > m_s.subintervalEnd)
                        {
                            std::ostringstream o;
                            o << "For host " << ivyscript_hostname << ", ivyslave failed to confirm that the \"" << commandString << "\" command was posted to all workload threads before the end of the subinterval." << std::endl;
                            log(logfilename,o.str());
                            std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true;
                            commandSuccess=false;
                            commandErrorMessage = o.str();
                            dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }

                        time_in_hand_before_subinterval_end = m_s.subintervalEnd - allworkloadthreadsposted;

                        try
                        {
                            hostCPUline = send_and_clip_response_upto( std::string("get subinterval result"), std::string("[CPU]"), ivytime(m_s.subinterval_seconds + m_s.catnap_time_seconds) );
                        }
                        catch (std::runtime_error& reex)
                        {
                            std::ostringstream o;
                            o << "Sending \"get subinterval result\" to ivyslave failed failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }

                        ivytime now;

                        now.setToNow();

                        if (now > m_s.nextSubintervalEnd)
                        {
                            std::ostringstream o;
                            o << "Subinterval length parameter subinterval_seconds is too short.  For host " << ivyscript_hostname
                                << ", ivyslave received the [CPU] line for the previous subinterval after the current subinterval was already over." << std::endl;
                            log(logfilename,o.str());
                            std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true;
                            commandSuccess=false;
                            commandErrorMessage = o.str();
                            dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }


                        if (!cpudata.fromString(hostCPUline))
                        {
                            std::ostringstream o;
                            o << "For host " << ivyscript_hostname << ", [CPU] line failed to parse correctly.  Call to avgcpubusypercent::fromString(\"" << hostCPUline << "\") failed." << std::endl;
                            log(logfilename,o.str());
                            std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true;
                            commandSuccess=false;
                            commandErrorMessage = o.str();
                            dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }
                        {
                            std::unique_lock<std::mutex> s_lk(m_s.master_mutex);

                            m_s.cpu_by_subinterval[m_s.rollups.current_index()].add(toLower(ivyscript_hostname),cpudata);
                        }
                        std::string detail_line="";
                        while (true)
                        {
                            try
                            {
                                detail_line = get_line_from_pipe(ivytime(2), std::string("get iogenerator detail line"));
                            }
                            catch (std::runtime_error& reex)
                            {
                                std::ostringstream o;
                                o << "For host " << ivyscript_hostname << ", get_line_from_pipe() for subsystem detail line failed saying \"" << reex.what() << "\"." << std::endl;
                                log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                                kill_ssh_and_harvest();
                                commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }

                            now.setToNow();
                            if (now > m_s.nextSubintervalEnd)
                            {
                                std::ostringstream o;
                                o << "Subinterval length parameter subinterval_seconds is too short.  For host " << ivyscript_hostname
                                    << ", ivyslave received a workload thread detail line for the previous subinterval after the current subinterval was already over." << std::endl;
                                log(logfilename,o.str());
                                std::cout << o.str();
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }

                            //*debug*/log(logfilename, std::string("debug got detail line = ") + detail_line + std::string("\n"));

                            //ivyslave: ostringstream o;
                            //ivyslave: o << '<' << pear.second->workloadID << '>'
                            //ivyslave: 	<< (pear.second->subinterval_array)[next_to_harvest_subinterval].input.toString()
                            //ivyslave: 	<< (pear.second->subinterval_array)[next_to_harvest_subinterval].output.toString() << std::endl;

                            if ('<' != detail_line[0])
                            {
                                std::ostringstream o;
                                o << "For host " << ivyscript_hostname << ", bogus iogenerator detail line didn\'t even start with \'<\'. bogus detail line was \"" << detail_line << "\"." << std::endl;
                                log(logfilename,o.str());
                                std::cout << o.str();
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }
                            if (startsWith(detail_line,std::string("<end>"))) break;

                            WorkloadID detailWID;
                            IogeneratorInput detailInput;
                            SubintervalOutput detailOutput;

                            std::istringstream detailstream(detail_line);

                            std::string err_msg;
                            if (!parseWorkloadIDfromIstream(err_msg, detailstream, detailWID))
                            {
                                std::ostringstream o;
                                o << "For host " << ivyscript_hostname << ", internal programming error - detail line WorkloadID failed to parse correctly or doesn't exist - \"" <<  detail_line << "\"." << std::endl << err_msg << std::endl;
                                log(logfilename,o.str());
                                std::cout << o.str();
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }

                            if (!detailInput.fromIstream(detailstream,logfilename))
                            {
                                std::ostringstream o;
                                o << "For host " << ivyscript_hostname << ", internal programming error - detail line IogeneratorInput failed to parse correctly - \"" <<  detail_line << "\"." << std::endl << err_msg << std::endl;
                                log(logfilename,o.str());
                                std::cout << o.str();
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }

                            if (!detailOutput.fromIstream(detailstream))
                            {
                                std::ostringstream o;
                                o << "For host " << ivyscript_hostname << ", internal programming error - detail line SubintervalOutput failed to parse correctly - \"" <<  detail_line << "\"." << std::endl << err_msg << std::endl;
                                log(logfilename,o.str());
                                std::cout << o.str();
                                kill_ssh_and_harvest();
                                commandComplete=true;
                                commandSuccess=false;
                                commandErrorMessage = o.str();
                                dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }

                            {
                                std::unique_lock<std::mutex> s_lk(m_s.master_mutex);

                                std::string my_error_message;

                                if (!m_s.rollups.add_workload_detail_line(my_error_message, detailWID, detailInput, detailOutput))
                                {
                                    std::ostringstream o;
                                    o << "Rollup of detail line failed. \"" << detail_line << "\" - " << my_error_message << std::endl;
                                    log(logfilename, o.str());
                                    kill_ssh_and_harvest();
                                    commandComplete = true;
                                    commandSuccess = false;
                                    commandErrorMessage = o.str();
                                    dead=true;
                                    s_lk.unlock();
                                    m_s.master_cv.notify_all();
                                    return;
                                }
                            }

                            m_s.master_cv.notify_all();
                        }

                        now.setToNow();
                        if (now > m_s.nextSubintervalEnd)
                        {
                            std::ostringstream o;
                            o << "Subinterval length parameter subinterval_seconds is too short.  For host " << ivyscript_hostname
                                << ", ivyslave received a workload thread detail line for the previous subinterval after the current subinterval was already over." << std::endl;
                            log(logfilename,o.str());
                            std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true;
                            commandSuccess=false;
                            commandErrorMessage = o.str();
                            dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }

                        commandComplete=true;
                        commandSuccess=true;
                    }
                    else
                    {
                        // if we do not recognize the command
                        log(logfilename,std::string("slave driver failed to recognize ivymaster command \"")+commandString+std::string("\".\n"));
                        kill_ssh_and_harvest();
                        dead=true;
                        commandComplete=true;
                        commandSuccess=false;
                        s_lk.unlock();
                        master_slave_cv.notify_all();
                        return;
                    }
                    // end - processing ivyslave command
                }


                commandComplete=true;
            }
            master_slave_cv.notify_all();
        }

        try
        {
            send_string("exit\n");
        }
        catch (std::runtime_error& reex)
        {
            std::ostringstream o;
            o << "For host " << ivyscript_hostname << ", sending \"exit\" command failed saying \"" << reex.what() << "\"." << std::endl;
            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
            kill_ssh_and_harvest();
            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
            master_slave_cv.notify_all();
            return;
        }


        ivytime one_second(1,0);
        nanosleep(&(one_second.t),NULL);

        // finished talking to the child

        // harvest the child's pid so it won't be ... a zombie!
        harvest_pid();

        // Once our work is complete, we issue an scp command to copy the logs back from the remote host.

        std::ostringstream copycmd;
        copycmd << "/usr/bin/scp " << IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER << "/ivyslave." << ivyscript_hostname << ".log* " << m_s.testFolder;
        log(logfilename, std::string("copying logs from remote host \"")+copycmd.str()+std::string("\"\n"));
        if ( 0 == system(copycmd.str().c_str()) )
        {
            if (routine_logging) { log (logfilename, std::string("copy command successful.")); }

        }
        else
        {
            ostringstream o;
            o << "copy command failed - rc = " << errno << " " << strerror(errno) << std::endl;
            log(logfilename, o.str());
        }

    }
    else
    {
        ostringstream logmsg;
        logmsg << "fork() to create child subthread to issue ssh failed errno = " << errno << " - " << strerror(errno) << std::endl;
        log(logfilename, logmsg.str());

        // failed to create child
        close(pipe_driver_subthread_to_slave_pipe[PIPE_READ]);
        close(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE]);
        close(slave_to_pipe_driver_subthread_pipe[PIPE_READ]);
        close(slave_to_pipe_driver_subthread_pipe[PIPE_WRITE]);
    }
    return;
}

// ivy_cmddev section

void pipe_driver_subthread::get_token()
{
    token.clear();
    token_valid_as_value=false;
    token_identifier=false;
    token_number=false;
    token_quoted_string=false;

    // First we step over whitespace.  Here when we hit the end of the line, we
    // get another line and keep scanning for a non-whitespace first character of a token.

    // Throw std::runtime_error if we can't get another line.

    while (true)
    {
        while ( parse_cursor < parse_line.length() && isspace(parse_line[parse_cursor]) ) parse_cursor++;
        if (parse_cursor < parse_line.length()) break;
        parse_line = get_line_from_pipe(ivytime(MAXWAITFORIVYCMDDEV), "reading a line of a response from ivy_cmddev");
        line_number++;
        parse_cursor=0;
        complete.setToNow();
    }

    // Push back first character of token

    char c = parse_line[parse_cursor++];
    token.push_back(c);

    // If that first character is alphabetic, we are parsing an identifier, and we keep pushing back additional alphanumeric or underscore characters

    if (isalpha(c))
    {
        while (parse_cursor < parse_line.length())
        {
            c = parse_line[parse_cursor];
            if ( (!isalnum(c)) && ('_' != c) ) break;
            token.push_back(c);
            parse_cursor++;
        }
        token_identifier=true;
        token_valid_as_value=true;
        return;
    }

    // If that first character is a single or double quote mark, we parse a character string constant.

    // Surrounding quote marks are stripped off.

    // A quoted string must be entirely contained in one line - we can revisit this, but thought this would catch errors earlier.

    // Escaped single quote \', double quote \" as well as \r \n, \t characters are un-escaped (captured??)

    if ('\'' == c || '\"' == c)
    {
        char quote = c;
        token.clear();

        while (true)
        {
            if (parse_cursor >= parse_line.length())
            {
                std::string msg="get_token() - ran off end of line looking for close quote.";
                log(logfilename, msg);
                throw std::invalid_argument(msg);
            }

            c = parse_line[parse_cursor++];

            if (c == quote) break;

            if ('\\' == c && parse_cursor < (-1+parse_line.length()))
            {
                char c2 = parse_line[parse_cursor+1];
                if ('\'' == c2 || '\"' == c2) // escaped quote character
                {
                    token.push_back(c2);
                    parse_cursor++;
                    continue;
                }
                if ('r' == c2)
                {
                    token.push_back('\r');
                    parse_cursor++;
                    continue;
                }
                if ('n' == c2)
                {
                    token.push_back('\n');
                    parse_cursor++;
                    continue;
                }
                if ('t' == c2)
                {
                    token.push_back('\t');
                    parse_cursor++;
                    continue;
                }
            }
            token.push_back(c);
        }
        token_quoted_string=true;
        token_valid_as_value=true;

        if (std::regex_match(token,identifier_regex))
        {
            token_identifier=true;
        }
        else if (std::regex_match(token,number_regex))
        {
            token_number=true;
        }

        return;
    }

    // If that first character is a period or a digit, parse nnn or .n or n. or n.n

    if ('.' == c || isdigit(c))
    {
        bool have_decimal_point {'.' == c};
        bool have_digit { 0!=isdigit(c) }; // isdigit returns an int.

        // more digits before decimal point?
        while ( (!have_decimal_point) && parse_cursor < parse_line.length() )
        {
            c = parse_line[parse_cursor];
            if (!isdigit(c)) break;
            have_digit=true;
            token.push_back(c);
            parse_cursor++;
        }

        if ((!have_decimal_point) && parse_cursor < parse_line.length() && '.' == c )
        {
            have_decimal_point=true;
            token.push_back(c);
            parse_cursor++;
        }

        while (have_decimal_point && parse_cursor<parse_line.length())
        {
            c = parse_line[parse_cursor];
            if (!isdigit(c)) break;
            have_digit=true;
            token.push_back(c);
            parse_cursor++;
        }

        if (have_digit)
        {
            token_valid_as_value=true;
            token_number=true;
            return;
        }
    }

    return;
}


void pipe_driver_subthread::process_ivy_cmddev_response(GatherData& gd, ivytime start) // throws std::invalid_argument, std::runtime_error
// Sorry - realize that this is not the best parser - come back and re-do later.
{

    parse_line = get_line_from_pipe(ivytime(MAXWAITFORIVYCMDDEV), "reading a line of a response from ivy_cmddev");
    line_number=1;
    parse_cursor=0;
    complete.setToNow();

    // overall opening {

    get_token();

    if ("{" != token)
    {
        std::ostringstream o;
        o << "Failed looking for overall opening \'{\'.  Instead first token is \"" << token << "\"." << std::endl;
        log(logfilename, o.str());
        throw std::invalid_argument(o.str());
    }

    while (true)
        // for each:
        // { element="LDEV", instance="00:45", data = { total_count=3487324, read_hit_count=57238743 } }
    {
        get_token();

        while ("," == token) get_token();

        if ("}" == token) break; // closing } overall

        if ("{" != token)
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed looking for opening \'{\' of { element=xxx, instance=yyy, data={attribute=value, ...} }.  Instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }

        get_token();
        if (!stringCaseInsensitiveEquality(token,std::string("element")))
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed looking for \"element\" in { element=xxx, instance=yyy, data={attribute=value, ...} }.  Instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }

        get_token();
        if ("=" != token)
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed looking for equals sign \'=\' after \"element\" in { element=xxx, instance=yyy, data={attribute=value, ...} }.  Instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }

        get_token();
        if (!token_identifier)
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed - \"element\" value must be an identifer starting with an alphabetic and continuing alphanumerics/underscores - instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }
        element = token;

        get_token();
        while ("," == token) get_token();
        if (!stringCaseInsensitiveEquality(token,std::string("instance")))
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed looking for \"instance\" in { element=xxx, instance=yyy, data={attribute=value, ...} }.  Instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }

        get_token();
        if ("=" != token)
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed looking for equals sign \'=\' after \"instance\" in { element=xxx, instance=yyy, data={attribute=value, ...} }.  Instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }

        get_token();
        if (!token_valid_as_value)
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed - \"instance\" value must be an identifer starting with an alphabetic and continuing alphanumerics/underscores, a number, or a quoted string - instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }
        instance = token;

        get_token();
        while ("," == token) get_token();
        if (!stringCaseInsensitiveEquality(token,std::string("data")))
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed looking for \"data\" in { element=xxx, instance=yyy, data={attribute=value, ...} }.  Instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }

        get_token();
        if ("=" != token)
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed looking for equals sign \'=\' after \"data\" in { element=xxx, instance=yyy, data={attribute=value, ...} }.  Instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }

        get_token();
        if ("{" != token) // opening '{' for attribute=value pairs
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed to find opening '{' for attribute=value pairs - instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }

        while (true) // for attribute=value pairs
        {
            get_token();
            while ("," == token) get_token();
            if ("}" == token) break; // closing } for attribute pairs

            if (!token_identifier)
            {
                std::ostringstream o;
                o << "process_ivy_cmddev_response() failed - attribute name must be an identifer starting with an alphabetic and continuing alphanumerics/underscores - instead token is \"" << token << "\"." << std::endl;
                log(logfilename, o.str());
                throw std::invalid_argument(o.str());
            }
            attribute = token;

            get_token();
            if ("=" != token)
            {
                std::ostringstream o;
                o << "process_ivy_cmddev_response() failed looking for equals sign \'=\' after attribue name \"" << attribute << "\" - instead token is \"" << token << "\"." << std::endl;
                log(logfilename, o.str());
                throw std::invalid_argument(o.str());
            }

            get_token();
            if (!token_valid_as_value)
            {
                std::ostringstream o;
                o << "process_ivy_cmddev_response() failed - for attribute \"" << attribute << "\", value must be an identifer starting with an alphabetic and continuing alphanumerics/underscores, a number, or a quoted string - instead token is \"" << token << "\"." << std::endl;
                log(logfilename, o.str());
                throw std::invalid_argument(o.str());
            }
            gd.data[element][instance][attribute] = metric_value(start, complete, token);
        }
        // we found closing '}' for attribute pairs
        get_token();
        while ("," == token) get_token();
        if ("}" != token)
        {
            std::ostringstream o;
            o << "process_ivy_cmddev_response() failed to find closing \'}\' for { element = xxxxx, instance=yyyy, data= { attribute=value, ...} } - instead token is \"" << token << "\"." << std::endl;
            log(logfilename, o.str());
            throw std::invalid_argument(o.str());
        }
    }
    // we found closing '}'

//*debug*/{
//*debug*/	std::ostringstream o;
//*debug*/	o << "debug - pipe_driver_subthread::process_ivy_cmddev_response() - after processing ivy_cmddev output, Gather data contents are :" << std::endl << gd << std::endl;
//*debug*/	log(logfilename, o.str());
//*debug*/	std::cout << o.str();
//*debug*/}

}






