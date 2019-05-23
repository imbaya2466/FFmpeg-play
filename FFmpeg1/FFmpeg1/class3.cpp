
/*
//������Ƶ��Ϊ�ӿڸĶ��޷����ţ��ȼ���ѧϰ
//֮��ο�����Ҷ����ϸʵ�֣�����������

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
  int size;  // size ��ʾ�� packet->size �еõ����ֽ���
  SDL_mutex *mutex;//������
  SDL_cond *cond;//��������
} PacketQueue;
//  SDL ����һ���������߳�����������Ƶ����ġ� 
PacketQueue audioq;

int quit = 0;  // ����֤��û�����ó����˳����ź�

// ��дһ����������ʼ������
void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}

// �Ѷ����ŵ����е���
int packet_queue_put(PacketQueue *q, AVPacket *pkt) {
  AVPacketList *pkt1;
  if (av_dup_packet(pkt) < 0) {
    return -1;
  }
  pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
  if (!pkt1) return -1;
  pkt1->pkt = *pkt;
  pkt1->next = NULL;

  // SDL_LockMutex()������ס������Ļ������������Ϳ�������������Ӷ�����
  SDL_LockMutex(q->mutex);

  if (!q->last_pkt)
    q->first_pkt = pkt1;
  else
    q->last_pkt->next = pkt1;
  q->last_pkt = pkt1;
  q->nb_packets++;
  q->size += pkt1->pkt.size;

  // Ȼ��
  // SDL_CondSignal()��ͨ��������������һ���źŸ����պ�����������ڵȴ��Ļ����������������Ѿ��������ˣ�Ȼ�����������
  SDL_CondSignal(q->cond);  //Ϊ�������ݲ�ȡ
  SDL_UnlockMutex(q->mutex);//Ϊ�˲�ͬʱ����
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
    } else if (!block) {//�Ƿ�������������ֱ�ӷ���0
      ret = 0;
      break;
    } else {
      //ѭ���ȴ���
      //SDL_CondWait()
      //����ҲΪ�������˽����������Ķ���Ȼ��ų������ڵõ��źź�ȥ����������
      SDL_CondWait(q->cond, q->mutex);
    }
  }
  SDL_UnlockMutex(q->mutex);
  return ret;
}

// audio_decode_frame()�������ݴ洢��һ���м仺���У���ͼ���ֽ�ת��Ϊ�������������ݲ�����ʱ���ṩ�����ǣ�
// ����������ʱ�����Ǳ���������ʹ�����Ժ����á�
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
      //һ����������в�ֹһ����Ƶ����Ҫ���úܶ��
      //len1 ��ʾ����ʹ�õ����ݵ��ڰ��еĴ�С��
      //data_size ��ʾʵ�ʷ��ص�ԭʼ��Ƶ���ݵĴ�С��
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

// �û����ݾ��Ǹ� SDL ��ָ�룬 stream ���Ǿ��ǽ�Ҫд��Ƶ���ݵĻ����������� len
// �ǻ������Ĵ�С ����Ƶ�豸��Ҫ��������ʱ���õĻص�����,�Ӷ������������ݡ�
void audio_callback(void *userdata, Uint8 *stream, int len) {
  AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
  int len1, audio_size;

  static uint8_t
      audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) /
                2];  // �����Ƶ����Ĵ�С��ffmpeg �����ǵ���Ƶ֡���ֵ�� 1.5
                     // �����Ը�����һ���ܺõĻ��塣
  static unsigned int audio_buf_size = 0;
  static unsigned int audio_buf_index = 0;

  while (len > 0) {
    //��Ҫ��ȡ������
    if (audio_buf_index >= audio_buf_size) {
     
      audio_size = audio_decode_frame(aCodecCtx, audio_buf, audio_buf_size);
      if (audio_size < 0) {
       
        //����ʧ�ܾ��������
        audio_buf_size = 1024;  // arbitrary?
        memset(audio_buf, 0, audio_buf_size);
      } else {
        audio_buf_size = audio_size;
      }
      audio_buf_index = 0;
    }
    //���������У���sdl���
    len1 = audio_buf_size - audio_buf_index;
    if (len1 > len) len1 = len;
    memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
    len -= len1;
    stream += len1;
    audio_buf_index += len1;//������ʹ���±꣬
  }
}

int class3(const char* filename) { 

  // Register all formats and codecs
  // av_register_all
  // ֻ��Ҫ����һ�Σ�����ע�����п��õ��ļ���ʽ�ͱ����⣬���ļ�����ʱ���ǽ��Զ�ƥ����Ӧ�ı����⡣
  av_register_all();

  // ��ʼ�� SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

  // Open video file
  // �Ӵ���ĵڶ�����������ļ�·��������������ȡ�ļ�ͷ��Ϣ��������Ϣ������
  // pFormatCtx �ṹ�嵱�С� ��������������������ֱ��ǣ�
  // ָ���ļ���ʽ����ʽ��ѡ�����������Ϊ NULL �� 0 ʱ��libavformat
  // ���Զ������Щ������
  AVFormatContext *pFormatCtx = NULL;
  if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    return -1;  // Couldn't open file

  // Retrieve stream information
  // �õ�����Ϣ ���stream��Ϣ
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1;  // Couldn't find stream information

  // ����Ϣ��ӡ����
  av_dump_format(pFormatCtx, 0, filename, 0);

  int i, videoStream, audioStream;
  videoStream = -1;
  audioStream = -1;
  // ��ȡ��Ƶ��Ƶ��
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

  // SDL ������Ƶ�ķ����������ģ���Ҫ���ú���Ƶ��ص�ѡ������ʣ��� SDL
  // �ṹ���������Ƶ�ʡ�freq������ͨ������ ����������
  // ��������һ���ص��������û����ݡ�����ʼ������Ƶ�� SDL
  // ������ص��ûص�������Ҫ���������������������
  // ��һ���ض��������ֽ������档������Щ��Ϣд�� SDL_AudioSpec �ṹ�������
  // ���� SDL_OpenAudio()�����Ὺ�������� ���ͷ�����һ�� AudioSpec
  // �ṹ������ǡ�
  // ��Ƶ����������
  AVCodecContext *aCodecCtx = NULL;
  aCodecCtx = pFormatCtx->streams[audioStream]->codec;

  // Set audio settings from codec info
  SDL_AudioSpec wanted_spec, spec;
  wanted_spec.freq = aCodecCtx->sample_rate;  // ������
  wanted_spec.format = AUDIO_S16SYS;  
        // ���ݸ�ʽ����S16SYS���еġ� S�����з��ŵ���˼�� 16
        // ����˼��ÿ�������� 16λ����
        // SYS����ʾ�ֽ�˳���յ�ǰϵͳ��˳��
  wanted_spec.channels = aCodecCtx->channels;  // ͨ����
  wanted_spec.silence = 0;  // ����������ʾ������ֵ�� ��Ϊ�������з��ŵģ����Ծ�����ֵͨ��Ϊ 0��
  wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;  // ��Ƶ���棬�����������õ� SDL
                              // ���������Ƶ����ʱ����Ӧ�ø�����������
  wanted_spec.callback =
      audio_callback;  // ����Ƶ�豸��Ҫ��������ʱ���õĻص�����
  wanted_spec.userdata =
      aCodecCtx;  // �ص�����

  //����ֵ��ʾִ�н�� spec��ʾ����ʵ����Ƶָ��
  //��Ϊ�Ѿ����˲�����������������ٶ����ʼ�հ���Ƶ��ϢƵ�ʲ���
  if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
    fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
    return -1;
  }
  // ��ȡ��Ƶ�Ľ�����
  AVCodec *aCodec = NULL;
  aCodec = avcodec_find_decoder(aCodecCtx->codec_id);
  if (!aCodec) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;
  }
  // ����Ƶ������
  AVDictionary *audioOptionsDict = NULL;
  avcodec_open2(aCodecCtx, aCodec, &audioOptionsDict);

  // ��ʼ����Ƶ���ݶ���
  packet_queue_init(&audioq);
  SDL_PauseAudio(0);

  // Get a pointer to the codec context for the video stream
  // pCodecCtx
  // ��������������õı�����������Ϣ������������Ҫͨ��������ض��Ľ�����Ȼ�����
  AVCodecContext *pCodecCtx = NULL;
  pCodecCtx = pFormatCtx->streams[videoStream]->codec;

  // Find the decoder for the video stream
  // ��ȡ��Ƶ������
  AVCodec *pCodec = NULL;
  pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
  if (pCodec == NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;  // Codec not found
  }
  // Open codec
  // ����Ƶ������
  AVDictionary *videoOptionsDict = NULL;
  if (avcodec_open2(pCodecCtx, pCodec, &videoOptionsDict) < 0)
    return -1;  // Could not open codec
  // if you still try to open it using avcodec_find_encoder it will open libfaac
  // only. Allocate video frame ������Ƶ֡�ڴ�����
  AVFrame *pFrame = NULL;
  pFrame = av_frame_alloc();

  // Make a screen to put our video

  SDL_Surface *screen = NULL;

#ifndef __DARWIN__
  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
  // ����һ���������Ϳ����Ļ
  // ��������������Ļ����ɫ���--0 ��ʾʹ�õ�ǰ��Ļ����ɫ���
  // ���ĸ������Ǳ�ʾ��������
  // http://www.cnblogs.com/landmark/archive/2012/05/04/2476213.html
  screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
  if (!screen) {
    fprintf(stderr, "SDL: could not set video mode - exiting\n");
    exit(1);
  }

  // Allocate a place to put our YUV image on that screen
  // ������������Ļ������һ�� YUV overlay�����԰���Ƶ�Ž�ȥ�ˡ�
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
    // �������Ƶ���ݣ���ô�ͽ��벢��ʾ
    if (packet.stream_index == videoStream) {
      // Decode video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

      // Did we get a video frame?
      if (frameFinished) {
        // Ҫ��ͼ����ס����Ϊ����Ҫ������д����������һ�������Ժ�������ĺ�ϰ�ߡ�
        SDL_LockYUVOverlay(bmp);
        // AVPicture�ṹ����һ������ָ��ָ��һ�����ĸ�Ԫ�ص�����ָ�룬
        // ��Ϊ���Ǵ���� YUV420P ֻ����ͨ��������ֻҪ�����������ݡ�
        AVPicture pict;
        pict.data[0] = bmp->pixels[0];
        pict.data[1] = bmp->pixels[2];
        pict.data[2] = bmp->pixels[1];

        pict.linesize[0] = bmp->pitches[0];
        pict.linesize[1] = bmp->pitches[2];
        pict.linesize[2] = bmp->pitches[1];

        // Convert the image into YUV format that SDL uses
        //����ͼƬת����YUV��ʽ
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
        av_free_packet(&packet);
      }
    } else if (packet.stream_index == audioStream) {
      // �������Ƶ�ͷ��õ���Ƶ����
      packet_queue_put(&audioq, &packet);
    } else {
      av_free_packet(&packet);
    }
    // Free the packet that was allocated by av_read_frame
    // �¼�ϵͳ�� SDL ������Ϊ����������꾭�����߸���һ���źŵ�ʱ���������
    // һ���¼��� ����ͨ�������Щ�¼���������ص��û����룬 ����Ҳ������ SDL
    // �¼�ϵͳ�����¼������� SDL ����д�������
    // ���ʱ���ر����ã����ǽ����ڽ̳� 4 �������ԡ�
    SDL_PollEvent(&event);
    switch (event.type) {
      case SDL_QUIT:
        quit = 1;  // ���ƶ����˳�
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

