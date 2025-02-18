#include "include/player.h"
#include "include/log.h"

#include <common/gl_common.h>
#include <Program/shader.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <bgfx/bgfx.h>
#include <config.h>
extern "C"
{
#include <libavutil/imgutils.h>
}

// 同步阈值，在这个范围内默认同步
// 阈值为24fps的一帧时间
const double SYNC_THRESHOLD = 0.04;

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

MediaPlayer::MediaPlayer(const std::string &filename, int videoWidth = 800, int videoHeight = 600) : filename_(filename), audio_data_(10), video_frames_(10), videoWidth(videoWidth), videoHeight(videoHeight)
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
    if (!OpenFile() || !InitGL() || !InitVideo() || !InitAudio() || !InitSDL())
        return false;
    return true;
}

void MediaPlayer::Stop()
{
    std::clog << "stop" << std::endl;
    quit_ = true;
    if (sharder_)
    {
        delete sharder_;
    }
    if (audio_dev_)
        SDL_CloseAudioDevice(audio_dev_);
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
    // 创建SwsContext
    // SWS_BILINEAR双线性插值算法，平滑过滤
    sws_ctx_.reset(sws_getContext(video_codec_ctx_->width, video_codec_ctx_->height, video_codec_ctx_->pix_fmt,
                                  video_codec_ctx_->width, video_codec_ctx_->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr));
    sharder_->use();
    // 创建YUV420纹理
    glGenTextures(3, textures);
    // Y
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    sharder_->setIntP("textureY", 0);
     // 设置环绕方式
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);     // x轴
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);     // y轴
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, video_codec_ctx_->width, video_codec_ctx_->height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    
   
    // U
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    sharder_->setIntP("textureU", 1);
    // 设置环绕方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);     // x轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);     // y轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, video_codec_ctx_->width / 2, video_codec_ctx_->height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    
    

    // V
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textures[2]);
    sharder_->setIntP("textureV", 2);
    // 设置环绕方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);     // x轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);     // y轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, video_codec_ctx_->width / 2, video_codec_ctx_->height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
   
    

    // y轴翻转
    glm::mat4 revert = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
    if (videoWidth > videoHeight)
    {
        // 如果宽度大于高度， 则说明是横屏，我们铺满宽度，高度等比缩放
        // 首先先还原被拉伸之前的比例
        double scale = (double)video_codec_ctx_->height / videoHeight;
        double wScale = videoWidth / (double)video_codec_ctx_->width;
        scale = scale * wScale;
        // 然后根据原有长宽比再次进行作坊
        revert = glm::scale(revert, glm::vec3(1.0f, scale, 1.0f));
    }
    else
    {
        double scale = (double)video_codec_ctx_->width / videoWidth;
        double hScale = videoHeight / (double)video_codec_ctx_->height;
        scale = scale * hScale;
        revert = glm::scale(revert, glm::vec3(scale, 1.0f, 1.0f));
    }
    std::string revertName = "revert";
    sharder_->setMat4(revertName, revert);
    sharder_->setBoolP("useTexture", true);
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

bool MediaPlayer::InitGL()
{
    bgfx::Init init;  
    #ifdef MACOS
    std::clog << "MACOS" << std::endl;
    init.type = bgfx::RendererType::Metal;
    #endif  
    auto window = initGlEnv(videoWidth, videoHeight, "DDYPlayer");
    if (!window)
    {
        return false;
    }
    window_.reset(window);
    sharder_ = new Shader("shaders/media/media.vert", "shaders/media/media.frag");
    sharder_->use();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glfwSwapInterval(1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    return true;
}

bool MediaPlayer::InitSDL()
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
    {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return false;
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
    while (!quit_ && (readRes = av_read_frame(fmt_ctx_.get(), &pkt)) >= 0)
    {

        if (pkt.stream_index == audio_stream_idx_)
        {
            ProcessAudioPacket(&pkt);
        }else  if (pkt.stream_index == video_stream_idx_)
        {
            ProcessVideoPacket(&pkt);
        }
        av_packet_unref(&pkt);
    }

    // quit_ = true;
    if (readRes < 0)
    {
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
        std::clog << "receive frame, pts" << frame->pts << std::endl;
        double now = glfwGetTime();
        if (playState_)
        {
            double pts = frame->pts * av_q2d(time_base_);
            double lasTime = playState_->clk->time;
            double playTime = lasTime + pts;
            double diff = playTime - now;
            // 如果错过帧播放时机，直接丢弃
            if (diff < 0)
            {
                continue;
            }
        }

        AVFrame *pFrameYUV = av_frame_alloc();
        pFrameYUV->format = AV_PIX_FMT_YUV420P;
        uint8_t *out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, video_codec_ctx_.get()->width, video_codec_ctx_.get()->height, 1));
        av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer, AV_PIX_FMT_YUV420P, video_codec_ctx_.get()->width, video_codec_ctx_.get()->height, 1);
        // 释放out_buffer
        av_free(out_buffer);

        sws_scale(sws_ctx_.get(), (const uint8_t *const *)frame->data, frame->linesize, 0, video_codec_ctx_.get()->height, pFrameYUV->data, pFrameYUV->linesize);
        PlayState *playState = new PlayState(pFrameYUV, new Clock(frame->pts * av_q2d(time_base_), now));
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

        std::pair<uint8_t *, size_t> data(output, out_samples * audio_codec_ctx_->ch_layout.nb_channels * 2);
        audio_data_.push(std::move(data));
    }
    av_frame_free(&frame);
}

void MediaPlayer::VideoLoop()
{
    while (!quit_ && glfwWindowShouldClose(window_.get()) == 0)
    {
        auto playState = video_frames_.pop();
        // 渲染
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        sharder_->use();
        // 更新纹理
        // Y
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, video_codec_ctx_->width, video_codec_ctx_->height, GL_RED, GL_UNSIGNED_BYTE, playState->frame->data[0]);
        int error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "update texture Y error" << error << std::endl;
        }
        // U
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, video_codec_ctx_->width / 2, video_codec_ctx_->height / 2, GL_RED, GL_UNSIGNED_BYTE, playState->frame->data[1]);
         error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "update texture U error" << error << std::endl;
        }
        // V
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textures[2]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, video_codec_ctx_->width / 2, video_codec_ctx_->height / 2, GL_RED, GL_UNSIGNED_BYTE, playState->frame->data[2]);
        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            std::cout << "update texture V error" << error << std::endl;
        }
        glBindVertexArray(vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        // glBindVertexArray(0);
        // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glfwSwapBuffers(window_.get());
        glfwPollEvents();
    }
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