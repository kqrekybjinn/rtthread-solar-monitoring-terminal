#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/md.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ssl.h"

#define HOST "asr.tencentcloudapi.com"
#define PORT "443"
#define SERVICE "asr"
#define ACTION "SentenceRecognition"
#define VERSION "2019-06-14"
#define CONTENT_TYPE "application/json; charset=utf-8"

struct config {
    char secret_id[160];
    char secret_key[160];
    char region[64];
    char engine[64];
    char format[16];
};

static void die(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static char *read_file(const char *path, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror(path);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long n = ftell(fp);
    if (n < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    char *buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    buf[n] = 0;
    *len = (size_t)n;
    return buf;
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    if ((*s == '"' || *s == '\'') && e > s + 1 && e[-1] == *s) {
        s++;
        e[-1] = 0;
    }
    return s;
}

static int load_config(const char *path, struct config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->region, "ap-shanghai");
    strcpy(cfg->engine, "16k_zh");
    strcpy(cfg->format, "wav");

    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror(path);
        return -1;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *s = trim(line);
        if (*s == 0 || *s == '#')
            continue;
        char *eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = 0;
        char *k = trim(s);
        char *v = trim(eq + 1);
        if (strcmp(k, "SECRET_ID") == 0)
            snprintf(cfg->secret_id, sizeof(cfg->secret_id), "%s", v);
        else if (strcmp(k, "SECRET_KEY") == 0)
            snprintf(cfg->secret_key, sizeof(cfg->secret_key), "%s", v);
        else if (strcmp(k, "REGION") == 0)
            snprintf(cfg->region, sizeof(cfg->region), "%s", v);
        else if (strcmp(k, "ENGINE_MODEL_TYPE") == 0)
            snprintf(cfg->engine, sizeof(cfg->engine), "%s", v);
        else if (strcmp(k, "VOICE_FORMAT") == 0)
            snprintf(cfg->format, sizeof(cfg->format), "%s", v);
    }
    fclose(fp);
    return cfg->secret_id[0] && cfg->secret_key[0] ? 0 : -1;
}

static char *base64_encode(const unsigned char *data, size_t len)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = ((len + 2) / 3) * 4;
    char *out = malloc(out_len + 1);
    if (!out)
        return NULL;
    size_t i = 0, j = 0;
    while (i < len) {
        uint32_t a = i < len ? data[i++] : 0;
        uint32_t b = i < len ? data[i++] : 0;
        uint32_t c = i < len ? data[i++] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;
        out[j++] = tbl[(triple >> 18) & 0x3f];
        out[j++] = tbl[(triple >> 12) & 0x3f];
        out[j++] = tbl[(triple >> 6) & 0x3f];
        out[j++] = tbl[triple & 0x3f];
    }
    if (len % 3) {
        out[out_len - 1] = '=';
        if (len % 3 == 1)
            out[out_len - 2] = '=';
    }
    out[out_len] = 0;
    return out;
}

static void hex_encode(const unsigned char *in, size_t len, char *out)
{
    static const char h[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2] = h[in[i] >> 4];
        out[i * 2 + 1] = h[in[i] & 15];
    }
    out[len * 2] = 0;
}

static void sha256_hex(const char *s, char out[65])
{
    unsigned char hash[32];
    mbedtls_sha256_ret((const unsigned char *)s, strlen(s), hash, 0);
    hex_encode(hash, sizeof(hash), out);
}

static void hmac_sha256(const unsigned char *key, size_t key_len,
                        const char *msg, unsigned char out[32])
{
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(info, key, key_len, (const unsigned char *)msg, strlen(msg), out);
}

static int ssl_write_all(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        int ret = mbedtls_ssl_write(ssl, buf + off, len - off);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        if (ret <= 0)
            return ret ? ret : -1;
        off += (size_t)ret;
    }
    return 0;
}

static char *https_post(const char *request, size_t request_len, size_t *resp_len)
{
    int ret;
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    char err[160];

    mbedtls_net_init(&net);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    const char *where = "unknown";

    where = "ctr_drbg_seed";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)"tencent-asr", 11);
    if (ret)
        goto fail;
    where = "net_connect";
    ret = mbedtls_net_connect(&net, HOST, PORT, MBEDTLS_NET_PROTO_TCP);
    if (ret)
        goto fail;
    where = "ssl_config_defaults";
    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret)
        goto fail;
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    where = "ssl_setup";
    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret)
        goto fail;
    where = "ssl_set_hostname";
    ret = mbedtls_ssl_set_hostname(&ssl, HOST);
    if (ret)
        goto fail;
    mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, NULL);
    where = "ssl_handshake";
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        goto fail;
    }
    where = "ssl_write";
    ret = ssl_write_all(&ssl, (const unsigned char *)request, request_len);
    if (ret)
        goto fail;

    size_t cap = 32768, len = 0;
    char *resp = malloc(cap + 1);
    if (!resp)
        goto fail_noerr;
    for (;;) {
        if (len + 4096 > cap) {
            cap *= 2;
            char *n = realloc(resp, cap + 1);
            if (!n) {
                free(resp);
                goto fail_noerr;
            }
            resp = n;
        }
        ret = mbedtls_ssl_read(&ssl, (unsigned char *)resp + len, cap - len);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
            break;
        if (ret < 0) {
            free(resp);
            goto fail;
        }
        len += (size_t)ret;
    }
    resp[len] = 0;
    *resp_len = len;

    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&net);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return resp;

fail:
    mbedtls_strerror(ret, err, sizeof(err));
    fprintf(stderr, "tls error at %s -0x%04x: %s\n", where, (unsigned int)-ret, err);
fail_noerr:
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&net);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return NULL;
}

static char *http_body(char *resp)
{
    char *p = strstr(resp, "\r\n\r\n");
    return p ? p + 4 : resp;
}

static char *json_get_string(const char *json, const char *key)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p)
        return NULL;
    p = strchr(p + strlen(pat), ':');
    if (!p)
        return NULL;
    p++;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '"')
        return NULL;
    p++;
    char *out = malloc(strlen(p) + 1);
    if (!out)
        return NULL;
    char *o = out;
    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            if (*p == 'n') *o++ = '\n';
            else if (*p == 'r') *o++ = '\r';
            else if (*p == 't') *o++ = '\t';
            else if (*p) *o++ = *p;
            if (*p)
                p++;
        } else {
            *o++ = *p++;
        }
    }
    *o = 0;
    return out;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s input.wav [config.env]\n", argv[0]);
        return 2;
    }
    const char *wav_path = argv[1];
    const char *config_path = argc > 2 ? argv[2] : "/userdata/voice-assistant/tencent_asr.env";

    struct config cfg;
    if (load_config(config_path, &cfg) < 0)
        die("missing SECRET_ID or SECRET_KEY in config");

    size_t wav_len;
    char *wav = read_file(wav_path, &wav_len);
    if (!wav)
        return 1;
    char *b64 = base64_encode((const unsigned char *)wav, wav_len);
    free(wav);
    if (!b64)
        die("base64 failed");

    size_t payload_cap = strlen(b64) + 512;
    char *payload = malloc(payload_cap);
    if (!payload)
        die("oom");
    snprintf(payload, payload_cap,
             "{\"SubServiceType\":2,\"ProjectId\":0,\"EngSerViceType\":\"%s\","
             "\"VoiceFormat\":\"%s\",\"Data\":\"%s\",\"SourceType\":1}",
             cfg.engine, cfg.format, b64);
    free(b64);

    time_t now = time(NULL);
    struct tm gtm;
    gmtime_r(&now, &gtm);
    char date[16];
    strftime(date, sizeof(date), "%Y-%m-%d", &gtm);
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%ld", (long)now);

    char hashed_payload[65];
    sha256_hex(payload, hashed_payload);
    char canonical[1024];
    snprintf(canonical, sizeof(canonical),
             "POST\n/\n\ncontent-type:%s\nhost:%s\nx-tc-action:%s\n\n"
             "content-type;host;x-tc-action\n%s",
             CONTENT_TYPE, HOST, "sentencerecognition", hashed_payload);
    char hashed_canonical[65];
    sha256_hex(canonical, hashed_canonical);

    char scope[128];
    snprintf(scope, sizeof(scope), "%s/%s/tc3_request", date, SERVICE);
    char string_to_sign[512];
    snprintf(string_to_sign, sizeof(string_to_sign),
             "TC3-HMAC-SHA256\n%s\n%s\n%s", timestamp, scope, hashed_canonical);

    unsigned char k_date[32], k_service[32], k_signing[32], sig_bin[32];
    char tc3_key[200];
    snprintf(tc3_key, sizeof(tc3_key), "TC3%s", cfg.secret_key);
    hmac_sha256((const unsigned char *)tc3_key, strlen(tc3_key), date, k_date);
    hmac_sha256(k_date, sizeof(k_date), SERVICE, k_service);
    hmac_sha256(k_service, sizeof(k_service), "tc3_request", k_signing);
    hmac_sha256(k_signing, sizeof(k_signing), string_to_sign, sig_bin);
    char signature[65];
    hex_encode(sig_bin, sizeof(sig_bin), signature);

    char auth[512];
    snprintf(auth, sizeof(auth),
             "TC3-HMAC-SHA256 Credential=%s/%s, SignedHeaders=content-type;host;x-tc-action, Signature=%s",
             cfg.secret_id, scope, signature);

    size_t req_cap = strlen(payload) + strlen(auth) + 1024;
    char *request = malloc(req_cap);
    if (!request)
        die("oom");
    int req_len = snprintf(request, req_cap,
        "POST / HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: %s\r\n"
        "X-TC-Action: %s\r\n"
        "X-TC-Version: %s\r\n"
        "X-TC-Timestamp: %s\r\n"
        "X-TC-Region: %s\r\n"
        "Authorization: %s\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        HOST, CONTENT_TYPE, ACTION, VERSION, timestamp, cfg.region, auth,
        strlen(payload), payload);
    free(payload);
    if (req_len <= 0 || (size_t)req_len >= req_cap)
        die("request too large");

    size_t resp_len = 0;
    char *resp = https_post(request, (size_t)req_len, &resp_len);
    free(request);
    if (!resp)
        return 1;

    char *body = http_body(resp);
    char *err = json_get_string(body, "Message");
    if (err) {
        fprintf(stderr, "Tencent ASR error: %s\n", err);
        free(err);
        fprintf(stderr, "%s\n", body);
        free(resp);
        return 1;
    }
    char *result = json_get_string(body, "Result");
    if (!result) {
        fprintf(stderr, "Tencent ASR: no Result in response\n%s\n", body);
        free(resp);
        return 1;
    }
    printf("%s\n", result);
    free(result);
    free(resp);
    return 0;
}
