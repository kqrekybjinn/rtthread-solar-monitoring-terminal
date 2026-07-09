#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

#define HOST "api.deepseek.com"
#define PORT "443"
#define PATH "/chat/completions"
#define CONTENT_TYPE "application/json"

struct config {
    char api_key[192];
    char model[96];
};

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
    strcpy(cfg->model, "deepseek-chat");

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
        if (strcmp(k, "DEEPSEEK_API_KEY") == 0)
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", v);
        else if (strcmp(k, "NULLCLAW_MODEL") == 0 || strcmp(k, "DEEPSEEK_MODEL") == 0)
            snprintf(cfg->model, sizeof(cfg->model), "%s", v);
    }
    fclose(fp);
    return cfg->api_key[0] ? 0 : -1;
}

static char *json_escape(const char *s)
{
    size_t cap = strlen(s) * 6 + 1;
    char *out = malloc(cap);
    if (!out)
        return NULL;
    char *o = out;
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '\\': *o++ = '\\'; *o++ = '\\'; break;
        case '"': *o++ = '\\'; *o++ = '"'; break;
        case '\n': *o++ = '\\'; *o++ = 'n'; break;
        case '\r': *o++ = '\\'; *o++ = 'r'; break;
        case '\t': *o++ = '\\'; *o++ = 't'; break;
        default:
            if (c < 0x20) {
                sprintf(o, "\\u%04x", c);
                o += 6;
            } else {
                *o++ = (char)c;
            }
        }
    }
    *o = 0;
    return out;
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

static char *https_post(const char *request, size_t request_len)
{
    int ret;
    const char *where = "unknown";
    char err[160];
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    mbedtls_net_init(&net);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    where = "ctr_drbg_seed";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)"deepseek-chat", 13);
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

static char *json_get_content(const char *json)
{
    const char *p = strstr(json, "\"content\"");
    if (!p)
        return NULL;
    p = strchr(p, ':');
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
        fprintf(stderr, "usage: %s prompt.txt [deepseek.env]\n", argv[0]);
        return 2;
    }
    const char *prompt_path = argv[1];
    const char *config_path = argc > 2 ? argv[2] : "/userdata/agent/deepseek.env";

    struct config cfg;
    if (load_config(config_path, &cfg) < 0) {
        fprintf(stderr, "missing DEEPSEEK_API_KEY in %s\n", config_path);
        return 1;
    }
    size_t prompt_len;
    char *prompt = read_file(prompt_path, &prompt_len);
    if (!prompt)
        return 1;
    char *escaped = json_escape(prompt);
    free(prompt);
    if (!escaped)
        return 1;

    size_t payload_cap = strlen(escaped) + strlen(cfg.model) + 512;
    char *payload = malloc(payload_cap);
    if (!payload) {
        free(escaped);
        return 1;
    }
    snprintf(payload, payload_cap,
             "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
             "\"temperature\":0.2,\"max_tokens\":220,\"stream\":false}",
             cfg.model, escaped);
    free(escaped);

    size_t req_cap = strlen(payload) + strlen(cfg.api_key) + 1024;
    char *request = malloc(req_cap);
    if (!request) {
        free(payload);
        return 1;
    }
    int req_len = snprintf(request, req_cap,
        "POST " PATH " HTTP/1.1\r\n"
        "Host: " HOST "\r\n"
        "Content-Type: " CONTENT_TYPE "\r\n"
        "Authorization: Bearer %s\r\n"
        "Connection: close\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s",
        cfg.api_key, strlen(payload), payload);
    free(payload);
    if (req_len <= 0 || (size_t)req_len >= req_cap) {
        free(request);
        return 1;
    }

    char *resp = https_post(request, (size_t)req_len);
    free(request);
    if (!resp)
        return 1;

    char *body = http_body(resp);
    char *content = json_get_content(body);
    if (!content) {
        fprintf(stderr, "DeepSeek response did not contain content\n%s\n", body);
        free(resp);
        return 1;
    }
    printf("%s\n", content);
    free(content);
    free(resp);
    return 0;
}
