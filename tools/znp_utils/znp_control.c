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
#define AREQ 0x40
#define AF   0x04

// Register AF endpoint
int af_register(int fd, unsigned char endpoint) {
    // AF_REGISTER payload:
    // Endpoint(1) + AppProfID(2) + AppDeviceId(2) + AppDevVer(1) + LatencyReq(1) +
    // AppNumInClusters(1) + AppInClusterList(n*2) + AppNumOutClusters(1) + AppOutClusterList(n*2)

    unsigned char payload[] = {
        endpoint,       // Endpoint
        0x04, 0x01,     // ProfileID: Home Automation (0x0104)
        0x00, 0x00,     // DeviceID: On/Off Switch
        0x00,           // DeviceVersion
        0x00,           // LatencyReq
        0x01,           // NumInClusters
        0x06, 0x00,     // InCluster: On/Off (0x0006)
        0x01,           // NumOutClusters
        0x06, 0x00      // OutCluster: On/Off (0x0006)
    };

    return znp_send(fd, SREQ, AF, 0x00, payload, sizeof(payload));
}

int send_zcl_command(int fd, unsigned short dst_addr, unsigned char dst_ep,
                     unsigned short cluster, unsigned char cmd_id) {
    static unsigned char trans_id = 1;

    // ZCL frame: FrameControl(1) + SeqNum(1) + CommandID(1)
    unsigned char zcl_frame[] = {
        0x01,       // Frame control: cluster-specific, client-to-server
        trans_id++, // Transaction sequence number
        cmd_id      // Command ID
    };

    unsigned char payload[20];
    int idx = 0;

    payload[idx++] = dst_addr & 0xFF;
    payload[idx++] = (dst_addr >> 8) & 0xFF;
    payload[idx++] = dst_ep;
    payload[idx++] = 1;  // SrcEndpoint
    payload[idx++] = cluster & 0xFF;
    payload[idx++] = (cluster >> 8) & 0xFF;
    payload[idx++] = trans_id;
    payload[idx++] = 0x00;  // Options
    payload[idx++] = 0x1E;  // Radius
    payload[idx++] = sizeof(zcl_frame);
    memcpy(&payload[idx], zcl_frame, sizeof(zcl_frame));
    idx += sizeof(zcl_frame);

    return znp_send(fd, SREQ, AF, 0x01, payload, idx);
}

int main(int argc, char *argv[]) {
    const char *port = "/dev/ttyUSB0";
    unsigned char buf[256];
    int n;

    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 3) {
        printf("Usage: %s <device_addr_hex> <command> [endpoint]\n", argv[0]);
        printf("Commands: on, off, toggle\n");
        printf("Example: %s 3190 toggle 1\n", argv[0]);
        return 1;
    }

    unsigned short addr = strtoul(argv[1], NULL, 16);
    const char *cmd = argv[2];
    unsigned char dst_ep = argc > 3 ? atoi(argv[3]) : 1;

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

    // First register an AF endpoint
    printf("Registering AF endpoint 1...\n");
    af_register(g_fd, 1);
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n > 0) {
        print_hex("Register response", buf, n);
        if (n >= 5 && buf[4] == 0x00) {
            printf("Endpoint registered successfully\n");
        } else if (n >= 5 && buf[4] == 0xB8) {
            printf("Endpoint already registered (OK)\n");
        }
    }

    printf("\nDevice: 0x%04X, Endpoint: %d\n", addr, dst_ep);
    printf("Command: %s\n\n", cmd);

    unsigned char zcl_cmd;
    if (strcmp(cmd, "on") == 0) {
        zcl_cmd = 0x01;
        printf("Sending ON command...\n");
    } else if (strcmp(cmd, "off") == 0) {
        zcl_cmd = 0x00;
        printf("Sending OFF command...\n");
    } else if (strcmp(cmd, "toggle") == 0) {
        zcl_cmd = 0x02;
        printf("Sending TOGGLE command...\n");
    } else {
        printf("Unknown command: %s\n", cmd);
        close(g_fd);
        return 1;
    }

    send_zcl_command(g_fd, addr, dst_ep, 0x0006, zcl_cmd);

    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n > 0) {
        print_hex("Response", buf, n);
        if (n >= 5 && buf[4] == 0x00) {
            printf("Command sent successfully!\n");
        } else {
            printf("Command failed, status: 0x%02X\n", buf[4]);
        }
    }

    // Wait for data confirm
    printf("\nWaiting for confirmation...\n");
    for (int i = 0; i < 3; i++) {
        n = znp_recv(g_fd, buf, sizeof(buf), 1000);
        if (n > 0) {
            print_hex("Async", buf, n);
            // AF_DATA_CONFIRM is 0x44 0x80
            if (n >= 5 && buf[2] == 0x44 && buf[3] == 0x80) {
                if (buf[4] == 0x00) {
                    printf("Device confirmed receipt!\n");
                }
            }
        }
    }

    close(g_fd);
    return 0;
}
