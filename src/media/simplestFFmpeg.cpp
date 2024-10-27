extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <common/gl_common.h>
#include <stb_image.h>
//glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "simplestFFmpeg.h"
#include <iostream>
#include <thread>
#include <Program/shader.h>
#include <condition_variable>
#include <cstdint>

/// @brief 顶点及纹理坐标
const float vertices[] = {
    // 顶点坐标          // 纹理坐标
    -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
    0.5f, 0.5f, 0.0f, 1.0f, 1.0f,
    -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
    0.5f, -0.5f, 0.0f, 1.0f, 0.0f};

const int indices[] = {
    0, 1, 2,
    2, 3, 1};

const int BufferSize = 3;

VideoFrameQueue *videoFrameQueue;
std::shared_ptr<Shader> shader;

void startRender(const GLuint *texture, GLuint &vao, GLuint &ebo, GLFWwindow *window)
{
    if (texture == nullptr)
    {
        return;
    }
    double startTime = 0;
    VideoMessage *videoMessage = videoFrameQueue->pop();
    if (videoMessage->status == StatusPreparing)
    {
        // 初始化纹理
        // Y
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, *texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoMessage->width, videoMessage->height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        // U
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, *(texture + 1));
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoMessage->width / 2, videoMessage->height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        // V
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, *(texture + 2));
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoMessage->width / 2, videoMessage->height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        //y轴翻转
        glm::mat4 revert = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
        shader->use();
        std::string revertName = "revert";
        shader->setMat4(revertName, revert);
    }
    delete videoMessage;
    videoMessage = nullptr;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    while (true)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindVertexArray(1);
        videoMessage = videoFrameQueue->pop();
        VedioFrame *frame = videoMessage->frame;
        if (frame == nullptr)
        {
            std::cout << "play is end" << std::endl;
            break;
        }
        if (frame->frame->format == AV_PIX_FMT_YUV420P)
        {
            std::cout << "frame format is yuv420p" << std::endl;
        }
        double currentTime = glfwGetTime();
        double playTime = frame->playTime;
        if (startTime == 0)
        {
            startTime = currentTime;
        }
        double diff = currentTime - startTime;
        // 如果错过播放时间帧，则跳过该帧渲染，如果不到那就等待下一帧的播放时间
        if (playTime > diff && playTime - diff > 0.1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds((int)((playTime - diff) * 1000)));
        }
        else if(playTime < diff)
        {
            std::cout << "frame delay" << currentTime - startTime - playTime << std::endl;
            continue;
        }
        shader->use();
        std::string useTexture = "useTexture";
        shader->setBool(useTexture, true);

        std::cout << "frame texture prepared" << std::endl;
        // 更新纹理
        // Y
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, *texture);

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoMessage->width, videoMessage->height, GL_RED, GL_UNSIGNED_BYTE, frame->frame->data[0]);
        int error = glGetError();
        if(error != GL_NO_ERROR)
        {
            std::cout << "update texture Y error" << error << std::endl;
        }
        // std::cout << "update texture Y" << std::endl;
        // U
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, *(texture + 1));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoMessage->width / 2, videoMessage->height / 2, GL_RED, GL_UNSIGNED_BYTE, frame->frame->data[1]);
        if(glGetError() != GL_NO_ERROR)
        {
            std::cout << "update texture U error" << std::endl;
        }
        // std::cout << "update texture U" << std::endl;
        //V
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, *(texture + 2));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoMessage->width / 2, videoMessage->height / 2, GL_RED, GL_UNSIGNED_BYTE, frame->frame->data[2]);
        if(glGetError() != GL_NO_ERROR)
        {
            std::cout << "update texture V error" << std::endl;
        }
        // std::cout << "update texture V" << std::endl;
        glBindVertexArray(vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        std::cout << "frame render draw element" << std::endl;
        glfwSwapBuffers(window);
        std::cout << "frame render swap buffer" << std::endl;
        delete videoMessage;
        videoMessage = nullptr;
        std::cout << "frame render delete buffer" << std::endl;
        glfwPollEvents();
    }
    delete videoFrameQueue;
}

void initRenderer(int width, int height)
{
    GLFWwindow *window = initGlEnv(width, height, "Titanic");
    shader = std::make_shared<Shader>("shaders/media/media.vert", "shaders/media/media.frag");
    shader->use();
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    // 创建纹理
    GLuint texture[3];
    glGenTextures(3, texture);
    // Y
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture[0]);
    std::string textureY = "textureY";
    shader->setInt(textureY, 0);
    // 设置环绕方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);     // x轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);     // y轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大
    // U
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture[1]);
    std::string textureU = "textureU";
    shader->setInt(textureU, 1);
    // 设置环绕方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);     // x轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);     // y轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大
    // V
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texture[2]);
    std::string textureV = "textureV";
    shader->setInt(textureV, 2);
    // 设置环绕方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);     // x轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);     // y轴
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // 缩小
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // 放大

    // 开启多级渐远纹理
    // glGenerateMipmap(GL_TEXTURE_2D);
    // 开启垂直同步
    glfwSwapInterval(1);
    startRender(texture, VAO, EBO, window);
}

void startPlay()
{
    videoFrameQueue = new VideoFrameQueue(BufferSize);
    const char *input = "media/Titanic.ts";
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    // 打开文件
    if (avformat_open_input(&pFormatCtx, input, NULL, NULL) != 0)
    {
        std::cout << "open file failed" << std::endl;
        return;
    }
    // 获取流信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        std::cout << "find stream info failed" << std::endl;
        return;
    }

    int videoStreamIndex = -1;

    for (int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        // 找到视频流
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex < 0)
    {
        std::cout << "find video stream failed" << std::endl;
        return;
    }
    // 编码信息
    AVCodecParameters *pCodecParamters = pFormatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec *pCodec = avcodec_find_decoder(pCodecParamters->codec_id);
    if (pCodec == nullptr)
    {
        std::cout << "find decoder failed" << std::endl;
        return;
    }
    // 获取解码上下文
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_parameters_to_context(pCodecCtx, pCodecParamters) < 0)
    {
        std::cout << "parameters to context failed" << std::endl;
        return;
    }
    VideoMessage *videoMessage = new VideoMessage(pCodecParamters->width, pCodecParamters->height, StatusPreparing, nullptr);
    videoFrameQueue->push(videoMessage);
    // 解码器打开
    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0)
    {
        std::cout << "open codec failed" << std::endl;
        return;
    }

    AVFrame *pFrame = av_frame_alloc();

    // 分配pFrameYUV的缓冲区
    int align = 1;

    SwsContext *pSwsCtx = sws_getContext(pCodecParamters->width, pCodecParamters->height, pCodecCtx->pix_fmt, pCodecParamters->width, pCodecParamters->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    while (true)
    {
        if (av_read_frame(pFormatCtx, packet) < 0)
        {
            videoFrameQueue->push(new VideoMessage(pCodecParamters->width, pCodecParamters->height, StatusEnd, nullptr));
            break;
        }
        if (packet->stream_index == videoStreamIndex)
        {
            // 这里最好是发送和解码放不同线程并行处理，以免单线程处理过慢导致渲染的延时
            int ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret < 0)
            {
                av_packet_unref(packet);
                continue;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(pCodecCtx, pFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    // 已经到文件末尾
                    break;
                }
                else if (ret < 0)
                {
                    // 解码出错
                    std::cout << "receive frame error, ret:" << ret << std::endl;
                    break;
                }
                AVFrame *pFrameYUV = av_frame_alloc();
                pFrameYUV->format = AV_PIX_FMT_YUV420P;
                uint8_t *out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecParamters->width, pCodecParamters->height, align));
                av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer, AV_PIX_FMT_YUV420P, pCodecParamters->width, pCodecParamters->height, align);
                // 转换成YUV420P
                sws_scale(pSwsCtx, (const uint8_t *const *)pFrame->data, pFrame->linesize, 0, pCodecParamters->height, pFrameYUV->data, pFrameYUV->linesize);
                VedioFrame *frame = new VedioFrame(pFrame->pts, &pFormatCtx->streams[videoStreamIndex]->time_base, pFrameYUV);
                std::cout << "push frame ready " << std::endl;
                videoFrameQueue->push(new VideoMessage(pCodecParamters->width, pCodecParamters->height, StatusPlaying, frame));
            }

            av_packet_unref(packet);
        }
    }
}

void playVideo()
{
    std::thread t(startPlay);
    initRenderer(800, 600);
}
