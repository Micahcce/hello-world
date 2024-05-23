#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H

#include <iostream>
#include <mutex>
#include <queue>

extern "C"
{
#include "libavcodec/avcodec.h"
}


class FrameQueue
{
public:
    FrameQueue() {}
    ~FrameQueue() {}

    // Push an audio frame into the queue
    void pushAudioFrame(const AVFrame* frame)
    {
        AVFrame* audioFrame = av_frame_alloc();
        av_frame_ref(audioFrame, frame);

        std::unique_lock<std::mutex> lock(mutex);
        audioFrames.push(audioFrame);
        cv.notify_one();
    }

    // Push a video frame into the queue
    void pushVideoFrame(const AVFrame* frame)
    {
        AVFrame* videoFrame = av_frame_alloc();
        av_frame_ref(videoFrame, frame);

        std::unique_lock<std::mutex> lock(mutex);
        videoFrames.push(videoFrame);
        cv.notify_one();
    }

    // Pop an audio frame from the queue
    AVFrame* popAudioFrame()
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (audioFrames.empty())
        {
            cv.wait(lock);
        }
        AVFrame* frame = audioFrames.front();
        audioFrames.pop();
        return frame;
    }

    // Pop a video frame from the queue
    AVFrame* popVideoFrame()
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (videoFrames.empty())
        {
            cv.wait(lock);
        }
        AVFrame* frame = videoFrames.front();
        videoFrames.pop();
        return frame;
    }

    int getVideoFrameCount(){return videoFrames.size();}
    int getAudioFrameCount(){return audioFrames.size();}

private:
    std::queue<AVFrame*> audioFrames; // Queue for audio frames
    std::queue<AVFrame*> videoFrames; // Queue for video frames
    std::mutex mutex;                   // Mutex for thread safety
    std::condition_variable cv;         // Condition variable for synchronization

};


#endif // FRAMEQUEUE_H
