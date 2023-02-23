#ifndef __PACKET_QUEUE_H__
#define __PACKET_QUEUE_H__

#include <mutex>
#include <thread>
#include <condition_variable>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/fifo.h>
}

typedef struct MyAVPacketList {
    AVPacket *pkt;
    int serial;
} MyAVPacketList;

class PacketQueue {
public:
    PacketQueue();

    virtual ~PacketQueue();

    int put(AVPacket *pkt);

    int get(AVPacket *pkt);

    void flush();

    void stop();

    int size() const;

private:
    int put_private(AVPacket *pkt);

private:
    AVFifoBuffer *pkt_list_;
    std::mutex mutex_;
    std::condition_variable cond_;
    int stop_request_;
    int nb_packets_;
};

#endif // __PACKET_QUEUE_H__
