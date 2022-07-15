#include "server.h"

#include "stream.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include <sys/stat.h>

int main(int argc, char **argv) {
  int threads = 24;
  double timeout = 60.0;
  const char *port = "5000";
  for (int i = 1; i < argc; i++) {
    std::string_view arg = argv[i];
    if (arg == "--threads" && (i + 1) < argc) {
      threads = std::atoi(argv[i + 1]);
    }
    if (arg == "--timeout" && (i + 1) < argc) {
      timeout = std::atoi(argv[i + 1]);
    }

    if (arg == "--port" && (i + 1) < argc) {
      port = argv[i + 1];
    }
  }

  std::cout << "Server starting with " << threads << " threads and " << timeout
            << " seconds timeout on port " << port << std::endl;

  Server server(threads);
  server.serve_static_files("../static");
  server.listen(port, timeout);
}
