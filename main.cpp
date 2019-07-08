#include <iostream>

#include "lldb/API/LLDB.h"

#include <dirent.h>
#include <fstream>
#include <zconf.h>
#include "cxxopts.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::to_string;

static void getPidOf(int& pid, int queryId, const string& queryName, std::string& exe) {
  DIR *dp = opendir("/proc");
  if (dp == nullptr) return;
  struct dirent *dirp;
  while (pid < 0 && (dirp = readdir(dp))) {
    int id = atoi(dirp->d_name); // NOLINT(cert-err34-c)
    if (id <= 0) continue;
    if (queryId != -1 && id != queryId) continue;
    if (pid == -1) {
      string cmdPath = string("/proc/") + dirp->d_name + "/cmdline";
      std::ifstream cmdFile(cmdPath.c_str());
      string cmdLine;
      getline(cmdFile, cmdLine);
      if (!cmdLine.empty()) {
        string searchLine = cmdLine;
        size_t pos = searchLine.find('\0');
        if (pos != string::npos)
          searchLine = searchLine.substr(0, pos);
        pos = searchLine.rfind('/');
        if (pos != string::npos)
          searchLine = searchLine.substr(pos + 1);
        if (queryName == searchLine)
          pid = id;
      }
    }

    if (pid != -1) {
      string exePath = string("/proc/") + dirp->d_name + "/exe";
      char* exePathStr = realpath(exePath.c_str(), nullptr);
      exe = string(exePathStr);
      free(exePathStr);
      break;
    }
  }

  closedir(dp);
}

int main(int argc, char **argv) {
  cxxopts::Options options("dart-inject", "Injects code into a running DartVM process");

  options.add_options()
    ("h,help", "Print help")
    ("l,thanos", "Thanos shared object location")
    ("p,pid", "Dart process id", cxxopts::value<int>(), "N");

  options.add_options("_")
    ("positional", "", cxxopts::value<std::vector<std::string>>());

  options.parse_positional({"positional"});

  try {
    auto arg = options.parse(argc, argv);

    if (arg.count("help") || !arg.count("positional")) {
      options
        .positional_help("<command>")
        .show_positional_help();

      cout << options.help({""}) << endl;
      cout << "Commands:" << endl;
      cout << "  spawn [uri]  Spawns the target URI as a new isolate" << endl;
      return 0;
    }

    int pid = -1;
    int queryId = -1;
    if (arg.count("pid")) queryId = arg["pid"].as<int>();
    string exePath;
    getPidOf(pid, queryId, "dart", exePath);

    string thanosLibPath;
    if (arg.count("thanos")) {
      thanosLibPath = arg["thanos"].as<string>();
    } else {
      char* cwd = getcwd(nullptr, PATH_MAX);
      thanosLibPath = cwd + string("/libthanos.so");
      free(cwd);
    }

    if (pid == -1) {
      cerr << "Error: Could not find dart process.";
      return 1;
    }

    auto &pargs = arg["positional"].as<std::vector<std::string>>();

    if (pargs[0] == "spawn") {
      if (pargs.size() != 2) {
        cerr << "Error: Wrong number of arguments." << endl;
        return 1;
      }

      lldb::SBDebugger::Initialize();
      auto debugger = lldb::SBDebugger::Create(false);

      if (!debugger.IsValid()) {
        cerr << "Error: Failed to create debugger." << endl;
        return 1;
      }
      lldb::SBError error;

      /*auto target = debugger.CreateTarget(exePath.c_str());

      if (error.Fail()) {
        cerr << "Error: " << error.GetCString() << endl;
        return 1;
      }

      auto attachInfo = lldb::SBAttachInfo(pid);
      auto process = target.Attach(attachInfo, error);

      if (error.Fail()) {
        cerr << "Error: " << error.GetCString() << endl;
        return 1;
      }

      if (!process.IsValid()) {
        cerr << "Error: Could not attach to process" << endl;
        return 1;
      }*/

      auto runCmd = [&](std::string cmdString) {
        lldb::SBCommandReturnObject returnObject;
        cout << cmdString << endl;
        auto interpreter = debugger.GetCommandInterpreter();
        auto status = interpreter.HandleCommand(cmdString.c_str(), returnObject);

        if (returnObject.IsValid()) {
          cout << "Success!" << endl;
          if (returnObject.HasResult()) {
            cout << returnObject.GetOutput() << endl;
          }
        } else {
          cout << "Error :( " << returnObject.GetError() << endl;
        }
      };

      runCmd("process attach -p " + to_string(pid));
      runCmd("expr (void*)dlopen(\"" + thanosLibPath + "\", 0x2)");
      runCmd("expr thanosInit()");
      runCmd("expr thanosSpawnUri(\"hello.dart\")");

      cout << "Done!" << endl;
    } else {
      cerr << "Error: Unknown command '" << pargs[0] << "'." << endl;
      return 1;
    }
  } catch (const cxxopts::OptionException& e) {
    cerr << "Error: " << e.what() << endl;
  }
}