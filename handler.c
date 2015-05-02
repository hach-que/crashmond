#include "crashmond.h"

int handle_crash(int argc, char **argv) {
  int pid;
  int signal;
  unsigned long executable_path_len = 4096uL;
  char* executable_path = (char*)malloc(executable_path_len + 1uL);
  unsigned long coredump_path_len = 4096uL;
  char* coredump_path = (char*)malloc(coredump_path_len + 1uL);
  unsigned long a;
  int i, fd;
  char buffer[BUFFER_LEN];
  int reading;
  ssize_t count, total;
  int ppid;
  int pipe_fds[2];
  char* socket_path;
  
  memset(executable_path, 0, executable_path_len + 1);
  memset(coredump_path, 0, coredump_path_len + 1);
  
  sd_journal_print(LOG_NOTICE, "handling crash dump with arguments:");
  for (i = 0; i < argc; i++) {
    sd_journal_print(LOG_NOTICE, " - %d: %s", i, argv[i]);
  }
  
  if (argc >= 6) {
    socket_path = argv[2];
    sd_journal_print(LOG_NOTICE, "will connect to %s", socket_path);
    
    pid = atoi(argv[3]);
    if (pid == 0) {
      sd_journal_print(LOG_CRIT, "unable to parse PID");
      exit(1);
    }
    sd_journal_print(LOG_NOTICE, "PID is %d", pid);
    
    signal = atoi(argv[4]);
    if (signal == 0) {
      sd_journal_print(LOG_CRIT, "unable to parse signal");
      exit(1);
    }
    sd_journal_print(LOG_NOTICE, "signal causing exit was %d", signal);
    
    a = 0;
    for (i = 5; i < argc; i++) {
      if (a + strlen(argv[i]) >= executable_path_len) {
        sd_journal_print(LOG_CRIT, "executable path too long");
        exit(1);
      }
      
      strcpy(executable_path + a, argv[i]);
      a += strlen(argv[i]);
      if (i < argc - 1) {
        executable_path[a] = ' ';
        a++;
      }
    }
    for (a = 0; a < strlen(executable_path); a++) {
      if (executable_path[a] == '!') {
        executable_path[a] = '/';
      }
    }
    sd_journal_print(LOG_NOTICE, "executable path is %s", executable_path);
  } else {
    sd_journal_print(LOG_CRIT, "not enough arguments (expected PID, signal and executable path)");
    exit(1);
  }
  
  sprintf(coredump_path, "/tmp/coredump.%d.XXXXXX", pid);
  fd = mkstemp(coredump_path);
  if (fd == -1) {
    sd_journal_print(LOG_CRIT, "unable to open temporary file to save core dump for GDB");
    exit(1);
  }
  
  sd_journal_print(LOG_NOTICE, "saving coredump for GDB into FD %d", fd);
  reading = 0;
  total = 0;
  do {
    count = read(STDIN_FILENO, buffer, BUFFER_LEN);
    total += count;
    write(fd, buffer, (size_t)count);
    if (count > 0) {
      reading = 0;
    } else {
      reading = 1;
    }
  } while (reading == 0);
  sd_journal_print(LOG_NOTICE, "coredump saved (wrote %zd bytes)", total);
  
  sd_journal_print(LOG_NOTICE, "seeking coredump to 0");
  if (lseek(fd, 0, SEEK_SET) == -1) {
    sd_journal_print(LOG_CRIT, "unable to seek coredump!");
    exit(1);
  }
  
  sd_journal_print(LOG_NOTICE, "constructing pipe to receive GDB output");
  if (pipe(pipe_fds) == -1) {
    sd_journal_print(LOG_CRIT, "unable to construct pipe");
    exit(1);
  }
  
  unsigned long gdb_arg_len = 4096uL * 2uL;
  char* gdb_core = (char*)malloc(gdb_arg_len + 1uL);
  memset(gdb_core, 0, gdb_arg_len + 1);
  sprintf(gdb_core, "--core=/dev/fd/%d", fd);
  
  sd_journal_print(LOG_NOTICE, "calculating GDB arguments");
  char* gdb_args[] = {
    "--batch",
    "--nh",
    "--nx",
    gdb_core,
    "-ex",
    "thread apply all bt full",
    "-ex",
    "quit",
    executable_path,
    NULL
  };
  for (i = 0; gdb_args[i] != NULL; i++) {
    sd_journal_print(LOG_NOTICE, " - %d: %s", i, gdb_args[i]);
  }
  
  sd_journal_print(LOG_NOTICE, "forking to execute GDB");
  ppid = fork();
  if (ppid == 0) {
    // Child process to become GDB
    close(pipe_fds[0]);
    
    dup2(STDERR_FILENO, STDOUT_FILENO);
    dup2(pipe_fds[1], STDOUT_FILENO);
    
    close(STDIN_FILENO);
    execv("/usr/bin/gdb", gdb_args);
    // Does not return to here since execv is called
  } else if (ppid == -1) {
    sd_journal_print(LOG_CRIT, "failed to fork to spawn GDB");
    exit(1);
  }
  
  // Parent process
  close(pipe_fds[1]);
  close(fd);
  
  sd_journal_print(LOG_NOTICE, "creating pipe for actual data send");
  int data_send[2];
  if (pipe(data_send) < 0) {
    sd_journal_print(LOG_CRIT, "unable to create pipes for actual data send");
    exit(1);
  }
  
  sd_journal_print(LOG_NOTICE, "connecting to socket");
  struct sockaddr_un server_addr;
  server_addr.sun_family = AF_UNIX;
  if (strlen(socket_path) >= sizeof(server_addr.sun_path) - 1) {
    sd_journal_print(LOG_CRIT, "socket path too long for connection");
    exit(1);
  }
  strcpy((char*)&server_addr.sun_path, socket_path);
  int client = socket(AF_UNIX, SOCK_STREAM, 0);
  if (client < 0) {
    sd_journal_print(LOG_CRIT, "unable to create client socket");
    exit(1);
  }
  
  int conn = connect(client, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (conn < 0) {
    sd_journal_print(LOG_CRIT, "unable to connect to server");
    close(client);
    exit(1);
  }
  
  sd_journal_print(LOG_NOTICE, "connected to socket");
  
  if (ancil_send_fd(client, data_send[0]) < 0) {
    sd_journal_print(LOG_CRIT, "failed to send FD to crash daemon");
    close(conn);
    close(client);
    exit(1);
  }
  
  sd_journal_print(LOG_NOTICE, "sent FD to crash daemon");
  close(data_send[0]);
  
  unsigned long header_len = 4096uL * 2uL;
  char* header = (char*)malloc(header_len + 1uL);
  memset(header, 0, header_len + 1);
  
  sd_journal_print(LOG_NOTICE, "writing out header data");
  sprintf(header, 
    "Automatic crash report\n"
    "============================\n"
    "UNIX Timestamp: %u\n"
    "Process ID: %d\n"
    "Signal: %d\n"
    "Executable Path: %s\n"
    "\n"
    "GDB Backtrace\n"
    "============================\n",
    (unsigned)time(NULL),
    pid,
    signal,
    executable_path);
  write(data_send[1], header, strlen(header));
  
  sd_journal_print(LOG_NOTICE, "reading GDB output from PID %d", ppid);
  reading = 0;
  total = 0;
  do {
    memset(buffer, 0, BUFFER_LEN);
    count = read(pipe_fds[0], buffer, BUFFER_LEN);
    if (count < 0) {
      sd_journal_print(LOG_ERR, "error encountered while reading data from GDB");
      close(data_send[1]);
      close(conn);
      close(client);
      exit(1);
    }
    total += count;
    write(data_send[1], buffer, count);
    if (count > 0) {
      reading = 0;
    } else {
      reading = 1;
    }
  } while (reading == 0);
  sd_journal_print(LOG_NOTICE, "GDB output forwarded (read %zd bytes)", total);
  
  close(data_send[1]);
  
  sd_journal_print(LOG_NOTICE, "crash handler completed successfully");
  
  return 0;
}
