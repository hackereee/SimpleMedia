#include <player.h>
#include "include/log.h"
#include <SDL2/SDL.h>

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
}

// 同步阈值，在这个范围内默认同步
// 阈值为24fps的一帧时间
const double SYNC_THRESHOLD = 0.04;

// 最大同步阈值
const double MAX_SYNC_THRESHOLD = 0.1;
// 最小同步阈值
const double MIN_SYNC_THRESHOLD = 0.04;


double nowTickets(){
    return SDL_GetTicks() / 1000.0;
}

PlayState::PlayState(AVFrame *frame, Clock *clk) : frame(frame), clk(clk) {}

PlayState::~PlayState()
{
    if (frame)
        av_frame_free(&frame);
    if (clk)
        delete clk;
}

Clock::Clock(double pts, double time) : pts(pts), time(time) {}

Clock::~Clock() {}

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

void FFmpegDeleter::operator()(SDL_Window *window)
{
    if (window)
        SDL_DestroyWindow(window);
}

void FFmpegDeleter::operator()(SDL_Renderer *renderer)
{
    if (renderer)
        SDL_DestroyRenderer(renderer);
}

void FFmpegDeleter::operator()(SDL_Texture *texture)
{
    if (texture)
        SDL_DestroyTexture(texture);
}

MediaPlayer::MediaPlayer(const std::string &filename, int videoWidth, int videoHeight) : filename_(filename), audio_data_(10), video_frames_(10),
video_packets_(32), videoWidth(videoWidth), videoHeight(videoHeight)
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
    if (!OpenFile() || !InitSDL() || !InitVideo() || !InitAudio())
        return false;
    initial_time_ = nowTickets();
    return true;
}

void MediaPlayer::Stop()
{
    std::clog << "stop" << std::endl;
    quit_ = true;

    if (audio_dev_)
        SDL_CloseAudioDevice(audio_dev_);
}

void MediaPlayer::Play()
{
    SDL_PauseAudioDevice(audio_dev_, 0);

    std::thread([this]()
                { DecodeLoop(); })
        .detach();
    std::thread([this]()
                { video_thread(); })
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
    std::clog << "video_stream_idx_: " << video_stream_idx_ << " audio_stream_idx_: " << audio_stream_idx_ << std::endl;
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
    time_base_ = std::move(stream->time_base);

    // 创建SwsContext用于格式转换
    sws_ctx_.reset(sws_getContext(video_codec_ctx_->width, video_codec_ctx_->height, video_codec_ctx_->pix_fmt,
                                  video_codec_ctx_->width, video_codec_ctx_->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr));

    // 创建SDL纹理
    texture_.reset(SDL_CreateTexture(
        renderer_.get(),
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        video_codec_ctx_->width,
        video_codec_ctx_->height));

    if (!texture_)
    {
        std::cerr << "无法创建SDL纹理: " << SDL_GetError() << std::endl;
        return false;
    }

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

    return true;
}

bool MediaPlayer::InitSDL()
{
    // 初始化SDL，支持视频、音频和事件
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0)
    {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }

    // 创建窗口
    SDL_Window *window = SDL_CreateWindow(
        "DDYPlayer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        videoWidth,
        videoHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        std::cerr << "无法创建窗口: " << SDL_GetError() << std::endl;
        return false;
    }
    window_.reset(window);

    // 创建渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window_.get(),
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer)
    {
        std::cerr << "无法创建渲染器: " << SDL_GetError() << std::endl;
        return false;
    }
    renderer_.reset(renderer);

    return true;
}

void MediaPlayer::DecodeLoop()
{
    AVPacket *pkt;
    int readRes = -1;
    while (!quit_ && (readRes = av_read_frame(fmt_ctx_.get(), pkt)) >= 0)
    {
        if (pkt->stream_index == audio_stream_idx_)
        {
            ProcessAudioPacket(pkt);
        }
        else if (pkt->stream_index == video_stream_idx_)
        {
            AVPacket *copy = av_packet_alloc();
            av_packet_ref(copy, pkt);
            video_packets_.push(copy);
        }
        av_packet_unref(pkt);
    }

    if (readRes == AVERROR_EOF)
    {
        std::clog << "readRes == AVERROR_EOF" << std::endl;
        pkt = av_packet_alloc();
        pkt->data = nullptr;
        pkt->size = 0;
        pkt->stream_index = video_stream_idx_;
        // 发送一个空包，通知解码器结束
        video_packets_.push(pkt);
    }
    else
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(readRes, errbuf, sizeof(errbuf));
        std::cerr << "无法读取帧: " << errbuf << std::endl;
    }
}

void MediaPlayer::video_thread()
{
    AVPacket *pkt;
    while (!quit_ && (pkt = video_packets_.pop()))
    {
        ProcessVideoPacket(pkt);
        av_packet_unref(pkt);
    }
}

void MediaPlayer::ProcessVideoPacket(AVPacket *pkt)
{

    if(pkt->size == 0){
        //空包，说明结束
        std::clog << "空包，说明结束" << std::endl;
        PlayState *playState = new PlayState(nullptr, new Clock(0, 0));
        playState->status = PlayStatus::STOPPED;
        video_frames_.push(playState);
        return;
    }

    if (avcodec_send_packet(video_codec_ctx_.get(), pkt) != 0)
        return;
    AVFrame *frame = av_frame_alloc();
    while (avcodec_receive_frame(video_codec_ctx_.get(), frame) == 0)
    {
        std::clog << "receive frame, pts " << frame->pts << std::endl;
        double now = nowTickets();
        if (playState_)
        {
            double pts = frame->pts * av_q2d(time_base_);
            double lasTime = playState_->clk->time;
            double playTime = lasTime + pts;
            double diff = playTime - now;
            // 如果错过帧播放时机，直接丢弃
            if (diff < 0)
            {
                av_frame_free(&frame);
                return;
            }
        }

        AVFrame *pFrameYUV = av_frame_alloc();
        pFrameYUV->format = AV_PIX_FMT_YUV420P;
        pFrameYUV->width = video_codec_ctx_->width;
        pFrameYUV->height = video_codec_ctx_->height;

        if (av_frame_get_buffer(pFrameYUV, 32) < 0)
        {
            std::cerr << "无法分配YUV帧缓冲区" << std::endl;
            av_frame_free(&pFrameYUV);
            av_frame_free(&frame);
            return;
        }

        sws_scale(sws_ctx_.get(),
                  (const uint8_t *const *)frame->data,
                  frame->linesize,
                  0,
                  video_codec_ctx_->height,
                  pFrameYUV->data,
                  pFrameYUV->linesize);
        auto pts = frame->pts * av_q2d(time_base_);
        PlayState *playState = new PlayState(pFrameYUV, new Clock(pts, initial_time_ + pts));
        playState_ = playState;
        video_frames_.push(playState);
    }
    av_frame_free(&frame);
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
        auto pts = frame->pts * av_q2d(time_base_);
        AudioData *data = new AudioData(output, out_samples * audio_codec_ctx_->ch_layout.nb_channels * 2, new Clock(pts, initial_time_ + pts));
        audio_data_.push(data);
    }
    av_frame_free(&frame);
}

void MediaPlayer::VideoLoop()
{
    SDL_Event event;
    bool running = true;

    while (!quit_ && running)
    {
        // 处理SDL事件
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
                break;
            }
        }

        if (!running)
            break;

        auto playState = video_frames_.pop();
        if (playState->status == PlayStatus::STOPPED)
        {
            break;
        }

        double delay = playState->clk->time - nowTickets();
        if(delay < 0 ){
            delay = 0;
        }

        if(current_audio_data_){
            //计算当前视频帧与音频帧的差值
            //视频时钟 - 音频时钟
            // 若结果 > 0, 视频快，等待音频
            // 若 < 0, 则要直接播放追赶音频
            // 这里还需注意视频本身也是有帧率的，所以需要考虑视频帧率
            double diff = playState->clk->time - current_audio_data_->clk->time;
            std::clog << "diff: " << diff << std::endl;
            //阈值范围在[MIN_SYNC_THRESHOLD, MAX_SYNC_THRESHOLD]之间
            int threshold = FFMAX(MIN_SYNC_THRESHOLD, FFMIN(MAX_SYNC_THRESHOLD, diff));
            if(diff < -threshold){
                delay = FFMAX(0, delay + diff);
            }else if(diff > threshold){
                delay = 2 * delay;
            }
        }
        std::clog << "delay time: " << delay << std::endl;
        SDL_Delay(delay * 1000);
        
        // 使用SDL更新纹理并渲染
        SDL_UpdateYUVTexture(
            texture_.get(),
            NULL,
            playState->frame->data[0], playState->frame->linesize[0],
            playState->frame->data[1], playState->frame->linesize[1],
            playState->frame->data[2], playState->frame->linesize[2]);

        // 清空屏幕
        SDL_SetRenderDrawColor(renderer_.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer_.get());

        // 计算视频显示区域，保持宽高比
        SDL_Rect rect;
        int window_width, window_height;
        SDL_GetWindowSize(window_.get(), &window_width, &window_height);

        float aspect_ratio = static_cast<float>(video_codec_ctx_->width) / video_codec_ctx_->height;
        float window_aspect_ratio = static_cast<float>(window_width) / window_height;

        if (aspect_ratio > window_aspect_ratio)
        {
            // 视频更宽，填充宽度
            rect.w = window_width;
            rect.h = static_cast<int>(window_width / aspect_ratio);
            rect.x = 0;
            rect.y = (window_height - rect.h) / 2;
        }
        else
        {
            // 视频更高，填充高度
            rect.h = window_height;
            rect.w = static_cast<int>(window_height * aspect_ratio);
            rect.x = (window_width - rect.w) / 2;
            rect.y = 0;
        }

        // 渲染视频帧
        SDL_RenderCopy(renderer_.get(), texture_.get(), NULL, &rect);
        SDL_RenderPresent(renderer_.get());
    }

    quit_ = true;
}

void MediaPlayer::AudioCallback(Uint8 *stream, int len)
{

    auto data = audio_data_.pop();
    
    auto lastData = current_audio_data_;
    current_audio_data_ = data;
    if(lastData){
        delete lastData;
    }
    int copy_size = std::min(len, static_cast<int>(data->size - audio_pos_));
    memcpy(stream, data->data + audio_pos_, copy_size);

    // 如果有剩余空间，用0填充
    if (copy_size < len)
    {
        memset(stream + copy_size, 0, len - copy_size);
    }

    audio_pos_ += copy_size;

    if (audio_pos_ >= data->size)
    {
        audio_pos_ = 0;
    }
}