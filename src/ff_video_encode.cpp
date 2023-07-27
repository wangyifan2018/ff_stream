#include "ff_video_encode.h"
#include <iostream>


VideoEnc_FFMPEG::VideoEnc_FFMPEG()
{
    pFormatCtx    = NULL;
    pOutfmtormat  = NULL;
    out_stream    = NULL;
    aligned_input = NULL;
    enc_frame_width    = 0;
    enc_frame_height   = 0;
    frame_idx      = 0;
    enc_pix_format = 0;
    dec_pix_format = 0;
    is_closed = 1;
}

VideoEnc_FFMPEG::~VideoEnc_FFMPEG()
{
    printf("#######VideoEnc_FFMPEG exit \n");
}

bool string_start_with(const char* s, const char* head, int head_len)
{
    for(int i=0; i<head_len; i++)
    {
        if(s[i]!=head[i])
            return false;
    }
    return true;
}

int VideoEnc_FFMPEG::get_output_type()
{
    if(string_start_with(output_path, "rtsp://", 7))
        return RTSP_STREAM;
    if(string_start_with(output_path, "rtmp://", 7))
        return RTMP_STREAM;
    if(string_start_with(output_path, "tcp://", 6) || string_start_with(output_path, "udp://", 6))
        return BASE_STREAM;
    return VIDEO_LOCAL_FILE;
}

int VideoEnc_FFMPEG::get_frame_rate()
{
    return enc_frame_rate;
}

int VideoEnc_FFMPEG::openEnc(const char* output, const char* codec_name ,
                              int framerate,int width, int height, int encode_pix_format, int bitrate,int sophon_idx)
{
    int ret             = 0;
    AVCodec *encoder    = NULL;
    AVDictionary *dict  = NULL;
    frame_idx           = 0;
    output_path         = output;
    enc_pix_format      = encode_pix_format;
    enc_frame_rate      = framerate;
    enc_frame_width     = width;
    enc_frame_height    = height;
    sophon_id           = sophon_idx;

    // encoder 
    encoder = avcodec_find_encoder_by_name(codec_name);
    if(!encoder){
        printf("Failed to find encoder please try again\n");
        return -1;
    }

    // encoder context
    enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) {
        av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
        return AVERROR(ENOMEM);
    }
    enc_ctx->codec_id           = encoder->id;
    enc_ctx->width              = width;
    enc_ctx->height             = height;
    enc_ctx->pix_fmt            = (AVPixelFormat)enc_pix_format;
    enc_ctx->bit_rate_tolerance = bitrate;
    enc_ctx->bit_rate           = (int64_t)bitrate;
    enc_ctx->gop_size           = 32;
    enc_ctx->time_base          = (AVRational){1, framerate};
    enc_ctx->framerate          = (AVRational){framerate,1};

    av_dict_set_int(&dict, "sophon_idx", sophon_idx, 0);
    av_dict_set_int(&dict, "gop_preset", 2, 0);
    av_dict_set_int(&dict, "is_dma_buffer", 1, 0);

    // get output type. 0 for rtsp, 1 for rtmp, 2 for tcp/udp, 3 for local video file.
    int output_type = get_output_type();
    printf("output type: %d\n", output_type);
    switch(output_type)
    {
        case RTSP_STREAM:
            avformat_alloc_output_context2(&pFormatCtx, NULL, "rtsp", output_path);
            break;
        case RTMP_STREAM:
            avformat_alloc_output_context2(&pFormatCtx, NULL, "flv", output_path);
            break;
        case BASE_STREAM:
            if(string_start_with(codec_name, "h264_bm", 7))
                avformat_alloc_output_context2(&pFormatCtx, NULL, "h264", output_path);
            if(string_start_with(codec_name, "hevc_bm", 7) || string_start_with(codec_name, "h265_bm", 7) )
                avformat_alloc_output_context2(&pFormatCtx, NULL, "hevc", output_path);
            break;
        case VIDEO_LOCAL_FILE:
            avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, output_path);
            pOutfmtormat = av_guess_format(NULL, output_path, NULL);
            if(pOutfmtormat->video_codec == AV_CODEC_ID_NONE){
                printf("Unable to assign encoder automatically by file name, please specify by parameter...\n");
                return -1;
            }
            pFormatCtx->oformat = pOutfmtormat;
            break;
        default:
            break;
    }
    
    if (!pFormatCtx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    // SPS PPS do not be tied with IDR, global header.
    if(pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // open encoder
    ret = avcodec_open2(enc_ctx, encoder, &dict);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder ");
        return ret;
    }
    av_dict_free(&dict);

    // new output stream
    out_stream = avformat_new_stream(pFormatCtx, encoder);
    ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream ");
        return ret;
    }
    out_stream->time_base           = enc_ctx->time_base;
    out_stream->avg_frame_rate      = (AVRational){framerate, 1};
    out_stream->r_frame_rate        = out_stream->avg_frame_rate;
    
    // io open
    if (!(pFormatCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&pFormatCtx->pb, output_path, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", output_path);
            return ret;
        }
    }

    // write header
    ret = avformat_write_header(pFormatCtx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    is_closed = 0;
    return 0;
}

/* data is alligned with 32 */
int VideoEnc_FFMPEG::writeFrame(AVFrame * inPic)
{
    int ret = 0 ;
    int got_output = 0;
    inPic->pts = frame_idx;
    frame_idx++;
    av_log(NULL, AV_LOG_DEBUG, "Encoding frame\n");

    /* encode filtered frame */
    AVPacket enc_pkt;
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);

    ret = avcodec_encode_video2(enc_ctx, &enc_pkt, inPic, &got_output);
    if (ret < 0)
        return ret;
    if (got_output == 0) {
        av_log(NULL, AV_LOG_WARNING, "No output from encoder\n");
        return -1;
    }
    /* prepare packet for muxing */
    av_packet_rescale_ts(&enc_pkt, enc_ctx->time_base, out_stream->time_base);
    /* mux encoded frame */
    ret = av_interleaved_write_frame(pFormatCtx, &enc_pkt);
    return ret;
}

int VideoEnc_FFMPEG::flush_encoder()
{
    int ret;
    int got_frame = 0;
    if (!(enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
        return 0;
    while (1) {

        AVPacket enc_pkt;
        enc_pkt.data = NULL;
        enc_pkt.size = 0;
        av_init_packet(&enc_pkt);
        ret = avcodec_encode_video2(enc_ctx, &enc_pkt, NULL, &got_frame);
        if (ret < 0)
            return ret;

        if (!got_frame)
            break;
        av_packet_rescale_ts(&enc_pkt, enc_ctx->time_base,out_stream->time_base);
        ret = av_interleaved_write_frame(pFormatCtx, &enc_pkt);
        if (ret < 0)
            break;
    }
    return ret;
}

void VideoEnc_FFMPEG::closeEnc()
{
    flush_encoder();
    av_write_trailer(pFormatCtx);
    avcodec_free_context(&enc_ctx);
    if (pFormatCtx && !(pFormatCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&pFormatCtx->pb);
    avformat_free_context(pFormatCtx);
    is_closed = 1;
}

int VideoEnc_FFMPEG::isClosed()
{
    if(is_closed)
        return 1;
    else
        return 0;
}

