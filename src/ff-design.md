# 音视频同步
## 问题及概念
1. 音频与视频时钟是什么？
2. 如何创建一个准确的音视频时钟
* 视频以每一帧创建
* 音频以每一个样本创建
3. 视频同步音频算法


## 播放器设置顺序
###  基本结构
### 视频
* duration：每一帧需要播放的时长; 
实现： （按顺序回退）
1. 优先使用av_frame_get_pkt_duration 获取帧率时长，如果返回 > 0 则说明解码器设置了pkt_duration,直接将该值 * 时间基(time_base)即可
2. 使用当前帧与上一针的pts差值计算
3. 通过 av_guess_frame_rate()返回帧率，通过av_q2d方法将帧率转为秒
4. 以上3步都无法正确返回时长时，采用固定时长，固定时长由固定帧率计算，这个误差会很大，但一般也不会走到这里，采用100FPS计算帧率也就是0.01秒/Frame

代码示例

```C++
double calculate_frame_duration(const AVFrame *current_frame, const AVFrame *previous_frame, 
                                AVRational time_base, AVRational frame_rate) {
    // 优先尝试使用 pkt_duration
    int64_t pkt_duration = av_frame_get_pkt_duration(current_frame);
    if (pkt_duration > 0) {
        return pkt_duration * av_q2d(time_base);
    }

    // 如果 pkt_duration 不可用，使用 PTS 差值
    if (current_frame && previous_frame) {
        return (current_frame->pts - previous_frame->pts) * av_q2d(time_base);
    }

    // 如果 PTS 不可用，退回到帧率计算
    if (av_q2d(frame_rate) > 0) {
        return 1.0 / av_q2d(frame_rate);
    }

    // 无法确定帧时长
    return 0.01;
}

```
### 音频
* duration: 每一帧音频的播放时长。
音频不像视频画面一样通过视频帧来组合，而是通过采样点采样率同时决定的，所以“一帧音频”为采样点数/采样率，采样点数就是每播放一个单位（一帧）音频的所有点数，所以音频duration计算如下：
1. 通过`av_frame_get_pkt_duration`获取时长，然后*时间基
2. 若1获取不到时长，则通过采样点数 / 采样率 
3. 若2还获取不到时长，我们返回以 1024 采样点 / 44100 采样率的时长

# 线程
* 播放一个多媒体流主要分为以下步骤：解复用 -> 解码 -> 音视频同步 -> 播放。解复用包含了文件I/O，解析与发送Packet；解码分为音频和视频解码；音视频同步一般采用视频同步到音频，以音频时间作为基；播放为音频和视频播放。

有了以上的简单梳理，将播放器的线程设计为一下4个：
1. 解复用线程 - 防止I/O阻塞流程
2. 音频解码线程
3. 视频解码线程
4. 音频播放线程
5. 视频播放使用GUI主线程

# 播放队列
建立2个队列，使用环形双缓冲队列，解复用后发送数据包给两个解码线程，音频线程根据其时长对每一帧音频数据解包前进行相应等待，等待结束进行解码；视频线程根据当前播放音频帧对视频帧进行同步，若当前播放音频已经超过了视频帧播放时间（根据PTS计算），则直接跳过该帧渲染取出下一帧；若当前视频帧比音频帧快，则通过两个duration和PTS计算出等待时长进行等待，完成等待后再继续播放。

# ffplay音视频同步（音频为主时钟）：
## 计算延时时间
代码如下：

```C
static double compute_target_delay(double delay, VideoState *is)
{
    double sync_threshold, diff = 0;

    /* update delay to follow master synchronisation source */
    if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = get_clock(&is->vidclk) - get_master_clock(is);

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
        if (!isnan(diff) && fabs(diff) < is->max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }

```

delay（理想情况下的每一帧延时时间）: 当前即将播放帧pts - 上一帧pts，delay计算函数：

```C
static double vp_duration(VideoState *is, Frame *vp, Frame *nextvp) {
    if (vp->serial == nextvp->serial) {
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) || duration <= 0 || duration > is->max_frame_duration)
            return vp->duration;
        else
            return duration;
    } else {
        return 0.0;
    }
}

```
时钟函数： getClock(), 不计算speed，也就是播放速度变化时为上一帧 - 上一帧时间 + 当前时间 = 上一帧 + 上一帧至现在消逝的时间
diff: 视频时钟 - 音频时钟
sync_threshold(同步阈值): `FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay)); `
* AV_SYNC_THRESHOLD_MIN: 最小同步阈值，固定为0.04，也就是24fps
* AV_SYNC_THRESHOLD_MAX： 最大同步阈值， 固定位0.1， 也就是 6fps
* FFMIN(AV_SYNC_THRESHOLD_MAX, delay): 假设delay比最大阈值还大，那说明其延时已经非常高，这种情况以 `AV_SYNC_THRESHOLD_MAX`为基准，若delay比最小阈值还大，则取`AV_SYNC_THRESHOLD_MIN`
* sync_threshold的意思是只能取，[0.04, 0.1]之间的数值作为同步阈值

```C
if (!isnan(diff) && fabs(diff) < is->max_frame_duration) {
            if (diff <= -sync_threshold)
                delay = FFMAX(0, delay + diff);
            else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
                delay = delay + diff;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }

```
* is->max_frame_duration: 固定值，理解为10s即可，以上就是说，如果音视频差值已经超过10s,那就无同步的必要，就让它这么继续播放下去吧
1. diff <= -sync_threshold:
* diff < 0 说明视频比音频要慢，如果超过这个阈值，那么就调整delay
* delay + diff: 是否为存在比0小的情况？我们首先看delay,前面说过delay来自`vp_duration`函数,正常情况下就是当前帧pts - 上一帧pts的差值，diff: 当前上一帧pts + 上一帧至此流逝的时间，若是在解码取出当前帧的时间比两帧pts差值要早，则就会出现 < 0的情况，所以这里delay = FFMAX(0, delay + diff)是有必要的！否则可能就会出现帧播放比预期FPS要快，这也是不对的哦。
2. diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD 
* AV_SYNC_FRAMEDUP_THRESHOLD : 0.1s
* 这个判断就是视频比音频要快，并且超过了同步阈值，此时将, 0.1s我们之前已经说过，这个代表6帧，这在现代视频里几乎是百分百的，6fps得多慢？所以，我们可以假定只要第一个条件满足，两帧之间的时间差比阈值还要大，这里说明时间长度超过最大值6fps, 那就要通过delay + diff做为等待时间，等待音频追赶上来
* diff >= sync_threshold， 这个和上面的几乎一致的，也就是其实这个判断我个人认为暂时是多余的，因为几乎不可能存在比6fps的播放帧率，所以这里可以简化为后两个逻辑分支同用一个延时算法 delay = delay + diff, 也就是可以改变为如下：
```C
else if(diff >= sync_threshold){
    delay += diff;
}
```

## 解码时同步
代码：
```C
static int get_video_frame(VideoState *is, AVFrame *frame)
{
    int got_picture;

    if ((got_picture = decoder_decode_frame(&is->viddec, frame, NULL)) < 0)
        return -1;

    if (got_picture) {
        double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE)
            dpts = av_q2d(is->video_st->time_base) * frame->pts;

        frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

        if (framedrop>0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {
            if (frame->pts != AV_NOPTS_VALUE) {
                double diff = dpts - get_master_clock(is);
                if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
                    diff - is->frame_last_filter_delay < 0 &&
                    is->viddec.pkt_serial == is->vidclk.serial &&
                    is->videoq.nb_packets) {
                    is->frame_drops_early++;
                    av_frame_unref(frame);
                    got_picture = 0;
                }
            }
        }
    }

    return got_picture;
}
```
详情可以参考：https://ffmpeg.xianwaizhiyin.net/ffplay/video_sync.html

