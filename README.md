# Multimedia Test

Example

build
```bash
mkdir build && cd build
cmake ..
make -j4
cd ..
```

run
```bash
./ff_stream_push.pcie input.mp4 rtsp://127.0.0.1:8554/test h264_bm NV12 1920 1080 25 1000 500 1 0
```
