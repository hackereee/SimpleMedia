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
#include <GLFW/glfw3.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

// 自定义智能指针释放器
struct FFmpegDeleter {
    void operator()(AVFormatContext* ctx);
    void operator()(AVCodecContext* ctx);
    void operator()(AVFrame* frame);
    void operator()(SwsContext* ctx);
    void operator()(SwrContext* ctx);
    void operator()(GLFWwindow* window);
};

class MediaPlayer {
public:
    MediaPlayer(const std::string& filename, int videoWidth, int videoHeight);
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
    void ProcessVideoPacket(AVPacket* pkt);
    void ProcessAudioPacket(AVPacket* pkt);
    void VideoLoop();
    void AudioCallback(Uint8* stream, int len);

    std::string filename_;
    bool quit_ = false;

    // FFmpeg 资源
    std::unique_ptr<AVFormatContext, FFmpegDeleter> fmt_ctx_;
    std::unique_ptr<AVCodecContext, FFmpegDeleter> video_codec_ctx_, audio_codec_ctx_;
    std::unique_ptr<AVFrame, FFmpegDeleter> frame_;
    std::unique_ptr<SwsContext, FFmpegDeleter> sws_ctx_;
    std::unique_ptr<SwrContext, FFmpegDeleter> swr_ctx_;

    // SDL 资源
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    SDL_AudioDeviceID audio_dev_ = 0;

    int videoWidth;
    int videoHeight;

    // 数据队列
    std::queue<AVFrame*> video_frames_;
    OkQueue<std::pair<uint8_t*, size_t>> audio_data_;
    size_t audio_pos_ = 0;
    std::mutex video_mutex_;
    int video_stream_idx_ = -1, audio_stream_idx_ = -1;
};

#endif // MEDIAPLAYER_H