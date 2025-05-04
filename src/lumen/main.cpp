#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "util/common.h"
#include "util/array.h"
#include "log/log.h"
#include "socket/socket_conf.h"
#include "socket/socket.h"

#define LOG_LOC "./lumen.log"

#define LOG_CONSOLE_THRESHOLD_THIS  LOG_THRESHOLD_DEFAULT
#define LOG_FILE_THRESHOLD_THIS     LOG_THRESHOLD_MAX

#define UNUSED(n) (void)n;

volatile sig_atomic_t running = 1;

void handler(int signum)
{
    UNUSED(signum)
    running = 0;
}

int main(int argc, const char** argv)
{
    UNUSED(argc)
    UNUSED(argv)

    LOG_init(LOG_LOC);
    LOG_start();
    LOG_INFO("Starting Lumen...");

    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; /* Restart functions if
                                 interrupted by handler */

    if (sigaction(SIGINT,  &sa, NULL) == -1) return -1;
    if (sigaction(SIGQUIT,  &sa, NULL) == -1) return -1;

    SocketConfig conf = SocketConfig();

    // Setup config priority
    const char* conf_loc[] = {NULL, CONF_DEFAULT_LOCATION};
    uint8_t conf_loc_len = UTIL_len(conf_loc);
    if (argc > 1) conf_loc[0] = argv[1];

    for (uint8_t iter = 0; iter < conf_loc_len; iter++)
        if(conf_loc[iter] && !conf.SetFilePath(conf_loc[iter]))
            break; // If file is successfully set, break loop

    conf.ParseConfig();
    conf.DumpToLog(LOG_INFO);

    Socket socket = Socket(&conf);
    socket.Start();

    LOG_INFO("Running Lumen...");
    while(running)
    {
        if(socket.WaitingCount())
        {
            Socket::Message *msg = socket.PopWaiting();
            for (int i = 0; i < msg->len - 1; i++)
                msg->payload[i] ^= 0x20;
            usleep(5e5);
            socket.SendMessage(msg);
        }
    }
    LOG_INFO("Exiting Lumen...");

    socket.Stop();

    LOG_stop();
    LOG_destory();
    return 0;
}