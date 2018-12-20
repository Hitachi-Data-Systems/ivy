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

// invoked "ivy scriptname.ivyscript" or just "ivy scriptname"

#include "ivyhelpers.h"
#include "ivytime.h"
// SHOULD GET AROUND TO GIVING IVYTIME THE ABILITY TO PARSE AT LEAST YYYY-MM-DD HH:MM:SS AND YYYY-MM-DD HH:MM:SS.NNNNNNNNN
// ... except that regexes seem to be broken and are known to be broken in this version of libstdc++

#include "ivydefines.h"
#include "RunningStat.h"
#include "discover_luns.h"
#include "LDEVset.h"
#include "IosequencerInput.h"
#include "IosequencerInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "ivylinuxcpubusy.h"
#include "Subinterval_CPU.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "LUN.h"
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
#include "ivy_engine.h"
#include "Subinterval_detail_line.h"


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

    if (last_message_time == ivytime(0))
    {
        last_message_time.setToNow();
    }
    else
    {
        ivytime now, delta;

        now.setToNow();
        delta = now - last_message_time;
        last_message_time = now;

        if (routine_logging) { log(logfilename, format_utterance("Master",s,delta)); }
    }

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


    while(true)
    {
        remaining=(start+timeout)-now;
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
            logmsg << "<Warning> For host " << ivyscript_hostname << ", pipe_driver_subthread::read_from_pipe() - child has exited."
            << "  pid = " << ssh_pid << ", after waitpid(ssh_pid, &pid_status, WNOHANG), pid_status = 0x" << std::hex << std::uppercase << pid_status;
            log(logfilename,logmsg.str());
            // return false;  - Commented out to see if this can occur spuriously 2016-09-25
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
    ///* POLLRDHUP - (since Linux 2.6.17) Stream socket peer closed connection, or shut down writing half of connection.
    //The _GNU_SOURCE feature test macro must be defined (before including any header files) in order to obtain this definition. */
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
    std::string description, // passed by reference to called routines to receive an error message when called function returns "false"
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
        last_read_from_pipe = read_from_pipe(timeout);
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

    if (last_message_time == ivytime(0))
    {
        last_message_time.setToNow();
    }
    else
    {
        ivytime now, delta;

        now.setToNow();
        delta = now - last_message_time;
        last_message_time = now;

        if ( trace_evaluate || (routine_logging && !reading_echo_line)) { log(logfilename, format_utterance(" Slave",s,delta)); }
    }
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
            o << std::endl << std::endl
                << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" << std::endl << std::endl
                << "<Warning>[from " << ivyscript_hostname << "]" << s.substr(9,s.size()-9) << std::endl << std::endl
                << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" << std::endl << std::endl;
            log (logfilename, o.str());
            log (m_s.masterlogfile, o.str());
            std::cout << o.str();
            continue;
        }

        if ( startsWith(s, std::string("<Error>")) || startsWith(s, std::string("Error:")) ) throw std::runtime_error(s);

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

    // give other end some time to kill its iosequencer & RMLIB subthreads

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
            logmsg << " to run ivy_cmddev.";
        }
        else
        {
            logmsg << " to run ivyslave.";
        }
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

        std::string cmd, arg, hitachi_product, serial, remote_logfilename;

        if (pCmdDevLUN)
        {
            cmd = m_s.path_to_ivy_executable+ IVY_CMDDEV_EXECUTABLE;
            arg = pCmdDevLUN->attribute_value("LUN_name");
            serial = pCmdDevLUN->attribute_value("serial_number");
            hitachi_product = pCmdDevLUN->attribute_value("Hitachi Product");
            {
                ostringstream fn;
                fn << IVYSLAVELOGFOLDERROOT IVYSLAVELOGFOLDER << "/log.ivyslave." << ivyscript_hostname << ".ivy_cmddev." << p_Hitachi_RAID_subsystem->serial_number << ".txt";
                remote_logfilename = fn.str();
            }
            if (routine_logging)
            {
                std::ostringstream o;
                o << "execl(\"usr/bin/ssh\", \"ssh\", \"-t\", \"-t\", \"" << login << "\", \"" << cmd << "\", \"-log\", \"" << arg << "\", \""
                    << hitachi_product << "\", \""<< serial << "\", \"" << remote_logfilename << "\")" << std::endl;
                log(logfilename,o.str());

                execl("/usr/bin/ssh","ssh","-t","-t", login.c_str(), cmd.c_str(), "-log", arg.c_str(), hitachi_product.c_str(), serial.c_str(), remote_logfilename.c_str(), (char*)NULL);
            }
            else
            {
                execl("/usr/bin/ssh","ssh","-t","-t", login.c_str(), cmd.c_str(),         arg.c_str(), hitachi_product.c_str(), serial.c_str(), remote_logfilename.c_str(), (char*)NULL);
            }
        }
        else
        {
            cmd = m_s.path_to_ivy_executable + IVYSLAVE_EXECUTABLE;
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

        int original_pipe_capacity = fcntl(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE],F_GETPIPE_SZ);

        if (original_pipe_capacity < 0)
        {
            ostringstream logmsg;
            logmsg << "<Error> Failed getting original capacity of pipe from pipe_driver_subthread to ivyslave."
                "  errno = " << errno << " - " << strerror(errno) << std::endl;
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

        if (routine_logging)
        {
            std::ostringstream x;
            x << "Original capacity of pipe to ivyslave is " << original_pipe_capacity << std::endl;
            log(logfilename, x.str());
        }

        int resize_pipe_rc = fcntl(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE],F_SETPIPE_SZ, 128*1024);

        if (resize_pipe_rc < 0)
        {
            ostringstream logmsg;
            logmsg << "<Error> Failed setting capacity of pipe from pipe_driver_subthread to ivyslave to the value 128 Ki8."
                "  errno = " << errno << " - " << strerror(errno) << std::endl;
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

        int updated_pipe_capacity = fcntl(pipe_driver_subthread_to_slave_pipe[PIPE_WRITE],F_GETPIPE_SZ);

        if (updated_pipe_capacity < 0)
        {
            ostringstream logmsg;
            logmsg << "<Error> Failed getting updated capacity of pipe from pipe_driver_subthread to ivyslave."
                "  errno = " << errno << " - " << strerror(errno) << std::endl;
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

        if (routine_logging)
        {
            std::ostringstream x;
            x << "Updated capacity of pipe to ivyslave is " << updated_pipe_capacity << std::endl;
            log(logfilename, x.str());
        }

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

        std::string copy_of_prompt = prompt;

        std::regex whirrled_regex( R"((.+)<=\|=>(.+))");

        std::smatch entire_whirrled;
        std::ssub_match whirrled_first, whirrled_last;

        std::string whirrled_version {};

        if (std::regex_match(copy_of_prompt, entire_whirrled, whirrled_regex))
        {
            whirrled_first = entire_whirrled[1];
            whirrled_last  = entire_whirrled[2];

            prompt           = whirrled_first.str();
            whirrled_version = whirrled_last.str();

            trim(prompt);
            trim(whirrled_version);
        }

        std::string hellowhirrled("Hello, whirrled! from ");

        log(logfilename, std::string("initial prompt from remote was \"")+copy_of_prompt+std::string("\"."));

        unsigned int i;
        for (i=0; i<=(prompt.size()-hellowhirrled.size()); i++)
        {
            if ( i == (prompt.size()-hellowhirrled.size()) )
            {
                std::ostringstream o;
                o << "<Error> Remote ";
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


//*debug*/std::cout << "debug: waiting 60 seconds to allow showluns.sh run with another ivy hammering a lun, slowing the binary search for the LUN size.\n";
//*debug*/sleep(60);

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
            if (pCmdDevLUN)
            {
                std::ostringstream o;
                o << "Have the lock - adding whirrled_version \"" << whirrled_version << "\" to  m_s.command_device_etc_version which initially was \"" << m_s.command_device_etc_version << "\"." << std::endl;
                m_s.command_device_etc_version += whirrled_version;
                o << "Then m_s.command_device_etc_version became \"" << m_s.command_device_etc_version << "\"." << std::endl;
                log(logfilename, o.str());
            }

        }
        master_slave_cv.notify_all();

        // process commands from ivymaster

        bool processingCommands {true};

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
                    process_cmddev_commands(s_lk);
                }
                // end of processing command device commands

                else
                // process commands to interact with a remote ivyslave instance
                {
                    if (0==std::string("[CreateWorkload]").compare(commandString))
                    {
                        ostringstream utterance;
                        utterance << "[CreateWorkload]" << commandWorkloadID << "[Parameters]" << commandIosequencerParameters << std::endl;

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
                        utterance << "[EditWorkload]" << p_edit_workload_IDs->toString() << "[Parameters]" << commandIosequencerParameters << std::endl;

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
                    else if
                    (
                        startsWith(commandString,std::string("Go!"))
                        // In "Go!<5,0>"  the <5,0> is the subinterval length as an ivytime::toString() representation - <seconds,nanoseconds>
                        // In "Go!<5,0>-spinloop" the optional "-spinloop" tells ivyslave workload threads to continuously check for things to do without ever waiting.
                        || 0==std::string("continue").compare(commandString)
                        || 0==std::string("cooldown").compare(commandString)
                        || 0==std::string("stop").compare(commandString)

                        // No matter which one of these commands we got, we send it and then we gather the subinterval result lines.
                    )
                    {
                        // Here we separate out sending the Go!/continue/cooldown/stop for which we get an OK.
                        // Originally, after sending the Go!/continue/cooldown/stop, we simply waited for the [CPU] line to appear.
                        // Now we get an OK after Go!/continue/cooldown/stop command has been posted to all the test
                        // host's workload threads.  This is so that we can print a message showing how much time
                        // we had in hand, i.e. how much margin we have in terms of the length of the subinterval in seconds.

                        ivytime before_sending_command;
                        before_sending_command.setToNow();

                        try
                        {
                            send_and_get_OK(commandString);
                        }
                        catch (std::runtime_error& reex)
                        {
                            ivytime now;
                            now.setToNow();
                            ivytime latency = now - before_sending_command;
                            std::ostringstream o;
                            o << "At "<< latency.format_as_duration_HMMSSns() << " after sending \"" << commandString
                                << "\" to ivyslave - did not get OK - failed failed saying \"" << reex.what() << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }

                        ivytime ivyslave_said_OK;
                        ivyslave_said_OK.setToNow();

                        ivytime latency = ivyslave_said_OK - before_sending_command;

                        if (latency.getlongdoubleseconds() > (0.25 *m_s.subinterval_seconds))
                        {

                            std::ostringstream o;
                            o
                                << " for host " << ivyscript_hostname
                                << " the response \"OK\" was received with a latency of " << latency.format_as_duration_HMMSSns()
                                << " after sending \"" << commandString << "\" to ivyslave"
                                << " at " << before_sending_command.format_as_datetime_with_ns() << "."
                                << "  The OK response was received after more than 1/4 of subinterval_seconds."
                                << std::endl;
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
                        commandFinish.setToNow();
                        commandSuccess=true;
                        commandComplete=true;
                    }
                    else if ( 0 == commandString.compare(std::string("get subinterval result")))
                    {
                        std::string hostCPUline;

                        try
                        {
                            hostCPUline = send_and_clip_response_upto( std::string("get subinterval result"), std::string("[CPU]"), ivytime(1.25*m_s.subinterval_seconds) );
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

                        // we have received "get subinterval result", which means we have just moved subintervalStart/End forward
                        if (now > m_s.nextSubintervalEnd)
                        {
                            std::ostringstream o;
                            o << "<Error> Subinterval length parameter subinterval_seconds may be too short.  For host " << ivyscript_hostname
                                << ", pipe_driver_subthread received the [CPU] line for the previous subinterval after the current subinterval was already over." << std::endl;
                            o << "  This can also be caused if an ivy command device is on a subsystem port that is saturated with other (ivy) activity, making communication with the command device run very slowly." << std::endl;

                            o << "now: "  << now.format_as_datetime_with_ns() << std::endl;
                            o << m_s.subintervalStart.format_as_datetime_with_ns()
                                    << " = m_s.subintervalStart   (" << m_s.subintervalStart.duration_from_now() << " seconds from now) "
                                    << " = m_s.subintervalStart   (" << m_s.subintervalStart.duration_from(m_s.get_go) << " seconds from Go! time) "
                                    << std::endl
                                << m_s.subintervalEnd.format_as_datetime_with_ns()
                                    << " = m_s.subintervalEnd     (" << m_s.subintervalEnd.duration_from_now() << " seconds from now) "
                                    << " = m_s.subintervalEnd     (" << m_s.subintervalEnd.duration_from(m_s.get_go) << " seconds from Go! time) "
                                    << std::endl
                                << m_s.nextSubintervalEnd.format_as_datetime_with_ns()
                                    << " = m_s.nextSubintervalEnd (" << m_s.nextSubintervalEnd.duration_from_now() << " seconds from now) "
                                    << " = m_s.nextSubintervalEnd (" << m_s.nextSubintervalEnd.duration_from(m_s.get_go) << " seconds from Go! time) "
                                    << std::endl
                                ;


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

                        // read & parse detail lines.  Will separately post everything later when we get the master lock

                        std::string detail_line="";

                        std::vector<Subinterval_detail_line> parsed_detail_lines {};

                        parsed_detail_lines.clear();

                        while (true)
                        {
                            try
                            {
                                ivy_float detail_line_timeout_seconds;

                                if (m_s.last_command_was_stop)
                                {
                                    detail_line_timeout_seconds = m_s.subinterval_seconds * 1.4;
                                    // longer because "stop" command invokes "catch in flight I/Os" which can take extra time.
                                    // There is no need to be prompt reading detail lines once we are stopped.
                                }
                                else
                                {
                                    detail_line_timeout_seconds = m_s.subinterval_seconds * 0.4;
                                    // two seconds for 5 second subintervals
                                }

                                detail_line = get_line_from_pipe(ivytime(detail_line_timeout_seconds), std::string("get iosequencer detail line"));
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

                            if (now > m_s.nextSubintervalEnd)  // here when we are processing detail lines, subintervalStart/End refer to the subinterval whose detail lines we are processing
                            {
                                std::ostringstream o;
                                o << "<Error> Subinterval length parameter subinterval_seconds may be too short.  For host " << ivyscript_hostname
                                    << ", ivyslave received a workload thread detail line for the previous subinterval over one subinterval late." << std::endl;
                                o << "This can also be caused if an ivy command device is on a subsystem port that is saturated with other (ivy) activity, making communication with the command device run very slowly." << std::endl;

                                o << "now: "  << now.format_as_datetime_with_ns() << std::endl;
                                o << m_s.subintervalStart.format_as_datetime_with_ns()
                                        << " = m_s.subintervalStart   (" << m_s.subintervalStart.duration_from_now() << " seconds from now) "
                                        << " = m_s.subintervalStart   (" << m_s.subintervalStart.duration_from(m_s.get_go) << " seconds from Go! time) "
                                        << std::endl
                                    << m_s.subintervalEnd.format_as_datetime_with_ns()
                                        << " = m_s.subintervalEnd     (" << m_s.subintervalEnd.duration_from_now() << " seconds from now) "
                                        << " = m_s.subintervalEnd     (" << m_s.subintervalEnd.duration_from(m_s.get_go) << " seconds from Go! time) "
                                        << std::endl
                                    << m_s.nextSubintervalEnd.format_as_datetime_with_ns()
                                        << " = m_s.nextSubintervalEnd (" << m_s.nextSubintervalEnd.duration_from_now() << " seconds from now) "
                                        << " = m_s.nextSubintervalEnd (" << m_s.nextSubintervalEnd.duration_from(m_s.get_go) << " seconds from Go! time) "
                                        << std::endl
                                    ;


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

                            if ('<' != detail_line[0])
                            {
                                std::ostringstream o;
                                o << "For host " << ivyscript_hostname << ", bogus iosequencer detail line didn\'t even start with \'<\'. bogus detail line was \"" << detail_line << "\"." << std::endl;
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

                            parsed_detail_lines.emplace_back(Subinterval_detail_line());
                            {
                                // nested block is to get a new reference each time

                                Subinterval_detail_line& sdl = parsed_detail_lines.back();

                                WorkloadID&        detailWID    = sdl.workloadID;
                                IosequencerInput&  detailInput  = sdl.iosequencerInput;
                                SubintervalOutput& detailOutput = sdl.subintervalOutput;

                                sdl.line = detail_line;

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
                                    o << "For host " << ivyscript_hostname << ", internal programming error - detail line IosequencerInput failed to parse correctly - \"" <<  detail_line << "\"." << std::endl << err_msg << std::endl;
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
                            }
                        }

                        // end of reading & parsing detail lines.

                        // read & parse sequential fill line

                        std::string seq_fill_progress_line      {};
                        std::string seq_fill_progress_prefix    {"min_seq_fill_fraction = "};
                        std::string seq_fill_progress_remainder {};

                        try
                        {
                            seq_fill_progress_line = get_line_from_pipe(ivytime(2), seq_fill_progress_prefix);
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

                        if (!startsWith(seq_fill_progress_prefix, seq_fill_progress_line, seq_fill_progress_remainder))
                        {
                            std::ostringstream o;
                            o << "For host " << ivyscript_hostname << ", was expecting a line starting with \"" << seq_fill_progress_prefix
                                << "\" instead got the line \"" << seq_fill_progress_line << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }

                        trim (seq_fill_progress_remainder);

                        ivy_float seq_fill_progress;
                        try
                        {
                            seq_fill_progress = number_optional_trailing_percent(seq_fill_progress_remainder, seq_fill_progress_prefix );  // throws std::invalid_argument
                        }
                        catch (std::invalid_argument& iaex)
                        {
                            std::ostringstream o;
                            o << "For host " << ivyscript_hostname << ", bad numeric value \"" << seq_fill_progress_remainder << "\" after equals sign in \"" << seq_fill_progress_line << "\"." << std::endl;
                            log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                            kill_ssh_and_harvest();
                            commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                            s_lk.unlock();
                            master_slave_cv.notify_all();
                            return;
                        }

                        // end of read & parse sequential fill line

                        // read & parse latencies line
                        {
                            std::string s {};
                            std::string remainder;
                            try
                            {
                                s = get_line_from_pipe(ivytime(2), std::string("get latencies:  line"));
                            }
                            catch (std::runtime_error& reex)
                            {
                                std::ostringstream o;
                                o << "For host " << ivyscript_hostname << ", get_line_from_pipe() for latencies line failed saying \"" << reex.what() << "\"." << std::endl;
                                log(logfilename,o.str()); log(m_s.masterlogfile,o.str()); std::cout << o.str();
                                kill_ssh_and_harvest();
                                commandComplete=true; commandSuccess=false; commandErrorMessage = o.str(); dead=true;
                                s_lk.unlock();
                                master_slave_cv.notify_all();
                                return;
                            }

                            if ( !startsWith(std::string("latencies: "), s, remainder) )
                            {
                                std::ostringstream o;
                                o << "<Error> internal programming error - was expecting a line starting with \"latencies \""
                                    << ", but saw the line \"" << s << "\"." << std::endl;
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
                                std::istringstream is(remainder);

                                if
                                (
                                     (!dispatching_latency_seconds_accumulator               .fromIstream(is))
                                  || (!lock_aquisition_latency_seconds_accumulator           .fromIstream(is))
                                  || (!switchover_completion_latency_seconds_accumulator     .fromIstream(is))
                                  || (!distribution_over_workloads_of_avg_dispatching_latency.fromIstream(is))
                                  || (!distribution_over_workloads_of_avg_lock_acquisition   .fromIstream(is))
                                  || (!distribution_over_workloads_of_avg_switchover         .fromIstream(is))
                                )
                                {
                                    std::ostringstream o;
                                    o << "<Error> For host " << ivyscript_hostname
                                        << ", internal programming error - failed parsing the latencies line. \"" << s << "\"." << std::endl;
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
                            }
                        }
                        // finished parsing latencies line.

                        {
                            std::unique_lock<std::mutex> m_lk(m_s.master_mutex);

                            m_s.cpu_by_subinterval[m_s.rollups.current_index()].add(toLower(ivyscript_hostname),cpudata);

                            if (seq_fill_progress < m_s.min_sequential_fill_progress) { m_s.min_sequential_fill_progress = seq_fill_progress;}

                            for (auto& x : parsed_detail_lines)
                            {
                                auto rc = m_s.rollups.add_workload_detail_line(x.workloadID, x.iosequencerInput, x.subintervalOutput);

                                if (!rc.first)
                                {
                                    std::ostringstream o;
                                    o << "Rollup of detail line failed. \"" << x.line << "\" - " << rc.second << std::endl;
                                    log(logfilename, o.str());
                                    kill_ssh_and_harvest();
                                    commandComplete = true;
                                    commandSuccess = false;
                                    commandErrorMessage = o.str();
                                    dead=true;  // but should we exit?
                                    m_lk.unlock();
                                    m_s.master_cv.notify_all();
                                    return;
                                }
                            }
                        }
                        m_s.master_cv.notify_all();

                        now.setToNow();

                        if (now > m_s.nextSubintervalEnd) // At this time, subintervalStart/End refer to the subinterval we are rolling up.
                        {
                            std::ostringstream o;
                            o << "<Error> Subinterval length parameter subinterval_seconds may be too short.  For host " << ivyscript_hostname
                                << ", ivyslave received a workload thread detail line for the previous subinterval after the current subinterval was already over." << std::endl;
                            o << "This can also be caused if an ivy command device is on a subsystem port that is saturated with other (ivy) activity, making communication with the command device run very slowly." << std::endl;
                            o << "now: "  << now.format_as_datetime_with_ns() << std::endl;
                            o << m_s.subintervalStart.format_as_datetime_with_ns()
                                    << " = m_s.subintervalStart   (" << m_s.subintervalStart.duration_from_now() << " seconds from now) "
                                    << " = m_s.subintervalStart   (" << m_s.subintervalStart.duration_from(m_s.get_go) << " seconds from Go! time) "
                                    << std::endl
                                << m_s.subintervalEnd.format_as_datetime_with_ns()
                                    << " = m_s.subintervalEnd     (" << m_s.subintervalEnd.duration_from_now() << " seconds from now) "
                                    << " = m_s.subintervalEnd     (" << m_s.subintervalEnd.duration_from(m_s.get_go) << " seconds from Go! time) "
                                    << std::endl
                                << m_s.nextSubintervalEnd.format_as_datetime_with_ns()
                                    << " = m_s.nextSubintervalEnd (" << m_s.nextSubintervalEnd.duration_from_now() << " seconds from now) "
                                    << " = m_s.nextSubintervalEnd (" << m_s.nextSubintervalEnd.duration_from(m_s.get_go) << " seconds from Go! time) "
                                    << std::endl
                                ;


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
                    // end of processing Go! command
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
                // end of ivyslave command processing

                commandComplete=true;
            }
            master_slave_cv.notify_all();
        }
        // after processingCommands loop

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
                o << "process_ivy_cmddev_response() failed - attribute name must be an identifier starting with an alphabetic and continuing alphanumerics/underscores - instead token is \"" << token << "\"." << std::endl;
                log(logfilename, o.str());
                throw std::invalid_argument(o.str());
            }
            attribute = token;

            get_token();
            if ("=" != token)
            {
                std::ostringstream o;
                o << "process_ivy_cmddev_response() failed looking for equals sign \'=\' after attribute name \"" << attribute << "\" - instead token is \"" << token << "\"." << std::endl;
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
}






