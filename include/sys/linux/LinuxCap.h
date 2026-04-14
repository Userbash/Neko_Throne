#pragma once

#include <QString>

bool Linux_HavePkexec();

QString Linux_FindCapProgsExec(const QString &name);

int Linux_Run_Command(const QString &commandName, const QString &args);

// Returns true if `path` resides on a filesystem mounted with the `nosuid`
// option. On such mounts the kernel silently ignores both the setuid/setgid
// bits and file capabilities, so setcap / chmod +s on the core binary will
// be applied to the file but not honored at exec time. Fedora Silverblue /
// Kinoite mount /var/home with nosuid by default, which is why the previous
// elevation flow appeared to succeed yet TUN setup still failed with EPERM.
bool Linux_IsPathNosuid(const QString &path);

// Returns true if the file at `path` currently has CAP_NET_ADMIN in its
// effective file capability set (as reported by `getcap`). The caller is
// still responsible for ensuring the filesystem honors file capabilities
// (see Linux_IsPathNosuid).
bool Linux_FileHasCapNetAdmin(const QString &path);

// Runs a privileged shell command synchronously and returns its exit code.
// Uses pkexec if available, otherwise `sudo -n` (non-interactive — sudo is
// never prompted for a password from a GUI where no TTY is attached).
int Linux_Run_Privileged_Shell(const QString &shellCmd);