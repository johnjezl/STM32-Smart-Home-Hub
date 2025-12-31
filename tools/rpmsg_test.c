#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>

struct rpmsg_endpoint_info {
    char name[32];
    uint32_t src;
    uint32_t dst;
};

#define RPMSG_CREATE_EPT_IOCTL _IOW(0xb5, 0x1, struct rpmsg_endpoint_info)

int main(int argc, char *argv[]) {
    uint32_t src = 0x401;
    uint32_t dst = 0x400;

    if (argc > 1) src = strtoul(argv[1], NULL, 0);
    if (argc > 2) dst = strtoul(argv[2], NULL, 0);

    int fd = open("/dev/rpmsg_ctrl0", O_RDWR);
    if (fd < 0) {
        perror("open /dev/rpmsg_ctrl0");
        return 1;
    }

    struct rpmsg_endpoint_info info;
    memset(&info, 0, sizeof(info));
    strncpy(info.name, "smarthub-test", sizeof(info.name) - 1);
    info.src = src;
    info.dst = dst;

    printf("Creating endpoint: name=%s, src=0x%x, dst=0x%x\n", info.name, info.src, info.dst);

    int ret = ioctl(fd, RPMSG_CREATE_EPT_IOCTL, &info);
    if (ret < 0) {
        perror("ioctl RPMSG_CREATE_EPT");
        close(fd);
        return 1;
    }

    printf("Endpoint created! Check /dev/rpmsg*\n");

    /* Keep file open */
    printf("Waiting... Press Ctrl+C to exit\n");
    while (1) {
        sleep(1);
    }

    close(fd);
    return 0;
}
