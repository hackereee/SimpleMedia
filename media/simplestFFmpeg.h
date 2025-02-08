
#ifndef SIMPLEST_FFMPEG_H
#define SIMPLEST_FFMPEG_H

#include <iostream>
#include <glad/glad.h>
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif

void playVideo();

void startRender(const GLuint &texture);



class VedioFrame
{
public:
    VedioFrame() {}
    VedioFrame(int pts, AVRational *timeBase, AVFrame *frame) : pts(pts), timeBase(timeBase)
    {
        this->frame = frame;
        this->playTime = pts * av_q2d(*timeBase);
    };
    ~VedioFrame()
    {
        std::cout << "delete frame begin" << std::endl;
        av_frame_free(&frame);
        std::cout << "delete frame end~~" << std::endl;
        
    };
    AVFrame *frame;
    double playTime;

private:
    int pts;
    AVRational *timeBase;
};

const int StatusPreparing = 1;
const int StatusPlaying = 2;
const int StatusEnd = 3;

class VideoMessage
{
public:
    VideoMessage(int64_t width, int64_t height, int status, VedioFrame *frame) : width(width), height(height), status(status), frame(frame){
    };
    virtual ~VideoMessage() {
        std::cout << "delete frame" << std::endl;
        delete frame;
        frame = nullptr;
        std::cout << "delete frame end" << std::endl;
    };
    int64_t width;
    int64_t height;
    int status;
    VedioFrame *frame;
};

// 双缓冲环形缓冲区
class VideoFrameQueue
{
public:
    VideoFrameQueue(int size) : size(size)
    {
        buffer = new VideoMessage *[size];
    };
    ~VideoFrameQueue()
    {
        delete[] buffer;
        // 释放锁
    };
    void push(VideoMessage *msg)
    {
        std::unique_lock<std::mutex> lock(mtx);
        // 如果缓冲区满了则释放锁并等待
        notFull.wait(lock, [this]()
                     { return (writeIndex + 1) % size != readIndex; });
        buffer[writeIndex] = msg;
        std::cout << "push index:" << writeIndex << std::endl;
        writeIndex = (writeIndex + 1) % size;
        notEmpty.notify_one();
        
    };
    VideoMessage *pop()
    {
        std::unique_lock<std::mutex> lock(mtx);
        // 如果缓冲区为空则释放锁并等待
        notEmpty.wait(lock, [this]()
                      { return writeIndex != readIndex; });
        VideoMessage *msg = buffer[readIndex];
         std::cout << "pop index:" << readIndex << std::endl;
        readIndex = (readIndex + 1) % size;
        notFull.notify_one();
        return msg;
    };

private:
    std::mutex mtx;
    std::condition_variable notFull;
    std::condition_variable notEmpty;
    VideoMessage **buffer;
    int size;
    int readIndex = 0;
    int writeIndex = 0;
};

#endif