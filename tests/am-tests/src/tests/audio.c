#include <amtest.h>

#define AUDIO_FREQ 44100
#define FPS 60

#define RESET     "\033[0m"
#define MAGENTA   "\033[35m"
#define RED       "\033[31m"

#define SHOW(color, fmt, ...) printf(color fmt RESET, ##__VA_ARGS__)

#include <stdio.h>

void show_progress(int progress, int total) {
  static int last_pos = -1;
  static int last_percent = -1;
  int percent = (progress * 100) / total;
  int bar_width = 50;

  int pos = (progress * bar_width) / total;

  if (pos != last_pos || percent != last_percent) {
    SHOW(MAGENTA, "\r[");
    for (int i = 0; i < bar_width; i++) {
      if (i < pos) {
        SHOW(MAGENTA, "#");
      } else {
        SHOW(MAGENTA, " ");
      }
    }
    SHOW(MAGENTA, "] %d%%", percent);

    last_pos = pos;
    last_percent = percent;
  }
}

void audio_test() {
  if (!io_read(AM_AUDIO_CONFIG).present) {
    SHOW(RED, "WARNING: %s does not support audio\n", TOSTRING(__ARCH__));
    return;
  }

  io_write(AM_AUDIO_CTRL, AUDIO_FREQ, 1, 1024);

  extern uint8_t audio_payload, audio_payload_end;
  uint32_t audio_len = &audio_payload_end - &audio_payload;
  int nplay = 0;
  Area sbuf;
  sbuf.start = &audio_payload;

  uint64_t now = io_read(AM_TIMER_UPTIME).us;
  uint32_t frame_audio_size =
      AUDIO_FREQ / FPS * sizeof(int16_t);

  while (nplay < audio_len) {
    int len = (audio_len - nplay > frame_audio_size ? frame_audio_size
                                                    : audio_len - nplay);
    sbuf.end = sbuf.start + len;
    io_write(AM_AUDIO_PLAY, sbuf);

    sbuf.start += len;
    nplay += len;
    show_progress(nplay, audio_len);

    uint64_t next = now + (1000 * 1000 / FPS);
    while (io_read(AM_TIMER_UPTIME).us < next);
    now = next;
  }

  // wait until the audio finishes
  while (io_read(AM_AUDIO_STATUS).count > 0);
}
