#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sound/asound.h>

#define CAP_RATE 48000U
#define OUT_RATE 16000U
#define CAP_CH 2U
#define OUT_CH 1U
#define PERIOD 1024UL
#define BUFFER 4096UL
#define WARMUP_FRAMES (CAP_RATE / 4U)

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
    interval_one(pii(&p, SNDRV_PCM_HW_PARAM_CHANNELS), CAP_CH);
    interval_one(pii(&p, SNDRV_PCM_HW_PARAM_RATE), CAP_RATE);
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
        fprintf(stderr, "device=%s\n", name);
        return -1;
    }
    if (ioctl(fd, SNDRV_PCM_IOCTL_PREPARE) < 0) {
        perror("PREPARE");
        return -1;
    }
    return 0;
}

static int read_frames(int fd, int16_t *buf, unsigned long frames)
{
    struct snd_xferi x;
    x.result = 0;
    x.buf = buf;
    x.frames = frames;
    if (ioctl(fd, SNDRV_PCM_IOCTL_READI_FRAMES, &x) < 0) {
        if (errno == EPIPE) {
            ioctl(fd, SNDRV_PCM_IOCTL_PREPARE);
            return 0;
        }
        perror("READI_FRAMES");
        return -1;
    }
    return (int)x.result;
}

static void put_u16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}

static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

static int write_wav_header(FILE *fp, uint32_t data_bytes)
{
    uint8_t h[44];
    memset(h, 0, sizeof(h));
    memcpy(h + 0, "RIFF", 4);
    put_u32le(h + 4, 36U + data_bytes);
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    put_u32le(h + 16, 16);
    put_u16le(h + 20, 1);
    put_u16le(h + 22, OUT_CH);
    put_u32le(h + 24, OUT_RATE);
    put_u32le(h + 28, OUT_RATE * OUT_CH * 2U);
    put_u16le(h + 32, OUT_CH * 2U);
    put_u16le(h + 34, 16);
    memcpy(h + 36, "data", 4);
    put_u32le(h + 40, data_bytes);
    return fwrite(h, 1, sizeof(h), fp) == sizeof(h) ? 0 : -1;
}

static int abs_s16(int16_t v)
{
    return v == INT16_MIN ? 32768 : (v < 0 ? -v : v);
}

static int16_t clamp_s16(float v)
{
    if (v > 32767.0f)
        return 32767;
    if (v < -32768.0f)
        return -32768;
    return (int16_t)v;
}

static float soft_clip(float v)
{
    const float knee = 22000.0f;
    const float limit = 31000.0f;
    float sign = v < 0.0f ? -1.0f : 1.0f;
    float a = v < 0.0f ? -v : v;
    if (a <= knee)
        return v;
    a = knee + (a - knee) * 0.25f;
    if (a > limit)
        a = limit;
    return sign * a;
}

int main(int argc, char **argv)
{
    const char *out_path = argc > 1 ? argv[1] : "/userdata/voice-assistant/last_input_16k.wav";
    int seconds = argc > 2 ? atoi(argv[2]) : 5;
    const char *cap_dev = argc > 3 ? argv[3] : "/dev/snd/pcmC0D0c";
    float gain = argc > 4 ? (float)atof(argv[4]) : 8.0f;

    if (seconds <= 0 || seconds > 60) {
        fprintf(stderr, "seconds must be 1..60\n");
        return 2;
    }
    if (gain <= 0.0f || gain > 128.0f) {
        fprintf(stderr, "gain must be >0 and <=128\n");
        return 2;
    }

    int cap = open(cap_dev, O_RDWR);
    if (cap < 0) {
        perror(cap_dev);
        return 1;
    }
    FILE *fp = fopen(out_path, "wb+");
    if (!fp) {
        perror(out_path);
        close(cap);
        return 1;
    }
    if (write_wav_header(fp, 0) < 0) {
        perror("write wav header");
        fclose(fp);
        close(cap);
        return 1;
    }
    if (set_hw(cap, cap_dev) < 0) {
        fclose(fp);
        close(cap);
        return 1;
    }

    int16_t *in = calloc(PERIOD * CAP_CH, sizeof(int16_t));
    int16_t out[PERIOD];
    if (!in) {
        perror("calloc");
        fclose(fp);
        close(cap);
        return 1;
    }

    uint32_t target_out_samples = (uint32_t)seconds * OUT_RATE;
    unsigned long done = 0;
    uint32_t out_samples = 0;
    int input_peak = 0;
    int output_peak = 0;
    int xruns = 0;
    float hp_prev_x = 0.0f;
    float hp_prev_y = 0.0f;
    float decim_sum = 0.0f;
    int decim_count = 0;

    fprintf(stderr, "record: %s -> %s, %d sec, gain %.2f, %uHz stereo to %uHz mono wav, warmup=%ums\n",
            cap_dev, out_path, seconds, gain, CAP_RATE, OUT_RATE,
            (unsigned int)(WARMUP_FRAMES * 1000U / CAP_RATE));

    while (out_samples < target_out_samples) {
        unsigned long todo = PERIOD;
        int got = read_frames(cap, in, todo);
        if (got < 0)
            break;
        if (got == 0) {
            xruns++;
            continue;
        }

        size_t out_n = 0;
        for (int f = 0; f < got; ++f) {
            unsigned long input_frame = done + (unsigned long)f;
            int16_t l = in[f * (int)CAP_CH];
            int16_t r = in[f * (int)CAP_CH + 1];
            int la = abs_s16(l);
            int ra = abs_s16(r);

            float x = (float)(la >= ra ? l : r);
            float hp = x - hp_prev_x + 0.995f * hp_prev_y;
            hp_prev_x = x;
            hp_prev_y = hp;

            if (input_frame < WARMUP_FRAMES) {
                decim_sum = 0.0f;
                decim_count = 0;
                continue;
            }

            if (la > input_peak)
                input_peak = la;
            if (ra > input_peak)
                input_peak = ra;

            decim_sum += hp;
            decim_count++;
            if (decim_count == 3) {
                float y = soft_clip((decim_sum / 3.0f) * gain);
                int16_t s = clamp_s16(y);
                int oa = abs_s16(s);
                if (oa > output_peak)
                    output_peak = oa;
                if (out_samples + (uint32_t)out_n < target_out_samples)
                    out[out_n++] = s;
                decim_sum = 0.0f;
                decim_count = 0;
            }
        }
        if (out_n > 0) {
            if (fwrite(out, sizeof(out[0]), out_n, fp) != out_n) {
                perror("write wav data");
                break;
            }
            out_samples += (uint32_t)out_n;
        }
        done += (unsigned long)got;
    }

    uint32_t data_bytes = out_samples * OUT_CH * 2U;
    if (fseek(fp, 0, SEEK_SET) == 0)
        write_wav_header(fp, data_bytes);
    fclose(fp);
    free(in);
    close(cap);

    fprintf(stderr, "done input_peak=%d output_peak=%d out_samples=%u xruns=%d\n",
            input_peak, output_peak, out_samples, xruns);
    return out_samples > 0 ? 0 : 1;
}
