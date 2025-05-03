#include <stdlib.h>
#include <unistd.h>

#include "util/common.h"
#include "log/log.h"

#define LOG_LOC "./lumen.log"

#define LOG_CONSOLE_THRESHOLD_THIS  LOG_THRESHOLD_DEFAULT
#define LOG_FILE_THRESHOLD_THIS     LOG_THRESHOLD_MAX

#define UNUSED(n) (void)n;

int main(int argc, const char** argv)
{
    UNUSED(argc)
    UNUSED(argv)

    LOG_init(LOG_LOC);
    LOG_start();

    LOG_INFO("Starting Lumen...");

    usleep(5e6);

    LOG_INFO("Exiting Lumen...");
    LOG_stop();
    LOG_destory();
    return 0;
}