#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/sctp.h>

#include "socket/socket_conf.h"
#include "data/s_list.h"

#define SOCKET_MSG_POOL_LEN 1024
#define SOCKET_MSG_LEN 1024

class Socket
{
public:
    // #pragma pack (push,1)
    typedef struct socket_msg
    {
        S_List_Node     node;
        uint16_t        len;
        uint8_t         payload[SOCKET_MSG_LEN];
    } Message;
    // #pragma pack (pop)

private:
    // private fields
    typedef enum socket_queue_idx
    {
        FREE,
        RECV,
        SEND,
        QUEUE_LEN,  // ALWAYS LAST
    } QueueIndex;

    volatile bool   running;
    pthread_t       pid;

    SocketConfig    *conf;
    Message*        msg_pool;
    uint32_t        msg_pool_len;
    S_List          queues[QUEUE_LEN];
    pthread_mutex_t locks[QUEUE_LEN];

    // private methods

public:
    Socket(SocketConfig* conf);
    ~Socket();

    int Start();
    int Stop();
    bool IsRunning();
    int PollIncoming(int conn_fd, struct sctp_sndrcvinfo *sndrcvinfo);
    int PollOutgoing(int conn_fd);

    int InitMessage(Message *msg);
    Message* PopMessage();
    Message* PopWaiting();
    int SendMessage(Message *msg);


    /**
     * @brief Get count of messages waiting to be processed
     */
    int WaitingCount();

    /**
     * @brief Get count of messages waiting to be sent
     */
    int PendingCount();
};