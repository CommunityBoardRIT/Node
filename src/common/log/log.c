#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "log/log.h"
#include "log/log_private.h"
#include "util/common.h"
#include "util/timestamp.h"

#define LOG_CLOCK CLOCK_REALTIME
#define LOG_TS_LEN (UTIL_TIMESTAMP_LEN + UTIL_TS_MSEC_LEN + 1)

#define LOG_CONSOLE_THRESHOLD_THIS  LOG_THRESHOLD_MAX
#define LOG_FILE_THRESHOLD_THIS     LOG_THRESHOLD_MAX

Log the_log;

static int8_t LOG_timestamp(char* dst_str)
{
    struct timespec ts;
    int ret = clock_gettime(LOG_CLOCK, &ts);
    char* str_iter = dst_str;
    
    ret = UTIL_time_iso8601_timestamp_UTC(str_iter, ts.tv_sec);
    if (ret == -1) return -1; // Can't log error: recursive call
    str_iter += ret;
    
    ret = UTIL_time_msec_timestamp(str_iter, ts.tv_nsec / 1000);
    if (ret == -1) return -1; // Can't log error: recursive call
    str_iter += ret;
    str_iter += sprintf(str_iter, "Z");
    return str_iter - dst_str;
}

static uint8_t get_level_name(char* dst, uint8_t level, bool color)
{
    if (level > 9 + (uint8_t) LOG_VERBOSE) return -1;

    int8_t vlev = -1;
    uint8_t offset = 0;

    if (level >= (uint8_t) LOG_VERBOSE)
    { 
        vlev = level - (uint8_t) LOG_VERBOSE;
        level = LOG_VERBOSE;
    }

    if (color)
        offset += sprintf(dst + offset, "%s", _LOG_level_color[level]);

    if (vlev != -1)
        offset += sprintf(dst + offset, "%s:%d ", log_level_names[level], vlev);
    else
        offset += sprintf(dst + offset, "%-9s ", log_level_names[level]);  // Left justified, 13 for "LOG_VERBOSE:0"
    
    if (color)
        offset += sprintf(dst + offset, "%s", COLOR_RESET);

    return offset;
}

static uint16_t format_log_entry(char* dst, LOG_Buffer *buf, bool use_color)
{
    uint16_t msg_iter = 0;

    // Timestamp
    char ts[LOG_TS_LEN];
    LOG_timestamp(&ts[0]);
    msg_iter += sprintf(dst + msg_iter, "[%s] ", ts);

    // Level
    msg_iter += get_level_name(dst + msg_iter, buf->level, use_color);

    // Message
    msg_iter += sprintf(dst + msg_iter, "%s", buf->msg);
    msg_iter += sprintf(dst + msg_iter, "\n");
    return msg_iter;
}

void LOG_thread_print(LOG_Buffer *buf)
{
    char msg[1024];

    // Console out
    if (buf->flags & (1 << LOG_BUFFER_FLAG_OUT_CONSOLE))
    {
        format_log_entry(&msg[0], buf, LOG_USE_COLOR);
        #if LOG_USE_STDERR
        if (buf->level <= LOG_ERROR)
            fprintf(stderr, "%s", msg);
        else printf("%s", msg);
        #else
        printf("%s", msg);
        #endif
    }

    // File out
    if (-1 != the_log.fd && buf->flags & (1 << LOG_BUFFER_FLAG_OUT_FILE))
    {
        uint16_t len = format_log_entry(&msg[0], buf, false) + 1;
        write(the_log.fd, &msg[0], len-1);
    }
}

static int8_t LOG_Buffer_init(LOG_Buffer *buf)
{
    if (!buf) return -1;
    memset(buf, 0, sizeof(LOG_Buffer));
    DATA_S_List_Node_init(&buf->node);

    return 0;
}

void LOG_thread_poll()
{
    pthread_mutex_lock (&the_log.wr_lock);
    S_List_Node *node = DATA_S_List_pop(&the_log.wr_queue);
    pthread_mutex_unlock (&the_log.wr_lock);

    if (!node) return;
    LOG_Buffer *buf = DATA_LIST_GET_OBJ(node, LOG_Buffer, node);
    LOG_thread_print(buf);

    pthread_mutex_lock (&the_log.free_lock);
    LOG_Buffer_init(buf);
    DATA_S_List_append(&the_log.free_queue, &buf->node);
    pthread_mutex_unlock (&the_log.free_lock);
}

static LOG_Buffer *get_buffer(Log *log)
{
    pthread_mutex_lock (&log->free_lock);
    S_List_Node* node = DATA_S_List_pop(&log->free_queue);
    pthread_mutex_unlock(&log->free_lock);

    if (!node) return NULL;
    LOG_Buffer* buf = DATA_LIST_GET_OBJ(node, LOG_Buffer, node);
    return buf;
}

static int8_t push_buffer(Log *log, LOG_Buffer *buf)
{
    if (!log || !buf) return -1;
    pthread_mutex_lock (&log->wr_lock);
    DATA_S_List_append(&log->wr_queue, &buf->node);
    pthread_mutex_unlock(&log->wr_lock);
    return 0;
}

int8_t LOG_print(const char* file, uint16_t line, int8_t console_threshold, int8_t file_threshold, int8_t level, const char* fmt, ...)
{
    // Check if message is important enough to print
    uint8_t flags = 0;
    if (level <= console_threshold) flags |= (1 << LOG_BUFFER_FLAG_OUT_CONSOLE);
    if (level <= file_threshold)    flags |= (1 << LOG_BUFFER_FLAG_OUT_FILE);
    if (!flags) return 0;

    // Get timestamp
    struct timespec ts;
    int ret = clock_gettime(LOG_CLOCK, &ts);
    if (ret == -1) return -1;

    // Get buffer
    LOG_Buffer tmp;
    LOG_Buffer *buf;

    if (the_log.config.en)
    {
        buf = get_buffer(&the_log);
        if(!buf)
        {
            printf("LOG OUT OF RESOURCES\n");
            return -1;
        }
    }
    else
    {
        LOG_Buffer_init(&tmp);
        buf = &tmp;
    }

    // Populate buffer
    buf->timestamp = ts;
    buf->level = level;
    buf->flags = flags;

    va_list list;
    va_start(list, fmt);
    if (the_log.config.print_loc)
    {
        buf->len += sprintf(&buf->msg[buf->len], "[%s : %4d]: ", file, line);
    }
    buf->len += vsprintf(&buf->msg[buf->len], fmt, list) + 1;
    va_end(list);

    // Handle buffer
    if (the_log.config.en)
    {
        // If the log is enabled, put on write queue
        ret = push_buffer(&the_log, buf);
        if (ret == -1) return -1;
    }
    else
    {
        // If the log is disabled, print the buffer directly
        LOG_thread_print(buf);
    }

    return 0;
}

static void *LOG_run(void*)
{
    // TODO: add exit polling
    while(1)
    {
        if (0 == the_log.wr_queue.len)
        {
            if (!the_log.config.en) break;
            usleep(LOG_POLL_PERIOD_US);
            continue;
        }
        LOG_thread_poll();
    }

    return 0;
}

static int8_t init_log_lists(Log *log)
{
    if (!log) return -1;
    
    DATA_S_List_init (&log->free_queue);
    DATA_S_List_init (&log->wr_queue);

    for (uint16_t i = 0; i < LOG_MAX_BUFFER; i++)
    {
        if (LOG_Buffer_init(&log->buffer_pool[i]) == -1) return -1;
        DATA_S_List_append(&the_log.free_queue, &log->buffer_pool[i].node);
    }

    return 0;
}

int8_t LOG_prep()
{
    memset(&the_log, 0, sizeof(Log));
    the_log.buffer_pool = (LOG_Buffer*) malloc(LOG_MAX_BUFFER * sizeof(LOG_Buffer));
    the_log.pool_count = LOG_MAX_BUFFER;

    if (-1 == pthread_mutex_init(&the_log.gen_lock,  NULL)) return -1;
    if (-1 == pthread_mutex_init(&the_log.wr_lock,   NULL)) return -1;
    if (-1 == pthread_mutex_init(&the_log.free_lock, NULL)) return -1;
    if (-1 == init_log_lists(&the_log)) return -1;
    the_log.config.print_loc = LOG_USE_LOCATION;
    the_log.fd = -1;
    return 0;
}

int8_t LOG_init(const char* log_loc)
{
    if (!log_loc) STD_FAIL;
    LOG_prep();
    the_log.config.path = log_loc;
    the_log.fd = open(the_log.config.path, LOG_FILE_FLAG, LOG_FILE_PERM);
    if (-1 == the_log.fd) LOG_WARN("Invalid log file path: (%d) %s", errno, strerror(errno));
    LOG_INFO("Log Module Initialized.");
    return 0;
}

int8_t LOG_start()
{
    pthread_create(&the_log.ptid, NULL, LOG_run, NULL);
    the_log.config.en = true;
    LOG_INFO("Log Module Running...");
    return 0;
}

int8_t LOG_stop()
{
    the_log.config.en = false; // Prevent new log statements
    pthread_join(the_log.ptid, NULL);
    return 0;
}

int8_t LOG_destory()
{
    if(-1 != the_log.fd) close(the_log.fd);
    pthread_mutex_destroy(&the_log.gen_lock);
    DATA_S_List_deinit(&the_log.free_queue);
    the_log.pool_count = 0;
    if(the_log.buffer_pool)
    {
        free(the_log.buffer_pool);
        the_log.buffer_pool = NULL;
    }
    return -1;
}