#ifndef __FF_VIDEO_DECODE_H
#define __FF_VIDEO_DECODE_H

#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

}

class VideoDec_FFMPEG
{
public:
    VideoDec_FFMPEG();
    ~VideoDec_FFMPEG();

    int openDec(const char* filename, int output_format_mode,
                int sophon_idx, int pcie_no_copyback);
    void closeDec();

    int grabFrame(AVFrame *frame);
    int isClosed();

private:
    AVFormatContext   *ifmt_ctx;
    AVCodec           *decoder;
    AVCodecContext    *video_dec_ctx ;
    AVCodecParameters *video_dec_par;

    int width;
    int height;
    int pix_fmt;

    int video_stream_idx;
    AVPacket pkt;
    int refcount;
    int is_closed;

    int openCodecContext(int *stream_idx,AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx,
                        enum AVMediaType type, int output_format_mode, int sophon_idx, int pcie_no_copyback);

};


#endif /*__FF_VIDEO_DECODE_H*/

