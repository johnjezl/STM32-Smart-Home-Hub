#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

static int g_fd = -1;

unsigned char calc_fcs(unsigned char *data, int len) {
    unsigned char fcs = 0;
    for (int i = 0; i < len; i++) fcs ^= data[i];
    return fcs;
}

int znp_send(int fd, unsigned char type, unsigned char subsys, unsigned char cmd,
             unsigned char *payload, int payload_len) {
    unsigned char frame[256];
    int idx = 0;
    frame[idx++] = 0xFE;
    frame[idx++] = payload_len;
    frame[idx++] = type | subsys;
    frame[idx++] = cmd;
    if (payload_len > 0) {
        memcpy(&frame[idx], payload, payload_len);
        idx += payload_len;
    }
    frame[idx] = calc_fcs(&frame[1], idx - 1);
    idx++;
    return write(fd, frame, idx);
}

int znp_recv(int fd, unsigned char *buf, int buflen, int timeout_ms) {
    fd_set rfds;
    struct timeval tv;
    int total = 0;
    while (total < buflen) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) break;
        int n = read(fd, buf + total, buflen - total);
        if (n <= 0) break;
        total += n;
        if (total >= 5 && buf[0] == 0xFE) {
            int expected = 4 + buf[1] + 1;
            if (total >= expected) break;
        }
        timeout_ms = 100;
    }
    return total;
}

void print_hex(const char *label, unsigned char *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) printf("%02x ", data[i]);
    printf("\n");
    fflush(stdout);
}

#define SREQ 0x20
#define UTIL 0x07
#define ZDO  0x05

int main(int argc, char *argv[]) {
    const char *port = "/dev/ttyUSB0";
    unsigned char buf[256];
    int n;

    setvbuf(stdout, NULL, _IONBF, 0);

    g_fd = open(port, O_RDWR | O_NOCTTY);
    if (g_fd < 0) { perror("open"); return 1; }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    tcgetattr(g_fd, &tty);
    cfmakeraw(&tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tty.c_cflag |= CS8 | CLOCAL | CREAD;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;
    tcsetattr(g_fd, TCSANOW, &tty);
    tcflush(g_fd, TCIOFLUSH);

    // UTIL_ASSOC_GET_WITH_ADDRESS - get association table
    printf("Getting associated devices...\n\n");
    
    // Query for up to 10 devices
    for (int i = 0; i < 10; i++) {
        unsigned char payload[] = {(unsigned char)i};
        znp_send(g_fd, SREQ, UTIL, 0x05, payload, 1);  // UTIL_ASSOC_FIND_DEVICE
        n = znp_recv(g_fd, buf, sizeof(buf), 1000);
        if (n > 5 && buf[4] != 0xFF) {
            printf("Device %d:\n", i);
            print_hex("  Raw", buf, n);
        } else {
            break;
        }
    }

    // Also try ZDO_MGMT_LQI_REQ on coordinator (0x0000) to get neighbor table
    printf("\nQuerying coordinator neighbor table...\n");
    unsigned char lqi_req[] = {
        0x00, 0x00,  // DstAddr: coordinator
        0x00         // StartIndex
    };
    znp_send(g_fd, SREQ, ZDO, 0x31, lqi_req, 3);  // ZDO_MGMT_LQI_REQ
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n > 0) print_hex("LQI Response", buf, n);
    
    // Wait for async response
    n = znp_recv(g_fd, buf, sizeof(buf), 3000);
    if (n > 0) {
        print_hex("LQI Async", buf, n);
        // Parse the response
        if (n >= 10 && buf[2] == 0x45 && buf[3] == 0xB1) {
            // ZDO_MGMT_LQI_RSP
            int status = buf[6];
            int total = buf[7];
            int start = buf[8];
            int count = buf[9];
            printf("\nNeighbor Table: status=%d, total=%d, start=%d, count=%d\n", 
                   status, total, start, count);
            
            int offset = 10;
            for (int i = 0; i < count && offset + 22 <= n; i++) {
                unsigned short nwkAddr = buf[offset + 16] | (buf[offset + 17] << 8);
                printf("  Device %d: NwkAddr=0x%04X\n", i, nwkAddr);
                offset += 22;  // Each entry is 22 bytes
            }
        }
    }

    close(g_fd);
    return 0;
}
