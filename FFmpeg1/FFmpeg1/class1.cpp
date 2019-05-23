

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}


#include "class1.h"



/*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
因为大量接口过期因此仅用于流程学习，不要细致研究接口功能了
*/

/*

// 帧  宽、高像素
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
  // 每行每像素是3个8字节代表RGB。
  for (y = 0; y < height; y++)
    fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

  // Close file
  fclose(pFile);
}


int class1(const char *filename) {
  av_register_all();
  // must when be NULL avformat_alloc_context will alloc
  // AVFormatContext代表着一个音视频格式的环境、包括输入文件流
  AVFormatContext *pFormatCtx = nullptr;
  // open file      handle filename format format (NULL is auto choose)
  if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0)
    return -1;  // Couldn’t open file

  // Retrieve stream information
  // this function fill pFormatCtx->streams     进一步填充环境信息
  if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    return -1;  // Couldn’t find stream information
  // this function used to show infomation     猜测为根据环境信息解析文件
  av_dump_format(pFormatCtx, 0, filename, 0);

  // 编解码信息    同时含有构建的解码器
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
  // Open codec  打开编解码器
  if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0)
    return -1;  // Could not open codec


  AVFrame *pFrame = NULL,*pFrameRGB = NULL;
  //　申请一帧的内存 用于放源
  pFrame = av_frame_alloc();
  //  申请一帧的内存 用于放转换后的
  pFrameRGB = av_frame_alloc();
  if (pFrameRGB == NULL) return -1;

  // 即使分配了帧空间，我们仍然需要空间来存放转换时的 raw 数据，我们用
  // avpicture_get_size 来得到需要的空间，然后手动分配。
  int numBytes =
      avpicture_get_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
  uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

  struct SwsContext *sws_ctx = sws_getContext(
      pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width,
      pCodecCtx->height, AV_PIX_FMT_BGR24, SWS_BILINEAR, NULL, NULL, NULL);


  // 我们使用 avpicture_fill 来关联新分配的缓冲区的帧。
  // AVPicture 结构体是 AVFrame 结构体的一个子集
  // avpicture_fill函数将ptr指向的数据填充到picture内，但并没有拷贝，只是将picture结构内的data指针指向了ptr的数据。
  avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_BGR24,
                 pCodecCtx->width, pCodecCtx->height);

  // 通过包来读取整个视频流， 然后解码到帧当中，一但一帧完成了， 将转换并保存它
  int i = 0, frameFinished;
  AVPacket packet;//packet包含完整的帧一个或多个
  while (av_read_frame(pFormatCtx, &packet) >= 0) {
    // Is this a packet from the video stream?
    if (packet.stream_index == videoStream) {
      // Decode video frame
      //解析为帧
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