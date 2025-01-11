extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
}

#include <common/gl_common.h>
#include <stb_image.h>
// glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "simplestFFmpeg.h"
#include <iostream>
#include <thread>
#include <Program/shader.h>
#include <condition_variable>
#include <cstdint>
#include <toolkit/bufferq.h>
#include <mutex>
#include <libavutil/time.h>

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

const int BufferSize = 3;

std::shared_ptr<Shader> shader;

std::shared_ptr<OkQueue<std::shared_ptr<VideoMessage>>> videoFrameQueue = std::make_shared<OkQueue<std::shared_ptr<VideoMessage>>>(BufferSize);

void startRender(const GLuint *texture, GLuint &vao, GLuint &ebo, GLFWwindow *window, int w, int h)
{
    if (texture == nullptr)
    {
        return;
    }
    double startTime = 0;
    auto videoMessage = videoFrameQueue->pop();
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
        // y轴翻转
        glm::mat4 revert = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
        if(w > h){
            //如果宽度大于高度， 则说明是横屏，我们铺满宽度，高度等比缩放
            //首先先还原被拉伸之前的比例
            double scale = (double)videoMessage->height / h;
            double wScale = w / (double)videoMessage->width;
            scale = scale * wScale;
            //然后根据原有长宽比再次进行作坊
            revert = glm::scale(revert, glm::vec3(1.0f, scale, 1.0f));
        }else{
            double scale = (double)videoMessage->width / w;
            double hScale = h / (double)videoMessage->height;
            scale = scale * hScale;
            revert = glm::scale(revert, glm::vec3(scale, 1.0f, 1.0f));
        }
        
        shader->use();
        std::string revertName = "revert";
        shader->setMat4(revertName, revert);

    }
    videoMessage = nullptr;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    while (glfwWindowShouldClose(window) == 0)
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
        if (error != GL_NO_ERROR)
        {
            std::cout << "update texture Y error" << error << std::endl;
        }
        // std::cout << "update texture Y" << std::endl;
        // U
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, *(texture + 1));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoMessage->width / 2, videoMessage->height / 2, GL_RED, GL_UNSIGNED_BYTE, frame->frame->data[1]);
        if (glGetError() != GL_NO_ERROR)
        {
            std::cout << "update texture U error" << std::endl;
        }
        // std::cout << "update texture U" << std::endl;
        // V
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, *(texture + 2));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoMessage->width / 2, videoMessage->height / 2, GL_RED, GL_UNSIGNED_BYTE, frame->frame->data[2]);
        if (glGetError() != GL_NO_ERROR)
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
        videoMessage = nullptr;
        std::cout << "frame render delete buffer" << std::endl;
        glfwPollEvents();
    }
}

GLFWwindow  *initRenderer(int width, int height)
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
    startRender(texture, VAO, EBO, window, width, height);
    return window;
}

void sendBegin(int width, int height)
{
    videoFrameQueue->push(std::make_shared<VideoMessage>(width, height, StatusPreparing, nullptr));
}

void sendEnd()
{
    videoFrameQueue->push(std::make_shared<VideoMessage>(0, 0, StatusEnd, nullptr));
}

void startPlay()
{
    std::cout << "请输入视频文件路径" << std::endl;
    std::string path;
    std::cin >> path;
    const char *input = path.c_str();
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
    int audioStreamIndex = -1;

    for (int i = 0; i < pFormatCtx->nb_streams; i++)
    {
        // 找到视频流
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
        }else if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audioStreamIndex = i;
        }
        if(videoStreamIndex != -1 && audioStreamIndex != -1){
            break;
        }
    }
    if (videoStreamIndex < 0 && audioStreamIndex < 0)
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

    // 解码器打开
    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0)
    {
        std::cout << "open codec failed" << std::endl;
        return;
    }

    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    sendBegin(pCodecParamters->width, pCodecParamters->height);
    AVFrame *pFrame = av_frame_alloc();
    // 分配pFrameYUV的缓冲区
    int align = 1;

    SwsContext *pSwsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    double firstTime = glfwGetTime();

    while (true)
    {

        if (av_read_frame(pFormatCtx, packet) < 0)
        {
            std::cout << "read frame failed" << std::endl;
            break;
        }
        if (packet->stream_index == videoStreamIndex)
        {
            int ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret < 0)
            {
                av_packet_unref(packet);
                continue;
            }
            while (ret >= 0)
            {
                ret = (avcodec_receive_frame(pCodecCtx, pFrame));
                if (ret == AVERROR(EAGAIN))
                {
                    break;
                }
                else if (ret == AVERROR_EOF)
                {
                    std::cout << "receive frame eof" << std::endl;
                    break;
                }
                else if (ret < 0)
                {
                    // 解码出错
                    std::cout << "receive frame error, ret:" << ret << std::endl;
                    break;
                }
                if (pFrame->pts == 0)
                {
                    firstTime = glfwGetTime();
                }
                else
                {
                    double currentTime = glfwGetTime();
                    double diff = currentTime - firstTime;
                    auto timebase = pFormatCtx->streams[videoStreamIndex]->time_base;
                    double playTime = pFrame->pts * av_q2d(timebase);
                    std::cout << "diff:" << diff << " playTime:" << playTime << std::endl;
                    if (diff - playTime > 0.01)
                    {
                        continue;
                    }
                    else if (playTime - diff > 0.001)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds((int)((playTime - diff) * 1000)));
                    }
                }
                AVFrame *pFrameYUV = av_frame_alloc();
                pFrameYUV->format = AV_PIX_FMT_YUV420P;
                uint8_t *out_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, align));
                av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer, AV_PIX_FMT_YUV420P, pCodecParamters->width, pCodecParamters->height, align);
                //释放out_buffer
                av_free(out_buffer);
                // 转换成YUV420P
                sws_scale(pSwsCtx, (const uint8_t *const *)pFrame->data, pFrame->linesize, 0, pCodecParamters->height, pFrameYUV->data, pFrameYUV->linesize);
                VedioFrame *frame = new VedioFrame(pFrame->pts, &pFormatCtx->streams[videoStreamIndex]->time_base, pFrameYUV);
                std::cout << "push frame ready " << std::endl;
                videoFrameQueue->push(std::make_shared<VideoMessage>(pCodecParamters->width, pCodecParamters->height, StatusPlaying, frame));
            }
        }
        av_packet_unref(packet);
    }
    sendEnd();
    //释放pSwsCtx
    sws_freeContext(pSwsCtx);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
}

void decodeAndPlay(AVFormatContext *pFormatContext, int& videoIndex, int& audioIndex){
}

void playAudio(AVFormatContext* pFromatContext, int audioIndex){
    if(audioIndex < 0){
        return;
    }
    AVCodecParameters *pCodecParamters = pFromatContext->streams[audioIndex]->codecpar;
    const AVCodec *pCodec = avcodec_find_decoder(pCodecParamters->codec_id);
    if(pCodec == nullptr){
        std::cout << "could not find the decoder" << std::endl;
        return;
    }
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_parameters_to_context(pCodecCtx, pCodecParamters) < 0){
        std::cout << "parameters to context failed" << std::endl;
        return;
    }
    if(avcodec_open2(pCodecCtx, pCodec, nullptr) < 0){
        std::cout << "open codec failed" << std::endl;
        return;
    }
    SDL_AudioSpec wanted_spec;
    //采样率
    wanted_spec.freq = pCodecCtx->sample_rate;
    // 采样格式
    wanted_spec.format = AUDIO_S16SYS;
}

// 音频回调
void audio_callback(void *userdata, Uint8 *stream, int len)
{
    
}


void playVideo()
{
    std::thread t(startPlay);
    auto window = initRenderer(800, 600);
    std::cout << "播放结束，自动退出！" << std::endl;
    glfwTerminate();
}
