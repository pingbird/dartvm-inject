#include <iostream>
#include <thread>
#include <cstring>
#include "/usr/lib/dart/include/dart_api.h"

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
  std::thread nthread([=]() {
    char* error;
    auto isolate = Dart_CreateIsolate(uriCopy, "thanos", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &error);
    delete[] uriCopy;
    if (error != nullptr) {
      std::cerr << "Thanos isolate: " << error << std::endl;
      return;
    }
  });
}