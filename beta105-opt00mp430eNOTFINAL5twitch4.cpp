//------------------------------------------------------------------------------
// Integrated Screen & System-Audio Streaming to Twitch (RTMP)
//------------------------------------------------------------------------------
//   $ g++ -std=c++17 -o beta105-opt00mp430eNOTFINAL5twitch3.exe beta105-opt00mp430eNOTFINAL5twitch3.cpp     -I"C:/msys64/mingw64/include"     -L"C:/msys64/mingw64/lib"     -lavcodec -lavformat -lavutil -lswscale     -lgdi32 -luser32 -lole32 -lswresample

// Required defines & includes
//   beta105-opt00mp430eNOTFINAL5twitch3.exe




#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <ctime>
#pragma comment(lib, "Ole32.lib")

extern "C" {
  #include <libavutil/opt.h>
  #include <libavutil/imgutils.h>
  #include <libavutil/channel_layout.h>
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libswscale/swscale.h>
  #include <libswresample/swresample.h>
}
#define FF_API_OLD_CHANNEL_LAYOUT 1

// Global variables for recording control
std::atomic<bool> isRecording(false);
std::atomic<bool> stopProgram(false);
std::mutex mtx;
std::condition_variable cv;

//------------------------------------------------------------------------------
// VIDEO CAPTURE CODE
//------------------------------------------------------------------------------

// Structure for a captured frame
struct CapturedFrame {
    uint8_t* data;
    int64_t timestamp; // microseconds
};

// A simple frame buffer for captured video frames
class FrameBuffer {
private:
    std::queue<CapturedFrame*> bufferQueue;
    size_t maxSize;
    std::mutex mtx;
public:
    FrameBuffer(size_t size) : maxSize(size) {}
    
    void push(CapturedFrame* frame) {
        std::lock_guard<std::mutex> lock(mtx);
        if (bufferQueue.size() >= maxSize) {
            CapturedFrame* oldFrame = bufferQueue.front();
            delete[] oldFrame->data;
            delete oldFrame;
            bufferQueue.pop();
        }
        bufferQueue.push(frame);
    }
    
    CapturedFrame* pop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (bufferQueue.empty()) return nullptr;
        CapturedFrame* frame = bufferQueue.front();
        bufferQueue.pop();
        return frame;
    }
    
    bool isEmpty() {
        std::lock_guard<std::mutex> lock(mtx);
        return bufferQueue.empty();
    }
};

// Capture the screen and fill the provided buffer with BGRA pixels.
bool captureScreen(uint8_t* buffer, int width, int height) {
    HDC hScreen = GetDC(NULL);
    HDC hDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    SelectObject(hDC, hBitmap);

    if (!BitBlt(hDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY)) {
        std::cerr << "Error: BitBlt failed.\n";
        DeleteObject(hBitmap);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        return false;
    }

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;  // top-down bitmap
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    GetDIBits(hDC, hBitmap, 0, height, buffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    return true;
}

//------------------------------------------------------------------------------
// FFmpeg Initialization for Video and Audio (RTMP FLV output)
//------------------------------------------------------------------------------

// Initialize the video encoder (NVENC) and create a video stream.
bool initializeVideoEncoder(AVFormatContext* fmtCtx, AVCodecContext** codecContext, int width, int height) {
    const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
        std::cerr << "Error: NVIDIA NVENC not found.\n";
        return false;
    }
    *codecContext = avcodec_alloc_context3(codec);
    if (!*codecContext) {
        std::cerr << "Error: Could not allocate video codec context.\n";
        return false;
    }
    (*codecContext)->width = width;
    (*codecContext)->height = height;
    (*codecContext)->time_base = {1, 1000000}; // microsecond resolution
    (*codecContext)->framerate = {30, 1};        // 30 FPS
    (*codecContext)->pkt_timebase = {1, 1000000};
    (*codecContext)->pix_fmt = AV_PIX_FMT_YUV420P;
    (*codecContext)->gop_size = 30;
    (*codecContext)->max_b_frames = 0;
    (*codecContext)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "p4", 0);
    av_dict_set(&opts, "bitrate", "3000000", 0);
    av_dict_set(&opts, "maxrate", "3000000", 0);
    av_dict_set(&opts, "bufsize", "6000000", 0);
    av_dict_set(&opts, "zerolatency", "1", 0);
    if (avcodec_open2(*codecContext, codec, &opts) < 0) {
        std::cerr << "Error: Could not open video codec.\n";
        av_dict_free(&opts);
        return false;
    }
    av_dict_free(&opts);

    AVStream* stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) {
        std::cerr << "Error: Could not create video stream.\n";
        return false;
    }
    avcodec_parameters_from_context(stream->codecpar, *codecContext);
    stream->time_base = (*codecContext)->time_base;
    stream->avg_frame_rate = (*codecContext)->framerate;
    return true;
}

// Initialize the audio encoder (AAC) and create an audio stream.
// Uses the WASAPI mix format (wfx) to set sample rate and channels.
bool initializeAudioEncoder(AVFormatContext* fmtCtx, AVCodecContext** audioCodecCtx, WAVEFORMATEX* wfx) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        std::cerr << "Error: AAC encoder not found.\n";
        return false;
    }
    *audioCodecCtx = avcodec_alloc_context3(codec);
    if (!*audioCodecCtx) {
        std::cerr << "Error: Could not allocate audio codec context.\n";
        return false;
    }
    (*audioCodecCtx)->sample_rate = wfx->nSamplesPerSec;
    
    // Set up channel layout using the new ch_layout field.
    av_channel_layout_default(&(*audioCodecCtx)->ch_layout, wfx->nChannels);
    if (wfx->nChannels == 1) {
        av_channel_layout_from_mask(&(*audioCodecCtx)->ch_layout, AV_CH_LAYOUT_MONO);
    } else if (wfx->nChannels == 2) {
        av_channel_layout_from_mask(&(*audioCodecCtx)->ch_layout, AV_CH_LAYOUT_STEREO);
    }
    // For more channels, the default layout is already set.
    
    (*audioCodecCtx)->sample_fmt = AV_SAMPLE_FMT_FLTP; // AAC expects planar float
    (*audioCodecCtx)->bit_rate = 128000; // 128 kbps example
    (*audioCodecCtx)->time_base = AVRational{1, (*audioCodecCtx)->sample_rate};
    
    if (avcodec_open2(*audioCodecCtx, codec, nullptr) < 0) {
        std::cerr << "Error: Could not open audio codec.\n";
        return false;
    }
    
    AVStream* stream = avformat_new_stream(fmtCtx, nullptr);
    if (!stream) {
        std::cerr << "Error: Could not create audio stream.\n";
        return false;
    }
    avcodec_parameters_from_context(stream->codecpar, *audioCodecCtx);
    stream->time_base = (*audioCodecCtx)->time_base;
    
    return true;
}

// Initialize FFmpeg format context for RTMP (FLV) streaming.
bool initializeFFmpeg(AVFormatContext** formatContext, AVCodecContext** videoCodecCtx,
                      AVCodecContext** audioCodecCtx, int width, int height, const char* rtmpUrl,
                      WAVEFORMATEX* audioWfx) {
    if (avformat_alloc_output_context2(formatContext, nullptr, "flv", rtmpUrl) < 0 || !*formatContext) {
        std::cerr << "Error: Could not allocate FLV format context.\n";
        return false;
    }
    if (!initializeVideoEncoder(*formatContext, videoCodecCtx, width, height)) {
        return false;
    }
    if (!initializeAudioEncoder(*formatContext, audioCodecCtx, audioWfx)) {
        return false;
    }
    if (!((*formatContext)->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&(*formatContext)->pb, rtmpUrl, AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Error: Could not open output RTMP URL.\n";
            return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// AUDIO CAPTURE VIA WASAPI (system audio, loopback)
//------------------------------------------------------------------------------
void audioCaptureThread(AVFormatContext* fmtCtx, AVCodecContext* audioCodecCtx) {
    // Initialize WASAPI for loopback capture.
    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    WAVEFORMATEX* pwfx = nullptr;
    
    hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "Audio: COM initialization failed.\n";
        return;
    }
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) {
        std::cerr << "Audio: Failed to create device enumerator.\n";
        CoUninitialize();
        return;
    }
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        std::cerr << "Audio: Failed to get default audio endpoint.\n";
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) {
        std::cerr << "Audio: Failed to activate audio client.\n";
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        std::cerr << "Audio: Failed to get mix format.\n";
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    
    // Set up resampler.
    AVChannelLayout src_layout;
    av_channel_layout_default(&src_layout, pwfx->nChannels);
    SwrContext* swr_ctx = nullptr;
    int ret = swr_alloc_set_opts2(&swr_ctx,
        &audioCodecCtx->ch_layout,            // destination channel layout
        audioCodecCtx->sample_fmt,              // destination sample format
        audioCodecCtx->sample_rate,             // destination sample rate
        &src_layout,                          // source channel layout
        (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16, // source format
        pwfx->nSamplesPerSec,                   // source sample rate
        0, nullptr);
    if (ret < 0 || !swr_ctx) {
        std::cerr << "Audio: Could not initialize resampler.\n";
        swr_free(&swr_ctx);
        // You might choose to continue without conversion if formats match.
    }
    
    // Initialize audio client for loopback capture.
    REFERENCE_TIME hnsBufferDuration = 10000000; // 1 second
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        hnsBufferDuration,
        0,
        pwfx,
        NULL
    );
    if (FAILED(hr)) {
        std::cerr << "Audio: Failed to initialize audio client.\n";
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    hr = pAudioClient->GetService(IID_PPV_ARGS(&pCaptureClient));
    if (FAILED(hr)) {
        std::cerr << "Audio: Failed to get capture client.\n";
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        std::cerr << "Audio: Failed to start audio capture.\n";
        pCaptureClient->Release();
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    
    AVPacket* packet = av_packet_alloc();
    int64_t audioPts = 0;
    
    while (isRecording) {
        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr))
            break;
        
        while (packetLength != 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesAvailable = 0;
            DWORD flags;
            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
            if (FAILED(hr))
                break;
            
            int bytesPerFrame = pwfx->nBlockAlign;
            int dataSize = numFramesAvailable * bytesPerFrame;
            
            AVFrame* frame = av_frame_alloc();
            frame->nb_samples = numFramesAvailable;
            frame->ch_layout = audioCodecCtx->ch_layout;
            frame->format = audioCodecCtx->sample_fmt;
            frame->sample_rate = audioCodecCtx->sample_rate;
            if (av_frame_get_buffer(frame, 0) < 0) {
                std::cerr << "Audio: Could not allocate frame buffer.\n";
                av_frame_free(&frame);
                break;
            }
            
            if (swr_ctx) {
                const uint8_t* inData[1] = { pData };
                int convRet = swr_convert(swr_ctx, frame->data, numFramesAvailable, inData, numFramesAvailable);
                if (convRet < 0) {
                    std::cerr << "Audio: swr_convert error.\n";
                }
            } else {
                memcpy(frame->data[0], pData, dataSize);
            }
            
            frame->pts = audioPts;
            audioPts += frame->nb_samples;
            
            if (avcodec_send_frame(audioCodecCtx, frame) >= 0) {
                while (avcodec_receive_packet(audioCodecCtx, packet) >= 0) {
                    av_packet_rescale_ts(packet, audioCodecCtx->time_base, fmtCtx->streams[1]->time_base);
                    packet->stream_index = 1;
                    av_interleaved_write_frame(fmtCtx, packet);
                    av_packet_unref(packet);
                }
            }
            av_frame_free(&frame);
            
            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hr))
                break;
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr))
                break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    // Flush encoder.
    avcodec_send_frame(audioCodecCtx, nullptr);
    while (avcodec_receive_packet(audioCodecCtx, packet) >= 0) {
        av_packet_rescale_ts(packet, audioCodecCtx->time_base, fmtCtx->streams[1]->time_base);
        packet->stream_index = 1;
        av_interleaved_write_frame(fmtCtx, packet);
        av_packet_unref(packet);
    }
    
    av_packet_free(&packet);
    if (swr_ctx)
        swr_free(&swr_ctx);
    
    pAudioClient->Stop();
    pCaptureClient->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();
}

//------------------------------------------------------------------------------
// RECORDING THREAD: Launches video and audio capture/encoding threads.
//------------------------------------------------------------------------------
void recordingThread(int width, int height) {
    // Set your Twitch RTMP URL (with your stream key)
    const char* rtmpUrl = "rtmp://live.twitch.tv/app/live_839259386_1LoEwIC6GVNG84QQKjwU6axAfNt1IY";
    
    // Obtain audio mix format from WASAPI.
    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    WAVEFORMATEX* audioWfx = nullptr;
    
    hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        std::cerr << "Audio Init: COM initialization failed.\n";
        return;
    }
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr)) {
        std::cerr << "Audio Init: Failed to create device enumerator.\n";
        CoUninitialize();
        return;
    }
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        std::cerr << "Audio Init: Failed to get default audio endpoint.\n";
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) {
        std::cerr << "Audio Init: Failed to activate audio client.\n";
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    hr = pAudioClient->GetMixFormat(&audioWfx);
    if (FAILED(hr)) {
        std::cerr << "Audio Init: Failed to get mix format.\n";
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return;
    }
    // We don't start the audio client here; it will be reinitialized in the audio thread.
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();
    
    // Initialize FFmpeg format context and encoders.
    AVFormatContext* formatContext = nullptr;
    AVCodecContext* videoCodecCtx = nullptr;
    AVCodecContext* audioCodecCtx = nullptr;
    if (!initializeFFmpeg(&formatContext, &videoCodecCtx, &audioCodecCtx, width, height, rtmpUrl, audioWfx)) {
        CoTaskMemFree(audioWfx);
        return;
    }
    if (avformat_write_header(formatContext, nullptr) < 0) {
        std::cerr << "Error: Could not write header to RTMP stream.\n";
        return;
    }
    CoTaskMemFree(audioWfx);
    
    FrameBuffer frameBuffer(30);
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Video capture thread.
    std::thread captureThread([&]() {
        while (isRecording) {
            uint8_t* buffer = new uint8_t[width * height * 4];
            auto captureStart = std::chrono::high_resolution_clock::now();
            if (!captureScreen(buffer, width, height)) {
                std::cerr << "Error capturing screen.\n";
                delete[] buffer;
                break;
            }
            auto captureEnd = std::chrono::high_resolution_clock::now();
            int64_t captureDuration = std::chrono::duration_cast<std::chrono::microseconds>(captureEnd - captureStart).count();
            int64_t waitTime = (1000000 / 30) - captureDuration;
            if (waitTime > 0)
                std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
            auto currentTime = std::chrono::high_resolution_clock::now();
            int64_t elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime).count();
            
            CapturedFrame* capFrame = new CapturedFrame;
            capFrame->data = buffer;
            capFrame->timestamp = elapsedTime;
            frameBuffer.push(capFrame);
        }
    });
    
    // Video encoding thread.
    std::thread encodingThread([&]() {
        while (isRecording || !frameBuffer.isEmpty()) {
            CapturedFrame* capFrame = frameBuffer.pop();
            if (!capFrame) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            AVFrame* frame = av_frame_alloc();
            frame->format = videoCodecCtx->pix_fmt;
            frame->width  = width;
            frame->height = height;
            if (av_frame_get_buffer(frame, 32) < 0) {
                std::cerr << "Error: Could not allocate video frame data.\n";
                av_frame_free(&frame);
                delete[] capFrame->data;
                delete capFrame;
                continue;
            }
            frame->pts = av_rescale_q(capFrame->timestamp, {1, 1000000}, videoCodecCtx->time_base);
            SwsContext* swsContext = sws_getContext(width, height, AV_PIX_FMT_BGRA,
                                                    width, height, AV_PIX_FMT_YUV420P,
                                                    SWS_BILINEAR, nullptr, nullptr, nullptr);
            uint8_t* srcSlice[1] = { capFrame->data };
            int srcStride[1] = { 4 * width };
            sws_scale(swsContext, srcSlice, srcStride, 0, height, frame->data, frame->linesize);
            sws_freeContext(swsContext);
            
            AVPacket* packet = av_packet_alloc();
            if (avcodec_send_frame(videoCodecCtx, frame) >= 0) {
                while (avcodec_receive_packet(videoCodecCtx, packet) >= 0) {
                    av_packet_rescale_ts(packet, videoCodecCtx->time_base, formatContext->streams[0]->time_base);
                    packet->stream_index = 0;
                    av_interleaved_write_frame(formatContext, packet);
                    av_packet_unref(packet);
                }
            }
            av_packet_free(&packet);
            av_frame_free(&frame);
            delete[] capFrame->data;
            delete capFrame;
        }
    });
    
    // Audio capture thread.
    std::thread audioThread(audioCaptureThread, formatContext, audioCodecCtx);
    
    captureThread.join();
    encodingThread.join();
    audioThread.join();
    
    // Flush video encoder.
    avcodec_send_frame(videoCodecCtx, nullptr);
    AVPacket* flushPkt = av_packet_alloc();
    while (avcodec_receive_packet(videoCodecCtx, flushPkt) >= 0) {
        av_packet_rescale_ts(flushPkt, videoCodecCtx->time_base, formatContext->streams[0]->time_base);
        flushPkt->stream_index = 0;
        av_interleaved_write_frame(formatContext, flushPkt);
        av_packet_unref(flushPkt);
    }
    av_packet_free(&flushPkt);
    
    av_write_trailer(formatContext);
    avcodec_free_context(&videoCodecCtx);
    avcodec_free_context(&audioCodecCtx);
    if (!(formatContext->oformat->flags & AVFMT_NOFILE))
        avio_closep(&formatContext->pb);
    avformat_free_context(formatContext);
    
    std::cout << "Streaming session ended.\n";
}

//------------------------------------------------------------------------------
// Low-level keyboard hook to toggle recording with the 'U' key.
//------------------------------------------------------------------------------
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (kbStruct->vkCode == 0x55) { // 'U' key
            if (wParam == WM_KEYDOWN) {
                isRecording = !isRecording;
                cv.notify_all();
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

//------------------------------------------------------------------------------
// Main: Sets up the recording thread and the keyboard hook.
//------------------------------------------------------------------------------
int main() {
    const int width = GetSystemMetrics(SM_CXSCREEN);
    const int height = GetSystemMetrics(SM_CYSCREEN);
    if (width <= 0 || height <= 0) {
        std::cerr << "Error: Invalid screen resolution.\n";
        return -1;
    }
    
    std::thread recorder([&]() {
        while (!stopProgram) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, []() { return isRecording || stopProgram; });
            if (stopProgram) break;
            std::cout << "Streaming started. Press U to stop.\n";
            recordingThread(width, height);
            std::cout << "Streaming stopped. Press U to start again.\n";
        }
    });
    
    HHOOK keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!keyboardHook) {
        std::cerr << "Error: Could not set keyboard hook.\n";
        return -1;
    }
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    stopProgram = true;
    cv.notify_all();
    recorder.join();
    UnhookWindowsHookEx(keyboardHook);
    return 0;
}
