#include "client/app.hpp"

int main() {
  ClientApp app;
  if (!app.initialize())
    return -1;

  app.run();

  app.end();
}
