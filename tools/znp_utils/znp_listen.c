#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

static int g_fd = -1;
static volatile int g_running = 1;

void sigint_handler(int sig) { g_running = 0; }

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
#define AREQ 0x40
#define AF   0x04

int main(int argc, char *argv[]) {
    const char *port = "/dev/ttyUSB0";
    unsigned char buf[256];
    int n;

    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGINT, sigint_handler);

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

    // Register AF endpoint for IAS Zone (0x0500)
    unsigned char af_reg[] = {
        1,              // Endpoint
        0x04, 0x01,     // ProfileID: Home Automation
        0x00, 0x00,     // DeviceID
        0x00, 0x00,     // DeviceVersion, LatencyReq
        0x01,           // NumInClusters
        0x00, 0x05,     // IAS Zone cluster (0x0500)
        0x00            // NumOutClusters
    };
    znp_send(g_fd, SREQ, AF, 0x00, af_reg, sizeof(af_reg));
    n = znp_recv(g_fd, buf, sizeof(buf), 1000);

    printf("Listening for tilt sensor... (tilt it, then Ctrl+C to exit)\n");
    printf("Tilt sensor address: 0xC343\n\n");

    while (g_running) {
        n = znp_recv(g_fd, buf, sizeof(buf), 1000);
        if (n > 0 && buf[0] == 0xFE) {
            unsigned char cmd0 = buf[2];
            unsigned char cmd1 = buf[3];
            
            // AF_INCOMING_MSG
            if (cmd0 == (AREQ | AF) && cmd1 == 0x81) {
                unsigned short cluster = buf[6] | (buf[7] << 8);
                unsigned short srcAddr = buf[8] | (buf[9] << 8);
                unsigned char srcEp = buf[10];
                unsigned char dataLen = buf[17];
                unsigned char *data = &buf[18];
                
                printf("=== INCOMING MESSAGE ===\n");
                printf("  From: 0x%04X ep%d\n", srcAddr, srcEp);
                printf("  Cluster: 0x%04X", cluster);
                if (cluster == 0x0500) printf(" (IAS Zone)");
                else if (cluster == 0x0006) printf(" (On/Off)");
                else if (cluster == 0x0001) printf(" (Power Config)");
                printf("\n");
                print_hex("  ZCL Data", data, dataLen);
                
                // Parse ZCL for IAS Zone status
                if (cluster == 0x0500 && dataLen >= 5) {
                    // Zone Status Change Notification
                    if (data[2] == 0x00) {  // Status change cmd
                        unsigned short zoneStatus = data[3] | (data[4] << 8);
                        printf("  Zone Status: 0x%04X\n", zoneStatus);
                        if (zoneStatus & 0x01) printf("  >>> ALARM: TILTED <<<\n");
                        else printf("  >>> NORMAL: FLAT <<<\n");
                    }
                }
                printf("\n");
            } else {
                print_hex("Other", buf, n);
            }
        }
    }

    printf("\nDone.\n");
    close(g_fd);
    return 0;
}
