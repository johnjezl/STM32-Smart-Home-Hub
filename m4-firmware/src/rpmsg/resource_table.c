/**
 * OpenAMP Resource Table for STM32MP1 M4 Core
 *
 * This resource table is used by the remoteproc framework on Linux
 * to configure shared memory and communication channels.
 */

#include <stdint.h>
#include <stddef.h>

/* Resource table magic and version */
#define RSC_TABLE_MAGIC     0x6E737274
#define RSC_TABLE_VERSION   1

/* Resource types */
#define RSC_CARVEOUT        0
#define RSC_DEVMEM          1
#define RSC_TRACE           2
#define RSC_VDEV            3
#define RSC_VENDOR          4

/* Virtio device types */
#define VIRTIO_ID_RPMSG     7

/* Resource table entry flags */
#define RSC_FLAG_F_VRING    (1 << 0)

/* VirtIO features */
#define VIRTIO_RPMSG_F_NS   0  /* Name service announcement */

/* Shared memory addresses - must match device tree configuration */
/* DT: vdev0vring0@10040000 (0x1000), vdev0vring1@10041000 (0x1000) */
#define VRING0_TX_ADDR      0x10040000
#define VRING0_RX_ADDR      0x10041000
#define VRING_ALIGNMENT     16
#define VRING_NUM_BUFFS     8

/* DT: vdev0buffer@10042000 (0x4000) - shared buffer area */
#define VDEV0_BUFFER_ADDR   0x10042000

/* Trace buffer - use portion of mcu-rsc-table area after resource table */
#define TRACE_BUFFER_ADDR   0x10049000
#define TRACE_BUFFER_SIZE   0x1000

/**
 * VirtIO ring descriptor
 */
struct fw_rsc_vdev_vring {
    uint32_t da;            /* Device address (from M4 perspective) */
    uint32_t align;         /* Alignment requirements */
    uint32_t num;           /* Number of buffers */
    uint32_t notifyid;      /* Notification ID */
    uint32_t pa;            /* Physical address (filled by host) */
};

/**
 * VirtIO device resource
 */
struct fw_rsc_vdev {
    uint32_t type;          /* RSC_VDEV */
    uint32_t id;            /* Virtio device ID */
    uint32_t notifyid;      /* Notification ID for kicks */
    uint32_t dfeatures;     /* Device features (negotiated) */
    uint32_t gfeatures;     /* Guest features (what we support) */
    uint32_t config_len;    /* Config space length */
    uint8_t status;         /* Device status */
    uint8_t num_of_vrings;  /* Number of vrings */
    uint8_t reserved[2];    /* Padding */
    struct fw_rsc_vdev_vring vring[2];  /* TX and RX vrings */
};

/**
 * Trace buffer resource
 */
struct fw_rsc_trace {
    uint32_t type;          /* RSC_TRACE */
    uint32_t da;            /* Device address */
    uint32_t len;           /* Buffer length */
    uint32_t reserved;      /* Padding */
    char name[32];          /* Trace buffer name */
};

/**
 * Resource table header
 */
struct resource_table {
    uint32_t ver;           /* Version (must be 1) */
    uint32_t num;           /* Number of resources */
    uint32_t reserved[2];   /* Reserved (must be 0) */
    uint32_t offset[2];     /* Offset to each resource */

    /* Resource entries */
    struct fw_rsc_vdev rpmsg_vdev;
    struct fw_rsc_trace trace;
};

/**
 * Resource table instance
 * Placed in .resource_table section for remoteproc to find
 */
__attribute__((section(".resource_table")))
struct resource_table resource_table = {
    .ver = RSC_TABLE_VERSION,
    .num = 2,
    .reserved = {0, 0},
    .offset = {
        offsetof(struct resource_table, rpmsg_vdev),
        offsetof(struct resource_table, trace),
    },

    /* RPMsg virtio device */
    .rpmsg_vdev = {
        .type = RSC_VDEV,
        .id = VIRTIO_ID_RPMSG,
        .notifyid = 0,
        .dfeatures = (1 << VIRTIO_RPMSG_F_NS),
        .gfeatures = 0,
        .config_len = 0,
        .status = 0,
        .num_of_vrings = 2,
        .reserved = {0, 0},
        .vring = {
            /* TX vring (M4 -> A7) */
            {
                .da = VRING0_TX_ADDR,
                .align = VRING_ALIGNMENT,
                .num = VRING_NUM_BUFFS,
                .notifyid = 0,
                .pa = 0,  /* Filled by host */
            },
            /* RX vring (A7 -> M4) */
            {
                .da = VRING0_RX_ADDR,
                .align = VRING_ALIGNMENT,
                .num = VRING_NUM_BUFFS,
                .notifyid = 1,
                .pa = 0,  /* Filled by host */
            },
        },
    },

    /* Trace buffer for debug output */
    .trace = {
        .type = RSC_TRACE,
        .da = TRACE_BUFFER_ADDR,
        .len = TRACE_BUFFER_SIZE,
        .reserved = 0,
        .name = "cm4_log",
    },
};

/**
 * Export resource table pointer for OpenAMP library
 */
void* get_resource_table(int rsc_id, int* len) {
    (void)rsc_id;
    *len = sizeof(resource_table);
    return &resource_table;
}
