#include "class2.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL.h>
#include <SDL_thread.h>
}

int class2(const char* filename) { 
  // Register all formats and codecs
  // av_register_all
  // 只需要调用一次，他会注册所有可用的文件格式和编解码库，当文件被打开时他们将自动匹配相应的编解码库。
  av_register_all();

  // 初始化 SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

   AVFormatContext *pFormatCtx = NULL;
  // Open video file
  // 从传入的第二个参数获得文件路径，这个函数会读取文件头信息，并把信息保存在
  // pFormatCtx 结构体当中。 这个函数后面两个参数分别是：
  // 指定文件格式、格式化选项，当我们设置为 NULL 或 0 时，libavformat
  // 会自动完成这些工作。
  if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    return -1;  // Couldn't open file

  // Retrieve stream information
  // 得到流信息  填充了 pFormatCtx->streams 流信息
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1;  // Couldn't find stream information

  // 把信息打印出来：
  av_dump_format(pFormatCtx, 0, filename, 0);

  // Find the first video stream
  // 获取第一个视频流
  int videoStream = -1;
  for (int i = 0; i < pFormatCtx->nb_streams; i++)
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStream = i;
      break;
    }
  if (videoStream == -1) return -1;  // Didn't find a video stream


   AVCodecContext *pCodecCtx = NULL;
  // Get a pointer to the codec context for the video stream
  // pCodecCtx
  // 包含了这个流在用的编解码的所有信息，但我们仍需要通过他获得特定的解码器然后打开他。
  pCodecCtx = pFormatCtx->streams[videoStream]->codec;

  // Find the decoder for the video stream
  // 为视频流获取特定的解码器。
  AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
  AVDictionary *optionsDict = NULL;
  if (pCodec == NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;  // Codec not found
  }
  // Open codec
  if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0)
    return -1;  // Could not open codec

  // Allocate video frame
  // 我们需要内存空间存储一帧数据
  AVFrame *pFrame = av_frame_alloc();


  // SDL 里显示图像的区域叫做 surface
  SDL_Surface *screen = NULL;
  // Make a screen to put our video
#ifndef __DARWIN__

  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
  // 建了一个给定长和宽的屏幕
  // 第三个参数是屏幕的颜色深度--0 表示使用当前屏幕的颜色深度
  // 第四个参数是标示窗口特性
  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
  if (!screen) {
    fprintf(stderr, "SDL: could not set video mode - exiting\n");
    exit(1);
  }

  // Allocate a place to put our YUV image on that screen
  // 现在我们在屏幕创建了一个 YUV overlay，可以把视频放进去了。
  SDL_Overlay *bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
                             SDL_YV12_OVERLAY, screen);

  //入与出的长宽、像素格式、缩放算法、入出滤波器、缩放参数
  struct SwsContext *sws_ctx =  sws_getContext(
      pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width,
      pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);


  AVPacket packet;
  int frameFinished = -1;
  SDL_Rect rect;
  SDL_Event event;

  //读一帧  每次显示的是一帧
  while (av_read_frame(pFormatCtx, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == videoStream) {
      // Decode video frame
      // 解码视频帧数据
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

      // Did we get a video frame?
      // 我们是否获取视频帧
      if (frameFinished) {
        // 要把图层锁住，因为我们要往上面写东西，这是一个避免以后发现问题的好习惯。
        SDL_LockYUVOverlay(bmp);

        // AVPicture结构体有一个数据指针指向一个有四个元素的数据指针，
        // 因为我们处理的 YUV420P 只有三通道，所以只要设置三组数据。
        AVPicture pict;
        // data为像素数据  一帧中的YUV是分别连续存储的
        pict.data[0] = bmp->pixels[0];//Y
        pict.data[1] = bmp->pixels[2];//U
        pict.data[2] = bmp->pixels[1];//V
        //定行数据宽度
        pict.linesize[0] = bmp->pitches[0];
        pict.linesize[1] = bmp->pitches[2];
        pict.linesize[2] = bmp->pitches[1];

        // Convert the image into YUV format that SDL uses
        //　将图片转换成YUV格式
        // 格式信息  输入数据每个通道指针、每个通道行字节数、处理起始行、处理多少行、输出通道信息
        sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
                  pFrame->linesize, 0, pCodecCtx->height, pict.data,
                  pict.linesize);

        SDL_UnlockYUVOverlay(bmp);

        // 我们仍然需要告诉 SDL 显示已经放进去的数据， 要传入一个表明电影位置、
        // 宽度、 高度、 缩放比例的矩形参数
        rect.x = 0;
        rect.y = 0;
        rect.w = pCodecCtx->width;
        rect.h = pCodecCtx->height;
        SDL_DisplayYUVOverlay(bmp, &rect);
      }
    }

    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);

    // 事件系统， SDL 被设置为但你点击，鼠标经过或者给它一个信号的时候，它会产生
    // 一个事件， 程序通过检查这些事件来处理相关的用户输入， 程序也可以向 SDL
    // 事件系统发送事件
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        SDL_Quit();
        exit(0);
        break;
      default:
        break;
    }
  }

  // Free the YUV frame
  av_free(pFrame);

  // Close the codec
  avcodec_close(pCodecCtx);

  // Close the video file
  avformat_close_input(&pFormatCtx);

  return 0;
}
