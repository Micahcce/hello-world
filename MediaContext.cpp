#include "MediaContext.h"



MediaContext::MediaContext()
{
    std::cout << "avformat_version : " << avformat_version() << std::endl;
    m_frameQueue = new FrameQueue;
    int ret;

    //1.创建上下文
    m_pFormatCtx = avformat_alloc_context();

    //2.打开文件
    ret = avformat_open_input(&m_pFormatCtx, "food.mp4", nullptr, nullptr);
    if(ret < 0)
        throw std::runtime_error("Error occurred in avformat_open_input");

    //3.上下文获取流信息
    ret = avformat_find_stream_info(m_pFormatCtx, nullptr);
    if(ret < 0)
        throw std::runtime_error("Error occurred in avformat_find_stream_info");

    //4.查找视频流和音频流
    m_videoIndex = av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(m_videoIndex < 0)
        throw std::runtime_error("Error occurred in av_find_best_stream");

    m_audioIndex = av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if(m_audioIndex < 0)
        throw std::runtime_error("Error occurred in av_find_best_stream");

    //5.获取视频数据
    av_dump_format(m_pFormatCtx, -1, nullptr, 0);

    //6.创建音视频解码器上下文
    m_pCodecCtx_video = avcodec_alloc_context3(nullptr);
    m_pCodecCtx_audio = avcodec_alloc_context3(nullptr);

    //7.解码器上下文获取参数
    ret = avcodec_parameters_to_context(m_pCodecCtx_video, m_pFormatCtx->streams[m_videoIndex]->codecpar);
    if(ret < 0)
        throw std::runtime_error("Error occurred in avcodec_parameters_to_context");

    ret = avcodec_parameters_to_context(m_pCodecCtx_audio, m_pFormatCtx->streams[m_audioIndex]->codecpar);
    if(ret < 0)
        throw std::runtime_error("Error occurred in avcodec_parameters_to_context");

    //8.查找解码器
    m_pCodec_video = avcodec_find_decoder(m_pCodecCtx_video->codec_id);
    m_pCodec_audio = avcodec_find_decoder(m_pCodecCtx_audio->codec_id);
    if(!(m_pCodec_video || m_pCodecCtx_audio))
        throw std::runtime_error("Error occurred in avcodec_find_decoder");

    //9.打开解码器并绑定上下文
    ret = avcodec_open2(m_pCodecCtx_video, m_pCodec_video, nullptr);
    if(ret < 0)
        throw std::runtime_error("Error occurred in avcodec_open2");

    ret = avcodec_open2(m_pCodecCtx_audio, m_pCodec_audio, nullptr);
    if(ret < 0)
        throw std::runtime_error("Error occurred in avcodec_open2");



//////////SDL//////////(SDL_Window在主线程创建才可以运行时移动)

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    m_window = SDL_CreateWindow("SDL2.0", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, m_pCodecCtx_video->width, m_pCodecCtx_video->height, SDL_WINDOW_RESIZABLE);
    m_renderer = SDL_CreateRenderer(m_window, -1, 0);
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_IYUV, 0, m_pCodecCtx_video->width, m_pCodecCtx_video->height);
    if(!m_texture)
        throw std::runtime_error("Error occurred in SDL");
    m_rect = SDL_Rect{0, 0, m_pCodecCtx_video->width, m_pCodecCtx_video->height};



//////////音频//////////
    //1.设置音频播放采样参数
    int out_sample_rate = m_pCodecCtx_audio->sample_rate;           //采样率    48000
    int out_channels =    m_pCodecCtx_audio->ch_layout.nb_channels; //通道数    2
    int out_nb_samples =  m_pCodecCtx_audio->frame_size;            //单个通道中的样本数  1024
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_FLT; //声音格式  SDL仅支持部分音频格式

    //7.依据参数计算输出缓冲区大小
    m_out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

    //8.创建音频数据缓冲区
    m_outBuff = (unsigned char *)av_malloc(MAX_AUDIO_FRAME_SIZE * out_channels);

    //9.创建重采样上下文
    m_swrCtx = swr_alloc();

    //10.分配重采样的上下文信息
    swr_alloc_set_opts2(&m_swrCtx,
                           &m_pCodecCtx_audio->ch_layout,             /*out*/
                           out_sample_fmt,                    /*out*///fltp->slt
                           out_sample_rate,                   /*out*/
                           &m_pCodecCtx_audio->ch_layout,               /*in*/
                           m_pCodecCtx_audio->sample_fmt,               /*in*/
                           m_pCodecCtx_audio->sample_rate,              /*in*/
                           0,
                           NULL);


    //11.swr上下文初始化
    swr_init(m_swrCtx);


    ///   SDL
    //12.配置音频播放结构体SDL_AudioSpec，和SwrContext的音频重采样参数保持一致
    SDL_AudioSpec wantSpec;
    wantSpec.freq = out_sample_rate;        //48000/1024=46.875帧
    wantSpec.format = AUDIO_FORMAT_MAP[out_sample_fmt];
    wantSpec.channels = out_channels;
    wantSpec.silence = 0;
    wantSpec.samples = out_nb_samples;
    wantSpec.callback = fill_audio;                 //回调函数
    wantSpec.userdata = this;

    //14.打开音频设备
    if(SDL_OpenAudio(&wantSpec, NULL) < 0)
    {
        printf("can not open SDL!\n");
        throw std::runtime_error("Error occurred in SDL_OpenAudio");
    }

    //15.开始播放
    SDL_PauseAudio(0);


//////////创建线程//////////

    //创建SDL音视频解码线程，并传入参数
    SDL_CreateThread(MediaContext::thread_media_decode, NULL, this);
    SDL_CreateThread(MediaContext::thread_video_display, NULL, this);
    SDL_CreateThread(MediaContext::thread_audio_display, NULL, this);
}

MediaContext::~MediaContext()
{
    swr_free(&m_swrCtx);
    avcodec_free_context(&m_pCodecCtx_video);
    avcodec_free_context(&m_pCodecCtx_audio);
    avformat_close_input(&m_pFormatCtx);
}




//视频解码线程
int MediaContext::thread_media_decode(void *data)
{
    MediaContext* pThis = (MediaContext*) data;
    int ret;

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    pThis->m_startTime = 41.5416 * AV_TIME_BASE;
    avformat_seek_file(pThis->m_pFormatCtx, -1, INT64_MIN, pThis->m_startTime, INT64_MAX, AVSEEK_FLAG_BACKWARD);

    //解码
    while(!pThis->m_thread_quit)
    {
        if(pThis->m_thread_pause)
        {
            SDL_Delay(10);
            continue;
        }

        //读取一个包数据
        if(av_read_frame(pThis->m_pFormatCtx, packet) < 0)
            break;

        if(packet->stream_index == pThis->m_videoIndex)
        {
            //发送一个包数据
            avcodec_send_packet(pThis->m_pCodecCtx_video, packet);

            //显示数据
            while(1)
            {
                ret = avcodec_receive_frame(pThis->m_pCodecCtx_video, frame);

                if(ret < 0) break;
                else if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

                //队列节点太多则暂停解码
                while(pThis->m_videoNodeCount >= MAX_NODE_NUMBER)
                    SDL_Delay(10);

                //存入队列
                pThis->m_frameQueue->pushVideoFrame(frame);
                pThis->m_videoNodeCount++;

                av_frame_unref(frame);
            }
        }
        else if(packet->stream_index == pThis->m_audioIndex)
        {
            avcodec_send_packet(pThis->m_pCodecCtx_audio, packet);      // 送一帧到解码器

            while(1)
            {
                ret = avcodec_receive_frame(pThis->m_pCodecCtx_audio, frame);
                if(ret < 0) break;
                else if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;

                //队列节点太多则暂停解码
                while(pThis->m_audioNodeCount >= MAX_NODE_NUMBER)
                    SDL_Delay(10);

                //存入队列
                pThis->m_frameQueue->pushAudioFrame(frame);
                pThis->m_audioNodeCount++;

                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);
    }

    av_frame_free(&frame);
    return 0;
}



//视频播放线程
int MediaContext::thread_video_display(void* data)
{
    MediaContext* pThis = (MediaContext*) data;

    //渲染
    AVFrame* frame = av_frame_alloc();
    int64_t start_time = av_gettime() - pThis->m_startTime;              //获取从公元1970年1月1日0时0分0秒开始的微秒值

    while(!pThis->m_thread_quit)
    {
        if(pThis->m_thread_pause)
        {
            SDL_Delay(10);
            start_time += 10*1000;
            continue;
        }

        frame = pThis->m_frameQueue->popVideoFrame();
        pThis->m_videoNodeCount--;

        SDL_UpdateYUVTexture(pThis->m_texture, &pThis->m_rect,
                             frame->data[0], frame->linesize[0],
                             frame->data[1], frame->linesize[1],
                             frame->data[2], frame->linesize[2]);
        SDL_RenderClear(pThis->m_renderer);
        SDL_RenderCopy(pThis->m_renderer, pThis->m_texture, nullptr, &pThis->m_rect);
        SDL_RenderPresent(pThis->m_renderer);

        //延时控制
        AVRational time_base_q = {1,AV_TIME_BASE};          // 1/1000000秒，即1us
        int64_t pts_time = av_rescale_q(frame->pkt_dts, pThis->m_pFormatCtx->streams[pThis->m_videoIndex]->time_base, time_base_q);  //该函数将<参数二>时基中的<参数一>时间戳转换<参数三>时基下的<返回值>时间戳
        int64_t now_time = av_gettime() - start_time;       // 计算当前程序已开始时间
        if(pts_time > now_time)                             // 若应帧解码时间>当前时间对比，延时到应解码的时间再开始解下一帧(若无此延时则计算机将会很快解码显示完)
            av_usleep(pts_time - now_time);

        av_frame_unref(frame);
    }
    av_frame_free(&frame);

    return 0;
}


//音频播放线程
int MediaContext::thread_audio_display(void *data)
{
    MediaContext* pThis = (MediaContext*) data;
    int ret;

    //渲染
    AVFrame* frame = av_frame_alloc();

    while(!pThis->m_thread_quit)
    {
        if(pThis->m_thread_pause)
        {
            SDL_Delay(10);
            continue;
        }


        frame = pThis->m_frameQueue->popAudioFrame();
        pThis->m_audioNodeCount--;

        ret = swr_convert(pThis->m_swrCtx, &pThis->m_outBuff, MAX_AUDIO_FRAME_SIZE, (const uint8_t **)frame->data, frame->nb_samples);
        if(ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Error while converting\n");
            break;
        }

        while(pThis->m_audioLen > 0)
            SDL_Delay(1);

        pThis->m_audioChunk = (unsigned char *)pThis->m_outBuff;
        pThis->m_audioPos = pThis->m_audioChunk;
        pThis->m_audioLen = pThis->m_out_buffer_size;

        av_frame_unref(frame);
    }
    av_frame_free(&frame);

    return 0;
}


//回调函数
void MediaContext::fill_audio(void *udata, Uint8 *stream, int len)
{
    MediaContext* pThis = (MediaContext*) udata;

    SDL_memset(stream, 0, len);
    if(pThis->m_audioLen == 0)return;

    len = (len > pThis->m_audioLen ? pThis->m_audioLen : len);

    SDL_MixAudio(stream, pThis->m_audioPos, len, SDL_MIX_MAXVOLUME);

    pThis->m_audioPos += len;
    pThis->m_audioLen -= len;
}


