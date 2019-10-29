//Copyright (c) 2019 Hitachi Vantara Corporation
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
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.#include <signal.h>

#include <unistd.h>

#include "ivy_engine.h"
#include "pipe_driver_subthread.h"

struct sigaction ivymaster_sigaction;

void ivymaster_signal_handler(int sig, siginfo_t *p_siginfo, void *context)
{
    execl (m_s.copy_back_ivy_logs_sh_filename.c_str(),(char*)0);

//    std::ostringstream o;
//
//    { o << "<Warning> signal " << sig << "(" << strsignal(sig) << ") received"; }
//
//    switch (sig)
//    {
//        case SIGILL:
//            switch (p_siginfo->si_code)
//            {
//                // The following values can be placed in si_code for a SIGILL signal:
//                case ILL_ILLOPC: o << " with code ILL_OPC - Illegal opcode." << std::endl; goto past_si_code;
//                case ILL_ILLOPN: o << " with code ILLOPN - Illegal operand." << std::endl; goto past_si_code;
//                case ILL_ILLADR: o << " with code ILL_ILLADR - Illegal addressing mode." << std::endl; goto past_si_code;
//                case ILL_ILLTRP: o << " with code ILL_ILLTRP - Illegal trap." << std::endl; goto past_si_code;
//                case ILL_PRVOPC: o << " with code ILL_PRVOPC - Privileged opcode." << std::endl; goto past_si_code;
//                case ILL_PRVREG: o << " with code ILL_PRVREG - Privileged register." << std::endl; goto past_si_code;
//                case ILL_COPROC: o << " with code ILL_COPROC - Coprocessor error." << std::endl; goto past_si_code;
//                case ILL_BADSTK: o << " with code ILL_COPROC - Internal stack error." << std::endl; goto past_si_code;
//
//                default: o << " with unknown SIGILL code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
//        case SIGFPE:
//            switch (p_siginfo->si_code)
//            {
//                // The following values can be placed in si_code for a SIGFPE signal:
//                case FPE_INTDIV: o << " with code FPE_INTDIV - Integer divide by zero." << std::endl; goto past_si_code;
//                case FPE_INTOVF: o << " with code FPE_INTOVF - Integer overflow." << std::endl; goto past_si_code;
//                case FPE_FLTDIV: o << " with code FPE_FLTDIV - Floating-point divide by zero." << std::endl; goto past_si_code;
//                case FPE_FLTOVF: o << " with code FPE_FLTOVF - Floating-point overflow." << std::endl; goto past_si_code;
//                case FPE_FLTUND: o << " with code FPE_FLTUND - Floating-point underflow." << std::endl; goto past_si_code;
//                case FPE_FLTRES: o << " with code FPE_FLTRES - Floating-point inexact result." << std::endl; goto past_si_code;
//                case FPE_FLTINV: o << " with code FPE_FLTINV - Floating-point invalid operation." << std::endl; goto past_si_code;
//                case FPE_FLTSUB: o << " with code FPE_FLTSUB - Subscript out of range." << std::endl; goto past_si_code;
//
//                default: o << " with unknown SIGFPE code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
//        case SIGSEGV:
//            switch (p_siginfo->si_code)
//            {
//                // The following values can be placed in si_code for a SIGSEGV signal:
//                case SEGV_MAPERR: o << " with code SEGV_MAPERR - Address not mapped to object." << std::endl; goto past_si_code;
//                case SEGV_ACCERR: o << " with code SEGV_ACCERR - Invalid permissions for mapped object." << std::endl; goto past_si_code;
//                //case SEGV_BNDERR: o << " with code SEGV_BNDERR - Failed address bound checks." << std::endl; goto past_si_code;
//                //case SEGV_PKUERR: o << " with code SEGV_PKUERR - Access was denied by memory protection keys.  See pkeys(7).  The protection key which applied to this access is available via si_pkey." << std::endl; goto past_si_code;
//
//                default: o << " with unknown SIGSEGV code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
//        case SIGBUS:
//            switch (p_siginfo->si_code)
//            {
//                // The following values can be placed in si_code for a SIGBUS signal:
//                case BUS_ADRALN: o << " with code BUS_ADRALN - Invalid address alignment." << std::endl; goto past_si_code;
//                case BUS_ADRERR: o << " with code BUS_ADRERR - Nonexistent physical address." << std::endl; goto past_si_code;
//                case BUS_OBJERR: o << " with code BUS_OBJERR - Object-specific hardware error." << std::endl; goto past_si_code;
//                //case BUS_MCEERR_AR: o << " with code BUS_MCEERR_AR - Hardware memory error consumed on a machine check; action required." << std::endl; goto past_si_code;
//                //case BUS_MCEERR_AO: o << " with code BUS_MCEERR_AO - Hardware memory error detected in process but not consumed; action optional." << std::endl; goto past_si_code;
//
//                default: o << " with unknown SIGBUS code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
//        case SIGTRAP:
//            switch (p_siginfo->si_code)
//            {
//                // The following values can be placed in si_code for a SIGTRAP signal:
//                case TRAP_BRKPT: o << " with code TRAP_BRKPT - Process breakpoint." << std::endl; goto past_si_code;
//                case TRAP_TRACE: o << " with code TRAP_TRACE - Process trace trap." << std::endl; goto past_si_code;
//                //case TRAP_BRANCH: o << " with code TRAP_BRANCH - Process taken branch trap." << std::endl; goto past_si_code;
//                //case TRAP_HWBKPT: o << " with code TRAP_HWBKPT - (IA64 only) Hardware breakpoint/watchpoint." << std::endl; goto past_si_code;
//
//                default: o << " with unknown SIGTRAP code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
//        case SIGCHLD:
//            switch (p_siginfo->si_code)
//            {
//                // The following values can be placed in si_code for a SIGCHLD signal:
//                case CLD_EXITED: o << " with code CLD_EXITED - Child has exited." << std::endl; goto past_si_code;
//                case CLD_KILLED: o << " with code CLD_KILLED - Child was killed." << std::endl; goto past_si_code;
//                case CLD_DUMPED: o << " with code CLD_DUMPED - Child terminated abnormally." << std::endl; goto past_si_code;
//                case CLD_TRAPPED: o << " with code CLD_TRAPPED - Traced child has trapped." << std::endl; goto past_si_code;
//                case CLD_STOPPED: o << " with code CLD_STOPPED - Child has stopped." << std::endl; goto past_si_code;
//                case CLD_CONTINUED: o << " with code CLD_CONTINUED - Stopped child has continued." << std::endl; goto past_si_code;
//
//                default: o << " with unknown SIGCHLD code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
//        case SIGIO: // and case SIGPOLL, which has the same signal number (alias)
//            switch (p_siginfo->si_code)
//            {
//                // The following values can be placed in si_code for a SIGIO/SIGPOLL signal:
//                case POLL_IN: o << " with code POLL_IN - Data input available." << std::endl; goto past_si_code;
//                case POLL_OUT: o << " with code POLL_OUT - Output buffers available." << std::endl; goto past_si_code;
//                case POLL_MSG: o << " with code POLL_MSG - Input message available." << std::endl; goto past_si_code;
//                case POLL_ERR: o << " with code POLL_ERR - I/O error." << std::endl; goto past_si_code;
//                case POLL_PRI: o << " with code POLL_PRI - High priority input available." << std::endl; goto past_si_code;
//                case POLL_HUP: o << " with code POLL_HUP - Device disconnected." << std::endl; goto past_si_code;
//
//                default: o << " with unknown SIGIO/SIGPOLL code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
////        case SIGSYS:
////            switch (p_siginfo->si_code)
////            {
////                // The following value can be placed in si_code for a SIGSYS signal:
////                case SYS_SECCOMP: o << " with code SYS_SECCOMP - Triggered by a seccomp(2) filter rule." << std::endl; goto past_si_code;
////
////                default: o << " with unknown SIGSYS code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
////            }
//        default:
//            switch (p_siginfo->si_code)
//            {
//                case SI_USER:    o << " with code SI_USER - kill."                 << std::endl; goto past_si_code;
//                case SI_KERNEL:  o << " with code SI_KERNEL - Sent by the kernel." << std::endl; goto past_si_code;
//                case SI_QUEUE:   o << " with code SI_QUEUE - sigqueue."            << std::endl; goto past_si_code;
//                case SI_TIMER:   o << " with code SI_TIMER - POSIX timer expired." << std::endl; goto past_si_code;
//                case SI_MESGQ:   o << " with code SI_MESGQ - POSIX message queue state changed; see mq_notify(3)." << std::endl; goto past_si_code;
//                case SI_ASYNCIO: o << " with code SI_ASYNCIO - AIO completed." << std::endl; goto past_si_code;
//                case SI_SIGIO:   o << " with code SI_SIGIO - SIGIO/SIGPOLL fills in si_code." << std::endl; goto past_si_code;
//                case SI_TKILL:   o << " with code SI_TKILL - tkill(2) or tgkill(2)." << std::endl; goto past_si_code;
//
//                default: o << " with unknown code " << p_siginfo->si_code << "." << std::endl; goto past_si_code;
//            }
//    }
//past_si_code:
//
//    auto pid = getpid();
//    auto tid = syscall(SYS_gettid);
//
//    //if (routine_logging)
//    { o << "getpid() = " << pid << ", gettid() = " << tid << ", getpgid(getpid()) = " << getpgid(getpid()) << std::endl; }
//
//    switch(sig)
//    {
//        case SIGINT:
//            o << "Control-C was pressed." << std::endl;
//            log(m_s.masterlogfile, o.str());
//            _exit(0);
//
//        case SIGCHLD:
//            switch (p_siginfo->si_code)
//            {
//                case CLD_EXITED:
//                    o << "Child exited." << std::endl;
//                    break;
//
//                case CLD_KILLED:
//                    o << "Child was killed." << std::endl;
//                    break;
//
//                case CLD_DUMPED:
//                    o << "Child has terminated abnormally and created a core file." << std::endl;
//                    break;
//
//                case CLD_TRAPPED:
//                    o << "Traced child has trapped." << std::endl;
//                    break;
//
//                case CLD_STOPPED:
//                    o << "Child has stopped." << std::endl;
//                    break;
//
//                case CLD_CONTINUED:
//                    o << "Stopped child has continued." << std::endl;
//                    break;
//
//                default:
//                    o << "Unknown sub-code " << p_siginfo->si_code << " for SIGCHLD." << std::endl;
//                    break;
//            }
//            o << "Child\'s pid = " << p_siginfo->si_pid << ", real uid = " << p_siginfo->si_uid << ", exit status or signal = " << p_siginfo->si_status << std::endl;
//            break;
//
//        case SIGHUP:
//            if (pid == tid) // if I'm main thread, I think
//            {
//                std::ostringstream o;
//                o << "ivy master thread got SIGHUP";
//                log(m_s.masterlogfile, o.str());
////                m_s.kill_subthreads_and_exit();
////                exit(-1);
//            }
//            else
//            {
//                for (const auto pear : m_s.host_subthread_pointers)
//                {
//                    pipe_driver_subthread* p_pds=pear.second;
//                    if (p_pds -> linux_gettid_thread_id == tid)
//                    {
//                        o << "pipe_driver_subthread for " << pear.first << " got SIGHUP - remote ivydriver disappeared?" << std::endl;
//                        break;
//                    }
//                    if (p_pds -> ssh_sub_subthread_tid == tid)
//                    {
//                        o << "pipe_driver_subthread for " << pear.first << "\'s ssh subthread got SIGHUP - remote ivydriver disappeared?" << std::endl;
//                        break;
//                    }
//                }
//                for (const auto pear : m_s.command_device_subthread_pointers)
//                {
//                    pipe_driver_subthread* p_pds=pear.second;
//                    if (p_pds -> linux_gettid_thread_id == tid)
//                    {
//                        o << "pipe_driver_subthread for command device for " << pear.first << " got SIGHUP - remote ivydriver disappeared?" << std::endl;
//                        break;
//                    }
//                    if (p_pds -> ssh_sub_subthread_tid == tid)
//                    {
//                        o << "pipe_driver_subthread for command device for " << pear.first << "\'s ssh subthread got SIGHUP - remote ivydriver disappeared?" << std::endl;
//                        break;
//                    }
//                }
//                o << "Got SIGHUP.  My pid is " << pid << ", my tid is " << tid << ", but don\'t know which process (master, test host control subthread, test host control subthread's ssh subthread, or ditto for command device is involved." << std::endl;
//            }
//
//            break;
//
//        case SIGUSR1:
//            break;
//
//        case SIGSEGV:
//            log(m_s.masterlogfile, o.str());
//            _exit(0);
//
//    }
//
//    //if (routine_logging)
//    { log (m_s.masterlogfile,o.str()); }

    return;
}


