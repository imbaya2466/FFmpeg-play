

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}


#include "class1.h"



/*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
��Ϊ�����ӿڹ�����˽���������ѧϰ����Ҫϸ���о��ӿڹ�����
*/

/*

// ֡  ��������
void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[32];
  int y;

  // Open file
  sprintf(szFilename, "frame%d.ppm", iFrame);
  pFile = fopen(szFilename, "wb");
  if (pFile == NULL) return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  // ÿ��ÿ������3��8�ֽڴ���RGB��
  for (y = 0; y < height; y++)
    fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

  // Close file
  fclose(pFile);
}


int class1(const char *filename) {
  av_register_all();
  // must when be NULL avformat_alloc_context will alloc
  // AVFormatContext������һ������Ƶ��ʽ�Ļ��������������ļ���
  AVFormatContext *pFormatCtx = nullptr;
  // open file      handle filename format format (NULL is auto choose)
  if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    return -1;  // Couldn��t open file

  // Retrieve stream information
  // this function fill pFormatCtx->streams     ��һ����价����Ϣ
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1;  // Couldn��t find stream information
  // this function used to show infomation     �²�Ϊ���ݻ�����Ϣ�����ļ�
  av_dump_format(pFormatCtx, 0, filename, 0);

  // �������Ϣ    ͬʱ���й����Ľ�����
  AVCodecContext *pCodecCtx;
  // Find the first video stream
  int videoStream = -1;
  for (int i = 0; i < pFormatCtx->nb_streams; i++)
    if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStream = i;
      break;
    }
  if (videoStream == -1) {
    return -1;
  }
  // CODEC= COded+DECoded
  pCodecCtx = pFormatCtx->streams[videoStream]->codec;

  AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
  AVDictionary *optionsDict = NULL;
  if (pCodec == NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1;  // Codec not found
  }
  // Open codec  �򿪱������
  if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0)
    return -1;  // Could not open codec


  AVFrame *pFrame = NULL,*pFrameRGB = NULL;
  //������һ֡���ڴ� ���ڷ�Դ
  pFrame = av_frame_alloc();
  //  ����һ֡���ڴ� ���ڷ�ת�����
  pFrameRGB = av_frame_alloc();
  if (pFrameRGB == NULL) return -1;

  // ��ʹ������֡�ռ䣬������Ȼ��Ҫ�ռ������ת��ʱ�� raw ���ݣ�������
  // avpicture_get_size ���õ���Ҫ�Ŀռ䣬Ȼ���ֶ����䡣
  int numBytes =
      avpicture_get_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
  uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

  struct SwsContext *sws_ctx = sws_getContext(
      pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width,
      pCodecCtx->height, AV_PIX_FMT_BGR24, SWS_BILINEAR, NULL, NULL, NULL);


  // ����ʹ�� avpicture_fill �������·���Ļ�������֡��
  // AVPicture �ṹ���� AVFrame �ṹ���һ���Ӽ�
  // avpicture_fill������ptrָ���������䵽picture�ڣ�����û�п�����ֻ�ǽ�picture�ṹ�ڵ�dataָ��ָ����ptr�����ݡ�
  avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_BGR24,
                 pCodecCtx->width, pCodecCtx->height);

  // ͨ��������ȡ������Ƶ���� Ȼ����뵽֡���У�һ��һ֡����ˣ� ��ת����������
  int i = 0, frameFinished;
  AVPacket packet;//packet����������֡һ������
  while (av_read_frame(pFormatCtx, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == videoStream) {
      // Decode video frame
      //����Ϊ֡
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
      // Did we get a video frame?
      if (frameFinished) {
        // Convert the image from its native format to RGB
        sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
                  pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data,
                  pFrameRGB->linesize);

        // Save the frame to disk
        if (++i <= 5) {
          SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
        } else {
          av_free_packet(&packet);
          break;
        }
      }
    }

    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);
  }

  // Free the RGB image
  av_free(buffer);
  av_free(pFrameRGB);

  // Free the YUV frame
  av_free(pFrame);

  // Close the codec
  avcodec_close(pCodecCtx);

  // Close the video file
  avformat_close_input(&pFormatCtx);

  return 0;

}
*/