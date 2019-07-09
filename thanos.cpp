#include <iostream>
#include <thread>
#include <cstring>

#define NDEBUG
#define RELEASE

#include "/usr/lib/dart/include/dart_api.h"
#include "bin/thread.h"
#include "vm/thread.h"
#include "vm/isolate.h"
#include "vm/lockers.h"
#include "vm/port.h"
#include "vm/snapshot.h"

void thanosInit() {
  static bool isInitialized = false;
  if (!isInitialized) {
    std::cout << "Thanos injection successful." << std::endl;
    isInitialized = true;
  }
}

void thanosSpawnUri(const char* uri) {
  auto uriCopy = new char[strlen(uri)];
  strcpy(uriCopy, uri);

  dart::OSThread::Start("thanosSpawnUri", [](dart::uword targs) {
    auto uriCopy = reinterpret_cast<char*>(targs);

    std::cout << "Creating isolate..." << std::endl;

    char* error;
    auto isolate = reinterpret_cast<dart::Isolate*>(
      dart::Isolate::CreateCallback()(uriCopy, "main", nullptr, nullptr, nullptr, nullptr, &error)
    );

    if (error != nullptr) {
      free(error);
      std::cerr << "Isolate creation error: " << error << std::endl << std::endl;
      return;
    } else if (isolate == nullptr) {
      std::cerr << "Isolate null" << std::endl;
      return;
    }

    isolate->MakeRunnable();

    std::cout << "Isolate created" << std::endl;

    dart::MutexLocker ml(isolate->mutex());

    if (isolate->is_runnable()) {
      std::cout << "Entering isolate" << std::endl;
      Dart_EnterIsolate(reinterpret_cast<Dart_Isolate>(isolate));
      std::cout << "Done!" << std::endl;
    } else {
      std::cerr << "Isolate not runnable" << std::endl;
    }
  }, reinterpret_cast<dart::uword>(uriCopy));
}