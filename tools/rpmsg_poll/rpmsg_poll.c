/**
 * rpmsg_poll - VirtIO vring polling daemon for M4 communication
 *
 * This tool directly polls the VirtIO vring shared memory to receive
 * messages from the M4 core without relying on IPCC notifications.
 *
 * Used as a workaround when TrustZone blocks M4 access to IPCC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>

/* VirtIO vring addresses from device tree */
#define VRING0_ADDR     0x10040000  /* TX vring (M4 -> A7) */
#define VRING1_ADDR     0x10041000  /* RX vring (A7 -> M4) */
#define VBUFFER_ADDR    0x10042000  /* Shared message buffers */
#define VRING_SIZE      0x1000      /* 4KB per vring */
#define VBUFFER_SIZE    0x4000      /* 16KB buffer pool */

/* VirtIO vring configuration */
#define VRING_NUM_DESCS  16
#define VRING_ALIGN      4096

/* VirtIO descriptor */
struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

/* VirtIO available ring */
struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VRING_NUM_DESCS];
};

/* VirtIO used ring entry */
struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

/* VirtIO used ring */
struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[VRING_NUM_DESCS];
};

/* RPMsg header */
struct rpmsg_hdr {
    uint32_t src;
    uint32_t dst;
    uint32_t reserved;
    uint16_t len;
    uint16_t flags;
    uint8_t data[];
};

/* Calculate vring offsets */
#define VRING_DESC_OFFSET   0
#define VRING_AVAIL_OFFSET  (VRING_NUM_DESCS * sizeof(struct vring_desc))
/* Used ring is aligned to VRING_ALIGN after avail */
#define VRING_USED_OFFSET   0xA0  /* For 16 descriptors with proper alignment */

static volatile int running = 1;
static int verbose = 0;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

void print_message(const struct rpmsg_hdr *hdr) {
    printf("RPMsg: src=0x%04x dst=0x%04x len=%u\n",
           hdr->src, hdr->dst, hdr->len);

    if (hdr->len > 0) {
        /* Try to print as string if printable */
        int printable = 1;
        for (uint16_t i = 0; i < hdr->len && printable; i++) {
            if (hdr->data[i] < 0x20 && hdr->data[i] != '\n' &&
                hdr->data[i] != '\r' && hdr->data[i] != '\t' &&
                hdr->data[i] != 0) {
                printable = 0;
            }
        }

        if (printable) {
            printf("  Data: %.*s\n", hdr->len, hdr->data);
        } else {
            printf("  Hex:\n");
            print_hex(hdr->data, hdr->len);
        }
    }
}

void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -i, --interval MS    Poll interval in milliseconds (default: 100)\n");
    fprintf(stderr, "  -v, --verbose        Verbose output\n");
    fprintf(stderr, "  -1, --once           Read once and exit\n");
    fprintf(stderr, "  -d, --dump           Dump vring state and exit\n");
    fprintf(stderr, "  -h, --help           Show this help\n");
}

void dump_vring_state(void *vring_base, void *buffer_base) {
    struct vring_desc *desc = (struct vring_desc *)vring_base;
    struct vring_avail *avail = (struct vring_avail *)((uint8_t *)vring_base + VRING_AVAIL_OFFSET);
    struct vring_used *used = (struct vring_used *)((uint8_t *)vring_base + VRING_USED_OFFSET);

    printf("=== VirtIO Vring State ===\n");
    printf("Available ring: flags=0x%04x idx=%u\n", avail->flags, avail->idx);
    printf("Used ring:      flags=0x%04x idx=%u\n", used->flags, used->idx);

    printf("\nDescriptors:\n");
    for (int i = 0; i < VRING_NUM_DESCS; i++) {
        if (desc[i].addr != 0 || desc[i].len != 0) {
            printf("  [%2d] addr=0x%08lx len=%4u flags=0x%04x next=%u\n",
                   i, (unsigned long)desc[i].addr, desc[i].len,
                   desc[i].flags, desc[i].next);
        }
    }

    printf("\nUsed ring entries:\n");
    for (int i = 0; i < VRING_NUM_DESCS; i++) {
        if (used->ring[i].id != 0 || used->ring[i].len != 0) {
            printf("  [%2d] id=%u len=%u\n", i, used->ring[i].id, used->ring[i].len);

            /* Try to show the message content */
            if (used->ring[i].id < VRING_NUM_DESCS) {
                uint32_t buf_offset = (uint32_t)(desc[used->ring[i].id].addr - VBUFFER_ADDR);
                if (buf_offset < VBUFFER_SIZE) {
                    struct rpmsg_hdr *hdr = (struct rpmsg_hdr *)((uint8_t *)buffer_base + buf_offset);
                    print_message(hdr);
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int interval_ms = 100;
    int once = 0;
    int dump = 0;

    static struct option long_options[] = {
        {"interval", required_argument, 0, 'i'},
        {"verbose",  no_argument,       0, 'v'},
        {"once",     no_argument,       0, '1'},
        {"dump",     no_argument,       0, 'd'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:v1dh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                interval_ms = atoi(optarg);
                if (interval_ms < 1) interval_ms = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case '1':
                once = 1;
                break;
            case 'd':
                dump = 1;
                break;
            case 'h':
            default:
                print_usage(argv[0]);
                return opt == 'h' ? 0 : 1;
        }
    }

    /* Open /dev/mem for physical memory access */
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem (need root)");
        return 1;
    }

    /* Map the TX vring (M4 -> A7) */
    void *vring0 = mmap(NULL, VRING_SIZE, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd, VRING0_ADDR);
    if (vring0 == MAP_FAILED) {
        perror("mmap vring0");
        close(fd);
        return 1;
    }

    /* Map the shared buffer pool */
    void *vbuffer = mmap(NULL, VBUFFER_SIZE, PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, VBUFFER_ADDR);
    if (vbuffer == MAP_FAILED) {
        perror("mmap vbuffer");
        munmap(vring0, VRING_SIZE);
        close(fd);
        return 1;
    }

    if (verbose) {
        printf("Mapped vring0 at %p (phys 0x%08x)\n", vring0, VRING0_ADDR);
        printf("Mapped vbuffer at %p (phys 0x%08x)\n", vbuffer, VBUFFER_ADDR);
    }

    if (dump) {
        dump_vring_state(vring0, vbuffer);
        munmap(vbuffer, VBUFFER_SIZE);
        munmap(vring0, VRING_SIZE);
        close(fd);
        return 0;
    }

    /* Set up signal handler for clean exit */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Get pointers to vring structures */
    struct vring_desc *desc = (struct vring_desc *)vring0;
    struct vring_used *used = (struct vring_used *)((uint8_t *)vring0 + VRING_USED_OFFSET);

    /* Track the last seen used index */
    uint16_t last_used_idx = used->idx;

    if (verbose) {
        printf("Starting poll, initial used.idx=%u\n", last_used_idx);
    }

    printf("Polling for M4 messages (interval=%dms)...\n", interval_ms);
    fflush(stdout);

    struct timespec ts = {
        .tv_sec = interval_ms / 1000,
        .tv_nsec = (interval_ms % 1000) * 1000000
    };

    while (running) {
        /* Check if new messages are available */
        uint16_t current_idx = used->idx;

        if (current_idx != last_used_idx) {
            /* Process new messages */
            while (last_used_idx != current_idx) {
                uint16_t ring_idx = last_used_idx % VRING_NUM_DESCS;
                uint32_t desc_id = used->ring[ring_idx].id;

                if (desc_id < VRING_NUM_DESCS) {
                    /* Get buffer address from descriptor */
                    uint64_t buf_addr = desc[desc_id].addr;
                    uint32_t buf_offset = (uint32_t)(buf_addr - VBUFFER_ADDR);

                    if (buf_offset < VBUFFER_SIZE) {
                        struct rpmsg_hdr *hdr = (struct rpmsg_hdr *)((uint8_t *)vbuffer + buf_offset);
                        print_message(hdr);
                        fflush(stdout);
                    } else if (verbose) {
                        printf("Warning: buffer offset 0x%x out of range\n", buf_offset);
                    }
                } else if (verbose) {
                    printf("Warning: descriptor id %u out of range\n", desc_id);
                }

                last_used_idx++;
            }

            if (once) {
                break;
            }
        }

        nanosleep(&ts, NULL);
    }

    printf("\nExiting...\n");

    munmap(vbuffer, VBUFFER_SIZE);
    munmap(vring0, VRING_SIZE);
    close(fd);

    return 0;
}
