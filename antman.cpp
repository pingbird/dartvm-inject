#include <iostream>
#include <thread>
#include <cstring>
#include <sstream>

#define NDEBUG
#define RELEASE

#include "/usr/lib/dart/include/dart_api.h"
#include "bin/thread.h"
#include "vm/thread.h"
#include "vm/isolate.h"
#include "vm/lockers.h"
#include "vm/port.h"
#include "vm/snapshot.h"
#include "vm/dart.h"
#include "vm/thread_pool.h"
#include "vm/version.h"

using std::string;
using std::to_string;

void antmanInit() {
  static bool isInitialized = false;
  if (!isInitialized) {
    std::cout << "Antman initialized." << std::endl;
    isInitialized = true;
  }
}

struct DartIsolateGuard {
  explicit DartIsolateGuard(dart::Isolate* isolate) {
    Dart_EnterIsolate(reinterpret_cast<Dart_Isolate>(isolate));
  }

  ~DartIsolateGuard() {
    Dart_ShutdownIsolate();
  }
};

struct DartScopeGuard {
  DartScopeGuard() {
    Dart_EnterScope();
  }

  ~DartScopeGuard() {
    Dart_ExitScope();
  }
};

void antmanSpawn(const char* uri) {
  auto uriCopy = new char[strlen(uri)];
  strcpy(uriCopy, uri);

  dart::OSThread::Start("antmanSpawnUri", [](dart::uword targs) {
    auto uriCopy = string(reinterpret_cast<char*>(targs));
    delete reinterpret_cast<char*>(targs);

    char* error;
    auto isolate = reinterpret_cast<dart::Isolate*>(
      dart::Isolate::CreateCallback()(uriCopy.c_str(), "main", nullptr, nullptr, nullptr, nullptr, &error)
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

    DartIsolateGuard isolateGuard(isolate);
    DartScopeGuard scopeGuard;

    if (!isolate->is_runnable()) {
      std::cerr << "Isolate not runnable" << std::endl;
      return;
    }

    auto compile = Dart_CompileToKernel(uriCopy.c_str(), nullptr, 0, false, nullptr);

    if (compile.status != Dart_KernelCompilationStatus_Ok) {
      if (compile.error) {
        std::cerr << "Error compiling: " << compile.error << std::endl;
      } else {
        std::cerr << "Error compiling: " << compile.status << std::endl;
      }
      return;
    }

    auto library = Dart_LoadLibraryFromKernel(compile.kernel, compile.kernel_size);

    auto res = Dart_Invoke(library, Dart_NewStringFromCString("main"), 0, nullptr);

    if (Dart_IsError(res)) {
      std::cerr << "Error running main: " << Dart_GetError(res) << std::endl;
      return;
    }
  }, reinterpret_cast<dart::uword>(uriCopy));
}

class AntIsolateVisitor : public dart::IsolateVisitor {
public:
  explicit AntIsolateVisitor(string* info) : info(info) {}
  ~AntIsolateVisitor() override = default;

  void VisitIsolate(dart::Isolate* isolate) override {
    isolate->mutator_thread()->EnterSafepoint();

    *info += "Isolate " + to_string(count) + ": " + string(isolate->name()) + "\n";
    if (isolate->IsPaused())
      *info += "  paused\n";
    else if (isolate->mutator_thread()->IsExecutingDartCode())
      *info += "  executing\n";
    else
      *info += "  running\n";

    isolate->ScheduleInterrupts();

    isolate->mutator_thread()->ExitSafepoint();

    count++;
  }

private:
  int count = 0;
  string* info;
};

const char* antmanInfo() {
  string info;

  info += "Version: " + string(dart::Version::String()) + "\n";

  AntIsolateVisitor visitor(&info);
  dart::JSONStream stream;
  dart::Service::PrintJSONForVM(&stream, false);
  dart::Isolate::VisitIsolates(&visitor);

  return strdup(info.c_str());
}