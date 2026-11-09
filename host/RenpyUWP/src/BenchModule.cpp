// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

// Built-in module "_renpy_uwp": host diagnostics for Python.
//   decode_benchmark(path, convert=True) -> dict
// Decodes every video frame of `path` as fast as possible with FFmpeg, using the same decoder
// options as Ren'Py's ffmedia.c (threads=auto), optionally converting each frame to RGBA with
// swscale like Ren'Py does. Playback through renpysound is paced to the video's frame rate, so it
// cannot show whether decoding is faster than real time; this can.

#include <Python.h>

#include <windows.h>

#include <cstdint>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace {

struct Result {
    long long frames = 0;
    double seconds = 0;
    int threads = 0;
    int threadType = 0;
    int width = 0, height = 0;
    const char* pixFmt = "?";
    const char* codec = "?";
    char error[256] = {0};
};

double Now() {
    static LARGE_INTEGER f = [] {
        LARGE_INTEGER x;
        QueryPerformanceFrequency(&x);
        return x;
    }();
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / static_cast<double>(f.QuadPart);
}

void Fail(Result& r, const char* what, int err) {
    char buf[128];
    av_strerror(err, buf, sizeof(buf));
    snprintf(r.error, sizeof(r.error), "%s: %s", what, buf);
}

void Run(const char* path, bool convert, Result& r) {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* ctx = nullptr;
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    SwsContext* sws = nullptr;
    uint8_t* rgba[4] = {nullptr};
    int rgbaLines[4] = {0};
    AVDictionary* opts = nullptr;
    AVCodec* codec = nullptr;
    int stream = -1, err = 0;

    if ((err = avformat_open_input(&fmt, path, nullptr, nullptr)) < 0) { Fail(r, "open", err); goto done; }
    if ((err = avformat_find_stream_info(fmt, nullptr)) < 0) { Fail(r, "stream info", err); goto done; }
    stream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (stream < 0 || !codec) { Fail(r, "no video stream", stream); goto done; }
    ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, fmt->streams[stream]->codecpar);
    av_dict_set(&opts, "threads", "auto", 0);  // as renpy/src/ffmedia.c
    if ((err = avcodec_open2(ctx, codec, &opts)) < 0) { Fail(r, "codec open", err); goto done; }
    r.threads = ctx->thread_count;
    r.threadType = ctx->active_thread_type;
    r.codec = codec->name;
    {
        double t0 = Now();
        bool eof = false;
        while (true) {
            if (!eof) {
                err = av_read_frame(fmt, pkt);
                if (err < 0) {
                    eof = true;
                    avcodec_send_packet(ctx, nullptr);  // flush
                } else {
                    if (pkt->stream_index == stream) avcodec_send_packet(ctx, pkt);
                    av_packet_unref(pkt);
                }
            }
            while ((err = avcodec_receive_frame(ctx, frame)) == 0) {
                ++r.frames;
                r.width = frame->width;
                r.height = frame->height;
                r.pixFmt = av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format));
                if (convert) {
                    if (!rgba[0]) {
                        av_image_alloc(rgba, rgbaLines, frame->width, frame->height, AV_PIX_FMT_RGBA, 32);
                    }
                    sws = sws_getCachedContext(sws, frame->width, frame->height,
                                               static_cast<AVPixelFormat>(frame->format), frame->width,
                                               frame->height, AV_PIX_FMT_RGBA, SWS_POINT, nullptr, nullptr, nullptr);
                    sws_scale(sws, frame->data, frame->linesize, 0, frame->height, rgba, rgbaLines);
                }
                av_frame_unref(frame);
            }
            if (err == AVERROR_EOF) break;
            if (err != AVERROR(EAGAIN)) { Fail(r, "decode", err); break; }
        }
        r.seconds = Now() - t0;
    }

done:
    av_dict_free(&opts);
    if (rgba[0]) av_freep(&rgba[0]);
    sws_freeContext(sws);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt);
}

PyObject* DecodeBenchmark(PyObject*, PyObject* args, PyObject* kwargs) {
    static const char* kw[] = {"path", "convert", nullptr};
    const char* path = nullptr;
    int convert = 1;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|p", const_cast<char**>(kw), &path, &convert)) return nullptr;
    Result r;
    Py_BEGIN_ALLOW_THREADS
    Run(path, convert != 0, r);
    Py_END_ALLOW_THREADS
    if (r.error[0]) {
        PyErr_Format(PyExc_RuntimeError, "decode_benchmark(%s): %s", path, r.error);
        return nullptr;
    }
    return Py_BuildValue("{s:L,s:d,s:d,s:i,s:s,s:i,s:i,s:s,s:s,s:O}", "frames", r.frames, "seconds", r.seconds, "fps",
                         r.seconds > 0 ? r.frames / r.seconds : 0.0, "threads", r.threads, "thread_type",
                         r.threadType == FF_THREAD_FRAME ? "frame" : r.threadType == FF_THREAD_SLICE ? "slice" : "none",
                         "width", r.width, "height", r.height, "pix_fmt", r.pixFmt, "codec", r.codec, "convert",
                         convert ? Py_True : Py_False);
}

PyObject* StackInfo(PyObject*, PyObject*) {
    ULONG_PTR low = 0, high = 0;
    GetCurrentThreadStackLimits(&low, &high);
    return PyLong_FromUnsignedLongLong(static_cast<unsigned long long>(high - low));
}

PyMethodDef kMethods[] = {
    {"decode_benchmark", reinterpret_cast<PyCFunction>(reinterpret_cast<void (*)()>(DecodeBenchmark)),
     METH_VARARGS | METH_KEYWORDS, "Decode all video frames of a file as fast as possible; returns timing."},
    {"thread_stack_size", StackInfo, METH_NOARGS, "Reserved stack size of the calling thread, in bytes."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kModule = {PyModuleDef_HEAD_INIT, "_renpy_uwp", "renpy-uwp host diagnostics", -1, kMethods};

}  // namespace

extern "C" PyObject* PyInit__renpy_uwp(void) { return PyModule_Create(&kModule); }
