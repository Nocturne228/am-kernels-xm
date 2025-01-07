#include <am.h>
#include <stdio.h>
#include <klib-macros.h>
#include <stdlib.h>

#define FPS 30
#define COLOR_WHITE 0xFFFFFF
#define COLOR_BLACK 0x000000

typedef struct {
  uint8_t pixel[VIDEO_ROW * VIDEO_COL / 8];
} frame_t;

static void sleep_until(uint64_t next) {
  while (io_read(AM_TIMER_UPTIME).us < next) ;
}

static uint8_t getbit(uint8_t *p, int idx) {
  int byte_idx = idx / 8;
  int bit_idx = idx % 8;
  bit_idx = 7 - bit_idx;
  uint8_t byte = p[byte_idx];
  uint8_t bit = (byte >> bit_idx) & 1;
  return bit;
}

int main() {
  extern uint8_t video_payload, video_payload_end;
  extern uint8_t audio_payload, audio_payload_end;
  int audio_len = 0, audio_left = 0;
  Area sbuf;

  ioe_init();

  // 获取屏幕配置
  AM_GPU_CONFIG_T info = io_read(AM_GPU_CONFIG);
  int screen_w = info.width;
  int screen_h = info.height;

  // 计算缩放比例
  int scale_x = screen_w / VIDEO_COL;
  int scale_y = screen_h / VIDEO_ROW;
  int scale = (scale_x < scale_y) ? scale_x : scale_y; // 取小的以确保完全显示
  if (scale < 1) scale = 1;

  // 计算实际显示区域
  int disp_w = VIDEO_COL * scale;
  int disp_h = VIDEO_ROW * scale;
  // 计算居中偏移
  int offset_x = (screen_w - disp_w) / 2;
  int offset_y = (screen_h - disp_h) / 2;

  // 创建行缓冲区
  uint32_t *fb = malloc(disp_w * sizeof(uint32_t));
  
  frame_t *f = (void *)&video_payload;
  frame_t *fend = (void *)&video_payload_end;

  bool has_audio = io_read(AM_AUDIO_CONFIG).present;
  if (has_audio) {
    io_write(AM_AUDIO_CTRL, AUDIO_FREQ, AUDIO_CHANNEL, 1024);
    audio_left = audio_len = &audio_payload_end - &audio_payload;
    sbuf.start = &audio_payload;
  }
  
  uint64_t now = io_read(AM_TIMER_UPTIME).us;
  
  // 先清屏为黑色
  for (int y = 0; y < screen_h; y++) {
    for (int x = 0; x < screen_w; x++) {
      fb[x] = COLOR_BLACK;
    }
    io_write(AM_GPU_FBDRAW, 0, y, fb, screen_w, 1, false);
  }
  
  for (; f < fend; f++) {
    // 逐行绘制
    for (int y = 0; y < VIDEO_ROW; y++) {
      // 准备这一行的缩放后像素
      for (int x = 0; x < VIDEO_COL; x++) {
        uint8_t p = getbit(f->pixel, y * VIDEO_COL + x);
        uint32_t color = p ? COLOR_BLACK : COLOR_WHITE;
        // 填充缩放后的像素
        for (int sx = 0; sx < scale; sx++) {
          fb[x * scale + sx] = color;
        }
      }
      // 绘制放大后的行（重复scale次以实现垂直方向的缩放）
      for (int sy = 0; sy < scale; sy++) {
        io_write(AM_GPU_FBDRAW, offset_x, offset_y + y * scale + sy, 
                fb, disp_w, 1, false);
      }
    }
    // 刷新屏幕
    io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);

    if (has_audio) {
      int should_play = (AUDIO_FREQ / FPS) * sizeof(int16_t);
      if (should_play > audio_left) should_play = audio_left;
      while (should_play > 0) {
        int len = (should_play > 4096 ? 4096 : should_play);
        sbuf.end = sbuf.start + len;
        io_write(AM_AUDIO_PLAY, sbuf);
        sbuf.start += len;
        should_play -= len;
      }
      audio_left -= should_play;
    }

    uint64_t next = now + (1000 * 1000 / FPS);
    sleep_until(next);
    now = next;
  }
  
  free(fb);
  return 0;
}