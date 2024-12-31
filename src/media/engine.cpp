#include <engine.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <log.h>
#include <media.h>

class PlayEngine ::PlayEngine
{

public:
    PlayEngine()
    {
    }
    ~PlayEngine()
    {
    }
    void play(const char *mediaPath)
    {
        init(mediaPath);
    }

private:

    void createAudio(Audio *audio, AVFormatContext *pFormatCtx, int audioStreamIndex)
    {
        const AVCodec *pCodec = avcodec_find_decoder(pFormatCtx->streams[audioStreamIndex]->codecpar->codec_id);
        if(!pCodec){
            logError("find audio codec failed");
            return;
        }
        const AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    }

    void init(const char *mediaPath)
    {
        if (!mediaPath)
        {
            logError("path is null");
            return;
        }
        AVFormatContext *pFormatCtx = avformat_alloc_context();
        if (avformat_open_input(&pFormatCtx, mediaPath, NULL, NULL) < 0)
        {
            logError("open file failed");
            return;
        }
        if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        {
            logError("find stream info failed");
            return;
        }
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        for (int i = 0; i < pFormatCtx->nb_streams; i++)
        {
            if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                videoStreamIndex = i;
            }
            else if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audioStreamIndex = i;
            }
        }
        if (videoStreamIndex < 0 && audioStreamIndex < 0)
        {
            logError("find video stream failed");
            return;
        }
    }
};