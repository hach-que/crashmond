#include "crashmond.h"

void init_daemon(char* socket_path);

void init_daemon(char* socket_path) {
  unsigned long binary_path_len = 256uL;
  unsigned long core_pattern_len = 4096uL;
  char* binary_path = (char*)malloc(binary_path_len + 1uL);
  char* core_pattern = (char*)malloc(core_pattern_len + 1uL);
  FILE* core_pattern_file;
  
  memset(binary_path, 0, binary_path_len + 1);
  memset(core_pattern, 0, core_pattern_len + 1);
  
  sd_journal_print(LOG_NOTICE, "starting");
  
  sd_journal_print(LOG_NOTICE, "locating current binary");
  if (readlink("/proc/self/exe", binary_path, binary_path_len) == -1) {
    sd_journal_print(LOG_CRIT, "unable to locate self");
    exit(1);
  }
  sd_journal_print(LOG_NOTICE, "located self at %s", binary_path);
  
  sprintf(core_pattern, "|%s --handle %s %%P %%s %%E", binary_path, socket_path);
  
  sd_journal_print(LOG_NOTICE, "setting core dump pattern to %s", core_pattern);
  core_pattern_file = fopen("/proc/sys/kernel/core_pattern", "w");
  if (core_pattern_file == NULL) {
    sd_journal_print(LOG_CRIT, "unable to open /proc/sys/kernel/core_pattern for writing");
    exit(1);
  }
  fwrite(core_pattern, sizeof(char), strlen(core_pattern), core_pattern_file);
  fclose(core_pattern_file);
}

int run_daemon(int argc, char** argv) {
  if (argc < 3) {
    sd_journal_print(LOG_CRIT, "expected path of socket and submission URL");
    exit(1);
  }
  
  char* socket_path = argv[1];
  char* url = argv[2];
  
  unlink(socket_path);
  
  int server, client, result;
  struct sockaddr_un server_addr;
  server_addr.sun_family = AF_UNIX;
  if (strlen(socket_path) >= sizeof(server_addr.sun_path) - 1) {
    sd_journal_print(LOG_CRIT, "socket path too long for connection");
    exit(1);
  }
  strcpy((char*)&server_addr.sun_path, socket_path);
  
  server = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server < 0) {
    sd_journal_print(LOG_CRIT, "unable to create UNIX socket for listening");
    exit(1);
  }
  
  result = bind(server, (struct sockaddr*)&server_addr, sizeof(server_addr));
  if (result < 0) {
    sd_journal_print(LOG_CRIT, "unable to bind to UNIX socket");
    exit(1);
  }
  
  result = listen(server, 1);
  if (result < 0) {
    sd_journal_print(LOG_CRIT, "unable to listen on UNIX socket");
    exit(1);
  }
  
  init_daemon(server_addr.sun_path);
  
  while (1) {
    client = accept(server, NULL, NULL);
    if (client < 0) {
      sd_journal_print(LOG_CRIT, "unable to accept client");
      exit(1);
    }
    
    int fd_to_recv;
    if (ancil_recv_fd(client, &fd_to_recv) < 0) {
      sd_journal_print(LOG_WARNING, "failed to receive FD from crash handler");
      close(client);
      continue;
    }
    
    sd_journal_print(LOG_NOTICE, "got FD %d from crash handler", fd_to_recv);
    close(client);
    
    if (submit_crash_report(url, fd_to_recv) != 0) {
      sd_journal_print(LOG_ERR, "unable to submit crash report");
    }
    
    close(fd_to_recv);
  }
}
