#include "class_new1.h"

extern "C" {
#include <libavcodec\avcodec.h>
#include <libavformat\avformat.h>
#include <libswscale\swscale.h>
#include <libavutil\imgutils.h>  
#include <sdl2\SDL.h>
#include <sdl2\SDL_rect.h>
#include <sdl2\SDL_render.h>
#include <sdl2\SDL_video.h>
}

#include <stdio.h>

int class1(const char* filename) { 
    // Initalizing these to NULL prevents segfaults!
  AVFormatContext* p_fmt_ctx = NULL;
  AVCodecContext* p_codec_ctx = NULL;
  AVCodecParameters* p_codec_par = NULL;
  AVCodec* p_codec = NULL;
  AVFrame* p_frm_raw = NULL;  // ֡���ɰ�����õ�ԭʼ֡
  AVFrame* p_frm_yuv = NULL;  // ֡����ԭʼ֡ɫ��ת���õ�
  AVPacket* p_packet = NULL;  // ���������ж�����һ������
  struct SwsContext* sws_ctx = NULL;
  int buf_size;
  uint8_t* buffer = NULL;
  int i;
  int v_idx;
  int ret;
  SDL_Window* screen;
  SDL_Renderer* sdl_renderer;
  SDL_Texture* sdl_texture;
  SDL_Rect sdl_rect;


  // ��ʼ��libavformat(���и�ʽ)��ע�����и�����/�⸴����
  // av_register_all();   // �ѱ�����Ϊ��ʱ�ģ�ֱ�Ӳ���ʹ�ü���

  // A1. ����Ƶ�ļ�����ȡ�ļ�ͷ�����ļ���ʽ��Ϣ�洢��"fmt context"��
  ret = avformat_open_input(&p_fmt_ctx, filename, NULL, NULL);
  if (ret != 0) {
    printf("avformat_open_input() failed\n");
    return -1;
  }

  // A2.
  // ��������Ϣ����ȡһ����Ƶ�ļ����ݣ����Խ��룬��ȡ��������Ϣ����pFormatCtx->streams
  //     p_fmt_ctx->streams��һ��ָ�����飬�����С��pFormatCtx->nb_streams
  ret = avformat_find_stream_info(p_fmt_ctx, NULL);
  if (ret < 0) {
    printf("avformat_find_stream_info() failed\n");
    return -1;
  }

  // ���ļ������Ϣ��ӡ�ڱ�׼�����豸��
  av_dump_format(p_fmt_ctx, 0, filename, 0);

  // A3. ���ҵ�һ����Ƶ��
  v_idx = -1;
  for (i = 0; i < p_fmt_ctx->nb_streams; i++) {
    if (p_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      v_idx = i;
      printf("Find a video stream, index %d\n", v_idx);
      break;
    }
  }
  if (v_idx == -1) {
    printf("Cann't find a video stream\n");
    return -1;
  }

  // A5. Ϊ��Ƶ������������AVCodecContext

  // A5.1 ��ȡ����������AVCodecParameters
  p_codec_par = p_fmt_ctx->streams[v_idx]->codecpar;
  // A5.2 ��ȡ������
  p_codec = avcodec_find_decoder(p_codec_par->codec_id);
  if (p_codec == NULL) {
    printf("Cann't find codec!\n");
    return -1;
  }
  // A5.3 ����������AVCodecContext
  // A5.3.1 p_codec_ctx��ʼ��������ṹ�壬ʹ��p_codec��ʼ����Ӧ��ԱΪĬ��ֵ
  p_codec_ctx = avcodec_alloc_context3(p_codec);

  // A5.3.2 p_codec_ctx��ʼ����p_codec_par ==> p_codec_ctx����ʼ����Ӧ��Ա
  ret = avcodec_parameters_to_context(p_codec_ctx, p_codec_par);
  if (ret < 0) {
    printf("avcodec_parameters_to_context() failed %d\n", ret);
    return -1;
  }

  // A5.3.3 p_codec_ctx��ʼ����ʹ��p_codec��ʼ��p_codec_ctx����ʼ�����
  ret = avcodec_open2(p_codec_ctx, p_codec, NULL);
  if (ret < 0) {
    printf("avcodec_open2() failed %d\n", ret);
    return -1;
  }

  // A6. ����AVFrame
  // A6.1 ����AVFrame�ṹ��ע�Ⲣ������data buffer(��AVFrame.*data[])
  p_frm_raw = av_frame_alloc();
  p_frm_yuv = av_frame_alloc();

  // A6.2 ΪAVFrame.*data[]�ֹ����仺���������ڴ洢sws_scale()��Ŀ��֡��Ƶ����
  //     p_frm_raw��data_buffer��av_read_frame()���䣬��˲����ֹ�����
  //     p_frm_yuv��data_buffer�޴����䣬����ڴ˴��ֹ�����
  buf_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, p_codec_ctx->width,
                                      p_codec_ctx->height, 1);
  // buffer����Ϊp_frm_yuv����Ƶ���ݻ�����
  buffer = (uint8_t*)av_malloc(buf_size);
  // ʹ�ø��������趨p_frm_yuv->data��p_frm_yuv->linesize
  av_image_fill_arrays(p_frm_yuv->data,      // dst data[]
                       p_frm_yuv->linesize,  // dst linesize[]
                       buffer,               // src buffer
                       AV_PIX_FMT_YUV420P,   // pixel format
                       p_codec_ctx->width,   // width
                       p_codec_ctx->height,  // height
                       1                     // align
  );

  // A7. ��ʼ��SWS context�����ں���ͼ��ת��
  //     �˴���6������ʹ�õ���FFmpeg�е����ظ�ʽ���ԱȲο�ע��B4
  //     FFmpeg�е����ظ�ʽAV_PIX_FMT_YUV420P��ӦSDL�е����ظ�ʽSDL_PIXELFORMAT_IYUV
  //     ��������õ�ͼ��Ĳ���SDL֧�֣�������ͼ��ת���Ļ���SDL���޷�������ʾͼ���
  //     ��������õ�ͼ����ܱ�SDL֧�֣��򲻱ؽ���ͼ��ת��
  //     ����Ϊ�˱����㣬ͳһת��ΪSDL֧�ֵĸ�ʽAV_PIX_FMT_YUV420P==>SDL_PIXELFORMAT_IYUV
  sws_ctx = sws_getContext(p_codec_ctx->width,    // src width
                           p_codec_ctx->height,   // src height
                           p_codec_ctx->pix_fmt,  // src format
                           p_codec_ctx->width,    // dst width
                           p_codec_ctx->height,   // dst height
                           AV_PIX_FMT_YUV420P,    // dst format
                           SWS_BICUBIC,           // flags
                           NULL,                  // src filter
                           NULL,                  // dst filter
                           NULL                   // param
  );

  // B1. ��ʼ��SDL��ϵͳ��ȱʡ(�¼������ļ�IO���߳�)����Ƶ����Ƶ����ʱ��
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    printf("SDL_Init() failed: %s\n", SDL_GetError());
    return -1;
  }

  // B2. ����SDL���ڣ�SDL 2.0֧�ֶര��
  //     SDL_Window�����г���󵯳�����Ƶ���ڣ�ͬSDL 1.x�е�SDL_Surface
  screen = SDL_CreateWindow("Simplest ffmpeg player's Window",
                            SDL_WINDOWPOS_UNDEFINED,  // �����Ĵ���X����
                            SDL_WINDOWPOS_UNDEFINED,  // �����Ĵ���Y����
                            p_codec_ctx->width, p_codec_ctx->height,
                            SDL_WINDOW_OPENGL);

  if (screen == NULL) {
    printf("SDL_CreateWindow() failed: %s\n", SDL_GetError());
    return -1;
  }

  // B3. ����SDL_Renderer
  //     SDL_Renderer����Ⱦ��
  sdl_renderer = SDL_CreateRenderer(screen, -1, 0);

  // B4. ����SDL_Texture
  //     һ��SDL_Texture��Ӧһ֡YUV���ݣ�ͬSDL 1.x�е�SDL_Overlay
  //     �˴���2������ʹ�õ���SDL�е����ظ�ʽ���ԱȲο�ע��A7
  //     FFmpeg�е����ظ�ʽAV_PIX_FMT_YUV420P��ӦSDL�е����ظ�ʽSDL_PIXELFORMAT_IYUV
  sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_IYUV,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  p_codec_ctx->width, p_codec_ctx->height);

  sdl_rect.x = 0;
  sdl_rect.y = 0;
  sdl_rect.w = p_codec_ctx->width;
  sdl_rect.h = p_codec_ctx->height;

  p_packet = (AVPacket*)av_malloc(sizeof(AVPacket));
  // A8. ����Ƶ�ļ��ж�ȡһ��packet
  //     packet��������Ƶ֡����Ƶ֡���������ݣ�������ֻ�������Ƶ֡����Ƶ֡��������Ƶ���ݲ����ᱻ
  //     �ӵ����Ӷ�����������ṩ�����ܶ����Ϣ
  //     ������Ƶ��˵��һ��packetֻ����һ��frame
  //     ������Ƶ��˵������֡���̶��ĸ�ʽ��һ��packet�ɰ���������frame��
  //                   ����֡���ɱ�ĸ�ʽ��һ��packetֻ����һ��frame
  while (av_read_frame(p_fmt_ctx, p_packet) == 0) {
    if (p_packet->stream_index == v_idx)  // ��������Ƶ֡
    {
      // A9. ��Ƶ���룺packet ==> frame
      // A9.1
      // �������ι���ݣ�һ��packet������һ����Ƶ֡������Ƶ֡���˴���Ƶ֡�ѱ���һ���˵�
      ret = avcodec_send_packet(p_codec_ctx, p_packet);
      if (ret != 0) {
        printf("avcodec_send_packet() failed %d\n", ret);
        return -1;
      }
      // A9.2
      // ���ս�������������ݣ��˴�ֻ������Ƶ֡��ÿ�ν���һ��packet����֮����õ�һ��frame
      ret = avcodec_receive_frame(p_codec_ctx, p_frm_raw);
      if (ret != 0) {
        printf("avcodec_receive_frame() failed %d\n", ret);
        continue;
      }

      // A10. ͼ��ת����p_frm_raw->data ==> p_frm_yuv->data
      // ��Դͼ����һƬ���������򾭹��������µ�Ŀ��ͼ���Ӧ���򣬴����ͼ�����������������
      // plane: ��YUV��Y��U��V����plane��RGB��R��G��B����plane
      // slice: ͼ����һƬ�������У������������ģ�˳���ɶ������ײ����ɵײ�������
      // stride/pitch:
      // һ��ͼ����ռ���ֽ�����Stride=BytesPerPixel*Width+Padding��ע�����
      // AVFrame.*data[]: ÿ������Ԫ��ָ���Ӧplane
      // AVFrame.linesize[]: ÿ������Ԫ�ر�ʾ��Ӧplane��һ��ͼ����ռ���ֽ���
      sws_scale(sws_ctx,                                 // sws context
                (const uint8_t* const*)p_frm_raw->data,  // src slice
                p_frm_raw->linesize,                     // src stride
                0,                                       // src slice y
                p_codec_ctx->height,                     // src slice height
                p_frm_yuv->data,                         // dst planes
                p_frm_yuv->linesize                      // dst strides
      );

      // B5. ʹ���µ�YUV�������ݸ���SDL_Rect
      SDL_UpdateYUVTexture(sdl_texture,             // sdl texture
                           &sdl_rect,               // sdl rect
                           p_frm_yuv->data[0],      // y plane
                           p_frm_yuv->linesize[0],  // y pitch
                           p_frm_yuv->data[1],      // u plane
                           p_frm_yuv->linesize[1],  // u pitch
                           p_frm_yuv->data[2],      // v plane
                           p_frm_yuv->linesize[2]   // v pitch
      );

      // B6. ʹ���ض���ɫ��յ�ǰ��ȾĿ��
      SDL_RenderClear(sdl_renderer);
      // B7. ʹ�ò���ͼ������(texture)���µ�ǰ��ȾĿ��
      SDL_RenderCopy(sdl_renderer,  // sdl renderer
                     sdl_texture,   // sdl texture
                     NULL,          // src rect, if NULL copy texture
                     &sdl_rect      // dst rect
      );
      // B8. ִ����Ⱦ��������Ļ��ʾ
      SDL_RenderPresent(sdl_renderer);

      // B9. ����֡��Ϊ25FPS���˴�����׼ȷ��δ���ǽ������ĵ�ʱ��
      SDL_Delay(40);
    }
    av_packet_unref(p_packet);
  }

  SDL_Quit();
  sws_freeContext(sws_ctx);
  av_free(buffer);
  av_frame_free(&p_frm_yuv);
  av_frame_free(&p_frm_raw);
  avcodec_close(p_codec_ctx);
  avformat_close_input(&p_fmt_ctx);

  return 0;
}
