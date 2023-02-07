## 声明
本项目采用宽松的MIT软件许可协议。

## 感谢
本项目灵感来自于开源社区，主要参考了以下几个开源项目（包括但不限于）：

**FFmpeg**

主要使用FFmpeg来做解封装(demux)和解码(decode)。代码来自于[ffplay.c](https://ffmpeg.org/doxygen/trunk/ffplay_8c_source.html)、[`demuxing_decoding.c`](https://ffmpeg.org/doxygen/trunk/demuxing_decoding_8c-example.html)和 [`hw_decode.c`](https://ffmpeg.org/doxygen/trunk/hw_decode_8c-example.html)

**Websocket**

采用开源项目[websocketpp-0.8.2](https://github.com/zaphoyd/websocketpp)作为服务器，跟前端保持一个长连接，信令和数据共用一个socket句柄，信令采用`JSON`格式，媒体数据采用`二进制`格式。代码来自于：[websocketpp/examples](https://github.com/zaphoyd/websocketpp/tree/master/examples)

**WebGL**

H5使用Canvas来绘图，采用YUV420格式。为了提升渲染性能，限定了视频分辨率，要求图像宽度必须为8的倍数（使用FFmpeg的libswscale模块进行转换）。代码来自于：[IVWEB 玩转 WASM 系列-WEBGL YUV渲染图像实践](https://juejin.cn/post/6844904008054751246)

**Web Audio**

FFmpeg解码出来的音频数据是PCM格式，使用H5的Web Audio Api来播放，代码来自于： [pcm-player](https://github.com/samirkumardas/pcm-player)

**前端示例**

见htdocs/player，代码来自于[YUV-Webgl-Video-Player](https://github.com/p4prasoon/YUV-Webgl-Video-Player)，分屏代码来自于[JavaScript之类操作：HTML5 canvas多分屏示例](https://blog.csdn.net/boonya/article/details/82784952)

开源社区的发展离不开大家的无私奉献，再次感谢上面的作者以及其他不知名的各位。

## 编译
```
cmake . -A "Win32" -B build
```

## 打包
打包脚本可以参考`scripts\package`

## 示例
![demo.png](docs/images/demo.png)