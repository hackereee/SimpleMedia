#ifndef MEDIA_H
#define MEDIA_H
#include <glfw/glfw3.h>
#include <common/gl_common.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <iostream>
#include <thread>
#include <libavformat/avformat.h>
#include <Program/shader.h>
#include <toolkit/bufferq.h>

extern "C" {
#include <libavcodec/avcodec.h>
}

class MediaContext
{
};

class Video

{
public:
    Video(/* args */);
    ~Video();
};

class Audio
{
public:
    AVCodecContext* pCodecCtx;
    AVFormatContext* pFormatCtx;
    const AVCodec* pCodec;
    int audioStreamIndex;
    SwrContext* swr_ctx;
    int channels;
    float duration;
    OkQueue<AVFrame*> frameQueue;
    Audio(AVFormatContext* pFormatCtx, int audioStreamIndex, int channels);

    ~Audio()
    {
        if (pCodecCtx)
        {
            avcodec_free_context(&pCodecCtx);
        }
    }

private:
    void start();
};

class GLObject
{
public:
    GLObject(int width, int height, const char* title);

    ~GLObject()
    {
        //删除window
        if (window != nullptr) { delete window; }
    }

    GLFWwindow* window;
    std::shared_ptr<Shader> shader;
    GLuint vao, vbo, ebo;

private:
    void init();
};

void play(const char* mediaPath);

#endif
