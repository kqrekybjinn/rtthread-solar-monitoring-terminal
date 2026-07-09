#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

#define MQTT_DEFAULT_HOST "example.mqtt-broker.local"
#define MQTT_DEFAULT_PORT "8883"
#define MQTT_DEFAULT_CLIENT_ID "rk3506_node_001"
#define MQTT_DEFAULT_USERNAME "rk3506_node_001"
#define MQTT_DEFAULT_PASSWORD "change-me"
#define MQTT_DEFAULT_TOPIC "rk3506/rk3506_node_001/heartbeat"
#define MQTT_KEEPALIVE_SEC 60
#define HEARTBEAT_SEC 10

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void log_msg(const char *fmt, ...)
{
    FILE *fp = fopen("/tmp/rc3506-mqtt.log", "a");
    if (!fp)
        fp = stderr;

    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);
    fprintf(fp, "%04d-%02d-%02d %02d:%02d:%02d ",
            tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
            tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);

    if (fp != stderr)
        fclose(fp);
}

static void log_mbedtls_error(const char *where, int ret)
{
    char buf[160];
    mbedtls_strerror(ret, buf, sizeof(buf));
    log_msg("%s failed: -0x%04x %s", where, (unsigned int)-ret, buf);
}

static const char *env_or_default(const char *name, const char *fallback)
{
    const char *value = getenv(name);
    return value && value[0] ? value : fallback;
}

static int encode_remaining_len(uint8_t *out, size_t len)
{
    int n = 0;
    do {
        uint8_t byte = len % 128;
        len /= 128;
        if (len > 0)
            byte |= 0x80;
        out[n++] = byte;
    } while (len > 0 && n < 4);
    return n;
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xff);
}

static int put_utf8(uint8_t *buf, size_t cap, size_t *off, const char *s)
{
    size_t len = strlen(s);
    if (len > 65535 || *off + 2 + len > cap)
        return -1;
    put_u16(buf + *off, (uint16_t)len);
    *off += 2;
    memcpy(buf + *off, s, len);
    *off += len;
    return 0;
}

static int ssl_write_all(mbedtls_ssl_context *ssl, const uint8_t *buf, size_t len)
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

static int ssl_read_exact(mbedtls_ssl_context *ssl, uint8_t *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        int ret = mbedtls_ssl_read(ssl, buf + off, len - off);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        if (ret <= 0)
            return ret ? ret : -1;
        off += (size_t)ret;
    }
    return 0;
}

static int mqtt_connect_packet(mbedtls_ssl_context *ssl)
{
    uint8_t payload[512];
    size_t off = 0;
    const char *client_id = env_or_default("MQTT_CLIENT_ID", MQTT_DEFAULT_CLIENT_ID);
    const char *username = env_or_default("MQTT_USERNAME", MQTT_DEFAULT_USERNAME);
    const char *password = env_or_default("MQTT_PASSWORD", MQTT_DEFAULT_PASSWORD);

    if (put_utf8(payload, sizeof(payload), &off, "MQTT") < 0)
        return -1;
    payload[off++] = 4;
    payload[off++] = 0xc2; /* username + password + clean session */
    payload[off++] = 0;
    payload[off++] = MQTT_KEEPALIVE_SEC;
    if (put_utf8(payload, sizeof(payload), &off, client_id) < 0)
        return -1;
    if (put_utf8(payload, sizeof(payload), &off, username) < 0)
        return -1;
    if (put_utf8(payload, sizeof(payload), &off, password) < 0)
        return -1;

    uint8_t pkt[600];
    size_t poff = 0;
    pkt[poff++] = 0x10;
    poff += encode_remaining_len(pkt + poff, off);
    memcpy(pkt + poff, payload, off);
    poff += off;

    int ret = ssl_write_all(ssl, pkt, poff);
    if (ret)
        return ret;

    uint8_t resp[4];
    ret = ssl_read_exact(ssl, resp, sizeof(resp));
    if (ret)
        return ret;
    if (resp[0] != 0x20 || resp[1] != 0x02 || resp[3] != 0x00) {
        log_msg("mqtt CONNACK rejected: %02x %02x %02x %02x",
                resp[0], resp[1], resp[2], resp[3]);
        return -1;
    }
    return 0;
}

static int mqtt_publish(mbedtls_ssl_context *ssl, const char *topic, const char *payload)
{
    uint8_t body[1024];
    size_t off = 0;
    if (put_utf8(body, sizeof(body), &off, topic) < 0)
        return -1;
    size_t plen = strlen(payload);
    if (off + plen > sizeof(body))
        return -1;
    memcpy(body + off, payload, plen);
    off += plen;

    uint8_t pkt[1100];
    size_t poff = 0;
    pkt[poff++] = 0x30; /* QoS0 publish */
    poff += encode_remaining_len(pkt + poff, off);
    memcpy(pkt + poff, body, off);
    poff += off;
    return ssl_write_all(ssl, pkt, poff);
}

static int mqtt_ping(mbedtls_ssl_context *ssl)
{
    const uint8_t pkt[2] = {0xc0, 0x00};
    return ssl_write_all(ssl, pkt, sizeof(pkt));
}

static void read_first_line(const char *path, char *buf, size_t len)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        snprintf(buf, len, "unknown");
        return;
    }
    if (!fgets(buf, len, fp))
        snprintf(buf, len, "unknown");
    fclose(fp);
    buf[strcspn(buf, "\r\n")] = 0;
}

static void make_heartbeat(char *out, size_t len, unsigned long seq)
{
    char host[64], ip[64] = "unknown";
    struct utsname uts;
    const char *client_id = env_or_default("MQTT_CLIENT_ID", MQTT_DEFAULT_CLIENT_ID);
    read_first_line("/proc/sys/kernel/hostname", host, sizeof(host));
    if (uname(&uts) < 0)
        memset(&uts, 0, sizeof(uts));

    FILE *fp = popen("ip -4 addr show usb0 2>/dev/null | awk '/inet /{print $2; exit}'", "r");
    if (fp) {
        if (!fgets(ip, sizeof(ip), fp))
            snprintf(ip, sizeof(ip), "unknown");
        pclose(fp);
        ip[strcspn(ip, "\r\n")] = 0;
    }

    time_t now = time(NULL);
    snprintf(out, len,
             "{\"device\":\"%s\",\"seq\":%lu,\"ts\":%ld,\"host\":\"%s\","
             "\"kernel\":\"%s\",\"machine\":\"%s\",\"usb0_ip\":\"%s\"}",
             client_id, seq, (long)now, host, uts.release, uts.machine, ip);
}

static int run_once(void)
{
    int ret;
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    const char *host = env_or_default("MQTT_HOST", MQTT_DEFAULT_HOST);
    const char *port = env_or_default("MQTT_PORT", MQTT_DEFAULT_PORT);
    const char *topic = env_or_default("MQTT_TOPIC", MQTT_DEFAULT_TOPIC);

    mbedtls_net_init(&net);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)"rc3506-mqtt", 11);
    if (ret) {
        log_mbedtls_error("ctr_drbg_seed", ret);
        goto out;
    }

    ret = mbedtls_net_connect(&net, host, port, MBEDTLS_NET_PROTO_TCP);
    if (ret) {
        log_mbedtls_error("net_connect", ret);
        goto out;
    }

    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret) {
        log_mbedtls_error("ssl_config_defaults", ret);
        goto out;
    }
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret) {
        log_mbedtls_error("ssl_setup", ret);
        goto out;
    }
    ret = mbedtls_ssl_set_hostname(&ssl, host);
    if (ret) {
        log_mbedtls_error("ssl_set_hostname", ret);
        goto out;
    }
    mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, NULL);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
            continue;
        log_mbedtls_error("ssl_handshake", ret);
        goto out;
    }

    ret = mqtt_connect_packet(&ssl);
    if (ret) {
        log_mbedtls_error("mqtt_connect", ret);
        goto out;
    }
    log_msg("mqtt connected to %s:%s", host, port);

    unsigned long seq = 1;
    time_t last_ping = time(NULL);
    while (!g_stop) {
        char hb[768];
        make_heartbeat(hb, sizeof(hb), seq++);
        ret = mqtt_publish(&ssl, topic, hb);
        if (ret) {
            log_mbedtls_error("mqtt_publish", ret);
            goto out;
        }
        log_msg("published %s", hb);

        time_t now = time(NULL);
        if (now - last_ping >= MQTT_KEEPALIVE_SEC / 2) {
            ret = mqtt_ping(&ssl);
            if (ret) {
                log_mbedtls_error("mqtt_ping", ret);
                goto out;
            }
            last_ping = now;
        }
        for (int i = 0; i < HEARTBEAT_SEC && !g_stop; ++i)
            sleep(1);
    }

    ret = 0;

out:
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_net_free(&net);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return ret;
}

int main(void)
{
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    log_msg("rc3506 mqtt heartbeat starting");

    while (!g_stop) {
        int ret = run_once();
        if (!g_stop) {
            log_msg("mqtt session ended ret=%d, reconnecting in 5s", ret);
            sleep(5);
        }
    }
    log_msg("rc3506 mqtt heartbeat stopped");
    return 0;
}
