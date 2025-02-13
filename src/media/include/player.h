#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H
#define GLFW_INCLUDE_NONE

#include <iostream>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <SDL2/SDL.h>
#include <toolkit/bufferq.h>
#include <GLFW/glfw3.h>
#include <Program/shader.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

class Clock
{
public:
    double pts;
    double time;
    Clock(double pts, double time);
    ~Clock();
};

class PlayState
{
public:
    AVFrame *frame;
    Clock *clk;
    PlayState(AVFrame *frame, Clock *clk);
    ~PlayState();
};

// 自定义智能指针释放器
struct FFmpegDeleter
{
    void operator()(AVFormatContext *ctx);
    void operator()(AVCodecContext *ctx);
    void operator()(AVFrame *frame);
    void operator()(SwsContext *ctx);
    void operator()(SwrContext *ctx);
    void operator()(GLFWwindow *window);
};

class MediaPlayer
{
public:
    MediaPlayer(const std::string &filename, int videoWidth, int videoHeight);
    ~MediaPlayer();

    bool Init();
    void Play();
    void Stop();

private:
    bool OpenFile();
    bool InitVideo();
    bool InitAudio();
    bool InitSDL();
    bool InitGL();
    void DecodeLoop();
    void ProcessVideoPacket(AVPacket *pkt);
    void ProcessAudioPacket(AVPacket *pkt);
    void VideoLoop();
    void AudioCallback(Uint8 *stream, int len);

    std::string filename_;
    bool quit_ = false;
    AVRational time_base_;

    // FFmpeg 资源
    std::unique_ptr<AVFormatContext, FFmpegDeleter> fmt_ctx_;
    std::unique_ptr<AVCodecContext, FFmpegDeleter> video_codec_ctx_, audio_codec_ctx_;
    std::unique_ptr<SwsContext, FFmpegDeleter> sws_ctx_;
    std::unique_ptr<SwrContext, FFmpegDeleter> swr_ctx_;
    std::unique_ptr<GLFWwindow, FFmpegDeleter> window_;

    PlayState *playState_ = nullptr;

    // SDL 资源
    SDL_AudioDeviceID audio_dev_ = 0;
    Shader *sharder_ = nullptr;
    GLuint textures[3];

    int videoWidth;
    int videoHeight;

    GLuint vao, vbo, ebo;

    // 数据队列
    OkQueue<PlayState *> video_frames_;
    OkQueue<std::pair<uint8_t *, size_t>> audio_data_;
    size_t audio_pos_ = 0;
    std::mutex video_mutex_;
    int video_stream_idx_ = -1, audio_stream_idx_ = -1;
};



#endif // MEDIAPLAYER_H