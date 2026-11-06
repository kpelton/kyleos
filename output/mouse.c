#include <asm/asm.h>
#include <include/types.h>
#include <irq/irq.h>
#include <output/mouse.h>
#include <output/output.h>

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

#define PS2_STATUS_OUTPUT 0x01
#define PS2_STATUS_INPUT 0x02
#define PS2_ENABLE_AUX 0xa8
#define PS2_READ_CONFIG 0x20
#define PS2_WRITE_CONFIG 0x60
#define PS2_WRITE_MOUSE 0xd4
#define MOUSE_DEFAULTS 0xf6
#define MOUSE_RESET 0xff
#define MOUSE_SET_SAMPLE_RATE 0xf3
#define MOUSE_GET_DEVICE_ID 0xf2
#define MOUSE_SET_RESOLUTION 0xe8
#define MOUSE_SCALING_1_TO_1 0xe6
#define MOUSE_ENABLE_STREAM 0xf4
#define MOUSE_ACK 0xfa
#define MOUSE_SELF_TEST_OK 0xaa

#define MOUSE_SAMPLE_RATE_200 200
#define MOUSE_SAMPLE_RATE_100 100
#define MOUSE_SAMPLE_RATE_80 80
#define MOUSE_RESOLUTION_8_COUNTS 3

#define MOUSE_IRQ 12
#define MOUSE_QUEUE_SIZE 128

static struct kyleos_mouse_event mouse_queue[MOUSE_QUEUE_SIZE];
/* IRQ12 is the sole producer; /dev/mouse and syscall 30 share the consumer. */
static int mouse_head;
static int mouse_tail;
static uint8_t packet[4];
static int packet_index;
static int packet_size;
static bool mouse_ready;

static bool wait_write(void)
{
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_INPUT) == 0)
            return true;
        io_wait();
    }
    return false;
}

static bool wait_read(void)
{
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & PS2_STATUS_OUTPUT) != 0)
            return true;
        io_wait();
    }
    return false;
}

static bool write_controller(uint8_t value)
{
    if (!wait_write())
        return false;
    outb(PS2_COMMAND, value);
    return true;
}

static bool write_data(uint8_t value)
{
    if (!wait_write())
        return false;
    outb(PS2_DATA, value);
    return true;
}

static bool read_data(uint8_t *value)
{
    if (!wait_read())
        return false;
    *value = inb(PS2_DATA);
    return true;
}

static bool write_mouse(uint8_t value)
{
    uint8_t response;

    if (!write_controller(PS2_WRITE_MOUSE) || !write_data(value) ||
        !read_data(&response))
        return false;
    return response == MOUSE_ACK;
}

static bool reset_mouse(uint8_t *device_id)
{
    uint8_t response;

    if (!write_controller(PS2_WRITE_MOUSE) || !write_data(MOUSE_RESET) ||
        !read_data(&response) || response != MOUSE_ACK ||
        !read_data(&response) || response != MOUSE_SELF_TEST_OK ||
        !read_data(device_id))
        return false;
    return true;
}

static bool set_sample_rate(uint8_t rate)
{
    return write_mouse(MOUSE_SET_SAMPLE_RATE) && write_mouse(rate);
}

static bool get_device_id(uint8_t *device_id)
{
    return write_mouse(MOUSE_GET_DEVICE_ID) && read_data(device_id);
}

static void queue_event(int dx, int dy, uint8_t buttons, int wheel)
{
    int head = __atomic_load_n(&mouse_head, __ATOMIC_RELAXED);
    int next = (head + 1) % MOUSE_QUEUE_SIZE;

    /* A full queue drops the newest motion packet; never block in IRQ12. */
    if (next == __atomic_load_n(&mouse_tail, __ATOMIC_ACQUIRE))
        return;
    mouse_queue[head].dx = dx;
    mouse_queue[head].dy = dy;
    mouse_queue[head].buttons = buttons;
    mouse_queue[head].wheel = wheel;
    mouse_queue[head].reserved[0] = 0;
    mouse_queue[head].reserved[1] = 0;
    __atomic_store_n(&mouse_head, next, __ATOMIC_RELEASE);
}

static int dequeue_event(struct kyleos_mouse_event *event)
{
    int tail = __atomic_load_n(&mouse_tail, __ATOMIC_RELAXED);

    if (__atomic_load_n(&mouse_head, __ATOMIC_ACQUIRE) == tail)
        return 0;
    *event = mouse_queue[tail];
    __atomic_store_n(&mouse_tail, (tail + 1) % MOUSE_QUEUE_SIZE,
                     __ATOMIC_RELEASE);
    return 1;
}

void mouse_init(void)
{
    uint8_t config;
    uint8_t device_id = 0;

    mouse_head = mouse_tail = packet_index = 0;
    packet_size = 3;
    mouse_ready = false;

    /* IRQ1 and IRQ12 share port 0x60. Keep their handlers from consuming
     * command responses while the auxiliary device is initialized. */
    IRQ_set_mask(1);
    IRQ_set_mask(MOUSE_IRQ);

    if (!write_controller(PS2_ENABLE_AUX) ||
        !write_controller(PS2_READ_CONFIG) || !read_data(&config)) {
        klog("PS/2 mouse: controller unavailable\n");
        IRQ_clear_mask(1);
        return;
    }

    config |= 0x02;  /* Enable IRQ12. */
    config &= ~0x20; /* Enable the auxiliary clock. */
    if (!write_controller(PS2_WRITE_CONFIG) || !write_data(config) ||
        !reset_mouse(&device_id) ||
        !write_mouse(MOUSE_DEFAULTS) ||
        !set_sample_rate(MOUSE_SAMPLE_RATE_200) ||
        !set_sample_rate(MOUSE_SAMPLE_RATE_100) ||
        !set_sample_rate(MOUSE_SAMPLE_RATE_80) ||
        !get_device_id(&device_id) ||
        !write_mouse(MOUSE_SCALING_1_TO_1) ||
        !write_mouse(MOUSE_SET_RESOLUTION) ||
        !write_mouse(MOUSE_RESOLUTION_8_COUNTS) ||
        !set_sample_rate(MOUSE_SAMPLE_RATE_200) ||
        !write_mouse(MOUSE_ENABLE_STREAM)) {
        klog("PS/2 mouse: initialization failed\n");
        IRQ_clear_mask(1);
        return;
    }

    if (device_id == 3 || device_id == 4)
        packet_size = 4;
    packet_index = 0;
    mouse_ready = true;
    IRQ_clear_mask(1);
    IRQ_clear_mask(MOUSE_IRQ);
    klog("PS/2 mouse ready (device %d, %d-byte packets)\n",
         device_id, packet_size);
}

void mouse_irq(void)
{
    uint8_t value = inb(PS2_DATA);

    if (!mouse_ready) {
        PIC_sendEOI(MOUSE_IRQ);
        return;
    }

    if (packet_index == 0 && (value & 0x08) == 0) {
        PIC_sendEOI(MOUSE_IRQ);
        return;
    }

    packet[packet_index++] = value;
    if (packet_index == packet_size) {
        packet_index = 0;
        if ((packet[0] & 0xc0) == 0) {
            int dx = (int)(signed char)packet[1];
            int dy = (int)(signed char)packet[2];
            int wheel = 0;

            if (packet_size == 4) {
                wheel = packet[3] & 0x0f;
                if (wheel & 0x08) wheel -= 16;
                /* PS/2 reports the opposite sign from GUI scroll APIs. */
                wheel = -wheel;
            }

            queue_event(dx, dy, packet[0] & 0x07, wheel);
        }
    }
    PIC_sendEOI(MOUSE_IRQ);
}

int mouse_poll(int *dx, int *dy, uint8_t *buttons)
{
    struct kyleos_mouse_event event;

    if (dx == NULL || dy == NULL || buttons == NULL)
        return 0;
    if (!dequeue_event(&event))
        return 0;
    *dx = event.dx;
    *dy = event.dy;
    *buttons = event.buttons;
    return 1;
}

int mouse_read_events(void *buffer, uint32_t count)
{
    struct kyleos_mouse_event *events = buffer;
    uint32_t capacity;
    uint32_t used = 0;

    if (buffer == NULL || count < sizeof(struct kyleos_mouse_event))
        return -1;
    capacity = count / sizeof(struct kyleos_mouse_event);
    while (used < capacity && dequeue_event(&events[used]))
        used++;
    return used * sizeof(struct kyleos_mouse_event);
}
