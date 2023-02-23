
#include "packetQueue.h"

PacketQueue::PacketQueue() {
    pkt_list_ = av_fifo_alloc(sizeof(MyAVPacketList));
    stop_request_ = 0;
    nb_packets_ = 0;
}

PacketQueue::~PacketQueue() {
    stop_request_ = 1;
    flush();
    av_fifo_freep(&pkt_list_);
}

int PacketQueue::put(AVPacket *pkt) {
    AVPacket *pkt1 = nullptr;
    int ret = 0;

    pkt1 = av_packet_alloc();
    if (!pkt1) {
        av_packet_unref(pkt);
        return -1;
    }
    av_packet_move_ref(pkt1, pkt);

    std::lock_guard<std::mutex> lck(mutex_);
    ret = put_private(pkt1);
    cond_.notify_all();

    if (ret < 0)
        av_packet_free(&pkt1);

    nb_packets_++;

    return ret;
}

int PacketQueue::get(AVPacket *pkt) {
    MyAVPacketList pkt1;

    std::unique_lock<std::mutex> lck(mutex_);
    cond_.wait(lck, [&]() {
        return stop_request_ == 1 || av_fifo_size(pkt_list_) >= sizeof(pkt1);
    });
    if (stop_request_ == 1)
        return -1;

    av_fifo_generic_read(pkt_list_, &pkt1, sizeof(pkt1), NULL);
    av_packet_move_ref(pkt, pkt1.pkt);
    av_packet_free(&pkt1.pkt);

    nb_packets_--;

    return 0;
}

void PacketQueue::flush() {
    MyAVPacketList pkt1;

    std::lock_guard<std::mutex> lck(mutex_);
    while (av_fifo_size(pkt_list_) >= sizeof(pkt1)) {
        av_fifo_generic_read(pkt_list_, &pkt1, sizeof(pkt1), NULL);
        av_packet_free(&pkt1.pkt);
    }
}

void PacketQueue::stop() {
    stop_request_ = 1;
    cond_.notify_all();
}

int PacketQueue::size() const
{
    return nb_packets_;
}

int PacketQueue::put_private(AVPacket *pkt) {
    MyAVPacketList pkt1;
    if (av_fifo_space(pkt_list_) < sizeof(pkt1)) {
        if (av_fifo_grow(pkt_list_, sizeof(pkt1)) < 0)
            return -1;
    }

    pkt1.pkt = pkt;
    av_fifo_generic_write(pkt_list_, &pkt1, sizeof(pkt1), NULL);
    return 0;
}
