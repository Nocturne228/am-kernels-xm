#include <amtest.h>

#define AUDIO_FREQ 44100 // 音频采样率
#define FRAME_RATE 30    // 每秒帧数

void audio_test() {
  if (!io_read(AM_AUDIO_CONFIG).present) {
    printf("WARNING: %s does not support audio\n", TOSTRING(__ARCH__));
    return;
  }

  io_write(AM_AUDIO_CTRL, AUDIO_FREQ, 1, 1024);

  extern uint8_t audio_payload, audio_payload_end;
  uint32_t audio_len = &audio_payload_end - &audio_payload;
  int nplay = 0;
  Area sbuf;
  sbuf.start = &audio_payload;

  uint64_t now = io_read(AM_TIMER_UPTIME).us;
  uint32_t frame_audio_size = AUDIO_FREQ / FRAME_RATE * sizeof(int16_t); // 每帧需要播放的音频数据大小

  while (nplay < audio_len) {
    // 计算本帧需要播放的音频长度
    int len = (audio_len - nplay > frame_audio_size ? frame_audio_size : audio_len - nplay);
    sbuf.end = sbuf.start + len;
    io_write(AM_AUDIO_PLAY, sbuf);

    sbuf.start += len;
    nplay += len;
    printf("Already play %d/%d bytes of data\n", nplay, audio_len);

    // 等待下一帧时间
    uint64_t next = now + (1000 * 1000 / FRAME_RATE); // 下一帧的时间戳
    while (io_read(AM_TIMER_UPTIME).us < next);       // 等待至下一帧时间
    now = next;                                       // 更新当前时间
  }

  // wait until the audio finishes
  while (io_read(AM_AUDIO_STATUS).count > 0);
}
