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

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "pipe_line_reader.h"

bool pipe_line_reader::read_from_pipe(ivytime timeout)
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
        // This loop keeps running until we time out or we have something ready to read from the pipe

        if (-1==fcntl(fd,F_GETFD)) // check to make sure fd is valid
        {
            std::ostringstream logmsg;
            logmsg << "<Error> pipe_line_reader::read_from_pipe() - fcntl() for pipe fd = " << fd << " failed. errno = " << errno << " - " << strerror(errno);
            log(ivydriver.slavelogfile,logmsg.str());
            return false;
        }

        if (remaining>ivytime(1))
            waitthistime=ivytime(1);   // wait a second at a time so we can check if the child pid exited.
        else
            waitthistime=remaining;

        FD_ZERO(&read_fd_set);
        FD_SET(fd,&read_fd_set);
        do {
        	rc=pselect(1+fd,&read_fd_set,NULL,NULL,&(waitthistime.t),NULL);
        } while (rc == -1 && errno == EINTR);
        if (-1==rc)
        {
            std::ostringstream logmsg;
            logmsg << "<Error> pipe_line_reader::read_from_pipe() - pselect() failed. errno = " << errno << " - " << strerror(errno);
            log(ivydriver.slavelogfile,logmsg.str());
            return false;
        }
        else if (0==rc)
        {
            // timeout expired
            now.setToNow();
            if (now >= (start+timeout))
            {
                std::ostringstream logmsg;
                logmsg << "<Error> pipe_line_reader::read_from_pipe() - timeout (" << timeout.format_as_duration_HMMSSns()
                << ") reading from pipe.  "
                << "start=" << start.format_as_datetime_with_ns() << ", now=" << now.format_as_datetime_with_ns()
                    << "  " << __FILE__ << " line " << __LINE__;
                log(ivydriver.slavelogfile,logmsg.str());
                return false;
            }
        }
        else if (0<rc) break;
    }

    // we have data ready to receive from the child

    rc = read( fd, (void*) &buffer, IVYMAXMSGSIZE );
    if (-1==rc)
    {
        std::ostringstream logmsg;
        logmsg << "<Error> pipe_line_reader::read_from_pipe() - read() from pipe failed errno = " << errno << " - " << strerror(errno);
        log(ivydriver.slavelogfile,logmsg.str());
        return false;
    }
    else if (0==rc)
    {
        std::ostringstream logmsg;
        logmsg << "<Error> pipe_line_reader::read_from_pipe() - read() from pipe got end of file.";
        log(ivydriver.slavelogfile,logmsg.str());
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


std::string pipe_line_reader::real_get_line_from_pipe
(
    ivytime timeout,
    std::string description // passed by reference to called routines to receive an error message when called function returns "false"
)
{
    std::string s;

    if ( next_char_from_slave_buf >= bytes_last_read_from_slave && !last_read_from_pipe )
    {
        std::ostringstream o;
        o << "<Error> pipe_line_reader::get_line_from_pipe(timeout=" << timeout.format_as_duration_HMMSSns() << ") - no characters available in buffer and read_from_pipe() failed - " << description;
        throw std::runtime_error(o.str());
    }

    if ( next_char_from_slave_buf >= bytes_last_read_from_slave )
    {
        last_read_from_pipe = read_from_pipe(timeout);
        if (!last_read_from_pipe)
        {
            std::ostringstream o;
            o << "<Error> pipe_line_reader::get_line_from_pipe(timeout=" << timeout.format_as_duration_HMMSSns() << ") - no characters available in buffer and read_from_pipe() failed - " << description;
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
        c = buffer[next_char_from_slave_buf++];
        if ('\n' == c) break;
        s.push_back(c);
    }

    return s;
}

std::string pipe_line_reader::get_reassembled_line  // reassembles fragmented lines
(
    ivytime timeout,
    std::string description // Used when there is some sort of failure to construct an error message written to the log file.
)
{
    std::string possibly_fragged_line = real_get_line_from_pipe(timeout,description);

    std::smatch fragment_smatch;
    std::ssub_match fragment_ssub_match;

    if ( ! std::regex_match( possibly_fragged_line, fragment_smatch, frag_regex ) )
    {
        return possibly_fragged_line;
    }


    fragment_ssub_match = fragment_smatch[1];

    std::string frag_text = fragment_ssub_match.str();

    std::string accumulated_so_far = frag_text;

    possibly_fragged_line = real_get_line_from_pipe(timeout,description); // second line

    while ( std::regex_match( possibly_fragged_line, fragment_smatch, frag_regex ) )
    {
        fragment_ssub_match = fragment_smatch[1];

        frag_text = fragment_ssub_match.str();

        accumulated_so_far += frag_text;

        possibly_fragged_line = real_get_line_from_pipe(timeout,description);
    }

    if ( ! std::regex_match( possibly_fragged_line, fragment_smatch, last_regex ) )
    {
        std::ostringstream o;
        o << "<Error> When reading a line immediately following a frag=><= line"
            << ", only more frag=><= lines or a last=><= line are permitted.  "
            << "Instead, received the " << possibly_fragged_line.size()
            << " character line \"" << possibly_fragged_line << "\"."
            << std::endl
            << "accumulated_so_far = \"" << accumulated_so_far << "\".\n";
        std::cout << o.str();
        throw std::runtime_error(o.str());
    }

    fragment_ssub_match = fragment_smatch[1];

    frag_text = fragment_ssub_match.str();

    accumulated_so_far += frag_text;

    return accumulated_so_far;
}

