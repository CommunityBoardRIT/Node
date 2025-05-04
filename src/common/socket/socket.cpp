#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

#include "socket/socket.h"
#include "socket/socket_conf.h"
#include "util/common.h"

#define LOG_CONSOLE_THRESHOLD_THIS  LOG_VERBOSE
#define LOG_FILE_THRESHOLD_THIS     LOG_THRESHOLD_MAX

int Socket::InitMessage(Socket::Message *msg)
{
    if (!msg) STD_FAIL

    memset(msg, 0, sizeof(Socket::Message));
    DATA_S_List_Node_init(&msg->node);

    pthread_mutex_lock(&locks[QueueIndex::FREE]);
    DATA_S_List_append(&queues[QueueIndex::FREE], &msg->node);
    pthread_mutex_unlock(&locks[QueueIndex::FREE]);
    return 0;
}

Socket::Socket(SocketConfig* conf)
{
    this->conf = conf;
    msg_pool_len = SOCKET_MSG_POOL_LEN;
    msg_pool = (Message*) malloc(msg_pool_len * sizeof(Message));

    // pthread_mutexattr_t attr;
    // pthread_mutexattr_init(&attr);
    // pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    for (uint8_t queue_idx = 0; queue_idx < QUEUE_LEN; queue_idx++)
    {
        DATA_S_List_init(&queues[queue_idx]);
        pthread_mutex_init(&locks[queue_idx], NULL);
    }

    for (uint32_t msg_idx = 0; msg_idx < msg_pool_len; msg_idx++)
    {
        InitMessage(&msg_pool[msg_idx]);
    }

    running = false;
    pid = 0;
}

Socket::~Socket()
{
    if (msg_pool) free(msg_pool);
    msg_pool = NULL;
}

int Socket::PollIncoming(int conn_fd, struct sctp_sndrcvinfo *sndrcvinfo)
{
    pthread_mutex_lock(&locks[QueueIndex::FREE]);
    S_List_Node *node = DATA_S_List_pop(&queues[QueueIndex::FREE]);
    pthread_mutex_unlock(&locks[QueueIndex::FREE]);

    if (!node) STD_FAIL;
    Message *msg = DATA_LIST_GET_OBJ(node, Message, node);

    int flags = 0, res = 0;
    res = sctp_recvmsg(conn_fd, &msg->payload[0], sizeof(msg->payload), NULL, 0, sndrcvinfo, &flags);
    if (res == -1)
    {
        InitMessage(msg);
        if (errno == EAGAIN) return 0;
        STD_FAIL;
    }

    msg->len = res;

    LOG_VERBOSE(0, "Received data: %s", &msg->payload[0]);
    pthread_mutex_lock(&locks[QueueIndex::RECV]);
    DATA_S_List_append(&queues[QueueIndex::RECV], &msg->node);
    pthread_mutex_unlock(&locks[QueueIndex::RECV]);

    // For some reason, right around here, args->sockets to just randomly change value, changing mutex addresses
    return 1;
}

int Socket::PollOutgoing(int conn_fd)
{
    pthread_mutex_lock(&locks[QueueIndex::SEND]);
    S_List_Node *node = DATA_S_List_pop(&queues[QueueIndex::SEND]);
    pthread_mutex_unlock(&locks[QueueIndex::SEND]);

    if (!node) STD_FAIL;
    Message *msg = DATA_LIST_GET_OBJ(node, Message, node);

    int res = 0;
    res = sctp_sendmsg(
        conn_fd,
        &msg->payload[0],
        msg->len,
        NULL,
        0,
        0,
        0,
        0,
        0,
        0);


    InitMessage(msg);

    if (res == -1)
    {
        if (errno == EAGAIN) return 0;
        LOG_ERROR("Failed to send message.");
        STD_FAIL;
    }

    return 1;
}

bool Socket::IsRunning()
{
    return running;
}

static void die (const char* msg)
{
    LOG_ERROR("SOCKET FAILURE: %s", msg);
    LOG_stop();
    exit(1);
}

struct run_args
{
    SocketConfig    *conf;
    Socket          *socket;
};

static void *run (void *a)
{
    // Init variables
    struct run_args *args = (struct run_args*) a;

    int listen_fd, conn_fd, ret;
    struct sctp_sndrcvinfo sndrcvinfo;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family         = AF_INET;
    // servaddr.sin_port           = args->conf->GetBindingPort();
    servaddr.sin_port           = htons(1208);
    // servaddr.sin_addr.s_addr    = args->conf->GetBindingAddress();
    servaddr.sin_addr.s_addr    = inet_addr("127.0.0.1");

    struct sctp_initmsg initmsg = {
        .sinit_num_ostreams = 5,
        .sinit_max_instreams = 5,
        .sinit_max_attempts = 4,
        .sinit_max_init_timeo = 60000U,
    };

    // Setup listening
    listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_SCTP);
    if (listen_fd < 0)
        die("socket");

    ret = bind(listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (ret < 0)
        die("bind");

    ret = setsockopt(listen_fd, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg));
    if (ret < 0)
        die("setsockopt");

    ret = listen(listen_fd, initmsg.sinit_max_instreams);
    if (ret < 0)
        die("listen");

    // Listening; Waiting for connection
    LOG_INFO("Waiting for connection");

    do
    {
        conn_fd = accept4(listen_fd, (struct sockaddr *) NULL, NULL, SOCK_NONBLOCK);
        if(!args->socket->IsRunning()) return 0;
    }
    while (conn_fd == -1 && errno == EAGAIN);
    if(conn_fd < 0 && errno != EAGAIN)
        die("accept()");

    // Connection detected
    LOG_INFO("New client connected");

    Socket* sock = args->socket;
    while(1)
    {
        if (sock->IsRunning() == false) break;
        while(sock->PendingCount())
        {
            if(sock->PollOutgoing(conn_fd) == -1) break;
            usleep(1e3);
        }

        int res = 0;
        // For some god forsaken reason, PollIncoming causes the args->socket pointer to change value
        while((res = sock->PollIncoming(conn_fd, &sndrcvinfo)) > 0)
        {
            usleep(1e3);
        }
        usleep(10e3);
    }

    LOG_INFO("Closing connection...");
    close(conn_fd);
    conn_fd = -1;
    return 0;
}

int Socket::Start()
{
    if (running) STD_FAIL;
    running = true;

    struct run_args args = {.conf = conf, .socket = this};
    int res = pthread_create(&pid, NULL, &run, &args);
    if (res == -1) STD_FAIL;
    return 0;
}

int Socket::Stop()
{
    if (!running) STD_FAIL;
    running = false;
    pthread_join(pid, NULL);
    return 0;
}

int Socket::SendMessage(Socket::Message *msg)
{
    if (!msg) STD_FAIL;

    pthread_mutex_lock(&locks[QueueIndex::SEND]);
    DATA_S_List_append(&queues[QueueIndex::SEND], &msg->node);
    pthread_mutex_unlock(&locks[QueueIndex::SEND]);
    return 0;
}

Socket::Message* Socket::PopMessage()
{
    pthread_mutex_lock(&locks[QueueIndex::FREE]);
    S_List_Node *node = DATA_S_List_pop(&queues[QueueIndex::FREE]);
    pthread_mutex_unlock(&locks[QueueIndex::FREE]);


    if (!node) return NULL;
    return DATA_LIST_GET_OBJ(node, Socket::Message, node);
}

Socket::Message* Socket::PopWaiting()
{
    pthread_mutex_lock(&locks[QueueIndex::RECV]);
    S_List_Node *node = DATA_S_List_pop(&queues[QueueIndex::RECV]);
    pthread_mutex_unlock(&locks[QueueIndex::RECV]);

    if (!node) return NULL;
    return DATA_LIST_GET_OBJ(node, Socket::Message, node);
}

int Socket::WaitingCount()
{
    return queues[QueueIndex::RECV].len;
}

int Socket::PendingCount()
{
    pthread_mutex_lock(&locks[QueueIndex::SEND]);
    int ret = queues[QueueIndex::SEND].len;
    pthread_mutex_unlock(&locks[QueueIndex::SEND]);
    return ret;
}
