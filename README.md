PRS
===

The Portable Runtime System (PRS) is a software library designed to ease the
process of porting a RTOS application to a general purpose platform.

Features
------------

  * customizable user space preemptive scheduling;
  * higher single-core context switch performance than usual operating system
    facilities;
  * dynamic process loading in a common, flat address space;
  * multi-core support through lock-free primitives;
  * Windows support;
  * Linux support.

Introduction
------------

By default, PRS initializes as many single-threaded priority schedulers as
there are CPU cores on the system. Each scheduler manages its own list of tasks
to run. New tasks can be created and added to schedulers at run-time. Tasks may
also be removed and destroyed. Each task has its own register context and
stack; tasks in PRS are *light-weight threads*, also known as *fibers*. Fibers
provide faster context switching than regular threads because there is no need
for the operating system to intervene.

Schedulers work on top of an abstraction which is called a *worker*. A worker
simulates an interruptible bare-metal-like environment where interrupts can
occur any time. It handles context switching between scheduler tasks and also
manages the flow of execution between an interruptible (preemptible) mode and
a non-interruptible mode. In interruptible mode, tasks can be preempted at any
time, just like regular threads.

Lock-free PRS data structures ensure that even when some workers are preempted
by the OS, other workers can still run freely.

PRS also features a process loader. The process loader loads executables like
a regular OS, except that the virtual address space is shared with PRS and
other executables. The entry point of the executables is executed through a
PRS task. PRS executables are linked with the PR API which features basic
services such as message queues, semaphores and task management.

PRS performs OS agnostic operations through its platform abstraction layer
(PAL). The PRS PAL currently features operations for managing threads, context
switching, virtual memory, exceptions, timers and more. 

Tested platforms
----------------

  * Windows 7 SP1 with MinGW64, GCC 4.9.3 and 5.3.0
  * RHEL 7.2 with GCC 4.8.5 (use `C99=1`)
  * Debian 8.3 with GCC 4.9.2

PRS only supports AMD64 architectures for now.

Installation
------------

On Windows, first install MinGW-w64. If MinGW binaries are not acessible
through the `PATH` environment variable, you may define the `MINGW_HOME` to
point to them. For example, `set MINGW_HOME=C:\MinGW`. Then, run `make` in the
root PRS directory.

On Linux, make sure the `gcc` and `g++` tools are installed, then run `make`.

Test code can be run using the `test` make target.

If C11 is not supported by your compiler, use the `C99=1` assignation on the
`make` command line. PRS can make use of C11 features such as the `stdatomic.h`
header and the `_Thread_local` specifier.

To compile in a debug configuration, use the `DEBUG=1` assignation.

Getting started
---------------

The `prs.log` file is generated upon execution of PRS in the current working
directory.

Examples in the `examples` directory show how to build dynamically loaded PRS
executables.

The `include/pr.h` file contains the APIs that are used by PRS executables.

The `prs/init.c` file contains the PRS initialization and exit sequences.

License
-------

This version of PRS is released under the [GNU Affero General Public License]
(https://www.gnu.org/licenses/agpl-3.0.en.html).

For inquiries about a commercial license, please contact
[portableruntimesystem@gmail.com](mailto:portableruntimesystem@gmail.com)
