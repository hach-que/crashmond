#ifndef CRASHMOND_H
#define CRASHMOND_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <systemd/sd-journal.h>
#include "libancillary/ancillary.h"

#define BUFFER_LEN 4096

int run_daemon(int argc, char** argv);
int handle_crash(int argc, char **argv);
int main(int argc, char **argv);
int submit_crash_report(char* url, int fd);

#endif
