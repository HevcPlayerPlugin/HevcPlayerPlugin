
#include "ffmpegWrapper.h"
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include "error.h"


#include <config.h>

#pragma warning (disable: 4996)
#pragma warning (disable: 4819)
#pragma warning (disable: 26812)

constexpr enum AVPixelFormat TARGET_PIX_FMT = AV_PIX_FMT_NV12;//  AV_PIX_FMT_YUV420P;
constexpr int HPP_HEADER_SIZE = 8;
constexpr int DISCARD_FRAME_FREQUENCY = 2;
constexpr int MAX_PACKET_VIDEO = 10;
constexpr int MAX_PACKET_AUDIO = 30;
constexpr int AUDIO_CHANNELS_DEFAULT = 2;

constexpr int gResolution_[][2] = {
        {256,  144},
        {640,  360},    // 建议模式：16分屏
        {800,  600},    //            9分屏
        {1280, 720},    //            4分屏
        {1920, 1080}    //            1分屏
};
static int checkVideoResolution(uint16_t width, uint16_t height) {
    for (const auto &vr: gResolution_) {
        if (vr[0] != width) continue;
        return vr[1] == height ? 0 : -1;
    }
    return -1;
}

static int showBanner() {
    av_log(NULL, AV_LOG_WARNING, "\nversion " FFMPEG_VERSION);
    av_log(NULL, AV_LOG_WARNING, " built with %s\n", CC_IDENT);
    av_log(NULL, AV_LOG_WARNING, "configuration: " FFMPEG_CONFIGURATION "\n\n");
    return 0;
}

AVPixelFormat FfmpegWrapper::hw_pix_fmt_ = AV_PIX_FMT_NONE;
FfmpegWrapper::FfmpegWrapper() : fmt_ctx_(nullptr), sws_init_(false), sws_ctx_(nullptr),
                                 video_dec_ctx_(nullptr), audio_dec_ctx_(nullptr), hw_device_ctx_(nullptr),
                                 sw_frame_(nullptr), frameYUV_(nullptr), stop_request_(0), video_dst_data_(nullptr),
                                 audio_dst_data_(nullptr), current_pts_audio_in_ms_(0), current_pts_video_in_ms_(0),
                                 output_width_(-1), output_height_(-1), audio_stream_(nullptr), video_stream_(nullptr),
                                 useGPU_(0), user_data_(nullptr), user_handle_(0), discard_frame_index_(0),
                                 discard_frame_enabled_(1), swr_init_(false), swr_ctx_(nullptr), audio_dev_(0),
                                 device_type_(AV_HWDEVICE_TYPE_NONE){
#ifdef _DEBUG
    showBanner();
    av_log_set_level(AV_LOG_INFO);
#else
    av_log_set_level(AV_LOG_FATAL);
#endif
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        LOG_ERROR << "Could not initialize SDL -" << SDL_GetError();
    }
}

FfmpegWrapper::~FfmpegWrapper() {
    if (!stop_request_) {
        stopPlay();
    }
    SDL_Quit();
}

int FfmpegWrapper::setCallback(void *user, uintptr_t handle, const FF_RAW_DATA_CALLBACK &pfn, const FF_EXCEPTION_CALLBACK& pfn2) {
    user_data_ = user;
    user_handle_ = handle;
    ff_send_data_callback_ = pfn;
    ff_exception_callback_ = pfn2;
    return 0;
}

int FfmpegWrapper::startPlay(const char *inputUrl, int width, int height,
                             int useGPU /*= 1*/, int useTCP /*= 1*/, int retryTimes/* = 3*/) {
    LOG_INFO << "[" << user_handle_ << "]startPlay url[" << inputUrl << "], GPU:" << useGPU;
    this->output_width_ = width;
    this->output_height_ = height;
    this->useGPU_ = useGPU;
    this->inputUrl_ = inputUrl;

    main_read_thread_handle_ = std::move(std::thread([this, useTCP, useGPU, retryTimes]() mutable {
        int ret = 0;
        int video_stream_index;
        int audio_stream_index;
        AVPacket *pkt = nullptr;
        do {
            if ((ret = open_input_url(inputUrl_.c_str(), useTCP, retryTimes)) != 0) {
                break;
            }

            /* retrieve stream information */
            if ((ret = avformat_find_stream_info(fmt_ctx_, NULL)) < 0) {
                LOG_ERROR << "Could not find stream information:" << av_err2str(ret);
                break;
            }

            if (open_codec_context(&video_stream_index, &video_dec_ctx_, fmt_ctx_, AVMEDIA_TYPE_VIDEO) >= 0) {
                video_stream_ = fmt_ctx_->streams[video_stream_index];
                if (0 != checkVideoResolution(output_width_, output_height_)) {
                    output_width_ = video_dec_ctx_->width;
                    output_height_ = video_dec_ctx_->height;
                }
            }

            if (open_codec_context(&audio_stream_index, &audio_dec_ctx_, fmt_ctx_, AVMEDIA_TYPE_AUDIO) >= 0) {
                audio_stream_ = fmt_ctx_->streams[audio_stream_index];
                if (audio_dec_ctx_ && audio_open() < 0) {
                    LOG_WARN << "audio open failed. Maybe too many request.";
                }
            }

            /* dump input information to stderr */
            av_dump_format(fmt_ctx_, 0, inputUrl_.c_str(), 0);

            if (!audio_stream_ && !video_stream_) {
                LOG_ERROR << "Could not find audio or video stream in the input, aborting";
                ret = -1;
                break;
            }

            sw_frame_ = av_frame_alloc();
            if (!sw_frame_) {
                LOG_ERROR << "Could not allocate frame";
                ret = AVERROR(ENOMEM);
                break;
            }

            pkt = av_packet_alloc();
            if (!pkt) {
                LOG_ERROR << "Could not allocate packet";
                ret = AVERROR(ENOMEM);
                break;
            }
        } while (0);

        if (ret != 0 && ff_exception_callback_ && user_data_) {
            ff_exception_callback_(user_data_, user_handle_, ret, (uint8_t *) "open url failed.");
            return;
        }

        /* read frames from the input */
        while (!stop_request_) {
            if (video_packet_queue_.size() >= MAX_PACKET_VIDEO || 
                audio_packet_queue_.size() >= MAX_PACKET_AUDIO) {
                std::this_thread::sleep_for(chrono::milliseconds(10));
                continue;
            }
            ret = av_read_frame(fmt_ctx_, pkt);
            if (ret < 0 || !pkt) {
                if (fmt_ctx_->pb && fmt_ctx_->pb->error)
                    break;

                std::this_thread::sleep_for(chrono::milliseconds(10));
                continue;
            }
            preTime_ = time(nullptr);

            // check if the packet belongs to a stream we are interested in, otherwise
            // skip it
            if (pkt->stream_index == video_stream_index)
                video_packet_queue_.put(pkt);
            else if (pkt->stream_index == audio_stream_index) {
                audio_packet_queue_.put(pkt);
            }
            av_packet_unref(pkt);
        }

        /* flush the decoders */
        if (video_dec_ctx_)
            video_packet_queue_.put(pkt);
        if (audio_dec_ctx_)
            audio_packet_queue_.put(pkt);

        if (!stop_request_ && ff_exception_callback_ && user_data_) {
            ff_exception_callback_(user_data_, user_handle_, ret, (const uint8_t*)av_err2str(ret));
        }
        av_packet_free(&pkt);
    }));
    audio_decode_thread_handle_ = std::thread(&FfmpegWrapper::audio_decode_thread, this);
    video_decode_thread_handle_ = std::thread(&FfmpegWrapper::video_decode_thread, this);

    return 0;
}

int FfmpegWrapper::stopPlay() {
    LOG_INFO << "[" << user_handle_ << "]stopPlay";
    stop_request_ = 1;
    video_packet_queue_.stop();
    audio_packet_queue_.stop();
    if (audio_decode_thread_handle_.joinable()) {
        audio_decode_thread_handle_.join();
    }
    if (video_decode_thread_handle_.joinable()) {
        video_decode_thread_handle_.join();
    }
    if (main_read_thread_handle_.joinable()) {
        main_read_thread_handle_.join();
    }

    sws_freeContext(sws_ctx_);
    swr_free(&swr_ctx_);

    avcodec_free_context(&video_dec_ctx_);
    avcodec_free_context(&audio_dec_ctx_);
    avformat_close_input(&fmt_ctx_);
    av_buffer_unref(&hw_device_ctx_);

    av_frame_free(&sw_frame_);
    av_frame_free(&frameYUV_);
    av_freep(&video_dst_data_);
    av_freep(&audio_dst_data_);

    SDL_CloseAudioDevice(audio_dev_);
    return 0;
}

int FfmpegWrapper::changeVideoResolution(int width, int height) {
    if (width == 0 || height == 0) {
        output_width_ = video_dec_ctx_->width;
        output_height_ = video_dec_ctx_->height;
    } else {
        output_width_ = width;
        output_height_ = height;
    }

    sws_init_ = false;

    return 0;
}

int FfmpegWrapper::openDiscardFrames(int enabled) {
    discard_frame_enabled_ = enabled;
    return 0;
}

int FfmpegWrapper::retrieve_frame(AVFrame* in, AVFrame** out) {
    int ret = 0;
    if (in->format != hw_pix_fmt_) {
        *out = in;
        return ret;
    }

    /* retrieve data from GPU to CPU */
    if ((ret = av_hwframe_transfer_data(sw_frame_, in, 0)) < 0) {
        LOG_ERROR << "Error transferring the data to system memory(" << av_err2str(ret);
        return 0;
    }
    *out = sw_frame_;
    return ret;
}

int FfmpegWrapper::scale_frame(AVFrame* in, AVFrame** out) {
    int ret = 0;
    if (in->width == output_width_ && in->height == output_height_ &&
        in->format == TARGET_PIX_FMT) {
        *out = in;
        return ret;
    }

    if (!sws_init_) {
        if (sws_ctx_) {
            sws_freeContext(sws_ctx_);
            sws_ctx_ = nullptr;
        }
        av_frame_free(&frameYUV_);
        av_freep(&video_dst_data_);
    }
    if (!sws_ctx_) {
        // 如果明确是要缩小并显示，建议使用SWS_POINT算法
        // 在不明确是放大还是缩小时，直接使用 SWS_FAST_BILINEAR 算法即可。
        sws_ctx_ = sws_getContext(in->width, in->height,
            (AVPixelFormat)in->format,
            output_width_, output_height_,
            TARGET_PIX_FMT,
            SWS_POINT, NULL, NULL, NULL);
    }

    if (!frameYUV_) {
        frameYUV_ = av_frame_alloc();
        if (!frameYUV_) {
            LOG_ERROR << "Could not allocate frame";
            ret = AVERROR(ENOMEM);
            return ret;
        }

        frameYUV_->format = TARGET_PIX_FMT;
        frameYUV_->width = output_width_;
        frameYUV_->height = output_height_;

        ret = av_frame_get_buffer(frameYUV_, 0);
        if (ret < 0) {
            LOG_ERROR << "Could not av_frame_get_buffer";
            return ret;
        }
        sws_init_ = true;
    }

    ret = sws_scale(sws_ctx_, (const uint8_t* const*)in->data, in->linesize,
        0, in->height, frameYUV_->data, frameYUV_->linesize);
    if (ret != frameYUV_->height) {
        LOG_ERROR << "Could not sws_scale frame";
        return ret;
    }
    *out = frameYUV_;
    return 0;
}

int FfmpegWrapper::output_video_frame(AVFrame *frame) {
    int ret = 0;
    AVFrame *tmp_frame = nullptr;
    AVFrame *tmp_frame2 = nullptr;

    // 抽帧
    if (discard_frame_enabled_ && (++discard_frame_index_ % DISCARD_FRAME_FREQUENCY == 0)) {
        return 0;
    }

    if ((ret = retrieve_frame(frame, &tmp_frame)) < 0) {
        return ret;
    }

    if ((ret = scale_frame(tmp_frame, &tmp_frame2)) < 0) {
        return ret;
    }

    int size = av_image_get_buffer_size(TARGET_PIX_FMT, output_width_, output_height_, 1);
    if (!video_dst_data_) {
        video_dst_data_ = (uint8_t *) av_malloc(size + HPP_HEADER_SIZE);
        if (!video_dst_data_) {
            LOG_ERROR << "Can not alloc buffer";
            return AVERROR(ENOMEM);
        }
    }

    if (frame->pts == AV_NOPTS_VALUE) {
        frame->pts = 0;
    } else if (frame->pts < 0) {
        return 0;
    }
    current_pts_video_in_ms_ = av_q2d(video_stream_->time_base) * frame->pts * 1000;

    video_dst_data_[0] = (uint8_t)(output_width_ >> 8);
    video_dst_data_[1] = (uint8_t)(output_width_);
    video_dst_data_[2] = (uint8_t)(output_height_ >> 8);
    video_dst_data_[3] = (uint8_t)(output_height_);
    video_dst_data_[4] = (uint8_t)(current_pts_video_in_ms_ >> 24);
    video_dst_data_[5] = (uint8_t)(current_pts_video_in_ms_ >> 16);
    video_dst_data_[6] = (uint8_t)(current_pts_video_in_ms_ >> 8);
    video_dst_data_[7] = (uint8_t)(current_pts_video_in_ms_);

    ret = av_image_copy_to_buffer(video_dst_data_ + HPP_HEADER_SIZE, size,
                                  (const uint8_t *const *) tmp_frame2->data,
                                  (const int *) tmp_frame2->linesize, TARGET_PIX_FMT,
                                  output_width_, output_height_, 1);
    if (ret < 0) {
        LOG_ERROR << "Can not copy image to buffer: " << av_err2str(ret);
        return ret;
    }

    /* write to rawvideo file */
    if (0) {
        FILE *fp = nullptr;
        char file[128] = { 0 };
        sprintf(file, "%s_%d_%d.yuv", av_get_pix_fmt_name(TARGET_PIX_FMT), output_width_, output_height_);
        fopen_s(&fp, file, "ab");
        if (fp) {
            fwrite(video_dst_data_ + HPP_HEADER_SIZE, size, 1, fp);
            fclose(fp);
        }
    }

    if (ff_send_data_callback_ && user_data_) {
        if ((ret = ff_send_data_callback_(user_data_, user_handle_,
                                          video_dst_data_, size + HPP_HEADER_SIZE)) != 0) {
//             if (_ff_exception_callback) {
//                 _ff_exception_callback(_user_data, _user_handle, ret, (uint8_t*)GetErrorInfo(ret));
//             }
        }
    }

    return 0;
}

int FfmpegWrapper::output_audio_frame(AVFrame *frame) {
    int ret = 0;
    if (!swr_init_ &&
        // SDL_QueueAudio方式不支持单声道和planar
        (av_get_default_channel_layout(frame->channels) == AV_CH_LAYOUT_MONO ||
         av_sample_fmt_is_planar(audio_dec_ctx_->sample_fmt))) {
        // init swresample
        swr_free(&swr_ctx_);
        swr_ctx_ = swr_alloc_set_opts(NULL,
            av_get_default_channel_layout(AUDIO_CHANNELS_DEFAULT), AV_SAMPLE_FMT_S16, frame->sample_rate,
            av_get_default_channel_layout(frame->channels), (AVSampleFormat)(frame->format), frame->sample_rate,
            0, NULL);
        if (!swr_ctx_ || swr_init(swr_ctx_) < 0) {
            swr_free(&swr_ctx_);
            return -1;
        }
        swr_init_ = true;
    }

    size_t audio_data_size = av_samples_get_buffer_size(NULL, AUDIO_CHANNELS_DEFAULT, 
        frame->nb_samples, AV_SAMPLE_FMT_S16, 0);

    if (!audio_dst_data_) {
        audio_dst_data_ = (uint8_t*)av_malloc(audio_data_size);
    }

    if (!audio_dst_data_) {
        LOG_ERROR << "Can not alloc buffer";
        return AVERROR(ENOMEM);
    }
    if (swr_ctx_) {
        const uint8_t** in = (const uint8_t**)frame->extended_data;
        if ((ret = swr_convert(swr_ctx_, &audio_dst_data_, frame->nb_samples, in, frame->nb_samples)) < 0) {
            LOG_ERROR << "swr_convert failed:" << av_err2str(ret);
            return ret;
        }
    }
    else {
        memcpy_s(audio_dst_data_, audio_data_size, frame->extended_data[0], audio_data_size);
    }

    if (0) {
        FILE* fp = nullptr;
        fopen_s(&fp, "audio.pcm", "ab");
        if (fp) {
            fwrite(audio_dst_data_ + HPP_HEADER_SIZE, 1, audio_data_size, fp);
            fclose(fp);
        }
    }

    if (audio_dev_ >= 2) {
        SDL_QueueAudio(audio_dev_, audio_dst_data_, audio_data_size);
        double delay = audio_data_size * 1000.0 / (frame->sample_rate * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) * 2) - 1;

        //int size = SDL_GetQueuedAudioSize(audio_dev_);
        //LOG_WARN << "queue size: " << size;
        //TODO:
        SDL_Delay(10);
    }

    return ret;
}

int FfmpegWrapper::decode_packet(AVCodecContext *dec, const AVPacket *pkt, AVFrame *frame) {
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0 && ret != AVERROR_EOF) {
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;

            return ret;
        }

        ret = (dec->codec->type == AVMEDIA_TYPE_VIDEO) 
            ? output_video_frame(frame) 
            : output_audio_frame(frame);

        av_frame_unref(frame);
        if (ret < 0) return ret;
    }

    return 0;
}

int FfmpegWrapper::open_codec_context(int *stream_idx,
                                      AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type) {
    int ret = 0;
    int stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        LOG_ERROR << "Could not find " << av_get_media_type_string(type)
                  << " stream in input " << inputUrl_;
        return ret;
    }

    stream_index = ret;
    st = fmt_ctx->streams[stream_index];

    /* find decoder for the stream */
    dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) {
        LOG_ERROR << "Failed to find " << av_get_media_type_string(type) << " codec";
        return AVERROR(EINVAL);
    }

    /* Allocate a codec context for the decoder */
    *dec_ctx = avcodec_alloc_context3(dec);
    if (!*dec_ctx) {
        LOG_ERROR << "Failed to allocate the " << av_get_media_type_string(type) << " codec context";
        return AVERROR(ENOMEM);
    }

    /* Copy codec parameters from input stream to output codec context */
    if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
        LOG_ERROR << "Failed to copy " << av_get_media_type_string(type)
            << " codec parameters to decoder context";
        return ret;
    }

    if (useGPU_ && type == AVMEDIA_TYPE_VIDEO && (ret = hw_decoder_open(dec, *dec_ctx)) < 0) {
        LOG_WARN << "Failed to open hw decoder.";
    }

    /* Init the decoders */
    if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
        LOG_ERROR << "Failed to open " << av_get_media_type_string(type) << " codec";
        return ret;
    }
    *stream_idx = stream_index;

    return 0;
}

int FfmpegWrapper::open_input_url(const char *inputUrl, int useTCP, int retryTimes) {
    int ret = 0;
    AVDictionary* format_options = nullptr;
    do {
        av_dict_set(&format_options, "rtsp_transport", useTCP ? "tcp" : "udp", 0);
        if (!(fmt_ctx_ = avformat_alloc_context())) {
            LOG_ERROR << "avformat_alloc_context error";
            ret = -1;
            break;
        }
        fmt_ctx_->interrupt_callback.callback = input_interrupt_cb;
        fmt_ctx_->interrupt_callback.opaque = &preTime_;
        preTime_ = time(nullptr);
        if ((ret = avformat_open_input(&fmt_ctx_, inputUrl, NULL, &format_options)) == 0) {
            break;
        }
        // try another mode
        useTCP = useTCP ^ 1;
        if (--retryTimes <= 0) {
            LOG_ERROR << "Could not open source file " << inputUrl << "(" << ret << ")" << av_err2str(ret);
            ret = FFOpenUrlFailed;
            break;
        }
        av_usleep(10000);
    } while (!stop_request_);
    av_dict_free(&format_options);

    return ret;
}

int FfmpegWrapper::hw_decoder_init(AVCodecContext *ctx) {
    int ret = 0;

    if ((ret = av_hwdevice_ctx_create(&hw_device_ctx_, device_type_, NULL, NULL, 0)) < 0) {
        LOG_ERROR << "Failed to create specified HW device.";
        return ret;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx_);

    return ret;
}

int FfmpegWrapper::hw_decoder_open(const AVCodec* dec, AVCodecContext* ctx)
{
    if (hw_get_config(dec, AV_HWDEVICE_TYPE_D3D11VA) < 0 &&
        hw_get_config(dec, AV_HWDEVICE_TYPE_DXVA2) < 0) {
        return -1;
    }

    ctx->get_format = hw_get_format;
    if (hw_decoder_init(ctx) < 0) {
        return -1;
    }

    return 0;
}

int FfmpegWrapper::audio_open()
{
    SDL_AudioSpec wantSpec, spec;
    wantSpec.freq = audio_dec_ctx_->sample_rate;
    wantSpec.format = AUDIO_S16SYS;
    wantSpec.channels = AUDIO_CHANNELS_DEFAULT;
    wantSpec.silence = 0;
    wantSpec.samples = FFMAX(512, 2 << av_log2(wantSpec.freq / 30));
    wantSpec.callback = NULL;

    if ((audio_dev_ = SDL_OpenAudioDevice(NULL, 0, &wantSpec, &spec, SDL_AUDIO_ALLOW_CHANNELS_CHANGE)) < 2) {
        LOG_ERROR << "can not open SDL!";
        return -1;
    }

    SDL_PauseAudioDevice(audio_dev_, 0);
    return 0;
}

int FfmpegWrapper::hw_get_config(const AVCodec *decoder, AVHWDeviceType type) {
    for (int i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            LOG_ERROR << "Decoder " << decoder->name << " does not support device type "
                << av_hwdevice_get_type_name(type);
            return -1;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX 
            && type == config->device_type) {
            device_type_ = config->device_type;
            hw_pix_fmt_ = config->pix_fmt;
            break;
        }
    }
    return 0;
}

enum AVPixelFormat FfmpegWrapper::hw_get_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    LOG_INFO << "trying format: " << av_get_pix_fmt_name(hw_pix_fmt_);
    const enum AVPixelFormat* p;
    for (p = pix_fmts; *p != -1; p++) {
        LOG_INFO << "available hardware decoder output format: " << av_get_pix_fmt_name(*p);
        if (*p == hw_pix_fmt_)
            return *p;
    }

    LOG_ERROR << "Failed to get " << av_get_pix_fmt_name(hw_pix_fmt_) << " format.";
    return AV_PIX_FMT_NONE;
}

void FfmpegWrapper::audio_decode_thread() {
    int ret = 0;
    try {
        AVPacketPtr pkt(av_packet_alloc(), [](AVPacket* p) {av_packet_free(&p);});
        AVFramePtr frame(av_frame_alloc(), [](AVFrame* f) {av_frame_free(&f);});
        do {
            if (stop_request_)
                break;
            if ((ret = audio_packet_queue_.get(pkt.get())) < 0)
                break;

            ret = decode_packet(audio_dec_ctx_, pkt.get(), frame.get());
            av_packet_unref(pkt.get());
            this_thread::sleep_for(chrono::milliseconds(1));
        } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

        LOG_INFO << "audio_decode_thread exit with " << av_err2str(ret);
        // stop_request_ 为1时不需要回调
        if (!stop_request_ && ff_exception_callback_ && user_data_) {
            ff_exception_callback_(user_data_, user_handle_, ret, (uint8_t *)av_err2str(ret));
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR << "audio thread exception(" << e.what() << ")";
    }
}

void FfmpegWrapper::video_decode_thread() {
    int ret = 0;
    try {
        AVPacketPtr pkt(av_packet_alloc(), [](AVPacket* p) {av_packet_free(&p); });
        AVFramePtr frame(av_frame_alloc(), [](AVFrame* f) {av_frame_free(&f); });
        std::chrono::steady_clock::time_point tp = std::chrono::steady_clock::now();
        do {
            if (stop_request_)
                break;
            if ((ret = video_packet_queue_.get(pkt.get())) < 0)
                break;

            ret = decode_packet(video_dec_ctx_, pkt.get(), frame.get());
            av_packet_unref(pkt.get());

            if (true) {
                int duration = 1000000 / av_q2d(video_stream_->avg_frame_rate);
                tp += std::chrono::microseconds(duration);
                std::this_thread::sleep_until(tp);
            }
        } while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

        LOG_INFO << "video_decode_thread exit with " << av_err2str(ret);
        // stop_request_ 为1时不需要回调
        if (!stop_request_ && ff_exception_callback_ && user_data_) {
            ff_exception_callback_(user_data_, user_handle_, ret, (uint8_t *)av_err2str(ret));
        }
    }
    catch (const std::exception &e) {
        LOG_ERROR << "video thread exception(" << e.what() << ")";
    }
}

int FfmpegWrapper::input_interrupt_cb(void *ctx) {
    time_t preTime = *((time_t *) ctx);
    time_t nowTime = time(nullptr);
    if (nowTime - preTime > 10) {
        LOG_ERROR << "input_interrupt_cb timeOut";
        return true;
    }
    return false;
}
