//------------------------------------------------------------------------------
// Integrated Screen & System-Audio Streaming to Twitch (RTMP)
//------------------------------------------------------------------------------
//   $ g++ -std=c++17 -o beta105-opt00mp430eNOTFINAL5twitch3.exe beta105-opt00mp430eNOTFINAL5twitch3.cpp     -I"C:/msys64/mingw64/include"     -L"C:/msys64/mingw64/lib"     -lavcodec -lavformat -lavutil -lswscale     -lgdi32 -luser32 -lole32 -lswresample

// Required defines & includes
//   live_839259386_1LoEwIC6GVNG84QQKjwU6axAfNt1IY





//------------------------------------------------------------------------------
// SECTION 1: INCLUDES, GLOBALS, AND CUDA INITIALIZATION
//------------------------------------------------------------------------------
// CUDA and NVENC includes
#include <cuda.h>
#include <cuda_runtime.h>
#include "NvEncoder/NvEncoderCuda.h"
#include "NvCodecUtils.h"
#include "nvEncodeAPI.h"
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
#include <vector>
#include <ctime>

#pragma comment(lib, "Ole32.lib")

// Fix MSYS2 __cdecl conflict
#undef __cdecl




class PublicNvEncoderCuda : public NvEncoderCuda {
public:
    PublicNvEncoderCuda(CUcontext cuContext, int width, int height, NV_ENC_BUFFER_FORMAT format)
        : NvEncoderCuda(cuContext, width, height, format) {}

    void AllocateBuffers(int32_t numBuffers) {
        NvEncoderCuda::AllocateInputBuffers(numBuffers);
    }

    bool IsHWEncoderInitializedPublic() {
        return NvEncoder::IsHWEncoderInitialized();
    }
};


template<typename T>
struct CanOutput { static const bool Value = true; };

template<typename T>
struct IsIterable { static const bool Value = false; };

template<typename T>
struct IsGettable { static const bool Value = false; };





namespace simplelogger {
    enum LogLevel { LOG_FATAL, LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG };
    class Logger {
    public:
        Logger(LogLevel level) {
            std::cerr << "[LOG] ";
        }
        template<typename T> Logger& operator<<(const T& msg) {
            std::cerr << msg;
            return *this;
        }
    };
}




namespace utils {
    struct TimeUtils {
        static std::string timeAsString(const char* format, time_t time) {
            char buffer[100];
            strftime(buffer, sizeof(buffer), format, localtime(&time));
            return std::string(buffer);
        }
    };
}










// FFmpeg includes
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

// Dummy execinfo.h for Windows compatibility
//namespace utils {
//    struct TimeUtils {
//        static std::string timeAsString(const char* format, time_t time) {
//            char buffer[100];
//            strftime(buffer, sizeof(buffer), format, localtime(&time));
//           return std::string(buffer);
//        }
//    };
//}



//#define LOG(level) simplelogger::Logger(simplelogger::level)
//#define LOG_FATAL simplelogger::LOG_FATAL
//#define LOG_ERROR simplelogger::LOG_ERROR
//#define LOG_WARNING simplelogger::LOG_WARNING



//class PublicNvEncoderCuda : public NvEncoderCuda {
//public:
//    using NvEncoderCuda::NvEncoderCuda;

    // Expose AllocateInputBuffers via a public method
//    void AllocateBuffers(int32_t numBuffers) {
//        NvEncoderCuda::AllocateInputBuffers(numBuffers);
//    }
//};



CUcontext InitializeCudaContext() {
    CUcontext cuContext = nullptr;
    CUdevice cuDevice;

    if (cuInit(0) != CUDA_SUCCESS) {
        std::cerr << "Failed to initialize CUDA!" << std::endl;
        return nullptr;
    }

    if (cuDeviceGet(&cuDevice, 0) != CUDA_SUCCESS) {
        std::cerr << "Failed to get CUDA device!" << std::endl;
        return nullptr;
    }

    if (cuCtxCreate(&cuContext, 0, cuDevice) != CUDA_SUCCESS) {
        std::cerr << "Failed to create CUDA context!" << std::endl;
        return nullptr;
    }

    return cuContext;
}









// Templates for logging system
//template<typename T>
//struct CanOutput { static const bool Value = true; };

//template<typename T>
//struct IsIterable { static const bool Value = false; };

//template<typename T>
//struct IsGettable { static const bool Value = false; };

// Simple logger implementation
//namespace simplelogger {
//    enum LogLevel { FATAL, ERROR, WARNING, INFO, DEBUG };
//    class Logger {
//    public:
//        Logger(LogLevel level) {
//            std::cerr << "[LOG] ";
//        }
//        template<typename T> Logger& operator<<(const T& msg) {
//            std::cerr << msg;
//            return *this;
//        }
//    };
//}

//#define LOG(level) simplelogger::Logger(simplelogger::level)
//#define FATAL simplelogger::FATAL
//#define ERROR simplelogger::ERROR
//#define LOG_ERROR simplelogger::LOG_ERROR

#define WARNING simplelogger::WARNING

// Public class to access protected NVENC methods
//class PublicNvEncoderCuda : public NvEncoderCuda {
//public:
//    PublicNvEncoderCuda(CUcontext cuContext, int width, int height, NV_ENC_BUFFER_FORMAT format)
//        : NvEncoderCuda(cuContext, width, height, format) {}
//    using NvEncoderCuda::IsHWEncoderInitialized;
//};
//class PublicNvEncoderCuda : public NvEncoderCuda {
//public:
//    PublicNvEncoderCuda(CUcontext cuContext, int width, int height, NV_ENC_BUFFER_FORMAT format)
//        : NvEncoderCuda(cuContext, width, height, format) {}
//    using NvEncoderCuda::IsHWEncoderInitialized;
//    using NvEncoderCuda::AllocateInputBuffers;
//    using NvEncoderCuda::ReleaseInputBuffers;
//    using NvEncoderCuda::CreateEncoder;
//    using NvEncoderCuda::DestroyEncoder;
//};


struct NvEncoderHelper {
    static void AllocateBuffers(NvEncoderCuda* encoder, int32_t numBuffers) {
//        encoder->NvEncoderCuda::AllocateInputBuffers(numBuffers);
//encoder->AllocateBuffers(numBuffers);
auto publicEncoder = dynamic_cast<PublicNvEncoderCuda*>(encoder);
if (publicEncoder) {
    publicEncoder->AllocateBuffers(numBuffers);
} else {
    std::cerr << "Error: Encoder is not of type PublicNvEncoderCuda.\n";
}

auto publicEncoder = dynamic_cast<PublicNvEncoderCuda*>(encoder);
if (publicEncoder) {
    publicEncoder->AllocateBuffers(numBuffers);
} else {
    std::cerr << "Error: Encoder is not of type PublicNvEncoderCuda.\n";
}


    }
};






class PublicNvEncoderCuda : public NvEncoderCuda {
public:
    PublicNvEncoderCuda(CUcontext cuContext, int width, int height, NV_ENC_BUFFER_FORMAT format)
        : NvEncoderCuda(cuContext, width, height, format) {}

    void AllocateBuffersManually(int32_t numBuffers, int width, int height) {
        std::vector<void*> cudaBuffers(numBuffers);
        size_t pitch = 0;
        for (int i = 0; i < numBuffers; i++) {
            CUdeviceptr dptr;
            CUresult res = cuMemAllocPitch(&dptr, &pitch, width * 4, height, 16);
            if (res != CUDA_SUCCESS) {
                std::cerr << "Failed to allocate CUDA buffer #" << i << std::endl;
                return;
            }
            cudaBuffers[i] = reinterpret_cast<void*>(dptr);
            std::cout << "Allocated buffer #" << i << " at address: " << dptr << std::endl;
        }
    }
};










// Initialize NVENC Encoder manually
CUdevice cuDevice;
CUcontext cuContext;

if (cuInit(0) != CUDA_SUCCESS) {
    std::cerr << "CUDA initialization failed.\n";
    return -1;
}

if (cuDeviceGet(&cuDevice, 0) != CUDA_SUCCESS) {
    std::cerr << "CUDA device get failed.\n";
    return -1;
}

if (cuCtxCreate(&cuContext, 0, cuDevice) != CUDA_SUCCESS) {
    std::cerr << "CUDA context creation failed.\n";
    return -1;
}

// Create NVENC Encoder
int width = 1920;
int height = 1080;
PublicNvEncoderCuda* encoder = new PublicNvEncoderCuda(cuContext, width, height, NV_ENC_BUFFER_FORMAT_ARGB);

// Manually allocate buffers
encoder->AllocateBuffersManually(30, width, height);


//2



//------------------------------------------------------------------------------
// SECTION 2: SCREEN CAPTURE CODE
//------------------------------------------------------------------------------

// Structure for a captured frame
struct CapturedFrame {
    std::vector<uint8_t> data; // RGBA pixel data
    int64_t timestamp;         // Timestamp in microseconds
};

// Frame buffer to manage captured frames
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
            delete bufferQueue.front();
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

// Capture the screen and fill the provided buffer with BGRA pixels
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
    bi.biHeight = -height; // Top-down bitmap
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    GetDIBits(hDC, hBitmap, 0, height, buffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hDC);
    ReleaseDC(NULL, hScreen);

    return true;
}



//3
//------------------------------------------------------------------------------
// SECTION 3: NVENC ENCODER SETUP
//------------------------------------------------------------------------------

// Initialize and configure the NVENC encoder
bool initializeNVENCEncoder(PublicNvEncoderCuda* pNvEncoder, int width, int height) {
    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };

    initializeParams.encodeConfig = &encodeConfig;
    initializeParams.frameRateNum = 60;
    initializeParams.frameRateDen = 1;
    initializeParams.maxEncodeWidth = width;
    initializeParams.maxEncodeHeight = height;
    initializeParams.encodeGUID = NV_ENC_CODEC_H264_GUID;
    initializeParams.presetGUID = NV_ENC_PRESET_P3_GUID; // Updated preset
    initializeParams.enablePTD = 1;
    initializeParams.tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;

    pNvEncoder->CreateEncoder(&initializeParams);

    // Check encoder initialization
//    if (!pNvEncoder->IsHWEncoderInitialized()) {
auto publicEncoder = dynamic_cast<PublicNvEncoderCuda*>(pNvEncoder);
if (publicEncoder && !publicEncoder->IsHWEncoderInitialized()) {

        std::cerr << "Failed to initialize NVENC encoder!\n";
        return false;
    }

    std::cout << "NVENC encoder initialized successfully!\n";
    return true;
}



//4




//------------------------------------------------------------------------------
// SECTION 4: FFMPEG ENCODER SETUP
//------------------------------------------------------------------------------

// Initialize the video encoder (NVENC) and create a video stream
bool initializeVideoEncoder(AVFormatContext* fmtCtx, AVCodecContext** codecContext, int width, int height) {
    const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
        std::cerr << "Error: NVIDIA NVENC encoder not found.\n";
        return false;
    }

    *codecContext = avcodec_alloc_context3(codec);
    if (!*codecContext) {
        std::cerr << "Error: Could not allocate video codec context.\n";
        return false;
    }

    (*codecContext)->width = width;
    (*codecContext)->height = height;
    (*codecContext)->time_base = {1, 1000}; // Time base in milliseconds
    (*codecContext)->framerate = {30, 1};   // 30 FPS
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





//5


//------------------------------------------------------------------------------
// SECTION 5: AUDIO ENCODER SETUP
//------------------------------------------------------------------------------

// Initialize the audio encoder (AAC) and create an audio stream
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

    // Set up channel layout based on the number of channels
    av_channel_layout_default(&(*audioCodecCtx)->ch_layout, wfx->nChannels);
    if (wfx->nChannels == 1) {
        av_channel_layout_from_mask(&(*audioCodecCtx)->ch_layout, AV_CH_LAYOUT_MONO);
    } else if (wfx->nChannels == 2) {
        av_channel_layout_from_mask(&(*audioCodecCtx)->ch_layout, AV_CH_LAYOUT_STEREO);
    }

    (*audioCodecCtx)->sample_fmt = AV_SAMPLE_FMT_FLTP; // AAC expects planar float
    (*audioCodecCtx)->bit_rate = 128000; // 128 kbps
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



//6


//------------------------------------------------------------------------------
// SECTION 6: AUDIO CAPTURE CODE (WASAPI LOOPBACK)
//------------------------------------------------------------------------------

// Capture audio from the default playback device using WASAPI loopback
void audioCaptureThread(AVFormatContext* fmtCtx, AVCodecContext* audioCodecCtx) {
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

    // Initialize audio client for loopback capture
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

    // Main audio capture loop
    while (isRecording) {
        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) break;

        while (packetLength != 0) {
            BYTE* pData = nullptr;
            UINT32 numFramesAvailable = 0;
            DWORD flags;

            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;

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

            memcpy(frame->data[0], pData, dataSize);
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
            if (FAILED(hr)) break;

            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    av_packet_free(&packet);
    pAudioClient->Stop();
    pCaptureClient->Release();
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();
}



//7

//------------------------------------------------------------------------------
// SECTION 7: VIDEO CAPTURE THREAD
//------------------------------------------------------------------------------

// Video capture thread using GDI screen capture
void videoCaptureThread(FrameBuffer& frameBuffer, int width, int height) {
    const int targetFrameTimeMicro = 1000000 / 30; // 30 FPS target
    auto startTime = std::chrono::high_resolution_clock::now();

    while (isRecording) {
        std::vector<uint8_t> buffer(width * height * 4);
        auto captureStart = std::chrono::high_resolution_clock::now();
        
        // Capture the screen into the buffer
        if (!captureScreen(buffer.data(), width, height)) {
            std::cerr << "Error capturing screen.\n";
            break;
        }

        auto captureEnd = std::chrono::high_resolution_clock::now();
        int64_t captureDuration = std::chrono::duration_cast<std::chrono::microseconds>(captureEnd - captureStart).count();
        int64_t waitTime = targetFrameTimeMicro - captureDuration;
        if (waitTime > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(waitTime));
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        int64_t elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime).count();

        // Add frame to the buffer
        CapturedFrame* capFrame = new CapturedFrame;
        capFrame->data = std::move(buffer);
        capFrame->timestamp = elapsedTime;
        frameBuffer.push(capFrame);
    }
}



//8


//------------------------------------------------------------------------------
// SECTION 8: VIDEO ENCODING THREAD
//------------------------------------------------------------------------------

// Video encoding thread: reads frames from the buffer and encodes them with NVENC
void videoEncodingThread(FrameBuffer& frameBuffer, AVFormatContext* fmtCtx, AVCodecContext* videoCodecCtx, int width, int height) {
    while (isRecording || !frameBuffer.isEmpty()) {
        CapturedFrame* capFrame = frameBuffer.pop();
        if (!capFrame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        AVFrame* frame = av_frame_alloc();
        frame->format = videoCodecCtx->pix_fmt;
        frame->width = width;
        frame->height = height;

        if (av_frame_get_buffer(frame, 32) < 0) {
            std::cerr << "Error: Could not allocate video frame data.\n";
            av_frame_free(&frame);
            delete capFrame;
            continue;
        }

        // Set presentation timestamp
        frame->pts = av_rescale_q(capFrame->timestamp, {1, 1000000}, videoCodecCtx->time_base);

        // Convert from BGRA to YUV420P using swscale
        SwsContext* swsContext = sws_getContext(width, height, AV_PIX_FMT_BGRA,
                                                width, height, AV_PIX_FMT_YUV420P,
                                                SWS_BILINEAR, nullptr, nullptr, nullptr);
        uint8_t* srcSlice[1] = { capFrame->data.data() };
        int srcStride[1] = { 4 * width };
        sws_scale(swsContext, srcSlice, srcStride, 0, height, frame->data, frame->linesize);
        sws_freeContext(swsContext);

        // Encode the video frame
        AVPacket* packet = av_packet_alloc();
        if (avcodec_send_frame(videoCodecCtx, frame) >= 0) {
            while (avcodec_receive_packet(videoCodecCtx, packet) >= 0) {
                av_packet_rescale_ts(packet, videoCodecCtx->time_base, fmtCtx->streams[0]->time_base);
                packet->stream_index = 0;
                av_interleaved_write_frame(fmtCtx, packet);
                av_packet_unref(packet);
            }
        }

        av_packet_free(&packet);
        av_frame_free(&frame);
        delete capFrame;
    }
}


//9





//------------------------------------------------------------------------------
// SECTION 9: FFMPEG STREAMING INITIALIZATION
//------------------------------------------------------------------------------

// Initialize FFmpeg for streaming to the specified RTMP URL
bool initializeFFmpeg(AVFormatContext** formatContext, AVCodecContext** videoCodecCtx,
                      AVCodecContext** audioCodecCtx, int width, int height, const char* rtmpUrl,
                      WAVEFORMATEX* audioWfx) {
    // Allocate output context
    if (avformat_alloc_output_context2(formatContext, nullptr, "flv", rtmpUrl) < 0 || !*formatContext) {
        std::cerr << "Error: Could not allocate FLV format context.\n";
        return false;
    }

    // Initialize video encoder
    if (!initializeVideoEncoder(*formatContext, videoCodecCtx, width, height)) {
        return false;
    }

    // Initialize audio encoder
    if (!initializeAudioEncoder(*formatContext, audioCodecCtx, audioWfx)) {
        return false;
    }

    // Open output URL if needed
    if (!((*formatContext)->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&(*formatContext)->pb, rtmpUrl, AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Error: Could not open output RTMP URL.\n";
            return false;
        }
    }

    return true;
}




//10



//------------------------------------------------------------------------------
// SECTION 10: RECORDING THREAD
//------------------------------------------------------------------------------

// The main recording thread that manages video, audio, and encoding
void recordingThread(int width, int height) {
    // Twitch RTMP URL (replace with your stream key)
    const char* rtmpUrl = "rtmp://live.twitch.tv/app/live_STREAMKEY";

    // Initialize audio format using WASAPI
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

    // Close the audio client after fetching format
    pAudioClient->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    // Initialize FFmpeg
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

    // Set up frame buffer for video frames
    FrameBuffer frameBuffer(30);

    // Launch video capture, video encoding, and audio capture threads
    std::thread captureThread(videoCaptureThread, std::ref(frameBuffer), width, height);
    std::thread encodingThread(videoEncodingThread, std::ref(frameBuffer), formatContext, videoCodecCtx, width, height);
    std::thread audioThread(audioCaptureThread, formatContext, audioCodecCtx);

    // Wait for all threads to finish
    captureThread.join();
    encodingThread.join();
    audioThread.join();

    // Flush the video encoder
    avcodec_send_frame(videoCodecCtx, nullptr);
    AVPacket* flushPkt = av_packet_alloc();
    while (avcodec_receive_packet(videoCodecCtx, flushPkt) >= 0) {
        av_packet_rescale_ts(flushPkt, videoCodecCtx->time_base, formatContext->streams[0]->time_base);
        flushPkt->stream_index = 0;
        av_interleaved_write_frame(formatContext, flushPkt);
        av_packet_unref(flushPkt);
    }
    av_packet_free(&flushPkt);

    // Finalize FFmpeg output
    av_write_trailer(formatContext);
    avcodec_free_context(&videoCodecCtx);
    avcodec_free_context(&audioCodecCtx);
    if (!(formatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&formatContext->pb);
    }
    avformat_free_context(formatContext);

    std::cout << "Streaming session ended.\n";
}



//11


//------------------------------------------------------------------------------
// SECTION 11: KEYBOARD HOOK
//------------------------------------------------------------------------------

// Low-level keyboard hook to toggle recording with the 'U' key
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (kbStruct->vkCode == 0x55) { // 'U' key
            if (wParam == WM_KEYDOWN) {
                isRecording = !isRecording;
                cv.notify_all();
                std::cout << "Recording toggled: " << (isRecording ? "ON" : "OFF") << "\n";
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}



//12


//------------------------------------------------------------------------------
// SECTION 12: MAIN FUNCTION
//------------------------------------------------------------------------------

// Main entry point of the program
int main() {
    const int width = GetSystemMetrics(SM_CXSCREEN);
    const int height = GetSystemMetrics(SM_CYSCREEN);

    // Initialize CUDA context
    CUcontext cuContext = InitializeCudaContext();
    if (!cuContext) {
        std::cerr << "Failed to initialize CUDA context.\n";
        return -1;
    }

    // Create an NVENC encoder instance
    PublicNvEncoderCuda* pNvEncoder = new PublicNvEncoderCuda(cuContext, width, height, NV_ENC_BUFFER_FORMAT_ARGB);
    if (!pNvEncoder) {
        std::cerr << "Failed to create NVENC encoder.\n";
        return -1;
    }

    // Set encoder parameters
    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };

    initializeParams.encodeConfig = &encodeConfig;
    initializeParams.frameRateNum = 60;
    initializeParams.frameRateDen = 1;
    initializeParams.maxEncodeWidth = width;
    initializeParams.maxEncodeHeight = height;
    initializeParams.encodeGUID = NV_ENC_CODEC_H264_GUID;
    initializeParams.presetGUID = NV_ENC_PRESET_P3_GUID;
    initializeParams.enablePTD = 1;
    initializeParams.tuningInfo = NV_ENC_TUNING_INFO_LOW_LATENCY;

    pNvEncoder->CreateEncoder(&initializeParams);

    // Verify initialization
//    if (!pNvEncoder->IsHWEncoderInitialized()) {
auto publicEncoder = dynamic_cast<PublicNvEncoderCuda*>(pNvEncoder);
if (publicEncoder && !publicEncoder->IsHWEncoderInitializedPublic()) {

        std::cerr << "Failed to initialize NVENC encoder!\n";
        return -1;
    }

    // Launch the recording thread
    std::thread recorderThread([&]() {
        recordingThread(width, height);
    });

    // Install a low-level keyboard hook to toggle recording with the 'U' key
    HHOOK keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!keyboardHook) {
        std::cerr << "Failed to install keyboard hook.\n";
        return -1;
    }

    std::cout << "Press 'U' to toggle recording. Press Ctrl+C to exit.\n";

    // Run the message loop to process keyboard events
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    recorderThread.join();
    pNvEncoder->DestroyEncoder();
    delete pNvEncoder;
    cuCtxDestroy(cuContext);
    UnhookWindowsHookEx(keyboardHook);

    std::cout << "Program terminated.\n";
    return 0;
}




































