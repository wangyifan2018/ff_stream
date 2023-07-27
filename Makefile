CC = aarch64-linux-gnu-g++

INC_DIR += -I/opt/sophon/libsophon-current/include/
INC_DIR += -I/opt/sophon/sophon-ffmpeg-latest/include/
INC_DIR += -I/opt/sophon/sophon-opencv-latest/include/opencv4

CXXFLAGS := -g -O2 -Wall -std=c++11 $(INC_DIR)
LOCAL_MEM_ADDRWIDTH           ?= 19
CXXFLAGS += -DCONFIG_LOCAL_MEM_ADDRWIDTH=$(LOCAL_MEM_ADDRWIDTH)

LDLIBS := -lbmrt -lbmlib -lbmcv -ldl \
	-lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_videoio \
	-lbmvideo -lswresample -lswscale -lavformat -lavutil -lavcodec -lpthread \
	-lyuv \

LIB_DIR = -L/opt/sophon/libsophon-current/lib
LIB_DIR += -L/opt/sophon/sophon-ffmpeg-latest/lib
LIB_DIR += -L/opt/sophon/sophon-opencv-latest/lib 

LDFLAGS = -Wl,-rpath=/opt/sophon/libsophon-current/lib
LDFLAGS += -Wl,-rpath=/opt/sophon/sophon-ffmpeg-latest/lib
LDFLAGS += -Wl,-rpath=/opt/sophon/sophon-opencv-latest/lib 
LDFLAGS += $(LIB_DIR)

all: ff_stream_push
ff_stream_push: main.cpp ff_video_decode.cpp ff_video_encode.cpp ff_avframe_convert.cpp
	$(CC) $^ $(CXXFLAGS) $(LDLIBS) $(LDFLAGS) -o $@
clean:
	rm -f ff_stream_push
