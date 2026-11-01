#include "server/app.hpp"

int main() {

  ServerApp app;

  app.initialize();
  app.run();
  app.end();

  return 0;
}
