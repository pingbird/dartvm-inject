#include <utility>
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

static bool verbose = false;

struct InjectionError : std::exception {
  explicit InjectionError(string what) : what(std::move(what)) {}
  string what;
};

void assertSBErr(lldb::SBError& err) {
  if (err.Fail()) {
    cerr << "Error: " << err.GetCString() << endl;
    exit(1);
  }
}

struct antmanInjector {
  antmanInjector() :
    debugger(lldb::SBDebugger::Create(false)),
    interpreter(debugger.GetCommandInterpreter()) {}

  string runCmd(const string& cmd) {
    if (verbose) cout << "Running command: " << cmd << endl;

    lldb::SBCommandReturnObject returnObject;
    auto status = interpreter.HandleCommand(cmd.c_str(), returnObject);

    if (returnObject.HasResult()) {
      if (returnObject.GetOutput()) {
        if (verbose) cout << "Result: " << returnObject.GetOutput() << endl;
        return returnObject.GetOutput();
      } else if (verbose) {
        cout << "Command finished sucessfully" << endl;
      }
    } else if (returnObject.GetError()) {
      throw InjectionError(returnObject.GetError());
    }

    return "";
  }

  lldb::SBValue expr(const char* cmd) {
    if (verbose) cout << "Evaluating expr: " << cmd << endl;
    auto val = target.EvaluateExpression(cmd);
    if (!val.IsValid()) {
      cerr << "Error: Expression not valid: " << val.GetError().GetCString() << endl;
      exit(1);
    }
    return val;
  }

  size_t sizeExpr(const char* cmd) {
    lldb::SBError err;
    auto o = expr(cmd).GetData().GetAddress(err, 0);
    assertSBErr(err);
    return o;
  }

  int int32Expr(const char* cmd) {
    lldb::SBError err;
    auto o = expr(cmd).GetData().GetSignedInt32(err, 0);
    assertSBErr(err);
    return o;
  }

  std::string strExpr(const char* cmd) {
    auto str = sizeExpr(cmd);
    auto strPtr = string(to_string(str));
    auto len = sizeExpr(("(size_t)strlen(" + strPtr + ")").c_str());
    auto o = std::string();
    o.resize(len);
    lldb::SBError err;
    process.ReadCStringFromMemory(str, (void*)&o[0], len, err);
    assertSBErr(err);
    expr(("free(" + strPtr + ")").c_str());
    return o;
  }

  void updateTarget() {
    target = debugger.GetSelectedTarget();
    process = target.GetProcess();
  }

  lldb::SBDebugger debugger;
  lldb::SBCommandInterpreter interpreter;
  lldb::SBTarget target;
  lldb::SBProcess process;
};

int main(int argc, char **argv) {
  cxxopts::Options options("dart-inject", "Injects code into a running DartVM process");

  options.add_options()
    ("h,help", "Print help")
    ("p,pid", "Dart process id", cxxopts::value<int>(), "N")
    ("v,verbose", "Enable debug prints")
    ("l,antman", "Override antman location");

  options.add_options("_")
    ("positional", "", cxxopts::value<std::vector<string>>());

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
    if (arg.count("pid")) pid = arg["pid"].as<int>();
    if (arg.count("v")) verbose = true;

    if (verbose) cout << "Debug prints enabled" << endl;

    std::string cwd;
    {
      char* ccwd = getcwd(nullptr, PATH_MAX);
      cwd = string(ccwd);
      free(ccwd);
    }

    if (verbose) cout << "Current directory is '" + cwd + "'" << endl;

    string antmanLibPath;
    if (arg.count("antman")) {
      antmanLibPath = arg["antman"].as<string>();
    } else {
      antmanLibPath = "libantman.so";
    }

    if (antmanLibPath[0] != '/') {
      antmanLibPath = cwd + "/" + antmanLibPath;
    }

    auto &pargs = arg["positional"].as<std::vector<string>>();

    lldb::SBDebugger::Initialize();
    antmanInjector injector;

    if (pid == -1) {
      injector.runCmd("process attach -n dart");
    } else {
      injector.runCmd("process attach -p " + to_string(pid));
    }
    injector.updateTarget();

    injector.runCmd("expr (void*)dlopen(\"" + antmanLibPath + "\", 0x2)");
    injector.runCmd("expr antmanInit()");

    auto debugger = lldb::SBDebugger::Create(false);

    if (!debugger.IsValid()) {
      cerr << "Error: Failed to create debugger." << endl;
      return 1;
    }

    // SPAWN //
    if (pargs[0] == "spawn") {
      if (pargs.size() != 2) {
        cerr << "Error: Wrong number of arguments." << endl;
        return 1;
      }

      std::string scriptPath = pargs[1];
      if (scriptPath[0] != '/') {
        scriptPath = cwd + "/" + scriptPath;
      }

      if (access(scriptPath.c_str(), F_OK) == -1) {
        throw InjectionError("Script file not found: '" + scriptPath + "'");
      }

      injector.runCmd("expr antmanSpawn(\"" + scriptPath + "\")");

    // INFO //
    } else if (pargs[0] == "info") {
      if (pargs.size() != 1) {
        cerr << "Error: Wrong number of arguments." << endl;
        return 1;
      }

      auto infoStr = injector.strExpr("(intptr_t)antmanInfo()");
      cout << infoStr << endl;
    } else {
      cerr << "Error: Unknown command '" << pargs[0] << "'." << endl;
      return 1;
    }
  } catch (const cxxopts::OptionException& e) {
    cerr << "Error: " << e.what() << endl;
    return 1;
  } catch (const InjectionError& e) {
    cerr << "Injection error: " << e.what << endl;
    return 1;
  }
}