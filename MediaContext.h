#ifndef MEDIACONTEXT_H
#define MEDIACONTEXT_H

#include <iostream>
#include <stdexcept>
#include <map>
#include "FrameQueue.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/time.h"
#include "SDL.h"
}


//  audio sample rates map from FFMPEG to SDL (only non planar)
static std::map<int,int> AUDIO_FORMAT_MAP = {
   // AV_SAMPLE_FMT_NONE = -1,
    {AV_SAMPLE_FMT_U8,  AUDIO_U8    },
    {AV_SAMPLE_FMT_S16, AUDIO_S16SYS},
    {AV_SAMPLE_FMT_S32, AUDIO_S32SYS},
    {AV_SAMPLE_FMT_FLT, AUDIO_F32SYS}
};


class MediaContext
{
public:
    MediaContext();
    ~MediaContext();

    static int thread_media_decode(void* data);
    static int thread_video_display(void* data);
    static int thread_audio_display(void* data);

    static void fill_audio(void * udata, Uint8 * stream, int len);

    bool getThreadQuit() {return m_thread_quit;}
    bool getThreadPause() {return m_thread_pause;}
    void setThreadQuit(bool status) {m_thread_quit = status;}
    void setThreadPause(bool status) {m_thread_pause = status;}

    //最大音频帧
    enum
    {
        MAX_AUDIO_FRAME_SIZE = 192000,       // 1 second of 48khz 32bit audio    //48000 * (32/8)
        MAX_NODE_NUMBER = 20
    };

private:
    FrameQueue* m_frameQueue;

    AVFormatContext* m_pFormatCtx;
    int m_videoIndex;
    int m_audioIndex;

    AVCodecContext* m_pCodecCtx_video;
    AVCodecContext* m_pCodecCtx_audio;
    const AVCodec* m_pCodec_video;
    const AVCodec* m_pCodec_audio;

    //音频变量
    SwrContext *m_swrCtx;
    unsigned char *m_outBuff;
    int m_out_buffer_size;

    unsigned int m_audioLen = 0;
    unsigned char *m_audioChunk = NULL;
    unsigned char *m_audioPos = NULL;


    //SDL
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;
    SDL_Rect m_rect;

    //线程变量
    bool m_thread_quit = false;
    bool m_thread_pause = false;

    //Frame队列数量计数
    int m_videoNodeCount = 0;
    int m_audioNodeCount = 0;

    int64_t m_startTime;
};

#endif // MEDIACONTEXT_H
