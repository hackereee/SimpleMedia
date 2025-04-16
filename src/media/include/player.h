#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <iostream>
#include <memory>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <SDL2/SDL.h>
#include <toolkit/bufferq.h>
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

enum PlayStatus
{
    PLAYING,
    PAUSED,
    STOPPED
};

class PlayState
{
public:
    AVFrame *frame;
    Clock *clk;
    PlayStatus status = PLAYING;
    PlayState(AVFrame *frame, Clock *clk);
    ~PlayState();
};

class AudioData
{
public:
    uint8_t *data;
    size_t size;
    Clock *clk;
    AudioData(uint8_t *data, size_t size, Clock *clk) : data(data), size(size), clk(clk) {};
    ~AudioData()
    {
        if (data)
        {
            delete data;
        }
        if (clk)
        {
            delete clk;
        }
    }
};

// 自定义智能指针释放器
struct FFmpegDeleter
{
    void operator()(AVFormatContext *ctx);
    void operator()(AVCodecContext *ctx);
    void operator()(SwsContext *ctx);
    void operator()(SwrContext *ctx);
    void operator()(SDL_Window *window);
    void operator()(SDL_Renderer *renderer);
    void operator()(SDL_Texture *texture);
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

    // SDL 资源
    SDL_AudioDeviceID audio_dev_ = 0;
    std::unique_ptr<SDL_Window, FFmpegDeleter> window_;
    std::unique_ptr<SDL_Renderer, FFmpegDeleter> renderer_;
    std::unique_ptr<SDL_Texture, FFmpegDeleter> texture_;

    PlayState *playState_ = nullptr;
    AudioData *current_audio_data_ = nullptr;
    

    int videoWidth;
    int videoHeight;
    double initial_time_ = 0;

    // 数据队列
    OkQueue<PlayState *> video_frames_;
    OkQueue<AudioData *> audio_data_;
    OkQueue<AVPacket *> video_packets_;
    size_t audio_pos_ = 0;
    std::mutex video_mutex_;
    int video_stream_idx_ = -1, audio_stream_idx_ = -1;

    // SDL计时

    void video_thread();
};

#endif // MEDIAPLAYER_H
