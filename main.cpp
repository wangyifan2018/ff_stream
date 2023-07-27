#include <sys/time.h>

#include <csignal>
#include <mutex>
#include <queue>
#include <thread>

#include "ff_avframe_convert.h"
#include "ff_video_decode.h"
#include "ff_video_encode.h"

#define MAX_Q_SIZE 10

unsigned int chip_id = 0;
int push_count = 0;
bool quit_flag = false;
std::mutex lock;
std::queue<AVFrame*> q;
int frame_count = 99999;
int sophon_idx = 0;

static void usage(char* program_name);
void handler(int sig);

void decode_encode_serial(void* ff_decoder, void* ff_encoder) {
  VideoEnc_FFMPEG* encoder = (VideoEnc_FFMPEG*)ff_encoder;
  VideoDec_FFMPEG* decoder = (VideoDec_FFMPEG*)ff_decoder;
  int sophon_id = encoder->sophon_id;
  int enc_w = encoder->enc_frame_width;
  int enc_h = encoder->enc_frame_height;
  int framerate = encoder->enc_frame_rate;
  int enc_pix_fmt = encoder->enc_pix_format;
  int output_type = encoder->get_output_type();
  int frame_interval = 1 * 1000 * 1000 / framerate;
  timeval curframe_start, curframe_push;
  bm_handle_t handle;
  bm_dev_request(&handle, sophon_id);

  while (push_count < frame_count) {
    if (output_type != VIDEO_LOCAL_FILE) gettimeofday(&curframe_start, NULL);

    AVFrame* frame = av_frame_alloc();
    AVFrame* enc_frame = av_frame_alloc();
    int ret = decoder->grabFrame(frame);
    if (ret < 0) break;
    AVFrameConvert(handle, frame, enc_frame, enc_h, enc_w, enc_pix_fmt,
                   chip_id);

    encoder->writeFrame(enc_frame);
    push_count++;
    av_frame_free(&frame);
    av_frame_free(&enc_frame);

    if (output_type != VIDEO_LOCAL_FILE) {
      gettimeofday(&curframe_push, NULL);
      int64_t start_time =
          curframe_start.tv_sec * 1000 * 1000 + curframe_start.tv_usec;
      int64_t current_time =
          curframe_push.tv_sec * 1000 * 1000 + curframe_push.tv_usec;
      int64_t cost = current_time - start_time;
      if (cost < frame_interval) usleep(frame_interval - cost);
    }
  }
  bm_dev_free(handle);
}

void* grab_thread(void* ff_decoder) {
  VideoDec_FFMPEG* decoder = (VideoDec_FFMPEG*)ff_decoder;
  while (!quit_flag) {
    if (q.size() < MAX_Q_SIZE) {
      AVFrame* frame = av_frame_alloc();
      decoder->grabFrame(frame);
      std::lock_guard<std::mutex> lk(lock);
      q.push(frame);
    } else {
      usleep(5 * 1000);
    }
  }
  return NULL;
}

void* push_thread(void* ff_encoder) {
  timeval curframe_start, curframe_push;
  VideoEnc_FFMPEG* encoder = (VideoEnc_FFMPEG*)ff_encoder;
  int sophon_id = encoder->sophon_id;
  int enc_w = encoder->enc_frame_width;
  int enc_h = encoder->enc_frame_height;
  int framerate = encoder->enc_frame_rate;
  int enc_pix_fmt = encoder->enc_pix_format;
  int output_type = encoder->get_output_type();
  int frame_interval = 1 * 1000 * 1000 / framerate;

  bm_handle_t handle;
  bm_dev_request(&handle, sophon_id);

  while (!quit_flag) {
    if (output_type != VIDEO_LOCAL_FILE) gettimeofday(&curframe_start, NULL);
    if (q.empty()) {
      usleep(2 * 1000);
      continue;
    }

    AVFrame* frame;
    {
      std::lock_guard<std::mutex> lk(lock);
      frame = q.front();
      q.pop();
    }

    push_count++;
    if (frame->format == AV_PIX_FMT_NONE) {
      continue;
    }

    AVFrame* enc_frame = av_frame_alloc();
    AVFrameConvert(handle, frame, enc_frame, enc_h, enc_w, enc_pix_fmt,
                   chip_id);
    encoder->writeFrame(enc_frame);
    av_frame_free(&enc_frame);
    av_frame_free(&frame);

    if (output_type != VIDEO_LOCAL_FILE) {
      gettimeofday(&curframe_push, NULL);
      int64_t start_time =
          curframe_start.tv_sec * 1000 * 1000 + curframe_start.tv_usec;
      int64_t current_time =
          curframe_push.tv_sec * 1000 * 1000 + curframe_push.tv_usec;
      int64_t cost = current_time - start_time;
      if (cost < frame_interval) usleep(frame_interval - cost);
    }
  }
  bm_dev_free(handle);
  return NULL;
}

int main(int argc, char** argv) {
  // bmlib_log_set_level(BMLIB_LOG_TRACE);

  if (argc < 12) {
    usage(argv[0]);
    return -1;
  }

  signal(SIGINT, handler);
  signal(SIGTERM, handler);

  frame_count = atoi(argv[9]);
  sophon_idx = atoi(argv[11]);
  bm_handle_t handle;
  bm_dev_request(&handle, sophon_idx);
  bm_get_chipid(handle, &chip_id);

  // decoder params
  int output_format = 100;
  int zero_copy = atoi(argv[10]);

  // encoder params
  char* encoder_name = argv[3];
  int enc_pix_fmt = 0;
  if (strcmp(argv[4], "I420") == 0)
    enc_pix_fmt = AV_PIX_FMT_YUV420P;
  else if (strcmp(argv[4], "NV12") == 0)
    enc_pix_fmt = AV_PIX_FMT_NV12;

  int enc_width = atoi(argv[5]);
  int enc_height = atoi(argv[6]);
  int enc_frame_rate = atoi(argv[7]);
  int enc_bitrate = atoi(argv[8]) * 1000;

  VideoDec_FFMPEG decoder;
  VideoEnc_FFMPEG encoder;

  decoder.openDec(argv[1], output_format, sophon_idx, zero_copy);
  encoder.openEnc(argv[2], encoder_name, enc_frame_rate, enc_width, enc_height,
                  enc_pix_fmt, enc_bitrate, sophon_idx);

  std::cout << "  ___           _             " << std::endl;
  std::cout << " / __| ___ _ __| |_  ___ _ _  " << std::endl;
  std::cout << " \\__ \\/ _ \\ '_ \\ ' \\/ _ \\ ' \\ " << std::endl;
  std::cout << " |___/\\___/ .__/|_|_\\___/_||_|" << std::endl;
  std::cout << "          |_|                  " << std::endl;
  std::cout << "rtsp push start... url: " << argv[2] << std::endl;
  std::cout
      << "------------------------------------------------------------------"
      << std::endl;

  /********************** multi thread **********************/
  std::thread grab(&grab_thread, (void*)&decoder);
  std::thread push(&push_thread, (void*)&encoder);

  grab.detach();
  push.detach();

  while (1) {
    if (push_count > frame_count) {
      quit_flag = true;
      break;
    }
    usleep(10 * 1000);
  }
  // wait for thread quit, then close.
  usleep(5 * 1000);

  /************************ serial *************************/
  //   decode_encode_serial((void*)&decoder, (void*)&encoder);

  decoder.closeDec();
  encoder.closeEnc();

  return 0;
}

static void usage(char* program_name) {
  av_log(NULL, AV_LOG_ERROR,
         "\t read, decode, resize, encode, push stream or save local video "
         "file. \n");
  av_log(NULL, AV_LOG_ERROR,
         "Usage: \n\t%s [src_filename] [output_filename] [width] [height] "
         "[encode_pixel_format] [encoder_name] [frame_rate] [bitrate] "
         "[zero_copy] [sophon_idx] \n",
         program_name);
  av_log(NULL, AV_LOG_ERROR,
         "\t[src_filename]            input file name x.mp4 x.ts...\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[output_filename]         encode output file name x.mp4,x.ts...\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[encoder_name]            encode h264_bm,h265_bm.\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[encode_pixel_format]     encode format I420,NV12.\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[width]                   encode 32<width<=4096.\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[height]                  encode 32<height<=4096.\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[frame_rate]              encode frame_rate.\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[bitrate]                 encode bitrate 500 < bitrate < 10000\n");
  av_log(NULL, AV_LOG_ERROR, "\t[frame count]             frame count\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[zero_copy]               PCie platform: 0: copy host mem, 1: "
         "nocopy.\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t                          SoC  platform: any number is acceptable, "
         "but it is invalid\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t[sophon_idx]              PCie platform: sophon devices idx\n");
  av_log(NULL, AV_LOG_ERROR,
         "\t                          SoC  platform: this option set 0, other "
         "number is invalid\n");
  av_log(
      NULL, AV_LOG_ERROR,
      "\t[optional: water mark file path]              water mark file path\n");
  av_log(NULL, AV_LOG_ERROR,
         "\tsuch as:  ./ff_stream_push input.mp4 rtsp://172.1.1.1:8554/test "
         "h264_bm NV12 640 640 25 1000 500 1 0\n");
}

void handler(int sig) {
  av_log(NULL, AV_LOG_INFO, "signal %d is received! \n", sig);
  av_log(NULL, AV_LOG_INFO, "program will exited \n");
  exit(0);
}
