# What
An experiment in circumventing the commonly suggested practice of using the
Linux kernel audit subsystem to gathering all `execve` syscalls on a host in
order to know every command that is run.

It's not an ironclad tactic!

This is a very simple shared object which defines an alternative `execve`
function. The new function doesn't actually send the syscall but instead loads
the target binary into a memory backed file descriptor and then executes *that*
with `fexecve` instead.

As an added bonus, these use anonymous filehandles in memory so other than
indications in `/proc` this is very stealthy.

# Demo
Build with `make` and then:

Without the `LD_PRELOAD`:
```
$ strace -f -e execve -- bash
execve("/sbin/bash", ["bash"], 0x7fff456e4d60 /* 71 vars */) = 0
$ ls
strace: Process 70876 attached
[pid 70876] execve("/sbin/ls", ["ls"], 0x558ff35a8910 /* 71 vars */) = 0
fexecve_preload.c  fexecve_preload.so  Makefile  README.md
[pid 70876] +++ exited with 0 +++
--- SIGCHLD {si_signo=SIGCHLD, si_code=CLD_EXITED, si_pid=70876, si_uid=1000, si_status=0, si_utime=0, si_stime=1} ---
$ exit
+++ exited with 0 +++
```

We can see that `execve` syscalls happened for both the `/sbin/bash` and
`/sbin/ls` binaries.

Then *with* the `LD_PRELOAD`:

```
$ LD_PRELOAD=./fexecve_preload.so strace -f -e execve -- bash
$ ls
strace: Process 70890 attached
fexecve_preload.c  fexecve_preload.so  Makefile  README.md
[pid 70890] +++ exited with 0 +++
--- SIGCHLD {si_signo=SIGCHLD, si_code=CLD_EXITED, si_pid=70890, si_uid=1000, si_status=0, si_utime=0, si_stime=1} ---
$ # lol
```

Nothing to see here - this shell is just idling, honest.

# Why
## `auditd` and pals
To illustrate that `execve` gathering is not foolproof, and not only can it be
circumvented but it can be done for the context of an entire session without
causing any inconvenience for the user.

I point this out because I've seen a lot of blog posts recently saying "just
enable execve gathering" in the audit subsystem, without any mention of
adjusting the backlog size or tuning what the kernel should actually *do* if it
runs out of memory to transmit messages to userspace. This is pretty dangerous,
and I've seen it deadlock a busy system which you obviously don't want to do in
production. The alternatives here are to drop messages in the event of
backpressure, which means your audit system is lossy which equally prevents you
from alerting on higher-signal IoCs like loginuids reading SSH sockets that
don't belong to them.

These tools *can* be incredibly helpful, but create a false sense of security
if you misuse them.

## Filesystem permissions
This also presents the interesting case that users can ignore the execute bit
as long as the read bit is set - in fact all filesystem special features around
execution (including SUID and added capabilities) are ignored as the binary is
simply read into executable memory and then executed.

This also counts for filesystems being mounted `noexec`

Interestingly enough this shows that in the case of SUID and `setcap` binaries
`execve` gathering is incredibly useful, as you can fairly solidly determine
who has actually run a privilege escalated binary.

# Disclaimers
Using this code may cause destruction and death, so probably don't put this in
`/etc/ld.so.preload`

I've not done the error handling on a nonexistent file yet, so behaviour is not
identical to `execve` (if you try to run a file that doesn't exist it will
segfault).

From a cursory reading of the `execve(2)` man page, it looks like I would need
to implement at least:

* `EACCESS` in the case of the *read* bit not being set rather than the execute
  bit
* `EACCESS` in the case that the provided file is not a regular file
* `EACCESS` in the case of missing search permissions for the file path
* `EIO` in the case that the file fails to be read *before* execution
* `ELOOP` in the case that there are too many levels of symbolic link
* `EMFILE` and `ENFILE` in the case that too many file descriptors are open
* `ENAMETOOLONG` if the path name is too long
* `ENOENT` if the pathname is invalid
* `ENOTDIR` if the directory portion is not a valid directory
* `ETXTBSY` if the executable was open for writing by another process

Likelihood is that most of these will already be being returned by some of the
file read operations anyway, I just need to learn how to C properly and
actually return them ;)

# Detection
This is an interesting one - I *think* it could be possible to heuristically
determine:

* `execve`/`fexecve` call pattern which could be monitored using eBPF.
* patterns in linked/memory file descriptors in the `/proc` filesystem
* Presence of `LD_PRELOAD` in environment variables
* `fexecve` gathering by eBPF, although you'd have to determine the actual
  content of the binary in the file descriptor in order to identify it, as
  there's no need for the user to be honest about the "name" for their
  memory-backed fd.
  * Notably I've *not* suggested using the audit subsystem for this because
    unlike its BSD cousins, Linux implements `fexecve` as a userspace function
    instead of a syscall, meaning it's not visible to the audit subsystem or
    `strace`.

# Followup work
## Errata and questions
If there's something you think is incorrect, unclear or you just want to talk
about, feel free to open an issue! If it's something you already know the
answer to, PRs are also welcome!

I'm not really an expert here so don't be shy :)

Not that this should need to be said, but that doesn't include recruiters,
marketeers or anyone else attempting to use the repo to make a buck: go bother
someone else.

For now this repo is canonically hosted at
[https://github.com/oholiab/no_execve](https://github.com/oholiab/no_execve)
but may move in the future if I'm feeling particularly sassy.

## Improvements
I might do the work in the "Disclaimers" section but I might not :)
