#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define RPMSG_NAME_SIZE 32
#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)

#define PWRCTL_MAGIC       0x52433641u
#define PWRCTL_VERSION     1u
#define PWRCTL_PAYLOAD_MAX 192u
#define PWRCTL_CMD_PING    1u

struct rpmsg_endpoint_info
{
    char name[RPMSG_NAME_SIZE];
    uint32_t src;
    uint32_t dst;
};

struct pwrctl_msg
{
    uint32_t magic;
    uint16_t version;
    uint16_t command;
    uint32_t sequence;
    uint32_t status;
    uint32_t payload_len;
    char payload[PWRCTL_PAYLOAD_MAX];
};

static int dev_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static int find_endpoint(char *path, size_t path_len, unsigned int before_mask)
{
    int best = -1;

    for (int i = 0; i < 32; i++)
    {
        char candidate[32];
        snprintf(candidate, sizeof(candidate), "/dev/rpmsg%d", i);
        if (!dev_exists(candidate))
            continue;
        if ((before_mask & (1u << i)) == 0)
        {
            snprintf(path, path_len, "%s", candidate);
            return 0;
        }
        best = i;
    }

    if (best >= 0)
    {
        snprintf(path, path_len, "/dev/rpmsg%d", best);
        return 0;
    }

    return -1;
}

static int read_u32_file(const char *path, uint32_t *value)
{
    char buf[32];
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    ssize_t n;
    char *end = NULL;
    unsigned long parsed;

    if (fd < 0)
        return -1;

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;

    buf[n] = '\0';
    parsed = strtoul(buf, &end, 0);
    if (end == buf)
        return -1;

    *value = (uint32_t)parsed;
    return 0;
}

static int find_endpoint_by_addr(char *path, size_t path_len, uint32_t src, uint32_t dst)
{
    DIR *dir = opendir("/sys/class/rpmsg");
    struct dirent *entry;

    if (dir == NULL)
        return -1;

    while ((entry = readdir(dir)) != NULL)
    {
        char src_path[320];
        char dst_path[320];
        char candidate[320];
        uint32_t endpoint_src;
        uint32_t endpoint_dst;

        if (strncmp(entry->d_name, "rpmsg", 5) != 0)
            continue;
        if (strcmp(entry->d_name, "rpmsg_ctrl0") == 0)
            continue;

        snprintf(src_path, sizeof(src_path), "/sys/class/rpmsg/%s/src", entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "/sys/class/rpmsg/%s/dst", entry->d_name);
        if (read_u32_file(src_path, &endpoint_src) != 0 ||
            read_u32_file(dst_path, &endpoint_dst) != 0)
            continue;

        if (endpoint_src == src && endpoint_dst == dst)
        {
            snprintf(candidate, sizeof(candidate), "/dev/%s", entry->d_name);
            if (dev_exists(candidate))
            {
                snprintf(path, path_len, "%s", candidate);
                closedir(dir);
                return 0;
            }
        }
    }

    closedir(dir);
    return -1;
}

static unsigned int endpoint_mask(void)
{
    unsigned int mask = 0;

    for (int i = 0; i < 32; i++)
    {
        char candidate[32];
        snprintf(candidate, sizeof(candidate), "/dev/rpmsg%d", i);
        if (dev_exists(candidate))
            mask |= (1u << i);
    }

    return mask;
}

int main(int argc, char **argv)
{
    const char *ctrl_path = "/dev/rpmsg_ctrl0";
    uint32_t src = 0x1000;
    uint32_t dst = 0x3000;
    unsigned int before;
    int ctrl;
    int fd;
    char endpoint_path[320];
    struct rpmsg_endpoint_info ept;
    struct pwrctl_msg msg;
    struct pollfd pfd;
    ssize_t n;

    if (argc > 1)
        ctrl_path = argv[1];

    ctrl = open(ctrl_path, O_RDWR | O_CLOEXEC);
    if (ctrl < 0)
    {
        perror("open rpmsg_ctrl");
        return 1;
    }

    if (find_endpoint_by_addr(endpoint_path, sizeof(endpoint_path), src, dst) == 0)
        goto endpoint_ready;

    before = endpoint_mask();

    memset(&ept, 0, sizeof(ept));
    snprintf(ept.name, sizeof(ept.name), "rpmsg_chrdev");
    ept.src = src;
    ept.dst = dst;

    if (ioctl(ctrl, RPMSG_CREATE_EPT_IOCTL, &ept) < 0)
    {
        if (errno != EEXIST)
        {
            perror("RPMSG_CREATE_EPT_IOCTL");
            close(ctrl);
            return 1;
        }
    }

    usleep(200000);
    if (find_endpoint(endpoint_path, sizeof(endpoint_path), before) != 0)
    {
        fprintf(stderr, "no /dev/rpmsgN endpoint found\n");
        close(ctrl);
        return 1;
    }

endpoint_ready:
    fd = open(endpoint_path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        perror("open rpmsg endpoint");
        close(ctrl);
        return 1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.magic = PWRCTL_MAGIC;
    msg.version = PWRCTL_VERSION;
    msg.command = PWRCTL_CMD_PING;
    msg.sequence = 1;

    n = write(fd, &msg, sizeof(msg));
    if (n != (ssize_t)sizeof(msg))
    {
        perror("write ping");
        close(fd);
        close(ctrl);
        return 1;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 3000) <= 0)
    {
        fprintf(stderr, "timeout waiting for AMP response on %s\n", endpoint_path);
        close(fd);
        close(ctrl);
        return 2;
    }

    memset(&msg, 0, sizeof(msg));
    n = read(fd, &msg, sizeof(msg));
    if (n < 0)
    {
        perror("read response");
        close(fd);
        close(ctrl);
        return 1;
    }

    printf("endpoint=%s bytes=%zd magic=0x%08x version=%u command=%u sequence=%u status=%u payload_len=%u\n",
           endpoint_path, n, msg.magic, msg.version, msg.command,
           msg.sequence, msg.status, msg.payload_len);
    if (msg.payload_len > 0 && msg.payload_len < PWRCTL_PAYLOAD_MAX)
    {
        msg.payload[msg.payload_len] = '\0';
        printf("payload=%s\n", msg.payload);
    }

    close(fd);
    close(ctrl);
    return 0;
}
