#include <media.h>
#include <log.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <SDL2/SDL.h>
}

class Audio::Audio
{
};

Audio::Audio(AVFormatContext *pFormatCtx, int audioStreamIndex, int channels) : pFormatCtx(pFormatCtx), audioStreamIndex(audioStreamIndex), channels(channels)
{
    if (!pFormatCtx)
    {
        logError("pCodecCtx is null");
        return;
    }
    pCodec = avcodec_find_decoder(pFormatCtx->streams[audioStreamIndex]->codecpar->codec_id);
    if (!pCodec)
    {
        logError("find audio codec failed");
        return;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx)
    {
        logError("alloc audio codec context failed");
        return;
    }
    if (avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[audioStreamIndex]->codecpar) < 0)
    {
        logError("copy audio codec context failed");
        return;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        logError("open audio codec failed");
        return;
    }
}

void initResmaple(SwrContext *swr_ctx, AVCodecContext *pCodecCtx, int &channels)
{
    swr_ctx = swr_alloc();
    if (!swr_ctx)
    {
        logError("alloc swr context failed");
        return;
    }
    AVChannelLayout *out_ch_layout = new AVChannelLayout();
    out_ch_layout->order = AV_CHANNEL_ORDER_NATIVE;
    // 立体声通道
    out_ch_layout->u.mask = AV_CH_LAYOUT_STEREO;
    av_channel_layout_default(out_ch_layout, channels);

    int swrOpt = swr_alloc_set_opts2(
        &swr_ctx,
        out_ch_layout,
        AV_SAMPLE_FMT_S16,
        pCodecCtx->sample_rate,
        &pCodecCtx->ch_layout,
        pCodecCtx->sample_fmt,
        pCodecCtx->sample_rate,
        0,
        NULL);
    if (swrOpt < 0)
    {
        logError("set swr options failed");
        return;
    }

    if (swr_init(swr_ctx) < 0)
    {
        logError("init swr failed");
        swr_free(&swr_ctx);
        return;
    }
}

void audio_callback(void *userdata, Uint8 *stream, int len)
{
    Audio *audio = (Audio *)userdata;
    AVFrame *pFrame = audio->frameQueue.pop();
    SwrContext *swr_ctx = audio->swr_ctx;
    int nb_samples = pFrame->nb_samples;
    int out_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, pFrame->sample_rate) + nb_samples, pFrame->sample_rate, pFrame->sample_rate, AV_ROUND_UP);
    int outBufferSize = av_samples_get_buffer_size(nullptr, audio->channels, out_nb_samples, AV_SAMPLE_FMT_S16, 1);
    if (outBufferSize < 0)
    {
        logError("get buffer size failed");
        av_frame_free(&pFrame);
        return;
    }
    uint8_t *outBuffer = (uint8_t *)av_malloc(outBufferSize);
    int ret = swr_convert(swr_ctx, &outBuffer, out_nb_samples, (const uint8_t **)pFrame->data, nb_samples);
    if (ret < 0)
    {
        logError("convert audio failed");
        av_frame_free(&pFrame);
        delete outBuffer;
        outBuffer = nullptr;
        return;
    }
    int copySize = std::min(outBufferSize, len);
    memcpy(stream, outBuffer, copySize);
    // 如果不够填充0,0是静音
    if(copySize < len){
        memset(stream + copySize, 0, len - copySize);
    }
    av_frame_free(&pFrame);
}

SDL_AudioSpec *initSDLAudio(Audio *audio)
{
    SDL_Init(SDL_INIT_AUDIO);
    SDL_AudioSpec *audioSepc = new SDL_AudioSpec();
    audioSepc->freq = audio->pCodecCtx->sample_rate;
    // 每个音频1个字节， 8位， AUDIO_S16SYS 16位 占2个字节
    audioSepc->format = AUDIO_S16SYS;
    audioSepc->channels = audio->pCodecCtx->ch_layout.nb_channels;
    audioSepc->samples = 1024;
    audioSepc->callback = audio_callback;
    audioSepc->userdata = audio;
    if (SDL_OpenAudio(audioSepc, NULL) < 0)
    {
        logError("open audio failed");
        return NULL;
    }
    return audioSepc;
}

void startAsync(void *data)
{
    Audio *audio = (Audio *)data;
    SDL_AudioSpec *audioSepc = initSDLAudio(audio);
    // resample
    // 初始化重采样器
    initResmaple(audio->swr_ctx, audio->pCodecCtx, audio->channels);
    if (SDL_OpenAudio(audioSepc, NULL) < 0)
    {
        logError("open audio failed");
        return;
    }

    // 开始播放
    SDL_PauseAudio(0);
    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    while (av_read_frame(audio->pFormatCtx, packet) >= 0)
    {
        if (packet->stream_index == audio->audioStreamIndex)
        {
            if (avcodec_send_packet(audio->pCodecCtx, packet) < 0)
            {
                logError("send packet failed");
                return;
            }
            AVFrame *pFrame = av_frame_alloc();
            if (avcodec_receive_frame(audio->pCodecCtx, pFrame) < 0)
            {
                logError("receive frame failed");
                return;
            }
            audio->frameQueue.push(pFrame);
        }
    }
}

void Audio::start()
{
    std::thread t(startAsync, this);
}
