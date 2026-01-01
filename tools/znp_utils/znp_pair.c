#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define LOG(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

static int g_fd = -1;
static volatile int g_running = 1;

void sigint_handler(int sig) {
    g_running = 0;
}

// Calculate FCS (XOR of all bytes except SOF)
unsigned char calc_fcs(unsigned char *data, int len) {
    unsigned char fcs = 0;
    for (int i = 0; i < len; i++) fcs ^= data[i];
    return fcs;
}

// Send ZNP frame
int znp_send(int fd, unsigned char type, unsigned char subsys, unsigned char cmd,
             unsigned char *payload, int payload_len) {
    unsigned char frame[256];
    int idx = 0;

    frame[idx++] = 0xFE;  // SOF
    frame[idx++] = payload_len;  // Length
    frame[idx++] = type | subsys;  // Cmd0
    frame[idx++] = cmd;  // Cmd1
    if (payload_len > 0) {
        memcpy(&frame[idx], payload, payload_len);
        idx += payload_len;
    }
    frame[idx] = calc_fcs(&frame[1], idx - 1);  // FCS
    idx++;

    return write(fd, frame, idx);
}

// Read ZNP frame with timeout
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

        // Check if we have a complete frame
        if (total >= 5 && buf[0] == 0xFE) {
            int expected = 4 + buf[1] + 1;  // SOF + len + cmd0 + cmd1 + payload + fcs
            if (total >= expected) break;
        }
        timeout_ms = 100;  // Short timeout for continuation
    }
    return total;
}

void print_hex(const char *label, unsigned char *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) printf("%02x ", data[i]);
    printf("\n");
}

// ZNP Commands
#define SREQ 0x20
#define AREQ 0x40
#define SRSP 0x60

#define SYS  0x01
#define AF   0x04
#define ZDO  0x05
#define SAPI 0x06
#define UTIL 0x07
#define APP_CNF 0x0F

// Command IDs
#define SYS_RESET_REQ    0x00
#define SYS_PING         0x01
#define SYS_VERSION      0x02
#define SYS_OSAL_NV_WRITE 0x09

#define ZDO_STARTUP_FROM_APP 0x40
#define ZDO_END_DEVICE_ANNCE_IND 0xC1
#define ZDO_TC_DEV_IND   0xCA
#define ZDO_PERMIT_JOIN_IND 0xCB
#define ZDO_MGMT_PERMIT_JOIN_REQ 0x36
#define ZDO_MGMT_PERMIT_JOIN_RSP 0xB6

#define APP_CNF_BDB_START_COMMISSIONING 0x05
#define APP_CNF_BDB_SET_CHANNEL 0x08

#define AF_REGISTER      0x00
#define AF_DATA_REQUEST  0x01
#define AF_INCOMING_MSG  0x81

#define UTIL_GET_DEVICE_INFO 0x00

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "/dev/ttyUSB0";
    unsigned char buf[256];
    int n;

    // Disable stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGINT, sigint_handler);

    g_fd = open(port, O_RDWR | O_NOCTTY);
    if (g_fd < 0) {
        perror("open");
        return 1;
    }

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

    printf("=== Zigbee Coordinator Pairing Mode ===\n");
    printf("Port: %s\n\n", port);

    // 1. Skip reset - just flush the port
    printf("[1] Initializing (skipping reset)...\n");
    tcflush(g_fd, TCIOFLUSH);
    usleep(500000);

    // 2. Ping to verify
    printf("[2] Verifying communication...\n");
    znp_send(g_fd, SREQ, SYS, SYS_PING, NULL, 0);
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n > 0 && buf[0] == 0xFE) {
        print_hex("    Ping response", buf, n);
    } else {
        printf("    ERROR: No response to ping\n");
        close(g_fd);
        return 1;
    }

    // 3. Get device info
    printf("[3] Getting device info...\n");
    znp_send(g_fd, SREQ, UTIL, UTIL_GET_DEVICE_INFO, NULL, 0);
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n >= 14 && buf[0] == 0xFE) {
        printf("    IEEE Address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
               buf[12], buf[11], buf[10], buf[9], buf[8], buf[7], buf[6], buf[5]);
        printf("    Short Address: 0x%02x%02x\n", buf[14], buf[13]);
        printf("    Device Type: %s\n", buf[15] == 0 ? "Coordinator" :
                                        buf[15] == 1 ? "Router" : "End Device");
        printf("    Device State: %d\n", buf[16]);
    }

    // 4. Start network (BDB commissioning)
    printf("[4] Starting Zigbee network...\n");
    unsigned char bdb_start[] = {0x04};  // Commissioning mode: Formation
    znp_send(g_fd, SREQ, APP_CNF, APP_CNF_BDB_START_COMMISSIONING, bdb_start, 1);
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n > 0) print_hex("    BDB response", buf, n);
    sleep(3);
    tcflush(g_fd, TCIOFLUSH);

    // 5. Enable permit join (254 seconds max)
    printf("[5] Enabling permit join for 254 seconds...\n");
    printf("    >>> PUT YOUR DEVICES IN PAIRING MODE NOW <<<\n\n");

    unsigned char permit_join[] = {
        0x02,        // AddrMode: 16-bit
        0xFC, 0xFF,  // DstAddr: 0xFFFC (all routers)
        0xFE,        // Duration: 254 seconds
        0x00         // TC_Significance
    };
    znp_send(g_fd, SREQ, ZDO, ZDO_MGMT_PERMIT_JOIN_REQ, permit_join, 5);
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n > 0) print_hex("    Permit join response", buf, n);

    // 6. Listen for device announcements
    printf("\n[6] Listening for devices (Ctrl+C to exit)...\n");
    printf("    ----------------------------------------\n");

    time_t start = time(NULL);
    int device_count = 0;

    while (g_running) {
        n = znp_recv(g_fd, buf, sizeof(buf), 1000);
        if (n > 0 && buf[0] == 0xFE) {
            unsigned char cmd0 = buf[2];
            unsigned char cmd1 = buf[3];

            // Device announce
            if (cmd0 == (AREQ | ZDO) && cmd1 == ZDO_END_DEVICE_ANNCE_IND) {
                device_count++;
                printf("\n*** DEVICE JOINED (#%d) ***\n", device_count);
                printf("    Short Addr: 0x%02x%02x\n", buf[6], buf[5]);
                printf("    IEEE Addr: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
                       buf[14], buf[13], buf[12], buf[11], buf[10], buf[9], buf[8], buf[7]);
                printf("    Capabilities: 0x%02x\n", buf[15]);
                print_hex("    Raw", buf, n);
            }
            // Trust center device indication
            else if (cmd0 == (AREQ | ZDO) && cmd1 == ZDO_TC_DEV_IND) {
                printf("\n*** TRUST CENTER: Device authenticated ***\n");
                printf("    Network Addr: 0x%02x%02x\n", buf[6], buf[5]);
                printf("    IEEE Addr: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
                       buf[14], buf[13], buf[12], buf[11], buf[10], buf[9], buf[8], buf[7]);
                print_hex("    Raw", buf, n);
            }
            // Permit join status
            else if (cmd0 == (AREQ | ZDO) && cmd1 == ZDO_PERMIT_JOIN_IND) {
                printf("    Permit join duration: %d seconds\n", buf[4]);
            }
            // AF incoming message
            else if (cmd0 == (AREQ | AF) && cmd1 == AF_INCOMING_MSG) {
                printf("\n*** INCOMING MESSAGE ***\n");
                print_hex("    Data", buf, n);
            }
            else {
                // Other messages
                printf("    [%02x %02x] ", cmd0, cmd1);
                print_hex("", buf, n);
            }
        }

        // Print status every 30 seconds
        time_t now = time(NULL);
        if ((now - start) % 30 == 0 && (now - start) > 0) {
            int remaining = 254 - (now - start);
            if (remaining > 0) {
                printf("    ... permit join: %d seconds remaining, %d device(s) found\n",
                       remaining, device_count);
            }
        }
    }

    printf("\n[7] Disabling permit join...\n");
    permit_join[3] = 0x00;  // Duration: 0
    znp_send(g_fd, SREQ, ZDO, ZDO_MGMT_PERMIT_JOIN_REQ, permit_join, 5);
    n = znp_recv(g_fd, buf, sizeof(buf), 1000);

    printf("\nPairing session ended. %d device(s) joined.\n", device_count);
    close(g_fd);
    return 0;
}
