#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int abs_s16(int16_t v)
{
    return v == INT16_MIN ? 32768 : (v < 0 ? -v : v);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: wav_level input.wav\n");
        return 2;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror(argv[1]);
        return 1;
    }

    uint8_t h[12];
    if (fread(h, 1, sizeof(h), fp) != sizeof(h) ||
        memcmp(h, "RIFF", 4) != 0 ||
        memcmp(h + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "not a wav file\n");
        fclose(fp);
        return 1;
    }

    uint16_t channels = 0;
    uint32_t rate = 0;
    uint16_t bits = 0;
    uint32_t data_bytes = 0;
    long data_offset = 0;

    for (;;) {
        uint8_t ch[8];
        if (fread(ch, 1, sizeof(ch), fp) != sizeof(ch))
            break;
        uint32_t size = rd32(ch + 4);
        long pos = ftell(fp);
        if (memcmp(ch, "fmt ", 4) == 0) {
            uint8_t fmt[32];
            if (size < 16 || size > sizeof(fmt))
                break;
            if (fread(fmt, 1, size, fp) != size)
                break;
            if (rd16(fmt) != 1)
                break;
            channels = rd16(fmt + 2);
            rate = rd32(fmt + 4);
            bits = rd16(fmt + 14);
        } else if (memcmp(ch, "data", 4) == 0) {
            data_offset = pos;
            data_bytes = size;
            break;
        } else {
            if (fseek(fp, size + (size & 1U), SEEK_CUR) != 0)
                break;
        }
    }

    if (channels == 0 || rate == 0 || bits != 16 || data_offset == 0 || data_bytes == 0) {
        fprintf(stderr, "unsupported wav\n");
        fclose(fp);
        return 1;
    }

    if (fseek(fp, data_offset, SEEK_SET) != 0) {
        perror("fseek");
        fclose(fp);
        return 1;
    }

    int peak = 0;
    double sum2 = 0.0;
    uint32_t samples = data_bytes / 2U;
    for (uint32_t i = 0; i < samples; ++i) {
        uint8_t b[2];
        if (fread(b, 1, 2, fp) != 2)
            break;
        int16_t s = (int16_t)rd16(b);
        int a = abs_s16(s);
        if (a > peak)
            peak = a;
        sum2 += (double)s * (double)s;
    }
    fclose(fp);

    double rms = samples ? sqrt(sum2 / (double)samples) : 0.0;
    double duration = (double)(samples / channels) / (double)rate;
    printf("duration=%.3f peak=%d rms=%.1f rate=%u channels=%u samples=%u\n",
           duration, peak, rms, rate, channels, samples);
    return 0;
}
