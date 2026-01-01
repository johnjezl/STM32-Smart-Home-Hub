#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

// Disable stdout buffering
#define LOG(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

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
    LOG("%s: ", label);
    for (int i = 0; i < len; i++) LOG("%02x ", data[i]);
    LOG("\n");
}

#define SREQ 0x20
#define AREQ 0x40
#define SRSP 0x60
#define SYS  0x01
#define SAPI 0x06
#define UTIL 0x07
#define APP_CNF 0x0F
#define ZDO  0x05

// NV Item IDs
#define ZCD_NV_STARTUP_OPTION     0x0003
#define ZCD_NV_LOGICAL_TYPE       0x0087
#define ZCD_NV_ZDO_DIRECT_CB      0x008F
#define ZCD_NV_CHANLIST           0x0084
#define ZCD_NV_PRECFGKEYS_ENABLE  0x0063

int main(int argc, char *argv[]) {
    const char *port = argc > 1 ? argv[1] : "/dev/ttyUSB0";
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

    LOG("=== Zigbee Coordinator Initialization ===\n");
    LOG("Port: %s\n\n", port);

    // Step 1: Ping
    LOG("[1] Pinging device...\n");
    znp_send(g_fd, SREQ, SYS, 0x01, NULL, 0);  // SYS_PING
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n < 5 || buf[0] != 0xFE) {
        LOG("    ERROR: No response\n");
        close(g_fd);
        return 1;
    }
    print_hex("    Response", buf, n);

    // Step 2: Reset to factory defaults
    LOG("\n[2] Setting startup option to clear state...\n");
    // SYS_OSAL_NV_WRITE: ZCD_NV_STARTUP_OPTION = 0x03 (clear config + state)
    unsigned char nv_startup[] = {
        0x03, 0x00,  // NV ID: ZCD_NV_STARTUP_OPTION (0x0003)
        0x00,        // Offset
        0x01,        // Length
        0x03         // Value: Clear config + clear state
    };
    znp_send(g_fd, SREQ, SYS, 0x09, nv_startup, sizeof(nv_startup));
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    print_hex("    Response", buf, n);

    // Step 3: Reset device
    LOG("\n[3] Resetting device...\n");
    unsigned char reset[] = {0x01};  // Type 1 = serial bootloader
    znp_send(g_fd, AREQ, SYS, 0x00, reset, 1);
    sleep(3);
    tcflush(g_fd, TCIOFLUSH);

    // Ping again
    LOG("    Pinging after reset...\n");
    znp_send(g_fd, SREQ, SYS, 0x01, NULL, 0);
    n = znp_recv(g_fd, buf, sizeof(buf), 3000);
    if (n < 5) {
        LOG("    ERROR: No response after reset\n");
        close(g_fd);
        return 1;
    }
    print_hex("    Response", buf, n);

    // Step 4: Set logical type to Coordinator
    LOG("\n[4] Setting logical type to COORDINATOR...\n");
    unsigned char nv_logtype[] = {
        0x87, 0x00,  // NV ID: ZCD_NV_LOGICAL_TYPE (0x0087)
        0x00,        // Offset
        0x01,        // Length
        0x00         // Value: 0 = Coordinator
    };
    znp_send(g_fd, SREQ, SYS, 0x09, nv_logtype, sizeof(nv_logtype));
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    print_hex("    Response", buf, n);

    // Step 5: Enable direct callbacks
    LOG("\n[5] Enabling ZDO direct callbacks...\n");
    unsigned char nv_zdocb[] = {
        0x8F, 0x00,  // NV ID: ZCD_NV_ZDO_DIRECT_CB (0x008F)
        0x00,        // Offset
        0x01,        // Length
        0x01         // Value: 1 = Enable
    };
    znp_send(g_fd, SREQ, SYS, 0x09, nv_zdocb, sizeof(nv_zdocb));
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    print_hex("    Response", buf, n);

    // Step 6: Set channel mask (all Zigbee channels 11-26)
    LOG("\n[6] Setting channel mask (all channels 11-26)...\n");
    unsigned char nv_chan[] = {
        0x84, 0x00,  // NV ID: ZCD_NV_CHANLIST (0x0084)
        0x00,        // Offset
        0x04,        // Length
        0x00, 0xF8, 0xFF, 0x07  // Channels 11-26 = 0x07FFF800
    };
    znp_send(g_fd, SREQ, SYS, 0x09, nv_chan, sizeof(nv_chan));
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    print_hex("    Response", buf, n);

    // Step 7: Reset again to apply settings
    LOG("\n[7] Resetting to apply settings...\n");
    znp_send(g_fd, AREQ, SYS, 0x00, reset, 1);
    sleep(3);
    tcflush(g_fd, TCIOFLUSH);

    // Ping
    znp_send(g_fd, SREQ, SYS, 0x01, NULL, 0);
    n = znp_recv(g_fd, buf, sizeof(buf), 3000);
    print_hex("    Ping", buf, n);

    // Step 8: Start network
    LOG("\n[8] Starting Zigbee network (ZDO_STARTUP_FROM_APP)...\n");
    unsigned char startup[] = {0x00};  // StartDelay = 0
    znp_send(g_fd, SREQ, ZDO, 0x40, startup, 1);
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    print_hex("    Response", buf, n);

    // Wait for network to form
    LOG("    Waiting for network formation...\n");
    sleep(5);

    // Drain any async messages
    while ((n = znp_recv(g_fd, buf, sizeof(buf), 500)) > 0) {
        print_hex("    Async", buf, n);
    }

    // Step 9: Check device info
    LOG("\n[9] Checking device info...\n");
    znp_send(g_fd, SREQ, UTIL, 0x00, NULL, 0);  // UTIL_GET_DEVICE_INFO
    n = znp_recv(g_fd, buf, sizeof(buf), 2000);
    if (n >= 17 && buf[0] == 0xFE) {
        print_hex("    Raw", buf, n);
        LOG("    IEEE Address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
               buf[12], buf[11], buf[10], buf[9], buf[8], buf[7], buf[6], buf[5]);
        LOG("    Short Address: 0x%02x%02x\n", buf[14], buf[13]);

        int devtype = buf[15];
        LOG("    Device Type: %s (%d)\n",
            devtype == 0 ? "COORDINATOR" : devtype == 1 ? "Router" : "End Device", devtype);
        LOG("    Device State: %d", buf[16]);
        if (buf[16] == 9) LOG(" (DEV_ZB_COORD - Network formed!)");
        else if (buf[16] == 0) LOG(" (DEV_HOLD)");
        else if (buf[16] == 1) LOG(" (DEV_INIT)");
        LOG("\n");

        if (devtype == 0 && buf[16] == 9) {
            LOG("\n*** SUCCESS: Coordinator configured and network formed! ***\n");
        } else {
            LOG("\n*** WARNING: Device type or state not as expected ***\n");
        }
    }

    close(g_fd);
    return 0;
}
