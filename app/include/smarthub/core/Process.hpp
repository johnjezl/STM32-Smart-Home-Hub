/**
 * Runs a program with an argument list, without a shell
 */
#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace smarthub::core {

struct ProcessResult {
    // The exit code, or -1 if the program couldn't be started, didn't exit normally or was
    // stopped for taking too long
    int exitCode = -1;
    bool timedOut = false;
    std::string output;
};

/**
 * Runs argv[0], looked up in PATH if it has no '/', with argv as its arguments, and waits for it.
 * Each argument reaches the program exactly as given, so values from outside need no quoting.
 * The program's stdin is /dev/null, and it inherits no descriptors besides 0 to 2.
 *
 * @param argv The program and its arguments
 * @param includeStderr Whether stderr goes into the output too, rather than /dev/null
 * @param timeout How long it may run before it's killed, with the output so far returned
 */
ProcessResult runProcess(const std::vector<std::string>& argv, bool includeStderr = false,
                         std::chrono::milliseconds timeout = std::chrono::seconds(30));

}  // namespace smarthub::core
