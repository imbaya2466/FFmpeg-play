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
  // ֻ��Ҫ����һ�Σ�����ע�����п��õ��ļ���ʽ�ͱ����⣬���ļ�����ʱ���ǽ��Զ�ƥ����Ӧ�ı����⡣
  av_register_all();

  // ��ʼ�� SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

   AVFormatContext *pFormatCtx = NULL;
  // Open video file
  // �Ӵ���ĵڶ�����������ļ�·��������������ȡ�ļ�ͷ��Ϣ��������Ϣ������
  // pFormatCtx �ṹ�嵱�С� ��������������������ֱ��ǣ�
  // ָ���ļ���ʽ����ʽ��ѡ�����������Ϊ NULL �� 0 ʱ��libavformat
  // ���Զ������Щ������
  if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    return -1;  // Couldn't open file

  // Retrieve stream information
  // �õ�����Ϣ  ����� pFormatCtx->streams ����Ϣ
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1;  // Couldn't find stream information

  // ����Ϣ��ӡ������
  av_dump_format(pFormatCtx, 0, filename, 0);

  // Find the first video stream
  // ��ȡ��һ����Ƶ��
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
  // ��������������õı�����������Ϣ������������Ҫͨ��������ض��Ľ�����Ȼ�������
  pCodecCtx = pFormatCtx->streams[videoStream]->codec;

  // Find the decoder for the video stream
  // Ϊ��Ƶ����ȡ�ض��Ľ�������
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
  // ������Ҫ�ڴ�ռ�洢һ֡����
  AVFrame *pFrame = av_frame_alloc();


  // SDL ����ʾͼ���������� surface
  SDL_Surface *screen = NULL;
  // Make a screen to put our video
#ifndef __DARWIN__

  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
  // ����һ���������Ϳ����Ļ
  // ��������������Ļ����ɫ���--0 ��ʾʹ�õ�ǰ��Ļ����ɫ���
  // ���ĸ������Ǳ�ʾ��������
  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
  if (!screen) {
    fprintf(stderr, "SDL: could not set video mode - exiting\n");
    exit(1);
  }

  // Allocate a place to put our YUV image on that screen
  // ������������Ļ������һ�� YUV overlay�����԰���Ƶ�Ž�ȥ�ˡ�
  SDL_Overlay *bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,
                             SDL_YV12_OVERLAY, screen);

  //������ĳ������ظ�ʽ�������㷨������˲��������Ų���
  struct SwsContext *sws_ctx =  sws_getContext(
      pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width,
      pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL);


  AVPacket packet;
  int frameFinished = -1;
  SDL_Rect rect;
  SDL_Event event;

  //��һ֡  ÿ����ʾ����һ֡
  while (av_read_frame(pFormatCtx, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == videoStream) {
      // Decode video frame
      // ������Ƶ֡����
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

      // Did we get a video frame?
      // �����Ƿ��ȡ��Ƶ֡
      if (frameFinished) {
        // Ҫ��ͼ����ס����Ϊ����Ҫ������д����������һ�������Ժ�������ĺ�ϰ�ߡ�
        SDL_LockYUVOverlay(bmp);

        // AVPicture�ṹ����һ������ָ��ָ��һ�����ĸ�Ԫ�ص�����ָ�룬
        // ��Ϊ���Ǵ���� YUV420P ֻ����ͨ��������ֻҪ�����������ݡ�
        AVPicture pict;
        // dataΪ��������  һ֡�е�YUV�Ƿֱ������洢��
        pict.data[0] = bmp->pixels[0];//Y
        pict.data[1] = bmp->pixels[2];//U
        pict.data[2] = bmp->pixels[1];//V
        //�������ݿ��
        pict.linesize[0] = bmp->pitches[0];
        pict.linesize[1] = bmp->pitches[2];
        pict.linesize[2] = bmp->pitches[1];

        // Convert the image into YUV format that SDL uses
        //����ͼƬת����YUV��ʽ
        // ��ʽ��Ϣ  ��������ÿ��ͨ��ָ�롢ÿ��ͨ�����ֽ�����������ʼ�С���������С����ͨ����Ϣ
        sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
                  pFrame->linesize, 0, pCodecCtx->height, pict.data,
                  pict.linesize);

        SDL_UnlockYUVOverlay(bmp);

        // ������Ȼ��Ҫ���� SDL ��ʾ�Ѿ��Ž�ȥ�����ݣ� Ҫ����һ��������Ӱλ�á�
        // ��ȡ� �߶ȡ� ���ű����ľ��β���
        rect.x = 0;
        rect.y = 0;
        rect.w = pCodecCtx->width;
        rect.h = pCodecCtx->height;
        SDL_DisplayYUVOverlay(bmp, &rect);
      }
    }

    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);

    // �¼�ϵͳ�� SDL ������Ϊ����������꾭�����߸���һ���źŵ�ʱ���������
    // һ���¼��� ����ͨ�������Щ�¼���������ص��û����룬 ����Ҳ������ SDL
    // �¼�ϵͳ�����¼�
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
