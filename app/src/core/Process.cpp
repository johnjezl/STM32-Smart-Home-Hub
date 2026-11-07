/**
 * Runs a program with an argument list, without a shell
 */

#include "smarthub/core/Process.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

namespace smarthub::core {

namespace {

// Looked up before fork, as only async-signal-safe calls are allowed in the child of a threaded
// program
std::string findExecutable(const std::string& name) {
    if (name.empty() || name.find('/') != std::string::npos) {
        return name;
    }
    const char* path = std::getenv("PATH");
    std::string directories = path ? path : "/usr/sbin:/usr/bin:/sbin:/bin";
    size_t start = 0;
    while (start <= directories.size()) {
        size_t end = directories.find(':', start);
        if (end == std::string::npos) {
            end = directories.size();
        }
        std::string directory = directories.substr(start, end - start);
        const std::string candidate = (directory.empty() ? "." : directory) + "/" + name;
        if (::access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
        start = end + 1;
    }
    return "";
}

void closeDescriptorsFrom(int first, int limit) {
#ifdef SYS_close_range
    if (::syscall(SYS_close_range, first, ~0U, 0) == 0) {
        return;
    }
#endif
    for (int fd = first; fd < limit; ++fd) {
        ::close(fd);
    }
}

}  // namespace

ProcessResult runProcess(const std::vector<std::string>& argv, bool includeStderr) {
    ProcessResult result;
    if (argv.empty()) {
        return result;
    }

    const std::string executable = findExecutable(argv[0]);
    if (executable.empty()) {
        return result;
    }

    std::vector<char*> arguments;
    arguments.reserve(argv.size() + 1);
    for (const auto& argument : argv) {
        arguments.push_back(const_cast<char*>(argument.c_str()));
    }
    arguments.push_back(nullptr);

    struct rlimit limits {};
    int descriptorLimit = 1024;
    if (::getrlimit(RLIMIT_NOFILE, &limits) == 0 && limits.rlim_cur != RLIM_INFINITY &&
        limits.rlim_cur < 65536) {
        descriptorLimit = static_cast<int>(limits.rlim_cur);
    }

    const int devNull = ::open("/dev/null", O_RDWR | O_CLOEXEC);
    if (devNull < 0) {
        return result;
    }
    int pipeFds[2];
    if (::pipe2(pipeFds, O_CLOEXEC) != 0) {
        ::close(devNull);
        return result;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(devNull);
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
        return result;
    }

    if (pid == 0) {
        // The app may block or ignore signals, and the child would inherit that
        sigset_t none;
        sigemptyset(&none);
        sigprocmask(SIG_SETMASK, &none, nullptr);
        signal(SIGPIPE, SIG_DFL);

        if (::dup2(devNull, STDIN_FILENO) < 0 || ::dup2(pipeFds[1], STDOUT_FILENO) < 0 ||
            ::dup2(includeStderr ? pipeFds[1] : devNull, STDERR_FILENO) < 0) {
            _exit(127);
        }
        closeDescriptorsFrom(STDERR_FILENO + 1, descriptorLimit);
        ::execv(executable.c_str(), arguments.data());
        _exit(127);
    }

    ::close(devNull);
    ::close(pipeFds[1]);

    char buffer[4096];
    for (;;) {
        const ssize_t count = ::read(pipeFds[0], buffer, sizeof(buffer));
        if (count > 0) {
            result.output.append(buffer, static_cast<size_t>(count));
        } else if (count == 0 || errno != EINTR) {
            break;
        }
    }
    ::close(pipeFds[0]);

    int status = 0;
    pid_t waited;
    do {
        waited = ::waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    if (waited == pid && WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    }
    return result;
}

}  // namespace smarthub::core
