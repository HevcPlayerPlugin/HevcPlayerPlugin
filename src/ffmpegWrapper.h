#ifndef __FFMPEG_WRAPPER_H__
#define __FFMPEG_WRAPPER_H__

#include <mutex>
#include <thread>
#include <functional>
#include <condition_variable>
#include "packetQueue.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/fifo.h>
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libavutil/ffversion.h>
}

#undef av_err2str
#define av_err2str(errnum) \
av_make_error_string((char*)_malloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)
#undef av_ts2timestr
#define av_ts2timestr(ts, tb) \
av_ts_make_time_string((char*)_malloca(AV_TS_MAX_STRING_SIZE), ts, tb)

typedef std::function<int(void *user, uintptr_t handle, uint8_t *data, size_t length)> FF_RAW_DATA_CALLBACK;
typedef std::function<int(void *user, uintptr_t handle, int err_code, const uint8_t *err_desc)> FF_EXCEPTION_CALLBACK;

class FfmpegWrapper {
public:
    using AVPacketPtr = std::shared_ptr<AVPacket>;
    using AVFramePtr = std::shared_ptr<AVFrame>;
    FfmpegWrapper();

    virtual ~FfmpegWrapper();

public:
    // suggest be called before startPlay, NOT MUST.
    int setCallback(void *user, uintptr_t handle, 
        const FF_RAW_DATA_CALLBACK &pfn, const FF_EXCEPTION_CALLBACK& pfn2);

    int startPlay(const char *inputUrl, int width, int height,
                  int useGPU = 1, int useTCP = 1, int retryTimes = 3);

    int stopPlay();

    int changeVideoResolution(int width, int height);

    // discard one frame per two frames, 0: close, 1: open
    int openDiscardFrames(int enabled);

private:
    int open_input_url(const char *inputUrl, int useTCP, int retryTimes);

    int hw_decoder_init(AVCodecContext *ctx);

    int audio_open();

    int hw_get_config(const AVCodec *decoder, AVHWDeviceType type);

    int retrieve_frame(AVFrame* in, AVFrame** out);

    int scale_frame(AVFrame* in, AVFrame** out);

    int output_video_frame(AVFrame *frame);

    int output_audio_frame(AVFrame *frame);

    int decode_packet(AVCodecContext *dec, const AVPacket *pkt, AVFrame *frame);

    int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx,
                           enum AVMediaType type);

    static enum AVPixelFormat hw_get_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);

    void audio_decode_thread();

    void video_decode_thread();

    static int input_interrupt_cb(void *ctx);

private:
    std::string inputUrl_;
    int useGPU_;
    AVBufferRef *hw_device_ctx_;
    AVHWDeviceType device_type_;
    static enum AVPixelFormat hw_pix_fmt_;

    AVFormatContext *fmt_ctx_;
    time_t preTime_;

    AVCodecContext *video_dec_ctx_;
    AVCodecContext *audio_dec_ctx_;

    bool sws_init_;
    struct SwsContext *sws_ctx_;

    bool swr_init_;
    struct SwrContext* swr_ctx_;

    int output_width_;
    int output_height_;

    uint32_t current_pts_audio_in_ms_;
    uint32_t current_pts_video_in_ms_;

    AVStream *video_stream_;
    AVStream *audio_stream_;

    PacketQueue audio_packet_queue_;
    PacketQueue video_packet_queue_;

    AVFrame *sw_frame_;
    AVFrame *frameYUV_;

    uint8_t *video_dst_data_;
    uint8_t *audio_dst_data_;

    std::thread audio_decode_thread_handle_;
    std::thread video_decode_thread_handle_;
    std::thread main_read_thread_handle_;
    int stop_request_;
    int discard_frame_enabled_;
    uint32_t discard_frame_index_;

    void *user_data_;
    uintptr_t user_handle_;
    FF_RAW_DATA_CALLBACK ff_send_data_callback_;
    FF_EXCEPTION_CALLBACK ff_exception_callback_;

    SDL_AudioDeviceID audio_dev_;
};


#endif // __FFMPEG_WRAPPER_H__
