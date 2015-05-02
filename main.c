#include "crashmond.h"

int main(int argc, char **argv) {
  if (argc >= 2 && strcmp(argv[1], "--handle") == 0) {
    return handle_crash(argc, argv);
  } else {
    return run_daemon(argc, argv);
  }
}
