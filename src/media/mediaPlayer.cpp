#include "include/player.h"
#include "include/log.h"

#include <common/gl_common.h>
#include <Program/shader.h>


/// @brief 顶点及纹理坐标
const float vertices[] = {
    // 顶点坐标          // 纹理坐标
    -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
    1.0f, -1.0f, 0.0f, 1.0f, 0.0f};

const int indices[] = {
    0, 1, 2,
    2, 3, 1};

// 自定义智能指针释放器实现
void FFmpegDeleter::operator()(AVFormatContext *ctx)
{
    if (ctx)
        avformat_close_input(&ctx);
}
void FFmpegDeleter::operator()(AVCodecContext *ctx)
{
    if (ctx)
        avcodec_free_context(&ctx);
}
void FFmpegDeleter::operator()(AVFrame *frame)
{
    if (frame)
        av_frame_free(&frame);
}
void FFmpegDeleter::operator()(SwsContext *ctx)
{
    if (ctx)
        sws_freeContext(ctx);
}
void FFmpegDeleter::operator()(SwrContext *ctx)
{
    if (ctx)
        swr_free(&ctx);
}

void FFmpegDeleter::operator()(GLFWwindow *window)
{
    if (window)
        glfwDestroyWindow(window);
}

MediaPlayer::MediaPlayer(const std::string &filename, int videoWidth = 800, int videoHeight = 600) : filename_(filename), audio_data_(10), videoWidth(videoWidth), videoHeight(videoHeight)
{
    avformat_network_init();
    this->Init();
}

MediaPlayer::~MediaPlayer()
{
    Stop();

    SDL_Quit();
}

bool MediaPlayer::Init()
{
    if (!OpenFile() || !InitVideo() || !InitAudio() || !InitSDL())
        return false;
    return true;
}

void MediaPlayer::Stop()
{
    std::clog << "stop" << std::endl;
    quit_ = true;
    if (audio_dev_)
        SDL_CloseAudioDevice(audio_dev_);
    if (texture_)
        SDL_DestroyTexture(texture_);
    if (renderer_)
        SDL_DestroyRenderer(renderer_);
    if (window_)
        SDL_DestroyWindow(window_);
}

void MediaPlayer::Play()
{
    SDL_PauseAudioDevice(audio_dev_, 0);
    std::thread([this]()
                { DecodeLoop(); })
        .detach();
    VideoLoop();
}

bool MediaPlayer::OpenFile()
{
    AVFormatContext *fmt_ctx = nullptr;
    if (int openRes = avformat_open_input(&fmt_ctx, filename_.c_str(), nullptr, nullptr) != 0)
    {

        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(openRes, errbuf, sizeof(errbuf));
        std::cerr << "无法打开文件: " << filename_ << "错误代码" << errbuf << std::endl;
        return false;
    }
    fmt_ctx_.reset(fmt_ctx);

    if (avformat_find_stream_info(fmt_ctx_.get(), nullptr) < 0)
    {
        std::cerr << "无法获取流信息" << std::endl;
        return false;
    }

    video_stream_idx_ = av_find_best_stream(fmt_ctx_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audio_stream_idx_ = av_find_best_stream(fmt_ctx_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    return (video_stream_idx_ >= 0 || audio_stream_idx_ >= 0);
}

bool MediaPlayer::InitVideo()
{
    if (video_stream_idx_ < 0)
        return true;

    AVStream *stream = fmt_ctx_->streams[video_stream_idx_];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
    {
        std::cerr << "未找到视频解码器" << std::endl;
        return false;
    }

    video_codec_ctx_.reset(avcodec_alloc_context3(codec));
    if (avcodec_parameters_to_context(video_codec_ctx_.get(), stream->codecpar) < 0)
    {
        std::cerr << "无法初始化视频解码器上下文" << std::endl;
        return false;
    }

    if (avcodec_open2(video_codec_ctx_.get(), codec, nullptr) < 0)
    {
        std::cerr << "无法打开视频解码器" << std::endl;
        return false;
    }

    frame_.reset(av_frame_alloc());
    sws_ctx_.reset(sws_getContext(
        video_codec_ctx_->width, video_codec_ctx_->height, video_codec_ctx_->pix_fmt,
        video_codec_ctx_->width, video_codec_ctx_->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr));

    return true;
}

bool MediaPlayer::InitAudio()
{
    if (audio_stream_idx_ < 0)
        return true;

    AVStream *stream = fmt_ctx_->streams[audio_stream_idx_];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec)
    {
        std::cerr << "未找到音频解码器" << std::endl;
        return false;
    }

    audio_codec_ctx_.reset(avcodec_alloc_context3(codec));
    if (avcodec_parameters_to_context(audio_codec_ctx_.get(), stream->codecpar) < 0)
    {
        std::cerr << "无法初始化音频解码器上下文" << std::endl;
        return false;
    }

    if (avcodec_open2(audio_codec_ctx_.get(), codec, nullptr) < 0)
    {
        std::cerr << "无法打开音频解码器" << std::endl;
        return false;
    }

    // FFmpeg 7.1 使用 AVChannelLayout
    swr_ctx_.reset(swr_alloc());
    av_opt_set_chlayout(swr_ctx_.get(), "in_chlayout", &audio_codec_ctx_->ch_layout, 0);
    av_opt_set_chlayout(swr_ctx_.get(), "out_chlayout", &audio_codec_ctx_->ch_layout, 0);
    av_opt_set(swr_ctx_.get(), "in_sample_rate", std::to_string(audio_codec_ctx_->sample_rate).c_str(), 0);
    av_opt_set(swr_ctx_.get(), "out_sample_rate", std::to_string(audio_codec_ctx_->sample_rate).c_str(), 0);
    av_opt_set_sample_fmt(swr_ctx_.get(), "in_sample_fmt", audio_codec_ctx_->sample_fmt, 0);
    av_opt_set_sample_fmt(swr_ctx_.get(), "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(swr_ctx_.get()) < 0)
    {
        std::cerr << "无法初始化音频重采样器" << std::endl;
        return false;
    }

    return true;
}

bool MediaPlayer::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
    {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }

    if (video_stream_idx_ >= 0)
    {
        window_ = SDL_CreateWindow("FFmpeg 7.1 Player",
                                   SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                   video_codec_ctx_->width, video_codec_ctx_->height, 0);
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YV12,
                                     SDL_TEXTUREACCESS_STREAMING, video_codec_ctx_->width, video_codec_ctx_->height);
    }

    if (audio_stream_idx_ >= 0)
    {
        SDL_AudioSpec wanted, obtained;
        wanted.freq = audio_codec_ctx_->sample_rate;
        wanted.format = AUDIO_S16SYS;
        wanted.channels = audio_codec_ctx_->ch_layout.nb_channels;
        wanted.samples = 1024;
        wanted.callback = [](void *userdata, Uint8 *stream, int len)
        {
            static_cast<MediaPlayer *>(userdata)->AudioCallback(stream, len);
        };
        wanted.userdata = this;

        audio_dev_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
        if (audio_dev_ == 0)
        {
            std::cerr << "无法打开音频设备: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    return true;
}

void MediaPlayer::DecodeLoop()
{
    AVPacket pkt;
    int readRes = -1;
    while (!quit_ && (readRes =  av_read_frame(fmt_ctx_.get(), &pkt)) >= 0)
    {
        if (pkt.stream_index == video_stream_idx_)
        {
            ProcessVideoPacket(&pkt);
        }
        else if (pkt.stream_index == audio_stream_idx_)
        {
            ProcessAudioPacket(&pkt);
        }
        av_packet_unref(&pkt);
    }
   
    // quit_ = true;
    if(readRes < 0){
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(readRes, errbuf, sizeof(errbuf));
        std::cerr << "无法读取帧: " << errbuf << std::endl;
    }
    
}

void MediaPlayer::ProcessVideoPacket(AVPacket *pkt)
{
    if (avcodec_send_packet(video_codec_ctx_.get(), pkt) != 0)
        return;
    AVFrame *frame = av_frame_alloc();
    while (avcodec_receive_frame(video_codec_ctx_.get(), frame) == 0)
    {
        std::lock_guard<std::mutex> lock(video_mutex_);
        video_frames_.push(frame);
    }
}

void MediaPlayer::ProcessAudioPacket(AVPacket *pkt)
{
    if (avcodec_send_packet(audio_codec_ctx_.get(), pkt) != 0)
        return;
    AVFrame *frame = av_frame_alloc();
    while (avcodec_receive_frame(audio_codec_ctx_.get(), frame) == 0)
    {
        uint8_t *output;
        int out_samples = swr_get_out_samples(swr_ctx_.get(), frame->nb_samples);
        av_samples_alloc(&output, nullptr, audio_codec_ctx_->ch_layout.nb_channels,
                         out_samples, AV_SAMPLE_FMT_S16, 0);
        out_samples = swr_convert(swr_ctx_.get(), &output, out_samples,
                                  (const uint8_t **)frame->data, frame->nb_samples);

        std::pair<uint8_t *, size_t> data(output, out_samples * audio_codec_ctx_->ch_layout.nb_channels * 2);
        audio_data_.push(std::move(data));
    }
    av_frame_free(&frame);
}

void MediaPlayer::VideoLoop()
{
    while (!quit_)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            
            if (event.type == SDL_QUIT){
                quit_ = true;
                std::clog << "event type quit" << std::endl;
            }
        }

        if (!video_frames_.empty())
        {
            std::lock_guard<std::mutex> lock(video_mutex_);
            AVFrame *frame = video_frames_.front();
            video_frames_.pop();

            SDL_UpdateYUVTexture(texture_, nullptr,
                                 frame->data[0], frame->linesize[0],
                                 frame->data[1], frame->linesize[1],
                                 frame->data[2], frame->linesize[2]);
            SDL_RenderClear(renderer_);
            SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
            SDL_RenderPresent(renderer_);

            av_frame_free(&frame);
        }
        SDL_Delay(10);
    }
    std::clog << "video loop quit!"  << std::endl;
}

void MediaPlayer::AudioCallback(Uint8 *stream, int len)
{
    // if (audio_data_.empty())
    // {
    //     memset(stream, 0, len);
    //     return;
    // }

     auto &data = audio_data_.pop();
    int copy_size = std::min(len, static_cast<int>(data.second - audio_pos_));
    memcpy(stream, data.first + audio_pos_, copy_size);
    audio_pos_ += copy_size;

    if (audio_pos_ >= data.second)
    {
        av_freep(&data.first);
        audio_pos_ = 0;
    }
}