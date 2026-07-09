#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sound/asound.h>

#define PLAY_RATE 48000U
#define PLAY_CH 2U
#define PERIOD 1024UL
#define BUFFER 4096UL

struct wav_info {
    uint16_t channels;
    uint32_t rate;
    uint16_t bits;
    uint32_t data_offset;
    uint32_t data_bytes;
};

static void mask_any(struct snd_mask *m)
{
    for (size_t i = 0; i < sizeof(m->bits) / sizeof(m->bits[0]); ++i)
        m->bits[i] = 0xffffffffU;
}

static void mask_one(struct snd_mask *m, unsigned int v)
{
    memset(m, 0, sizeof(*m));
    m->bits[v >> 5] |= 1U << (v & 31);
}

static void interval_any(struct snd_interval *i)
{
    memset(i, 0, sizeof(*i));
    i->min = 0;
    i->max = 0xffffffffU;
}

static void interval_one(struct snd_interval *i, unsigned int v)
{
    memset(i, 0, sizeof(*i));
    i->min = v;
    i->max = v;
    i->integer = 1;
}

static struct snd_mask *pm(struct snd_pcm_hw_params *p, int param)
{
    return &p->masks[param - SNDRV_PCM_HW_PARAM_FIRST_MASK];
}

static struct snd_interval *pii(struct snd_pcm_hw_params *p, int param)
{
    return &p->intervals[param - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}

static int set_hw(int fd, const char *name)
{
    struct snd_pcm_hw_params p;
    memset(&p, 0, sizeof(p));
    for (size_t i = 0; i < sizeof(p.masks) / sizeof(p.masks[0]); ++i)
        mask_any(&p.masks[i]);
    for (size_t i = 0; i < sizeof(p.intervals) / sizeof(p.intervals[0]); ++i)
        interval_any(&p.intervals[i]);

    mask_one(pm(&p, SNDRV_PCM_HW_PARAM_ACCESS), SNDRV_PCM_ACCESS_RW_INTERLEAVED);
    mask_one(pm(&p, SNDRV_PCM_HW_PARAM_FORMAT), SNDRV_PCM_FORMAT_S16_LE);
    mask_one(pm(&p, SNDRV_PCM_HW_PARAM_SUBFORMAT), SNDRV_PCM_SUBFORMAT_STD);
    interval_one(pii(&p, SNDRV_PCM_HW_PARAM_CHANNELS), PLAY_CH);
    interval_one(pii(&p, SNDRV_PCM_HW_PARAM_RATE), PLAY_RATE);
    interval_one(pii(&p, SNDRV_PCM_HW_PARAM_PERIOD_SIZE), PERIOD);
    interval_one(pii(&p, SNDRV_PCM_HW_PARAM_BUFFER_SIZE), BUFFER);
    p.rmask = 0xffffffffU;

    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_REFINE, &p) < 0) {
        perror("HW_REFINE");
        fprintf(stderr, "device=%s\n", name);
        return -1;
    }
    if (ioctl(fd, SNDRV_PCM_IOCTL_HW_PARAMS, &p) < 0) {
        perror("HW_PARAMS");
        fprintf(stderr, "device=%s\n", name);
        return -1;
    }

    struct snd_pcm_sw_params sw;
    memset(&sw, 0, sizeof(sw));
    sw.avail_min = PERIOD;
    sw.start_threshold = PERIOD;
    sw.stop_threshold = BUFFER;
    sw.boundary = 0x40000000UL;
    if (ioctl(fd, SNDRV_PCM_IOCTL_SW_PARAMS, &sw) < 0) {
        perror("SW_PARAMS");
        return -1;
    }
    if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) {
        perror("PREPARE");
        return -1;
    }
    return 0;
}

static int write_frames(int fd, int16_t *buf, unsigned long frames)
{
    struct snd_xferi x;
    x.result = 0;
    x.buf = buf;
    x.frames = frames;
    if (ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &x) < 0) {
        if (errno == EPIPE) {
            ioctl(fd, SNDRV_PCM_IOCTL_PREPARE);
            return 0;
        }
        perror("WRITEI_FRAMES");
        return -1;
    }
    return (int)x.result;
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int parse_wav(FILE *fp, struct wav_info *wi)
{
    uint8_t h[12];
    if (fread(h, 1, sizeof(h), fp) != sizeof(h))
        return -1;
    if (memcmp(h, "RIFF", 4) != 0 || memcmp(h + 8, "WAVE", 4) != 0)
        return -1;

    memset(wi, 0, sizeof(*wi));
    for (;;) {
        uint8_t ch[8];
        long chunk_data_pos;
        if (fread(ch, 1, sizeof(ch), fp) != sizeof(ch))
            break;
        uint32_t size = rd32(ch + 4);
        chunk_data_pos = ftell(fp);
        if (memcmp(ch, "fmt ", 4) == 0) {
            uint8_t fmt[32];
            if (size < 16 || size > sizeof(fmt))
                return -1;
            if (fread(fmt, 1, size, fp) != size)
                return -1;
            if (rd16(fmt) != 1)
                return -1;
            wi->channels = rd16(fmt + 2);
            wi->rate = rd32(fmt + 4);
            wi->bits = rd16(fmt + 14);
        } else if (memcmp(ch, "data", 4) == 0) {
            wi->data_offset = (uint32_t)chunk_data_pos;
            wi->data_bytes = size;
            if (fseek(fp, size, SEEK_CUR) != 0)
                return -1;
        } else {
            if (fseek(fp, size, SEEK_CUR) != 0)
                return -1;
        }
        if (size & 1)
            fseek(fp, 1, SEEK_CUR);
        if (wi->channels && wi->rate && wi->bits && wi->data_offset && wi->data_bytes)
            break;
    }
    if ((wi->channels != 1 && wi->channels != 2) || wi->bits != 16 || wi->rate < 8000 || wi->rate > 96000)
        return -1;
    return 0;
}

static int16_t clamp_s16(float v)
{
    if (v > 32767.0f)
        return 32767;
    if (v < -32768.0f)
        return -32768;
    return (int16_t)v;
}

static int16_t sample_at(const int16_t *samples, const struct wav_info *wi, uint32_t frame, unsigned int channel)
{
    uint32_t frames = wi->data_bytes / (wi->channels * 2U);
    if (frame >= frames)
        frame = frames - 1;
    if (wi->channels == 1)
        channel = 0;
    return samples[frame * wi->channels + channel];
}

int main(int argc, char **argv)
{
    const char *wav_path = argc > 1 ? argv[1] : "/userdata/voice-assistant/last_reply.wav";
    const char *play_dev = argc > 2 ? argv[2] : "/dev/snd/pcmC1D0p";
    float gain = argc > 3 ? (float)atof(argv[3]) : 1.0f;
    if (gain <= 0.0f || gain > 8.0f) {
        fprintf(stderr, "gain must be >0 and <=8\n");
        return 2;
    }

    FILE *fp = fopen(wav_path, "rb");
    if (!fp) {
        perror(wav_path);
        return 1;
    }
    struct wav_info wi;
    if (parse_wav(fp, &wi) < 0) {
        fprintf(stderr, "unsupported wav: need PCM S16_LE mono/stereo\n");
        fclose(fp);
        return 1;
    }
    int16_t *samples = malloc(wi.data_bytes);
    if (!samples) {
        perror("malloc");
        fclose(fp);
        return 1;
    }
    if (fseek(fp, wi.data_offset, SEEK_SET) != 0 ||
        fread(samples, 1, wi.data_bytes, fp) != wi.data_bytes) {
        perror("read wav data");
        free(samples);
        fclose(fp);
        return 1;
    }
    fclose(fp);

    int play = open(play_dev, O_RDWR);
    if (play < 0) {
        perror(play_dev);
        free(samples);
        return 1;
    }
    if (set_hw(play, play_dev) < 0) {
        close(play);
        free(samples);
        return 1;
    }

    uint32_t src_frames = wi.data_bytes / (wi.channels * 2U);
    uint64_t out_frames = ((uint64_t)src_frames * PLAY_RATE + wi.rate - 1U) / wi.rate;
    int16_t *out = calloc(PERIOD * PLAY_CH, sizeof(int16_t));
    if (!out) {
        perror("calloc");
        close(play);
        free(samples);
        return 1;
    }

    fprintf(stderr, "play: %s %uHz %uch -> %s %uHz stereo, frames=%llu, gain %.2f\n",
            wav_path, wi.rate, wi.channels, play_dev, PLAY_RATE,
            (unsigned long long)out_frames, gain);

    uint64_t produced = 0;
    while (produced < out_frames) {
        unsigned long todo = PERIOD;
        if ((uint64_t)todo > out_frames - produced)
            todo = (unsigned long)(out_frames - produced);
        for (unsigned long f = 0; f < todo; ++f) {
            uint64_t num = (produced + f) * (uint64_t)wi.rate;
            uint32_t idx = (uint32_t)(num / PLAY_RATE);
            float frac = (float)(num % PLAY_RATE) / (float)PLAY_RATE;
            for (unsigned int ch = 0; ch < PLAY_CH; ++ch) {
                int16_t a = sample_at(samples, &wi, idx, ch);
                int16_t b = sample_at(samples, &wi, idx + 1U, ch);
                float y = ((float)a + ((float)b - (float)a) * frac) * gain;
                out[f * PLAY_CH + ch] = clamp_s16(y);
            }
        }
        int written = write_frames(play, out, todo);
        if (written < 0)
            break;
        produced += todo;
    }

    free(out);
    close(play);
    free(samples);
    return produced == out_frames ? 0 : 1;
}
