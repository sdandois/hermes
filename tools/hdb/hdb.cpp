#ifndef HERMES_ENABLE_DEBUGGER

#include <iostream>
int main(void) {
  std::cout << "hdb compiled without Hermes debugger enabled." << std::endl;
  return EXIT_FAILURE;
}

#else // defined(HERMES_ENABLE_DEBUGGER)

#include <getopt.h>
#include <hermes/DebuggerAPI.h>
#include <hermes/hermes.h>
#include <jsi/jsi.h>
#include <signal.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <thread>

// Note that we cannot use LLVM here because LLVM is built without
// exceptions/RTTI, and so we will incur link errors.
using namespace facebook::hermes;
using namespace facebook::jsi;

// Declare global variables for HDB Help text
static const std::map<std::string, std::string> commandToHelpText = {
    {"frame",
     "Modifies selected frame based on specified frame_id (integer)\n\n"
     "USAGE: frame <frame_id>\n"},
    {"set",
     "Sets pauseOnThrow on or off for errors.\n\n"
     "USAGE: set pauseOnThrow <on/uncaught/off>\n"},
    {"break",
     "Sets breakpoint on a given SourceLocation. \n\n"
     "Location Formats accepted are:\n\t"
     "<line>\n\t"
     "<filename> <line>\n\t"
     "<line> <column>\n\t"
     "<filename> <line> <column>\n"
     "Optionally a conditional breakpoint can be specified as: if <condition>\n\n"
     "USAGE: break <filename> <line> <column> <condition>\n"},
    {"delete",
     "Deletes all or specified breakpoints.\n\n"
     "USAGE: delete [all/<breakpoint_id>]\n"},
    {"enable",
     "Mark a breakpoint as enabled. Breakpoints are by default enabled.\n\n"
     "USAGE: enable <breakpoint_id>\n"},
    {"disable",
     "Disable a breakpoint given. Returns true if successful.\n\n"
     "USAGE: disable <breakpoint_id>\n"},
    {"info",
     "Displays information about a breakpoint or the paused ProgramState.\n\n"
     "USAGE: info <breakpoint_id> to display state of breakpoint\n"
     "       info variables to display the paused ProgramState and variables\n"},
    {"backtrace",
     "Prints the current backtrace.\n\n"
     "USAGE: backtrace\n"},
    {"expand",
     "For all property references e.g., foo.bar, prints the properties e.g., foo.bar.baz\n\n"
     "USAGE: expand\n"},
    {"continue",
     "Debugger command: Continues execution only stopping at a breakpoint.\n\n"
     "USAGE: continue\n"},
    {"step",
     "Debugger command: Execute the current line, stop at the first possible occasion.\nThis can be either a function call or the next line in the current function)\n\n"
     "USAGE: step\n"},
    {"next",
     "Debugger command: Continue execution until the next line in the current function is reached or the function returns.\n'next' doesn't stop inside a called function unlike 'step'.\n\n"
     "USAGE: step\n"},
    {"finish",
     "Debugger command: Continue execution until the current function returns.\n\n"
     "USAGE: finish\n"},
    {"exec",
     "Debugger command: Executes the given line.\n\n"
     "USAGE: exec\n"},
    {"sourceMap",
     "Given a file ID, retrieves the source map URL, provided it has been stored in the file. Else, returns an empty string.\n\n"
     "USAGE: sourceMap <fileId>\n"}};
static const std::string topLevelHelpText =
    "These hdb commands are defined internally. Type `help' to see this list.\n"
    "Type `help name' to find out more about the function `name'.\n\n"
    "frame [frame_id]\n"
    "set pauseOnThrow [on/uncaught/off]\n"
    "break <filename> <line> <column> <condition>\n"
    "delete [all/<breakpoint_id>]\n"
    "enable <breakpoint_id>\n"
    "disable <breakpoint_id>\n"
    "info <breakpoint_id> <variables>\n"
    "backtrace\n"
    "expand\n"
    "sourceMap <fileId>\n"
    "Debugger commands: continue, step, next, finish, exec\n";

namespace {

/// Removes the first token delimited by any character in \p separators from \p
/// str, and sets \p str to the remainder. \return the token. or an empty string
/// if none.
std::string chompToken(std::string *str, const char *separators = " \t") {
  std::string result;
  // Chomp leading spaces, then split at the first space.
  str->erase(0, str->find_first_not_of(separators));
  auto sepOrNpos = str->find_first_of(separators);
  result.assign(*str, 0, sepOrNpos);
  str->erase(0, sepOrNpos);
  return result;
}

void printUsageAndExit() {
  std::cerr
      << "USAGE: hdb [--break-at-start] [--break-after=<secs>] [--lazy] <input JS file>\n";
  exit(EXIT_FAILURE);
}

/// Pointer to debugger for use in our async break signal handlers.
debugger::Debugger *volatile gDebugger = nullptr;

// A signal handler that triggers an async pause in the debugger.
void triggerAsyncPause(int sig) {
  if (gDebugger)
    gDebugger->triggerAsyncPause();
}

/// Control whether a SIGINT (aka control-C) should result in an async pause
/// (\p flag = true) or the default behavior (\p flag = false).
void setSIGINTShouldPause(bool flag) {
  signal(SIGINT, flag ? triggerAsyncPause : SIG_DFL);
}

/// Trigger an asynchrous pause after the given \p delaySecs. If \p delaySecs is
/// 0, trigger an async pause immediately.
void schedulePause(double delaySecs) {
  assert(delaySecs >= 0 && std::isfinite(delaySecs) && "Invalid delay");
  if (delaySecs == 0) {
    gDebugger->triggerAsyncPause();
  } else {
    std::thread t([=] {
      std::this_thread::sleep_for(std::chrono::duration<double>(delaySecs));
      gDebugger->triggerAsyncPause();
    });
    t.detach();
  }
}

} // namespace

struct HDBDebugger : public debugger::EventObserver {
  /// Construct, referencing a runtime.
  HDBDebugger(HermesRuntime &runtime) : runtime_(runtime) {}

  /// Parse a string \p input as a debugger command and its arguments.
  /// This parses only commands that should result in returning from the
  /// didPause handler; commands that manipulate the debugger state are handled
  /// in parseAndRunImmediateCommand(). Returns the resulting debugger command
  /// by reference in \p outCommand. \return true if we got a Command, false
  /// otherwise.
  bool parseCommand(
      std::string input,
      debugger::Debugger &debugger,
      debugger::Command *outCommand) const {
    using namespace std;
    using debugger::Command;
    using debugger::StepMode;

    // Parse the command.
    std::string command = chompToken(&input);
    if (command == "continue" || command == "c") {
      std::cout << "Continuing execution" << std::endl;
      *outCommand = Command::continueExecution();
    } else if (command == "step" || command == "s") {
      *outCommand = Command::step(StepMode::Into);
    } else if (command == "next" || command == "n") {
      *outCommand = Command::step(StepMode::Over);
    } else if (command == "finish" || command == "out" || command == "o") {
      *outCommand = Command::step(StepMode::Out);
    } else if (command == "exec" || command == "e") {
      *outCommand = Command::eval(input, frame_);
    } else {
      return false;
    }
    return true;
  }

  /// Print a stack trace \p stackTrace, printing a sigil in front of \p
  /// calloutFrame to identify it. If calloutFrame exceeds the frame count, no
  /// frame will receive the sigil.
  void printStackTrace(
      const debugger::StackTrace &stackTrace,
      uint32_t calloutFrame = UINT32_MAX) const {
    for (uint32_t i = 0, e = stackTrace.callFrameCount(); i < e; ++i) {
      const auto frame = stackTrace.callFrameForIndex(i);
      std::string funcName = frame.functionName;
      std::cout << (i == calloutFrame ? '>' : ' ') << ' ';
      if (funcName.empty()) {
        std::cout << i << ": (unknown)" << std::endl;
      } else if (funcName == "(native)") {
        std::cout << i << ": " << funcName << std::endl;
      } else {
        std::cout << i << ": " << funcName << ": " << frame.location.fileName
                  << ':' << frame.location.line << ':' << frame.location.column
                  << std::endl;
      }
    }
  }

  /// Print information about current variables in the selected frame.
  void printVariables(const debugger::ProgramState &state) {
    if (frame_ >= state.getStackTrace().callFrameCount()) {
      std::cout << "Invalid frame " << frame_ << std::endl;
      return;
    }
    auto thisInfo = state.getVariableInfoForThis(frame_);
    std::cout << thisInfo.name << " = "
              << thisInfo.value.toString(runtime_).utf8(runtime_) << std::endl;
    auto lexicalInfo = state.getLexicalInfo(frame_);
    for (uint32_t depth = 0, max = lexicalInfo.getScopesCount(); depth < max;
         depth++) {
      uint32_t count = lexicalInfo.getVariablesCountInScope(depth);
      for (uint32_t i = 0; i < count; i++) {
        auto varInfo = state.getVariableInfo(frame_, depth, i);
        std::cout << std::setw(2) << depth << ": " << std::setw(10)
                  << varInfo.name << " = "
                  << varInfo.value.toString(runtime_).utf8(runtime_)
                  << std::endl;
      }
    }
  }

  /// Parse a string \p input as an immediate command (for example, setting a
  /// breakpoint). \return true if we got an immediate command, false otherwise.
  bool parseAndRunImmediateCommand(
      std::string input,
      debugger::Debugger &debugger) {
    using debugger::BreakpointID;
    using debugger::PauseOnThrowMode;

    std::string command = chompToken(&input);
    if (command == "frame" || command == "f") {
      // Modify the selected frame.
      try {
        frame_ = std::stoul(input);
        std::cout << "Selected frame " << frame_ << '\n';
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid frame: " << e.what() << '\n';
      }
    } else if (command == "set") {
      std::string toSet = chompToken(&input);
      std::string value = chompToken(&input);
      if (toSet == "pauseOnThrow" && value == "on") {
        debugger.setPauseOnThrowMode(PauseOnThrowMode::All);
        std::cout << "Set pauseOnThrow: all errors\n";
      } else if (toSet == "pauseOnThrow" && value == "uncaught") {
        debugger.setPauseOnThrowMode(PauseOnThrowMode::Uncaught);
        std::cout << "Set pauseOnThrow: uncaught errors\n";
      } else if (toSet == "pauseOnThrow" && value == "off") {
        debugger.setPauseOnThrowMode(PauseOnThrowMode::None);
        std::cout << "Disabled pauseOnThrow\n";
      } else {
        std::cout << "Invalid 'set' command\n";
      }
    } else if (command == "break" || command == "b") {
      // Break command supports an "if" token.
      // When it exists, the rest of the input is interpreted as a condition.
      // Chomp one token at a time, and stop if we see "if",
      // since every location argument except the first must be a number.
      const char *ifStr = "if";
      bool conditional = false;
      std::vector<std::string> toks{};
      // In conditional breakpoints, the 4th token could be "if",
      // so read 4 tokens.
      for (uint32_t i = 0; i < 4; ++i) {
        auto tok = chompToken(&input);
        if (tok.empty()) {
          break;
        }
        if (tok == ifStr) {
          conditional = true;
          break;
        }
        toks.push_back(tok);
      }
      try {
        debugger::SourceLocation loc{};
        // Formats accepted:
        // <line>
        // <filename> <line>
        // <line> <column>
        // <filename> <line> <column>
        switch (toks.size()) {
          case 1:
            loc.line = std::stoul(toks[0]);
            loc.column = debugger::kInvalidLocation;
            loc.fileId = debugger::kInvalidLocation;
            break;
          case 2:
            try {
              // Attempt to parse as <line> <column> and use <filename> <line>
              // as a fallback.
              loc.line = std::stoul(toks[0]);
              loc.column = std::stoul(toks[1]);
              loc.fileId = debugger::kInvalidLocation;
            } catch (const std::invalid_argument &e) {
              // If this fails, then give up on parsing the command.
              loc.fileName = toks[0];
              loc.line = std::stoul(toks[1]);
              loc.column = debugger::kInvalidLocation;
              loc.fileId = debugger::kInvalidLocation;
            }
            break;
          case 3:
            loc.fileName = toks[0];
            loc.line = std::stoul(toks[1]);
            loc.column = std::stoul(toks[2]);
            loc.fileId = debugger::kInvalidLocation;
            break;
          default:
            std::cout << "Invalid breakpoint request\n";
            return false;
        }
        // If the breakpoint was conditional, we've already taken the "if"
        // token, so use the rest of the line as the condition.
        BreakpointID breakpointId = debugger.setBreakpoint(loc);
        if (conditional) {
          debugger.setBreakpointCondition(breakpointId, input);
        }
        if (breakpointId == debugger::kInvalidBreakpoint) {
          // Failed to set the breakpoint.
          // TODO: Improve error reporting here.
          std::cout << "Invalid or duplicate breakpoint not set\n";
        } else {
          const auto info = debugger.getBreakpointInfo(breakpointId);
          const auto &loc =
              info.resolved ? info.resolvedLocation : info.requestedLocation;
          std::cout << "Set breakpoint " << breakpointId << " at "
                    << loc.fileName << '[' << loc.fileId << ']' << ':'
                    << loc.line << ':' << loc.column
                    << (info.resolved ? "" : " (unresolved)");
          if (conditional) {
            // input has a leading space already.
            std::cout << " if" << input;
          }
          std::cout << '\n';
        }
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid breakpoint request: " << e.what() << '\n';
      }
    } else if (command == "delete") {
      std::string request = chompToken(&input);
      if (request == "all" || request == "a") {
        debugger.deleteAllBreakpoints();
        std::cout << "Deleted all breakpoints\n";
      } else {
        try {
          BreakpointID breakpointId = std::stoul(request);
          debugger.deleteBreakpoint(breakpointId);
          std::cout << "Deleted breakpoint " << breakpointId << "\n";
        } catch (const std::invalid_argument &e) {
          std::cout << "Invalid breakpoint: " << e.what() << '\n';
        }
      }
    } else if (command == "enable") {
      try {
        BreakpointID breakpointId = std::stoul(input);
        debugger.setBreakpointEnabled(breakpointId, true);
        std::cout << "Enabled breakpoint " << breakpointId << "\n";
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid breakpoint: " << e.what() << '\n';
      }
    } else if (command == "disable") {
      try {
        BreakpointID breakpointId = std::stoul(input);
        debugger.setBreakpointEnabled(breakpointId, false);
        std::cout << "Disabled breakpoint " << breakpointId << "\n";
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid breakpoint: " << e.what() << '\n';
      }
    } else if (command == "info" || command == "i") {
      std::string request = chompToken(&input);
      if (request == "breakpoints" || request == "b") {
        const std::vector<BreakpointID> ids = debugger.getBreakpoints();
        for (const auto &id : ids) {
          const auto info = debugger.getBreakpointInfo(id);
          assert(info.id == id && "invalid breakpoint ID");
          const auto &loc =
              info.resolved ? info.resolvedLocation : info.requestedLocation;
          std::cout << id << ' ' << (info.enabled ? 'E' : 'D') << ' '
                    << loc.fileName << ':' << loc.line << ':' << loc.column
                    << (info.resolved ? "" : " (unresolved)") << '\n';
        }
      } else if (request == "variables" || request == "v") {
        printVariables(debugger.getProgramState());
      }
    } else if (command == "backtrace" || command == "bt") {
      printStackTrace(debugger.getProgramState().getStackTrace(), frame_);
    } else if (command == "expand") {
      try {
        expandProperties(debugger, input);
      } catch (const std::exception &e) {
        std::cout << e.what() << '\n';
      }
    } else if (command == "sourceMap") {
      try {
        uint32_t fileId = std::stoul(input);
        auto sourceMappingUrl = debugger.getSourceMappingUrl(fileId);
        if (sourceMappingUrl.empty()) {
          std::cout << "Source map not found for file\n";
        } else {
          std::cout << sourceMappingUrl << '\n';
        }
      } catch (const std::invalid_argument &e) {
        std::cout << "Invalid fileId: " << e.what() << '\n';
      }
    } else if (command == "help" || command == "h") {
      // Given help <command> print definition of command otherwise
      // print the list of available commands
      std::string helpCmd = chompToken(&input);
      if (commandToHelpText.find(helpCmd) != commandToHelpText.end()) {
        std::cout << commandToHelpText.at(helpCmd);
      } else if (!helpCmd.empty()) {
        std::cout << "Not a valid command. See below for list of commands.\n\n"
                  << topLevelHelpText;
      } else {
        std::cout << topLevelHelpText;
      }
    } else {
      return false;
    }
    return true;
  }

  /// Print the properties of \p value.
  void outputValue(const Value &value, bool isThrownValue = false) const {
    Runtime &r = runtime_;
    if (!value.isObject()) {
      std::cout << value.toString(r).utf8(r) << std::endl;
    } else {
      auto valueObj = value.getObject(r);
      auto props = valueObj.getPropertyNames(r);
      size_t propCount = props.size(r);

      // If this is a thrown value, search for a "message" property in the list.
      if (isThrownValue) {
        auto messageStr = String::createFromAscii(r, "message");
        bool haveMessage = false;
        for (size_t i = 0; i < propCount; i++) {
          if (String::strictEquals(
                  r, props.getValueAtIndex(r, i).getString(r), messageStr)) {
            haveMessage = true;
            break;
          }
        }
        // If not found and the property exists, add it to the list.
        if (!haveMessage && valueObj.hasProperty(r, messageStr)) {
          props.setProperty(r, Value((int)propCount++).toString(r), messageStr);
        }
      }

      bool compact = (propCount <= 1);
      std::cout << '{';
      for (size_t i = 0; i < propCount; i++) {
        auto keyString = props.getValueAtIndex(r, i).getString(r);
        auto value = valueObj.getProperty(r, keyString);
        auto valueString = value.toString(r);
        std::cout << (compact ? " " : "\n  ") << keyString.utf8(r) << ": "
                  << valueString.utf8(r);
      }
      std::cout << (compact ? " }\n" : "\n}\n");
    }
    std::cout.flush();
  }

  /// Given a property reference like foo.bar, print the properties of the
  /// result.
  void expandProperties(
      debugger::Debugger &debugger,
      const std::string &input) {
    Runtime &r = runtime_;

    // Trim whitespace.
    std::string remaining(input, input.find_first_not_of(" \t"));

    // Get the initial variable.
    Value variable =
        getVariable(debugger.getProgramState(), chompToken(&remaining, "."));
    // Walk the properties.
    while (!remaining.empty()) {
      if (!variable.isObject()) {
        throw std::runtime_error("Variable is not an object");
      }
      std::string component = chompToken(&remaining, ".");
      variable = variable.getObject(r).getProperty(r, component.c_str());
    }
    outputValue(variable);
  }

  Value getVariable(
      const debugger::ProgramState &state,
      const std::string &name) {
    auto lexicalInfo = state.getLexicalInfo(frame_);
    for (uint32_t depth = 0, max = lexicalInfo.getScopesCount(); depth < max;
         depth++) {
      uint32_t count = lexicalInfo.getVariablesCountInScope(depth);
      for (uint32_t i = 0; i < count; i++) {
        auto info = state.getVariableInfo(frame_, depth, i);
        if (info.name == name)
          return std::move(info.value);
      }
    }
    throw std::runtime_error("No variable named " + name);
  }

  /// Print information about the paused program state \p state to stdout.
  void printPauseInfo(const debugger::ProgramState &state) {
    using debugger::PauseReason;
    bool showLocation = true;
    switch (state.getPauseReason()) {
      case PauseReason::ScriptLoaded:
        std::cout << "Break on script load in ";
        break;
      case PauseReason::DebuggerStatement:
        std::cout << "Break on 'debugger' statement in ";
        break;
      case PauseReason::Breakpoint:
        std::cout << "Break on breakpoint " << state.getBreakpoint() << " in ";
        break;
      case PauseReason::Exception:
        std::cout << "Break on exception in ";
        break;
      case PauseReason::AsyncTrigger:
        std::cout << "Interrupted in ";
        break;
      case PauseReason::StepFinish:
        std::cout << "Stepped to ";
        break;
      case PauseReason::EvalComplete: {
        auto evalResult = state.getEvalResult();
        if (!evalResult.isException) {
          outputValue(evalResult.value);
        } else {
          const auto &exception = evalResult.exceptionDetails;
          std::cout << "Exception: " << exception.text << std::endl;
          printStackTrace(exception.getStackTrace(), -1);
          std::cout << "Thrown value is: ";
          outputValue(evalResult.value, true);
        }
        showLocation = false;
        break;
      }
    }
    if (showLocation) {
      auto frame = state.getStackTrace().callFrameForIndex(0);
      std::string funcName = frame.functionName;
      if (funcName.empty())
        funcName = "(unknown)";
      std::cout << funcName << ": " << frame.location.fileName << '['
                << frame.location.fileId << ']' << ':' << frame.location.line
                << ':' << frame.location.column << std::endl;
    }
  }

  /// Override of event observer callback. The debugger has paused; we get to
  /// report on the pause and then return an action.
  debugger::Command didPause(debugger::Debugger &debugger) {
    setSIGINTShouldPause(false);
    const auto &state = debugger.getProgramState();
    printPauseInfo(state);

    std::string line;
    for (;;) {
      if (::isatty(STDIN_FILENO)) {
        std::cout << "(hdb) ";
      }
      if (!getline(line)) {
        // Input exhausted. If we're interactive, exit on control-D, otherwise
        // just continue.
        if (::isatty(STDIN_FILENO))
          exit(0);
        return debugger::Command::continueExecution();
      } else if (line.empty()) {
        if (defaultCommand_.empty() || !::isatty(STDIN_FILENO)) {
          continue;
        }
        line = defaultCommand_;
      }

      defaultCommand_ = line;

      // Try interpreting it as an immediate command.
      if (parseAndRunImmediateCommand(line, debugger)) {
        continue;
      }

      // Try parsing the command.
      debugger::Command command = debugger::Command::continueExecution();
      if (parseCommand(line, debugger, &command)) {
        std::cout.flush();
        setSIGINTShouldPause(true);
        return command;
      } else {
        std::cout << "Unrecognized command" << std::endl;
      }
    }
  }

  void breakpointResolved(
      debugger::Debugger &debugger,
      debugger::BreakpointID breakpoint) {
    const auto info = debugger.getBreakpointInfo(breakpoint);
    assert(info.resolved && "resolved event on unresolved breakpoint");
    const auto &loc = info.resolvedLocation;
    std::cout << "Breakpoint " << breakpoint << " resolved to " << loc.fileName
              << '[' << loc.fileId << ']' << ':' << loc.line << ':'
              << loc.column << '\n';
  }

 private:
  /// Wrapper around std::getline().
  /// Read a line from cin, storing it into \p line.
  /// \return true if we have a line, false if input was exhausted.
  static bool getline(std::string &line) {
    for (;;) {
      // On receiving EINTR, getline() in libc++ appears to incorrectly mark
      // cin's EOF bit. This means that sucessive getline() calls will return
      // EOF. Workaround this iostream bug by clearing the cin flags on EINTR.
      errno = 0;
      if (std::getline(std::cin, line)) {
        return true;
      } else if (errno == EINTR) {
        std::cin.clear();
      } else {
        // Input exhausted.
        return false;
      }
    }
  }

  /// Which frame is selected, for the purpose of exec.
  uint32_t frame_ = 0;

  /// The command to run if no command is entered by a user.
  std::string defaultCommand_{};

  /// Our runtime.
  HermesRuntime &runtime_;
};

int main(int argc, char **argv) {
  const char *filename = nullptr;
  int breakAtStart = 0;
  int lazy = 0;
  double breakAfterDelay = -1;

  const struct option longopts[] = {
      {"break-at-start", no_argument, &breakAtStart, 1},
      {"lazy", no_argument, &lazy, 1},
      {"break-after", required_argument, nullptr, 'b'},
      {nullptr, 0, nullptr, 0}};

  int ch;
  while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
    switch (ch) {
      case 0:
        // Processed long option.
        break;
      case 'b': {
        char *endptr = nullptr;
        breakAfterDelay = std::strtod(optarg, &endptr);
        if (*endptr != '\0' || breakAfterDelay < 0 ||
            !std::isfinite(breakAfterDelay)) {
          std::cerr << "Invalid delay" << std::endl;
          exit(EXIT_FAILURE);
        }
        break;
      }
      default:
        printUsageAndExit();
        break;
    }
  }

  HermesRuntime::DebugFlags debugFlags{};
  debugFlags.lazy = lazy;

  filename = argv[optind];
  if (!filename)
    printUsageAndExit();

  // Read the file in 'filename'.
  std::ifstream fileStream(filename);
  if (!fileStream) {
    const char *err = strerror(errno);
    std::cerr << "Unable to open file " << argv[1] << ": " << err << std::endl;
    return EXIT_FAILURE;
  }
  std::string fileContents(
      (std::istreambuf_iterator<char>(fileStream)),
      std::istreambuf_iterator<char>());

  std::unique_ptr<HermesRuntime> runtime = makeHermesRuntime();
  HDBDebugger debugger(*runtime);
  runtime->getDebugger().setEventObserver(&debugger);
  runtime->getDebugger().setShouldPauseOnScriptLoad(breakAtStart);
  gDebugger = &runtime->getDebugger();
  try {
    setSIGINTShouldPause(true);
    if (breakAfterDelay >= 0) {
      schedulePause(breakAfterDelay);
    }
    runtime->debugJavaScript(fileContents, filename, debugFlags);
  } catch (const facebook::jsi::JSIException &e) {
    std::cout << "JavaScript terminated via uncaught exception: " << e.what()
              << '\n';
    gDebugger = nullptr;
    return EXIT_FAILURE;
  }
  gDebugger = nullptr;
  return 0;
}

#endif // HERMES_ENABLE_DEBUGGER
