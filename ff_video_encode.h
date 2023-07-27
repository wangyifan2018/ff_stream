#ifndef __FF_VIDEO_ENCODE_
#define __FF_VIDEO_ENCODE_

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include <stdio.h>
#include <unistd.h>
}

#define STEP_ALIGNMENT 32

enum OUTPUT_TYPE{
    RTSP_STREAM=0,
    RTMP_STREAM,
    BASE_STREAM,
    VIDEO_LOCAL_FILE
};

class VideoEnc_FFMPEG
{
public:
    VideoEnc_FFMPEG();
    ~VideoEnc_FFMPEG();

    int openEnc(const char* output, const char* codec_name ,int framerate, 
                int width, int height, int encode_pix_format, int bitrate,int sophon_idx);

    void closeEnc();
    int  writeFrame(AVFrame * inputPicture);
    int  get_frame_rate();
    int  get_output_type();
    int  flush_encoder();
    int  isClosed();
    int               sophon_id;
    int               enc_frame_width;
    int               enc_frame_height;
    int               enc_frame_rate;
    int               enc_pix_format;

private:
    AVFormatContext * pFormatCtx;
    AVOutputFormat  * pOutfmtormat;
    AVCodecContext  * enc_ctx;

    AVStream        * out_stream;
    uint8_t         * aligned_input;
    const char*       output_path;
    int               dec_pix_format;
    int               frame_idx;
    int               is_closed;
};

#endif
