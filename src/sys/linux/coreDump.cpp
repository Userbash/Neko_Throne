#include <include/sys/linux/coreDump.h>
#include <sys/resource.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>
#include <ctime>

static void posix_crash_handler(int sig) {
    void *array[50];
    size_t size;

    size = backtrace(array, 50);

    // Try multiple locations for crash log
    const char* paths[] = {"crash.log", "/tmp/throne_crash.log", "/var/tmp/throne_crash.log"};
    FILE *f = nullptr;
    for (const char* path : paths) {
        f = fopen(path, "a");
        if (f) break;
    }

    if (f) {
        time_t now = time(nullptr);
        fprintf(f, "\n--- CRASH DETECTED --- \nTime: %sSignal: %d\n", ctime(&now), sig);
        backtrace_symbols_fd(array, size, fileno(f));
        fprintf(f, "----------------------\n");
        fclose(f);
    }

    fprintf(stderr, "\n--- CRASH DETECTED (Signal %d) ---\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "----------------------------------\n");

    // Check if core dump is explicitly enabled
    const char* allow_core_dump = getenv("THRONE_ALLOW_CORE_DUMP");
    if (allow_core_dump != nullptr && allow_core_dump[0] != '\0') {
        signal(sig, SIG_DFL);
        raise(sig);
    } else {
        _exit(1);
    }
}

void setup_crash_handlers() {
    rlimit rl;
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rl);

    signal(SIGSEGV, posix_crash_handler);
    signal(SIGABRT, posix_crash_handler);
    signal(SIGFPE,  posix_crash_handler);
    signal(SIGILL,  posix_crash_handler);
}
