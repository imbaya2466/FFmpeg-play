
/*
//这里音频因为接口改动无法播放，先继续学习
//之后参考雷与叶的详细实现！！！！！！

#include "class3.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL.h>
#include <SDL_thread.h>
}

#ifdef __MINGW32__
#undef main 
#endif

#include <stdio.h>

#define SDL_AUDIO_BUFFER_SIZE 1024 * 4
#define MAX_AUDIO_FRAME_SIZE 192000

typedef struct PacketQueue {
  AVPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int size;  // size 表示从 packet->size 中得到的字节数
  SDL_mutex *mutex;//互斥量
  SDL_cond *cond;//条件变量
} PacketQueue;
//  SDL 是在一个独立的线程中来进行音频处理的。 
PacketQueue audioq;

int quit = 0;  // 来保证还没有设置程序退出的信号

// 编写一个函数来初始化队列
void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

// 把东西放到队列当中
int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
  AVPacketList *pkt1;
  if (av_dup_packet(pkt) < 0) {
    return -1;
  }
  pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
  if (!pkt1) return -1;
  pkt1->pkt = *pkt;
  pkt1->next = NULL;

  // SDL_LockMutex()用来锁住队列里的互斥量，这样就可以往队列里面加东西了
  SDL_LockMutex(q->mutex);

  if (!q->last_pkt)
    q->first_pkt = pkt1;
  else
    q->last_pkt->next = pkt1;
  q->last_pkt = pkt1;
  q->nb_packets++;
  q->size += pkt1->pkt.size;

  // 然后
  // SDL_CondSignal()会通过条件变量发送一个信号给接收函数（如果它在等待的话）来告诉它现在已经有数据了，然后解锁互斥量
  SDL_CondSignal(q->cond);  //为了有数据才取
  SDL_UnlockMutex(q->mutex);//为了不同时处理
  return 0;
}

static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
  AVPacketList *pkt1;
  int ret;

  SDL_LockMutex(q->mutex);
  for (;;) {
    if (quit) {
      ret = -1;
      break;
    }

    pkt1 = q->first_pkt;
    if (pkt1) {
      q->first_pkt = pkt1->next;
      if (!q->first_pkt) q->last_pkt = NULL;
      q->nb_packets--;
      q->size -= pkt1->pkt.size;
      *pkt = pkt1->pkt;
      av_free(pkt1);
      ret = 1;
      break;
    } else if (!block) {//是否阻塞，不阻塞直接返回0
      ret = 0;
      break;
    } else {
      //循环等待锁
      //SDL_CondWait()
      //函数也为我们做了解锁互斥量的动作然后才尝试着在得到信号后去重新锁定它
      SDL_CondWait(q->cond, q->mutex);
    }
  }
  SDL_UnlockMutex(q->mutex);
  return ret;
}

// audio_decode_frame()，把数据存储在一个中间缓冲中，企图将字节转变为流，当我们数据不够的时候提供给我们，
// 当数据塞满时帮我们保存数据以使我们以后再用。
int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf,
                       int buf_size) {
  static AVPacket pkt;
  static uint8_t *audio_pkt_data = NULL;
  static int audio_pkt_size = 0;
  static AVFrame frame;

  int len1, data_size = 0;

  for (;;) {
    while (audio_pkt_size > 0) {
      int got_frame = 0;
      //一个包里可能有不止一个音频，需要调用很多次
      //len1 表示解码使用的数据的在包中的大小，
      //data_size 表示实际返回的原始音频数据的大小。
      len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
      if (len1 < 0) {
     
        audio_pkt_size = 0;
        break;
      }
      audio_pkt_data += len1;
      audio_pkt_size -= len1;
      if (got_frame) {
        data_size = av_samples_get_buffer_size(NULL, aCodecCtx->channels,
                                               frame.nb_samples,
                                               aCodecCtx->sample_fmt, 1);
        memcpy(audio_buf, frame.data[0], data_size);
      }
      if (data_size <= 0) {
       
        continue;
      }
      
      return data_size;
    }
    if (pkt.data) av_free_packet(&pkt);

    if (quit) {
      return -1;
    }

    if (packet_queue_get(&audioq, &pkt, 1) < 0) {
      return -1;
    }
    audio_pkt_data = pkt.data;
    audio_pkt_size = pkt.size;
  }
}

// 用户数据就是给 SDL 的指针， stream 就是就是将要写音频数据的缓冲区，还有 len
// 是缓冲区的大小 当音频设备需要更多数据时调用的回调函数,从队列里面拿数据。
void audio_callback(void *userdata, Uint8 *stream, int len) {
  AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
  int len1, audio_size;

  static uint8_t
      audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) /
                2];  // 这个音频缓冲的大小是ffmpeg 给我们的音频帧最大值的 1.5
                     // 倍，以给我们一个很好的缓冲。
  static unsigned int audio_buf_size = 0;
  static unsigned int audio_buf_index = 0;

  while (len > 0) {
    //需要读取并解码
    if (audio_buf_index >= audio_buf_size) {
     
      audio_size = audio_decode_frame(aCodecCtx, audio_buf, audio_buf_size);
      if (audio_size < 0) {
       
        //解码失败就输出静音
        audio_buf_size = 1024;  // arbitrary?
        memset(audio_buf, 0, audio_buf_size);
      } else {
        audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }
    //缓冲区仍有，向sdl填充
    len1 = audio_buf_size - audio_buf_index;
    if (len1 > len) len1 = len;
    memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
    len -= len1;
    stream += len1;
    audio_buf_index += len1;//缓冲区使用下标，
  }
}

int class3(const char* filename) { 

  // Register all formats and codecs
  // av_register_all
  // 只需要调用一次，他会注册所有可用的文件格式和编解码库，当文件被打开时他们将自动匹配相应的编解码库。
  av_register_all();

  // 初始化 SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

  // Open video file
  // 从传入的第二个参数获得文件路径，这个函数会读取文件头信息，并把信息保存在
  // pFormatCtx 结构体当中。 这个函数后面两个参数分别是：
  // 指定文件格式、格式化选项，当我们设置为 NULL 或 0 时，libavformat
  // 会自动完成这些工作。
  AVFormatContext *pFormatCtx = NULL;
  if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    return -1;  // Couldn't open file

  // Retrieve stream information
  // 得到流信息 填充stream信息
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1;  // Couldn't find stream information

  // 把信息打印出来
  av_dump_format(pFormatCtx, 0, filename, 0);

  int i, videoStream, audioStream;
  videoStream = -1;
  audioStream = -1;
  // 获取音频视频流
  for (i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
        videoStream < 0) {
      videoStream = i;
    }
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
        audioStream < 0) {
      audioStream = i;
    }
  }
  if (videoStream == -1) return -1;  // Didn't find a video stream
  if (audioStream == -1) return -1;

  // SDL 播放音频的方法是这样的：你要设置好音频相关的选项，采样率（在 SDL
  // 结构体里面叫做频率“freq”），通道数和 其他参数，
  // 还设置了一个回调函数和用户数据。当开始播放音频， SDL
  // 会持续地调用回调函数来要求它把声音缓冲数据填充
  // 进一个特定数量的字节流里面。当把这些信息写到 SDL_AudioSpec 结构体里面后，
  // 调用 SDL_OpenAudio()，它会开启声音设 备和返回另一个 AudioSpec
  // 结构体给我们。
  // 音频解码器环境
  AVCodecContext *aCodecCtx = NULL;
  aCodecCtx = pFormatCtx->streams[audioStream]->codec;

  // Set audio settings from codec info
  SDL_AudioSpec wanted_spec, spec;
  wanted_spec.freq = aCodecCtx->sample_rate;  // 采样率
  wanted_spec.format = AUDIO_S16SYS;  
        // 数据格式：“S16SYS”中的“ S”是有符号的意思， 16
        // 的意思是每个样本是 16位，“
        // SYS”表示字节顺序按照当前系统的顺序。
  wanted_spec.channels = aCodecCtx->channels;  // 通道数
  wanted_spec.silence = 0;  // 这是用来表示静音的值。 因为声音是有符号的，所以静音的值通常为 0。
  wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;  // 音频缓存，它让我们设置当 SDL
                              // 请求更多音频数据时我们应该给它多大的数据
  wanted_spec.callback =
      audio_callback;  // 当音频设备需要更多数据时调用的回调函数
  wanted_spec.userdata =
      aCodecCtx;  // 回调参数

  //返回值表示执行结果 spec表示返回实际音频指标
  //因为已经有了采样率所以无论填充速度如何始终按音频信息频率播放
  if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
    fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
    return -1;
  }
  // 获取音频的解码器
  AVCodec *aCodec = NULL;
  aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
  if (!aCodec) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;
  }
  // 打开音频解码器
  AVDictionary *audioOptionsDict = NULL;
  avcodec_open2(aCodecCtx, aCodec, &audioOptionsDict);

  // 初始化音频数据队列
  packet_queue_init(&audioq);
  SDL_PauseAudio(0);

  // Get a pointer to the codec context for the video stream
  // pCodecCtx
  // 包含了这个流在用的编解码的所有信息，但我们仍需要通过他获得特定的解码器然后打开他
  AVCodecContext *pCodecCtx = NULL;
  pCodecCtx = pFormatCtx->streams[videoStream]->codec;

  // Find the decoder for the video stream
  // 获取视频解码器
  AVCodec *pCodec = NULL;
  pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
  if (pCodec == NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;  // Codec not found
  }
  // Open codec
  // 打开视频解码器
  AVDictionary *videoOptionsDict = NULL;
  if (avcodec_open2(pCodecCtx, pCodec, &videoOptionsDict) < 0)
    return -1;  // Could not open codec
  // if you still try to open it using avcodec_find_encoder it will open libfaac
  // only. Allocate video frame 分配视频帧内存数据
  AVFrame *pFrame = NULL;
  pFrame = av_frame_alloc();

  // Make a screen to put our video

  SDL_Surface *screen = NULL;

#ifndef __DARWIN__
  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
  // 建了一个给定长和宽的屏幕
  // 第三个参数是屏幕的颜色深度--0 表示使用当前屏幕的颜色深度
  // 第四个参数是标示窗口特性
  // http://www.cnblogs.com/landmark/archive/2012/05/04/2476213.html
  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
  if (!screen) {
    fprintf(stderr, "SDL: could not set video mode - exiting\n");
    exit(1);
  }

  // Allocate a place to put our YUV image on that screen
  // 现在我们在屏幕创建了一个 YUV overlay，可以把视频放进去了。
  SDL_Overlay *bmp = NULL;
  bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
                             SDL_YV12_OVERLAY, screen);
  SwsContext *sws_ctx = NULL;
  sws_ctx = sws_getContext(
      pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width,
      pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);

  // Read frames and save first five frames to disk
  i = 0;
  SDL_Rect rect;
  SDL_Event event;
  AVPacket packet;
  int frameFinished;
  while (av_read_frame(pFormatCtx, &packet) >= 0) {
    // Is this a packet from the video stream?
    // 如果是视频数据，那么就解码并显示
    if (packet.stream_index == videoStream) {
      // Decode video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

      // Did we get a video frame?
      if (frameFinished) {
        // 要把图层锁住，因为我们要往上面写东西，这是一个避免以后发现问题的好习惯。
        SDL_LockYUVOverlay(bmp);
        // AVPicture结构体有一个数据指针指向一个有四个元素的数据指针，
        // 因为我们处理的 YUV420P 只有三通道，所以只要设置三组数据。
        AVPicture pict;
        pict.data[0] = bmp->pixels[0];
        pict.data[1] = bmp->pixels[2];
        pict.data[2] = bmp->pixels[1];

        pict.linesize[0] = bmp->pitches[0];
        pict.linesize[1] = bmp->pitches[2];
        pict.linesize[2] = bmp->pitches[1];

        // Convert the image into YUV format that SDL uses
        //　将图片转换成YUV格式
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
        av_free_packet(&packet);
      }
    } else if (packet.stream_index == audioStream) {
      // 如果是音频就放置到音频队列
      packet_queue_put(&audioq, &packet);
    } else {
      av_free_packet(&packet);
    }
    // Free the packet that was allocated by av_read_frame
    // 事件系统， SDL 被设置为但你点击，鼠标经过或者给它一个信号的时候，它会产生
    // 一个事件， 程序通过检查这些事件来处理相关的用户输入， 程序也可以向 SDL
    // 事件系统发送事件，当用 SDL 来编写多任务程
    // 序的时候特别有用，我们将会在教程 4 里面领略。
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        quit = 1;  // 控制队列退出
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

*/

