/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021 Jean Gressmann <jean@0x42.de>
 *
 */


#include <FreeRTOS.h>
#include <task.h>


#include <tusb.h>
#include <sllin_board.h>
#include <usb_descriptors.h>
#include <leds.h>

#if SLLIN_ENABLE_E2E
static const uint8_t Crc8_SAE_J1850[256] = {
	0x00, 0x1D, 0x3A, 0x27, 0x74, 0x69, 0x4E, 0x53, 0xE8, 0xF5, 0xD2, 0xCF, 0x9C, 0x81, 0xA6, 0xBB,
	0xCD, 0xD0, 0xF7, 0xEA, 0xB9, 0xA4, 0x83, 0x9E, 0x25, 0x38, 0x1F, 0x02, 0x51, 0x4C, 0x6B, 0x76,
	0x87, 0x9A, 0xBD, 0xA0, 0xF3, 0xEE, 0xC9, 0xD4, 0x6F, 0x72, 0x55, 0x48, 0x1B, 0x06, 0x21, 0x3C,
	0x4A, 0x57, 0x70, 0x6D, 0x3E, 0x23, 0x04, 0x19, 0xA2, 0xBF, 0x98, 0x85, 0xD6, 0xCB, 0xEC, 0xF1,
	0x13, 0x0E, 0x29, 0x34, 0x67, 0x7A, 0x5D, 0x40, 0xFB, 0xE6, 0xC1, 0xDC, 0x8F, 0x92, 0xB5, 0xA8,
	0xDE, 0xC3, 0xE4, 0xF9, 0xAA, 0xB7, 0x90, 0x8D, 0x36, 0x2B, 0x0C, 0x11, 0x42, 0x5F, 0x78, 0x65,
	0x94, 0x89, 0xAE, 0xB3, 0xE0, 0xFD, 0xDA, 0xC7, 0x7C, 0x61, 0x46, 0x5B, 0x08, 0x15, 0x32, 0x2F,
	0x59, 0x44, 0x63, 0x7E, 0x2D, 0x30, 0x17, 0x0A, 0xB1, 0xAC, 0x8B, 0x96, 0xC5, 0xD8, 0xFF, 0xE2,
	0x26, 0x3B, 0x1C, 0x01, 0x52, 0x4F, 0x68, 0x75, 0xCE, 0xD3, 0xF4, 0xE9, 0xBA, 0xA7, 0x80, 0x9D,
	0xEB, 0xF6, 0xD1, 0xCC, 0x9F, 0x82, 0xA5, 0xB8, 0x03, 0x1E, 0x39, 0x24, 0x77, 0x6A, 0x4D, 0x50,
	0xA1, 0xBC, 0x9B, 0x86, 0xD5, 0xC8, 0xEF, 0xF2, 0x49, 0x54, 0x73, 0x6E, 0x3D, 0x20, 0x07, 0x1A,
	0x6C, 0x71, 0x56, 0x4B, 0x18, 0x05, 0x22, 0x3F, 0x84, 0x99, 0xBE, 0xA3, 0xF0, 0xED, 0xCA, 0xD7,
	0x35, 0x28, 0x0F, 0x12, 0x41, 0x5C, 0x7B, 0x66, 0xDD, 0xC0, 0xE7, 0xFA, 0xA9, 0xB4, 0x93, 0x8E,
	0xF8, 0xE5, 0xC2, 0xDF, 0x8C, 0x91, 0xB6, 0xAB, 0x10, 0x0D, 0x2A, 0x37, 0x64, 0x79, 0x5E, 0x43,
	0xB2, 0xAF, 0x88, 0x95, 0xC6, 0xDB, 0xFC, 0xE1, 0x5A, 0x47, 0x60, 0x7D, 0x2E, 0x33, 0x14, 0x09,
	0x7F, 0x62, 0x45, 0x58, 0x0B, 0x16, 0x31, 0x2C, 0x97, 0x8A, 0xAD, 0xB0, 0xE3, 0xFE, 0xD9, 0xC4,
};

static const uint8_t Crc8_8H2F[256] = {
	0x00, 0x2F, 0x5E, 0x71, 0xBC, 0x93, 0xE2, 0xCD, 0x57, 0x78, 0x09, 0x26, 0xEB, 0xC4, 0xB5, 0x9A,
	0xAE, 0x81, 0xF0, 0xDF, 0x12, 0x3D, 0x4C, 0x63, 0xF9, 0xD6, 0xA7, 0x88, 0x45, 0x6A, 0x1B, 0x34,
	0x73, 0x5C, 0x2D, 0x02, 0xCF, 0xE0, 0x91, 0xBE, 0x24, 0x0B, 0x7A, 0x55, 0x98, 0xB7, 0xC6, 0xE9,
	0xDD, 0xF2, 0x83, 0xAC, 0x61, 0x4E, 0x3F, 0x10, 0x8A, 0xA5, 0xD4, 0xFB, 0x36, 0x19, 0x68, 0x47,
	0xE6, 0xC9, 0xB8, 0x97, 0x5A, 0x75, 0x04, 0x2B, 0xB1, 0x9E, 0xEF, 0xC0, 0x0D, 0x22, 0x53, 0x7C,
	0x48, 0x67, 0x16, 0x39, 0xF4, 0xDB, 0xAA, 0x85, 0x1F, 0x30, 0x41, 0x6E, 0xA3, 0x8C, 0xFD, 0xD2,
	0x95, 0xBA, 0xCB, 0xE4, 0x29, 0x06, 0x77, 0x58, 0xC2, 0xED, 0x9C, 0xB3, 0x7E, 0x51, 0x20, 0x0F,
	0x3B, 0x14, 0x65, 0x4A, 0x87, 0xA8, 0xD9, 0xF6, 0x6C, 0x43, 0x32, 0x1D, 0xD0, 0xFF, 0x8E, 0xA1,
	0xE3, 0xCC, 0xBD, 0x92, 0x5F, 0x70, 0x01, 0x2E, 0xB4, 0x9B, 0xEA, 0xC5, 0x08, 0x27, 0x56, 0x79,
	0x4D, 0x62, 0x13, 0x3C, 0xF1, 0xDE, 0xAF, 0x80, 0x1A, 0x35, 0x44, 0x6B, 0xA6, 0x89, 0xF8, 0xD7,
	0x90, 0xBF, 0xCE, 0xE1, 0x2C, 0x03, 0x72, 0x5D, 0xC7, 0xE8, 0x99, 0xB6, 0x7B, 0x54, 0x25, 0x0A,
	0x3E, 0x11, 0x60, 0x4F, 0x82, 0xAD, 0xDC, 0xF3, 0x69, 0x46, 0x37, 0x18, 0xD5, 0xFA, 0x8B, 0xA4,
	0x05, 0x2A, 0x5B, 0x74, 0xB9, 0x96, 0xE7, 0xC8, 0x52, 0x7D, 0x0C, 0x23, 0xEE, 0xC1, 0xB0, 0x9F,
	0xAB, 0x84, 0xF5, 0xDA, 0x17, 0x38, 0x49, 0x66, 0xFC, 0xD3, 0xA2, 0x8D, 0x40, 0x6F, 0x1E, 0x31,
	0x76, 0x59, 0x28, 0x07, 0xCA, 0xE5, 0x94, 0xBB, 0x21, 0x0E, 0x7F, 0x50, 0x9D, 0xB2, 0xC3, 0xEC,
	0xD8, 0xF7, 0x86, 0xA9, 0x64, 0x4B, 0x3A, 0x15, 0x8F, 0xA0, 0xD1, 0xFE, 0x33, 0x1C, 0x6D, 0x42,
};

static inline uint8_t crc8_update(uint8_t const * const lut, uint8_t crc, uint8_t data)
{
	return lut[data ^ crc];
}

#define Crc8_P11_Init() 0x00
#define Crc8_P11_Update(crc, data) crc8_update(Crc8_SAE_J1850, crc, data)
#define Crc8_P11_Finalize(crc) crc

#define Crc8_P22_Init() 0xFF
#define Crc8_P22_Update(crc, data) crc8_update(Crc8_8H2F, crc, data)
#define Crc8_P22_Finalize(crc) ((crc) ^ 0xFF)


SLLIN_RAMFUNC static void update_lin_crc(uint8_t index, uint8_t id)
{
	struct sllin_frame_data * const fd = &frame_data[index];
	bool is_classic_crc = (fd->classic_crc_flags & (UINT64_C(1) << id)) != 0;
	bool is_enhanced_crc = (fd->enhanced_crc_flags & (UINT64_C(1) << id)) != 0;

	if (likely(is_classic_crc || is_enhanced_crc)) {
		// update LIN crc
		fd->crc[id] = sllin_crc_start();
		if (is_enhanced_crc) {
			fd->crc[id] = sllin_crc_update1(fd->crc[id], sllin_id_to_pid(id));
		}
		fd->crc[id] = sllin_crc_update(fd->crc[id], fd->data[id], fd->len[id]);
		fd->crc[id] = sllin_crc_finalize(fd->crc[id]);
	} else {
		LOG("ch%u: ID %02X: crc type not set\n", index, id);
	}
}
#endif // #if SLLIN_ENABLE_E2E

enum {
	SLLIN_ERROR_TERMINATOR = 7,
	SLLIN_OK_TERMINATOR = 13,
	SLLIN_QUEUE_ELEMENT_TYPE_TIME_STAMP = SLLIN_QUEUE_ELEMENT_TYPE_COUNT,
};


SLLIN_RAMFUNC static void tusb_device_task(void* param);
SLLIN_RAMFUNC static void lin_usb_task(void *param);



static struct usb {
	StackType_t usb_device_stack[configMINIMAL_SECURE_STACK_SIZE];
	StaticTask_t usb_device_stack_mem;
	uint8_t port;
} usb;

#if SLLIN_ENABLE_E2E
	#define SL_BUFFER_SIZE (1U << 8)
	#define USB_TASK_STACK_SIZE (configMINIMAL_SECURE_STACK_SIZE + configMINIMAL_SECURE_STACK_SIZE / 2)
#else
	#define SL_BUFFER_SIZE (1U << 7)
	#define USB_TASK_STACK_SIZE configMINIMAL_SECURE_STACK_SIZE
#endif

typedef uint16_t sl_index_t;

static struct lin {
	StackType_t usb_task_stack[USB_TASK_STACK_SIZE];
	StaticTask_t usb_task_mem;
	TaskHandle_t usb_task_handle;
	sllin_queue_element rx_fifo[8];
	uint8_t rx_sl_buffer[SL_BUFFER_SIZE];
	uint8_t tx_sl_buffer[SL_BUFFER_SIZE];

	sllin_conf conf;
	int led_state;
	sl_index_t rx_sl_gi; // not an index, uses full range of type
	sl_index_t rx_sl_pi; // not an index, uses full range of type
	sl_index_t rx_sl_alt_gi; // not an index, uses full range of type
	sl_index_t rx_sl_alt_pi; // not an index, uses full range of type
	sl_index_t tx_sl_gi; // not an index, uses full range of type
	sl_index_t tx_sl_pi; // not an index, uses full range of type
	uint8_t rx_fifo_gi; // not an index, uses full range of type
	uint8_t rx_fifo_pi; // not an index, uses full range of type

	bool tx_full;
	uint8_t rx_full;
	bool enabled;
	bool report_time_per_frame;
	bool report_time_periodically;

#if SLLIN_DEBUG
	int32_t last_ts;
#endif
} lins[SLLIN_BOARD_LIN_COUNT];

struct sllin_frame_data frame_data[SLLIN_BOARD_LIN_COUNT];


static inline void clear_rx_fifo(uint8_t index)
{
	struct lin *lin = NULL;

	lin = &lins[index];

	uint8_t pi = __atomic_load_n(&lin->rx_fifo_pi, __ATOMIC_ACQUIRE);
	__atomic_store_n(&lin->rx_fifo_gi, pi, __ATOMIC_RELAXED);
}


static void reset_channel_state(uint8_t index)
{
	struct lin *lin = NULL;
	struct sllin_frame_data *fd = NULL;

	lin = &lins[index];
	fd = &frame_data[index];

	lin->rx_sl_gi = 0;
	lin->rx_sl_pi = 0;
	lin->rx_sl_alt_gi = 0;
	lin->rx_sl_alt_pi = 0;
	lin->tx_sl_gi = 0;
	lin->tx_sl_pi = 0;
	lin->enabled = false;
	lin->tx_full = false;
	lin->rx_full = 0;
	lin->report_time_per_frame = false;
	lin->report_time_periodically = false;
#if SLLIN_DEBUG
	lin->last_ts = -1;
#endif

	clear_rx_fifo(index);

	// clear frame data
	memset(fd, 0, sizeof(*fd));
}


int main(void)
{
	// no uart here :(
	sllin_board_init_begin();
	LOG("sllin_board_init_begin\n");

	LOG("led_init\n");
	led_init();


	LOG("tusb_init\n");
	tusb_init();

	(void) xTaskCreateStatic(
		&tusb_device_task,
		"tusb",
		TU_ARRAY_SIZE(usb.usb_device_stack),
		NULL,
		SLLIN_TASK_PRIORITY,
		usb.usb_device_stack,
		&usb.usb_device_stack_mem);
	(void) xTaskCreateStatic(
		&led_task,
		"led",
		TU_ARRAY_SIZE(led_task_stack),
		NULL,
		SLLIN_TASK_PRIORITY,
		led_task_stack,
		&led_task_mem);


	sllin_board_init_end();
	LOG("sllin_board_init_end\n");

	for (unsigned i = 0; i < SLLIN_BOARD_LIN_COUNT; ++i) {
		struct lin *lin = &lins[i];

		lin->usb_task_handle = xTaskCreateStatic(
								&lin_usb_task,
								NULL,
								TU_ARRAY_SIZE(lin->usb_task_stack),
								(void*)(uintptr_t)i,
								SLLIN_TASK_PRIORITY,
								lin->usb_task_stack,
								&lin->usb_task_mem);

		lin->conf.master = false;
		lin->conf.sleep_timeout_ms = 4000;
		lin->conf.bitrate = 19200;

		reset_channel_state(i);
	}

	LOG("vTaskStartScheduler\n");
	vTaskStartScheduler();

	LOG("sllin_board_reset\n");
	sllin_board_reset();


	return 0;
}

SLLIN_RAMFUNC static inline void set_led(uint8_t index, uint8_t state)
{
	struct lin *lin = NULL;

	lin = &lins[index];

	if (unlikely(lin->led_state != state)) {
		lin->led_state = state;
		sllin_board_led_lin_status_set(index, state);
	}
}

SLLIN_RAMFUNC static inline uint8_t char_to_nibble(char c)
{
	if (likely(c >= '0' && c <= '9')) {
		return c - '0';
	}

	if (c >= 'a' && c <= 'f') {
		return (c - 'a') + 0xa;
	}

	return ((c - 'A') & 0xf) + 0xa;

}

SLLIN_RAMFUNC static inline char nibble_to_char(uint8_t nibble)
{
	return "0123456789abcdef"[nibble & 0xf];
}

static inline void channel_off(uint8_t index)
{
	sllin_board_lin_uninit(index);
	reset_channel_state(index);
	set_led(index, SLLIN_LIN_LED_STATUS_DISABLED);
}

static inline void channels_off(void)
{
	for (size_t i = 0; i < TU_ARRAY_SIZE(lins); ++i) {
		channel_off((uint8_t)i);
	}
}

SLLIN_RAMFUNC static inline void tx_queue(uint8_t index, uint8_t ch)
{
	struct lin *lin = &lins[index];

	lin->tx_sl_buffer[lin->tx_sl_pi++ % TU_ARRAY_SIZE(lin->tx_sl_buffer)] = ch;
}

#define tx_queue_ok(index) tx_queue(index, SLLIN_OK_TERMINATOR)
#define tx_queue_error(index) tx_queue(index, SLLIN_ERROR_TERMINATOR)

#define rx_left(index) ((sl_index_t)(lin->rx_sl_pi - lin->rx_sl_gi))

SLLIN_RAMFUNC static inline void ring_buffer_memcpy(
	uint8_t* rb_ptr,
	sl_index_t const rb_size,
	sl_index_t* in_out_put_index,
	uint8_t const* ptr,
	sl_index_t size)
{
	sl_index_t pi;

	SLLIN_DEBUG_ASSERT(rb_size >= size);

	pi = *in_out_put_index % rb_size;

	if (likely(pi + size <= rb_size)) {
		memcpy(&rb_ptr[pi], ptr, size);
	} else {
		sl_index_t copy = rb_size - pi;
		memcpy(&rb_ptr[pi], ptr, copy);
		memcpy(&rb_ptr[0], ptr + copy, size - copy);

	}

	*in_out_put_index += size;
}

SLLIN_RAMFUNC static void sllin_make_time_stamp_string(uint8_t index, uint16_t time_stamp_ms)
{
	struct lin *lin = NULL;
	uint8_t ts[4];

	lin = &lins[index];

	ts[3] = nibble_to_char(time_stamp_ms);
	time_stamp_ms >>= 4;
	ts[2] = nibble_to_char(time_stamp_ms);
	time_stamp_ms >>= 4;
	ts[1] = nibble_to_char(time_stamp_ms);
	time_stamp_ms >>= 4;
	ts[0] = nibble_to_char(time_stamp_ms);

	lin->tx_sl_buffer[lin->tx_sl_pi++ % TU_ARRAY_SIZE(lin->tx_sl_buffer)] = ts[0];
	lin->tx_sl_buffer[lin->tx_sl_pi++ % TU_ARRAY_SIZE(lin->tx_sl_buffer)] = ts[1];
	lin->tx_sl_buffer[lin->tx_sl_pi++ % TU_ARRAY_SIZE(lin->tx_sl_buffer)] = ts[2];
	lin->tx_sl_buffer[lin->tx_sl_pi++ % TU_ARRAY_SIZE(lin->tx_sl_buffer)] = ts[3];
}

SLLIN_RAMFUNC static inline void sllin_store_tx_queue_full_error_response(uint8_t index)
{
	static const char can_crc_error_frame[] = "\x07T2000000480002000000000000"; // CAN_ERR_CRTL_TX_OVERFLOW
	struct lin *lin = NULL;

	lin = &lins[index];
	ring_buffer_memcpy(
		lin->tx_sl_buffer,
		TU_ARRAY_SIZE(lin->tx_sl_buffer),
		&lin->tx_sl_pi,
		(uint8_t const*)can_crc_error_frame,
		sizeof(can_crc_error_frame) - 1);

	if (lin->report_time_per_frame) {
		uint16_t ts = sllin_time_stamp_ms();

		sllin_make_time_stamp_string(index, ts);
	}

	tx_queue_ok(index);
}

SLLIN_RAMFUNC static inline uint8_t rx_next_char(uint8_t index)
{
	struct lin *lin = &lins[index];

	return lin->rx_sl_buffer[lin->rx_sl_gi++ % TU_ARRAY_SIZE(lin->rx_sl_buffer)];
}

SLLIN_RAMFUNC static void sllin_process_command(uint8_t index, bool is_side_channel);

#if SLLIN_ENABLE_E2E

static int8_t is_hex_char(char c)
{
	if (likely(c >= '0' && c <= '9')) {
		return c - '0';
	}

	if (c >= 'a' && c <= 'f') {
		return (c - 'a') + 0xa;
	}

	if (likely(c >= 'A' && c <= 'F')) {
		return (c - 'A') + 0xa;
	}

	return -1;
}


static inline void e2e_update(uint8_t index, uint8_t id)
{
	sllin_autosar_e2e_conf* e2e = &frame_data[index].e2e[id];

	switch (e2e->profile) {
	case AUTOSAR_E2E_PROFILE_NONE:
		update_lin_crc(index, id);
		break;
	default:
		sllin_lin_task_tx_complete(index, id);
		break;
	}
}

static inline bool is_space(int c) { return c == ' '; }
static bool read_hex_number(uint8_t index, unsigned* out_value)
{
	struct lin *lin = &lins[index];
	bool result = false;

	*out_value = 0;

	while (lin->rx_sl_gi != lin->rx_sl_pi) {
		int hex = is_hex_char(rx_next_char(index));

		if (hex < 0) {
			break;
		}

		*out_value <<= 4;
		*out_value |= hex;

		result = true;
	}

	return result;
}

static void skip_space(uint8_t index)
{
	struct lin *lin = &lins[index];

	while (lin->rx_sl_gi != lin->rx_sl_pi) {
		if (is_space(lin->rx_sl_buffer[lin->rx_sl_gi % TU_ARRAY_SIZE(lin->rx_sl_buffer)])) {
			++lin->rx_sl_gi;
		} else {
			break;
		}
	}
}

static void skip_nonspace(uint8_t index)
{
	struct lin *lin = &lins[index];

	while (lin->rx_sl_gi != lin->rx_sl_pi) {
		if (!is_space(lin->rx_sl_buffer[lin->rx_sl_gi % TU_ARRAY_SIZE(lin->rx_sl_buffer)])) {
			++lin->rx_sl_gi;
		} else {
			break;
		}
	}
}

static void sllin_process_autosar(uint8_t index)
{
	struct lin *lin = &lins[index];
	uint8_t ch = 0;

	if (lin->enabled) {
		bool bad = true;

		do {
			unsigned crc_offset = 0; // bits
			unsigned id = 0;
			sllin_autosar_e2e_conf* e2e = NULL;
			uint8_t profile = AUTOSAR_E2E_PROFILE_NONE;
			bool read_profile_configuration_ok = false;

			skip_nonspace(index); // A(UTOSAR)
			skip_space(index);

			if (rx_left(index) < 4 ||
				rx_next_char(index) != 'E' ||
				rx_next_char(index) != '2' ||
				rx_next_char(index) != 'E') {
				LOG("ch%u not E2E\n", index);
				break;
			}

			skip_space(index);

			if (!read_hex_number(index, &id) || id >= 64) {
				LOG("ch%u no LIN ID\n", index);
				break;
			}

			LOG("ch%u LIN ID: %02X\n", index, id);

			skip_space(index);

			e2e = &frame_data[index].e2e[id];

			// read P11|P22|NONE
			if (rx_left(index) < 4) {
				LOG("ch%u insufficient profile length\n", index);
				break;
			}

			ch = rx_next_char(index);

			if (ch == 'P') {
				ch = rx_next_char(index);

				if (ch == '1') {
					profile = AUTOSAR_E2E_PROFILE_11;
				} else {
					profile = AUTOSAR_E2E_PROFILE_22;
				}

				skip_nonspace(index);

				skip_space(index);

				LOG("ch%u profile: %s\n", index, profile == AUTOSAR_E2E_PROFILE_11 ? "P11" : "P22");

				if (profile == AUTOSAR_E2E_PROFILE_11) {
					unsigned counter_offset = 0; // bits
					unsigned data_id;
					unsigned data_id_nibble_offset = 0; // bits
					bool data_id_mode_both = false;


					// <crc offset> <counter offset> <data_id_nibble_offset> <data_id_mode>
					if (read_hex_number(index, &crc_offset) && crc_offset <= 56) {
						skip_space(index);
						LOG("ch%u crc offset: %u\n", index, crc_offset);
						if (read_hex_number(index, &counter_offset) && counter_offset <= 60) {
							skip_space(index);
							LOG("ch%u counter offset: %u\n", index, counter_offset);
							if (read_hex_number(index, &data_id_nibble_offset) && data_id_nibble_offset <= 60) {
								skip_space(index);
								LOG("ch%u nibble offset: %u\n", index, data_id_nibble_offset);
								if (rx_left(index) > 0) {
									ch = rx_next_char(index);
									if (ch == 'B') {
										skip_nonspace(index);
										data_id_mode_both = true;
										read_profile_configuration_ok = true;
									} else if (ch == 'N') {
										skip_nonspace(index);
										data_id_mode_both = false;
										read_profile_configuration_ok = true;
									} else {
										LOG("ch%u bad mode\n", index);
									}
								}
							}
						}
					}


					if (!read_profile_configuration_ok) {
						break;
					}

					skip_space(index);

					// 16 bit data id
					if (!read_hex_number(index, &data_id)) {
						LOG("ch%u no data id\n", index);
						break;
					}

					data_id &= 0xFFFF;

					bad = false;

					e2e->profile = profile;
					e2e->u.p11.data_id = data_id;
					e2e->u.p11.crc_offset = crc_offset;
					e2e->u.p11.counter_offset = counter_offset;
					e2e->u.p11.data_id_nibble_offset = data_id_nibble_offset;
					e2e->u.p11.data_id_mode_both = data_id_mode_both;
					e2e->u.p11.counter = 0;
					e2e_update(index, id);

					LOG(
						"ch%u id=%02X E2E P11 crc=%u counter=%u nibble=%u mode=%u dataid=%04X\n",
						index, id, crc_offset, counter_offset, data_id_nibble_offset, data_id_mode_both, data_id);
				} else {
					// P22
					unsigned data_ids[16];

					if (!read_hex_number(index, &crc_offset) || crc_offset > 48) {
						LOG("ch%u no/invalid crc offset\n", index);
						break;
					}

					skip_space(index);

					read_profile_configuration_ok = true;

					for (int i = 0; i < 16; ++i) {
						if (!read_hex_number(index, &data_ids[i])) {
							LOG("ch%u no data id index %d\n", index, i);
							read_profile_configuration_ok = false;
							break;
						}

						skip_space(index);
					}

					if (!read_profile_configuration_ok) {
						break;
					}

					bad = false;

					e2e->profile = profile;
					e2e->u.p22.crc_offset = crc_offset;
					for (int i = 0; i < 16; ++i) {
						e2e->u.p22.data_ids[i] = data_ids[i];
					}
					e2e->u.p11.counter = 0;
					e2e_update(index, id);

					LOG("ch%u id=%02X E2E P22 crc=%u dataids: ", index, id, crc_offset);

					for (int i = 0; i < 16; ++i) {
						LOG("%02X ", data_ids[i]);
					}

					LOG("\n");
				}
			} else {
				bad = false;
				e2e->profile = profile;

				LOG("ch%u id=%02X E2E disabled\n", index, id);
			}
		} while (0);

		if (bad) {
			LOG("ch%u malformed command\n", index);
			tx_queue_error(index);
		} else {
			tx_queue_ok(index);
		}
	} else {
		LOG("ch%u refusing to perform AUTOSAR configuration when closed\n", index);
		tx_queue_error(index);
	}
}

static void sllin_process_sff_frame(uint8_t index, bool is_side_channel)
{
	struct lin *lin = &lins[index];
	sl_index_t const count = lin->rx_sl_pi - lin->rx_sl_gi + 1; // t
	const size_t MIN_LEN = 5;
	const sl_index_t cmd_start_gi = lin->rx_sl_gi - 1;

	if (likely(count >= MIN_LEN)) {
		uint_least16_t can_id = 0;
		uint_least16_t bits = 0;
		uint_least8_t len = 0;

		bits <<= 4;
		bits |= char_to_nibble(rx_next_char(index));
		bits <<= 4;
		bits |= char_to_nibble(rx_next_char(index));
		bits <<= 4;
		bits |= char_to_nibble(rx_next_char(index));
		can_id = bits & 0x7FF;

		len = char_to_nibble(rx_next_char(index));
		len = tu_min8(len, 8); // DLC value > 8 are same as 8

		if (likely(count >= MIN_LEN + 2 * len)) {
			if (SLLIN_SFF_CAN_ID_CMD_REQUEST_RESPONSE == can_id) {
				/* Side channel to send / receive custom or standard commands of CAN SFF encapsulation */
				if (likely(!is_side_channel)) {
					bool run = len < 8;

					if (lin->rx_sl_alt_pi == lin->rx_sl_alt_gi) {
						// start from initial command, alt side channel grows slower than alt commands
						lin->rx_sl_alt_pi = cmd_start_gi;
						lin->rx_sl_alt_gi = cmd_start_gi;
						LOG("ch%u: alt gi=pi=%u\n", index, lin->rx_sl_alt_pi);
					}

					for (unsigned i = 0; i < len; ++i) {
						uint8_t hi = rx_next_char(index);
						uint8_t lo = rx_next_char(index);
						uint8_t byte = (char_to_nibble(hi) << 4) | char_to_nibble(lo);

						lin->rx_sl_buffer[lin->rx_sl_alt_pi++ % TU_ARRAY_SIZE(lin->rx_sl_buffer)] = byte;
					}



					if (run && lin->rx_sl_alt_pi != lin->rx_sl_alt_gi) {
#if defined(SLLIN_DEBUG) && SLLIN_DEBUG
						LOG("ch%u: alt CMD: ", index);
						for (sl_index_t cmd_get_index = lin->rx_sl_alt_gi; cmd_get_index != lin->rx_sl_alt_pi; ++cmd_get_index) {
							sl_index_t cmd_gi = cmd_get_index % TU_ARRAY_SIZE(lin->rx_sl_buffer);
							LOG("%c", lin->rx_sl_buffer[cmd_gi]);
						}

						LOG("\n");
#endif
						// remember regular rx pi / gi
						sl_index_t const rx_pi = lin->rx_sl_pi;
						sl_index_t const rx_gi = lin->rx_sl_gi;
						sl_index_t tx_pi = 0;
						sl_index_t tx_gi = 0;

						// activate alt channel command
						lin->rx_sl_pi = lin->rx_sl_alt_pi;
						lin->rx_sl_gi = lin->rx_sl_alt_gi;

						// clear alt space
						lin->rx_sl_alt_gi = lin->rx_sl_alt_pi;

						sllin_process_command(index, true);

						// restore prev rx gi / gi
						lin->rx_sl_pi = rx_pi;
						lin->rx_sl_gi = rx_gi;

						// pick up response and return on alt channel
						tx_pi = lin->tx_sl_pi;
						tx_gi = lin->tx_sl_gi;

						// clear side channel response space
						lin->tx_sl_gi = lin->tx_sl_pi;

#if defined(SLLIN_DEBUG) && SLLIN_DEBUG
						LOG("ch%u: alt RES: ", index);
						for (sl_index_t res_get_index = tx_gi; res_get_index != tx_pi; ++res_get_index) {
							sl_index_t res_gi = res_get_index % TU_ARRAY_SIZE(lin->tx_sl_buffer);
							LOG("%c", lin->tx_sl_buffer[res_gi]);
						}

						LOG("\n");
#endif
						// acknowlege SFF CAN frame
						tx_queue(index, 'z');
						tx_queue_ok(index);

						while (tx_gi != tx_pi) {
							// compute response len
							uint_least8_t response_len = 0;

							while (((sl_index_t)(tx_gi + response_len)) != tx_pi) {
								uint8_t ch = lin->tx_sl_buffer[((sl_index_t)(tx_gi + response_len)) % TU_ARRAY_SIZE(lin->tx_sl_buffer)];

								++response_len;

								if (ch == SLLIN_OK_TERMINATOR || ch == SLLIN_ERROR_TERMINATOR) {
									break;
								}
							}

							LOG("ch%u: alt response len %u\n", index, response_len);

							// queue side channel response
							for (uint_least8_t i = 0; i < response_len; ) {
								uint_least8_t frame_len = tu_min8(8, response_len - i);

								tx_queue(index, 't');
								tx_queue(index, '0');
								tx_queue(index, '0');
								tx_queue(index, '0');
								tx_queue(index, nibble_to_char(frame_len));

								for (unsigned j = 0; j < frame_len; ++j) {
									sl_index_t gi = (tx_gi + j) % TU_ARRAY_SIZE(lin->tx_sl_buffer);

									tx_queue(index, nibble_to_char(lin->tx_sl_buffer[gi] >> 4));
									tx_queue(index, nibble_to_char(lin->tx_sl_buffer[gi]));
								}

								tx_queue_ok(index);

								i += frame_len;
							}

							if ((response_len % 8) == 0) {
								// send ZLP
								tx_queue(index, 't');
								tx_queue(index, '0');
								tx_queue(index, '0');
								tx_queue(index, '0');
								tx_queue(index, '0');
								tx_queue_ok(index);
							}

							tx_gi += response_len;
						}
					} else {
						// acknowlege SFF CAN frame
						tx_queue(index, 'z');
						tx_queue_ok(index);
					}
				} else {
					LOG("ch%u CAN alternate command channel abuse\n", index);
					tx_queue_error(index);
					lin->rx_sl_alt_gi = lin->rx_sl_alt_pi;
				}
			} else {
				LOG("ch%u malformed command\n", index);
				tx_queue_error(index);
				lin->rx_sl_alt_gi = lin->rx_sl_alt_pi;
			}
		} else {
			LOG("ch%u malformed command\n", index);
			tx_queue_error(index);
		}
	} else {
		LOG("ch%u malformed command\n", index);
		tx_queue_error(index);
	}

}

/* must remain in RAM, called from interrupt context */
SLLIN_RAMFUNC extern void sllin_lin_task_tx_complete(uint8_t index, uint8_t id)
{
	struct sllin_frame_data* const fd = &frame_data[index];
	sllin_autosar_e2e_conf* const e2e = &fd->e2e[id];

	if (unlikely(AUTOSAR_E2E_PROFILE_NONE != e2e->profile)) {
		switch (e2e->profile) {
		default:
			break;
		case AUTOSAR_E2E_PROFILE_11: {
			uint8_t counter_shift = e2e->u.p11.counter_offset & 0x4;
			uint8_t counter_mask = 0xF << counter_shift;
			uint8_t e2e_crc = Crc8_P11_Init();
			uint8_t e2e_crc_byte_offset = e2e->u.p11.crc_offset / 8;

			if (!e2e->u.p11.data_id_mode_both) {
				uint8_t data_nibble_shift = e2e->u.p11.data_id_nibble_offset & 0x4;
				uint8_t data_nibble_mask = 0xF << data_nibble_shift;

				// nibble, copy lower 4 bits of second byte
				fd->data[id][e2e->u.p11.data_id_nibble_offset / 8] &= ~data_nibble_mask;
				fd->data[id][e2e->u.p11.data_id_nibble_offset / 8] |= ((e2e->u.p11.data_id >> 8) & 0xF) << data_nibble_shift;
			}

			fd->data[id][e2e->u.p11.counter_offset / 8] &= ~counter_mask;
			fd->data[id][e2e->u.p11.counter_offset / 8] |= e2e->u.p11.counter << counter_shift;

			if (e2e->u.p11.data_id_mode_both) {
				e2e_crc = Crc8_P11_Update(e2e_crc, e2e->u.p11.data_id);
				e2e_crc = Crc8_P11_Update(e2e_crc, e2e->u.p11.data_id >> 8);
			} else {
				e2e_crc = Crc8_P11_Update(e2e_crc, e2e->u.p11.data_id);
				e2e_crc = Crc8_P11_Update(e2e_crc, 0);
			}

			if (e2e_crc_byte_offset > 0) {
				for (uint8_t i = 0; i < e2e_crc_byte_offset; ++i) {
					e2e_crc = Crc8_P11_Update(e2e_crc, fd->data[id][i]);
				}

				if (unlikely(fd->len[id] > (e2e_crc_byte_offset + 1))) {
					for (uint8_t i = e2e_crc_byte_offset + 1, e = fd->len[id]; i < e; ++i) {
						e2e_crc = Crc8_P11_Update(e2e_crc, fd->data[id][i]);
					}
				}
			} else {
				for (uint8_t i = 1, e = fd->len[id]; i < e; ++i) {
					e2e_crc = Crc8_P11_Update(e2e_crc, fd->data[id][i]);
				}
			}

			fd->data[id][e2e_crc_byte_offset] = Crc8_P11_Finalize(e2e_crc);
			// LOG("ch %u: E2E crc=%02X counter=%X\n", index, fd->data[id][e2e_crc_byte_offset], e2e->u.p11.counter);

			// prepare for next
			++e2e->u.p11.counter;
			e2e->u.p11.counter &= 0xF;

			// update LIN crc
			update_lin_crc(index, id);
		} break;
		case AUTOSAR_E2E_PROFILE_22: {
			uint8_t e2e_crc = Crc8_P22_Init();
			uint8_t e2e_crc_byte_offset = e2e->u.p22.crc_offset / 8;
			uint8_t e2e_counter_byte_offset = e2e_crc_byte_offset + 1;

			// fd->data[id][e2e_crc_byte_offset] = e2e->u.p22.data_ids[e2e->u.p22.counter];
			fd->data[id][e2e_counter_byte_offset] &= 0xF0;
			fd->data[id][e2e_counter_byte_offset] |= e2e->u.p22.counter;

			if (e2e_crc_byte_offset > 0) {
				for (uint8_t i = 0; i < e2e_crc_byte_offset; ++i) {
					e2e_crc = Crc8_P22_Update(e2e_crc, fd->data[id][i]);
				}

				for (uint8_t i = e2e_crc_byte_offset + 1, e = fd->len[id]; i < e; ++i) {
					e2e_crc = Crc8_P22_Update(e2e_crc, fd->data[id][i]);
				}
			} else {
				for (uint8_t i = 1, e = fd->len[id]; i < e; ++i) {
					e2e_crc = Crc8_P22_Update(e2e_crc, fd->data[id][i]);
				}
			}

			e2e_crc = Crc8_P22_Update(e2e_crc, e2e->u.p22.data_ids[e2e->u.p22.counter]);
			fd->data[id][e2e_crc_byte_offset] = Crc8_P22_Finalize(e2e_crc);

			// LOG("ch %u: E2E crc=%02X counter=%X dataid=%02X\n", index, fd->data[id][e2e_crc_byte_offset], e2e->u.p22.counter, e2e->u.p22.data_ids[e2e->u.p22.counter]);

			// prepare for next
			++e2e->u.p22.counter;
			e2e->u.p22.counter &= 0xF;

			// update LIN crc
			update_lin_crc(index, id);
		} break;
		}
	}
}

#endif // #if SLLIN_ENABLE_E2E


SLLIN_RAMFUNC static void sllin_process_eff_frame(uint8_t index)
{
	const unsigned MIN_LEN = 10;
	struct lin *lin = &lins[index];
	sl_index_t const count = lin->rx_sl_pi - lin->rx_sl_gi + 1; // T

	if (likely(lin->enabled)) {
		if (likely(count >= 9)) {
			bool error = false;
			uint32_t eff_id = 0;
			uint8_t len = 0;
			uint8_t lin_id = 0;

			eff_id |= char_to_nibble(rx_next_char(index));
			eff_id <<= 4;
			eff_id |= char_to_nibble(rx_next_char(index));
			eff_id <<= 4;
			eff_id |= char_to_nibble(rx_next_char(index));
			eff_id <<= 4;
			eff_id |= char_to_nibble(rx_next_char(index));
			eff_id <<= 4;
			eff_id |= char_to_nibble(rx_next_char(index));
			eff_id <<= 4;
			eff_id |= char_to_nibble(rx_next_char(index));
			eff_id <<= 4;
			eff_id |= char_to_nibble(rx_next_char(index));
			eff_id <<= 4;
			eff_id |= char_to_nibble(rx_next_char(index));

			lin_id = eff_id & 0x3f;

			len = char_to_nibble(rx_next_char(index));

			if (eff_id & SLLIN_ID_FLAG_FRAME_STORE) {
				if (likely(len <= 8)) {
					if (likely(len * 2 + MIN_LEN >= count)) {
						struct sllin_frame_data *fd = &frame_data[index];
						unsigned const crc_comp = (eff_id >> SLLIN_ID_FLAG_FRAME_CRC_COMP_SHIFT) & SLLIN_ID_FLAG_FRAME_CRC_COMP_MASK;
						uint8_t *data = fd->data[lin_id];

						LOG("ch%u id=%x len=%u store data=", index, lin_id, len);

						for (unsigned i = 0; i < len; ++i) {
							uint8_t hi = rx_next_char(index);
							uint8_t lo = rx_next_char(index);
							uint8_t byte = (char_to_nibble(hi) << 4) | char_to_nibble(lo);

							data[i] = byte;

							LOG("%c", hi);
							LOG("%c", lo);
						}

						LOG("\n");

						fd->len[lin_id] = len;

						switch (crc_comp) {
						case SLLIN_ID_FLAG_FRAME_CRC_COMP_NONE:
							fd->crc[lin_id] = (eff_id >> SLLIN_ID_FLAG_CRC_SHIFT) & SLLIN_ID_FLAG_CRC_MASK;
							fd->classic_crc_flags &= ~(UINT64_C(1) << lin_id);
							fd->enhanced_crc_flags &= ~(UINT64_C(1) << lin_id);
							LOG("ch%u id=%x store crc=%x\n", index, lin_id, fd->crc[lin_id]);
							break;
						case SLLIN_ID_FLAG_FRAME_CRC_COMP_CLASSIC: {
							sllin_crc_t crc = sllin_crc_start();
							crc = sllin_crc_update(crc, data, len);
							fd->crc[lin_id] = sllin_crc_finalize(crc);
							fd->classic_crc_flags |= UINT64_C(1) << lin_id;
							fd->enhanced_crc_flags &= ~(UINT64_C(1) << lin_id);
							LOG("ch%u id=%x compute classic crc=%x\n", index, lin_id, fd->crc[lin_id]);
						} break;
						case SLLIN_ID_FLAG_FRAME_CRC_COMP_ENHANCED: {
							sllin_crc_t crc = sllin_crc_start();
							crc = sllin_crc_update1(crc, sllin_id_to_pid(lin_id));
							crc = sllin_crc_update(crc, data, len);
							fd->crc[lin_id] = sllin_crc_finalize(crc);
							fd->classic_crc_flags &= ~(UINT64_C(1) << lin_id);
							fd->enhanced_crc_flags |= UINT64_C(1) << lin_id;
							LOG("ch%u id=%x compute enhanced crc=%x\n", index, lin_id, fd->crc[lin_id]);
						} break;
						}
#if SLLIN_ENABLE_E2E
						e2e_update(index, lin_id);
#endif
					} else {
						error = true;
					}
				} else {
					error = true;
				}
			}

			sllin_board_lin_slave_respond(index, lin_id, (eff_id & SLLIN_ID_FLAG_FRAME_ENABLE) == SLLIN_ID_FLAG_FRAME_ENABLE);
			LOG("ch%u id=%x response %s\n", index, lin_id, (eff_id & SLLIN_ID_FLAG_FRAME_ENABLE) == SLLIN_ID_FLAG_FRAME_ENABLE ? "enabled" : "disabled");

			if (unlikely(error)) {
				LOG("ch%u malformed command\n", index);
				tx_queue_error(index);
			} else {
				tx_queue(index, 'Z');
				tx_queue_ok(index);
			}
		} else {
			LOG("ch%u malformed command\n", index);
			tx_queue_error(index);
		}
	} else {
		LOG("ch%u refusing to accept frame meta data when closed\n", index);
		tx_queue_error(index);
	}
}


SLLIN_RAMFUNC static void sllin_process_command(uint8_t index, bool is_side_channel)
{
	// http://www.can232.com/docs/canusb_manual.pdf
	struct lin *lin = &lins[index];

	sl_index_t const count = lin->rx_sl_pi - lin->rx_sl_gi;
	char const cmd_start_char = rx_next_char(index);

#if !SLLIN_ENABLE_E2E
	(void)is_side_channel;
#endif

	switch (cmd_start_char) {
	case '\n':
		break;
#if SLLIN_ENABLE_E2E
	case 'A': // AUTOSAR
		sllin_process_autosar(index);
		break;
	case 't':
		sllin_process_sff_frame(index, is_side_channel);
		break;
#endif
	case 'S': // CAN bitrate, values 0-8
		if (lin->enabled) {
			LOG("ch%u refusing to configure LIN when open\n", index);
			tx_queue_error(index);
		} else {
			if (unlikely(count < 2)) {
				LOG("ch%u malformed command\n", index);
				tx_queue_error(index);
			} else {
				unsigned seconds = rx_next_char(index) - '0';

				seconds = tu_min8(seconds, 6);

				lin->conf.sleep_timeout_ms = (4 + seconds) * 1000;

				tx_queue_ok(index);
			}
		}
		break;
	case 's': // BTR0/BTR1 used to set bitrate
		if (unlikely(count < 4)) {
			LOG("ch%u malformed command\n", index);
			tx_queue_error(index);
		} else {
			if (lin->enabled) {
				LOG("ch%u refusing to configure LIN when open\n", index);
				tx_queue_error(index);
			} else {
				uint8_t c3 = rx_next_char(index);
				uint8_t c2 = rx_next_char(index);
				uint8_t c1 = rx_next_char(index);
				uint8_t c0 = rx_next_char(index);
				uint16_t bitrate = (char_to_nibble(c3) << 12)
								| (char_to_nibble(c2) << 8)
								| (char_to_nibble(c1) << 4)
								| (char_to_nibble(c0) << 0);

				if (!bitrate)  {
					LOG("ch%u zero bitrate is invalid\n", index);
					tx_queue_error(index);
				} else {
					lin->conf.bitrate = bitrate;
					LOG("ch%u bitrate=%u\n", index, lin->conf.bitrate);
					tx_queue_ok(index);
				}
			}
		}
		break;
	case 'L': // slave
	case 'O': // master
		clear_rx_fifo(index);
		lin->enabled = true;
		lin->conf.master = cmd_start_char == 'O';
		set_led(index, SLLIN_LIN_LED_STATUS_ON_BUS_AWAKE_PASSIVE);
		sllin_board_lin_init(index, &lin->conf);
		LOG("ch%u open %s bitrate=%u sleep timeout=%u [ms]\n", index, lin->conf.master ? "master" : "slave", lin->conf.bitrate, lin->conf.sleep_timeout_ms);
		tx_queue_ok(index);
		break;
	case 'C':
		LOG("ch%u close\n", index);
		lin->enabled = false;
		tud_cdc_n_write_clear(index);
		sllin_board_lin_uninit(index);
		set_led(index, SLLIN_LIN_LED_STATUS_ENABLED_OFF_BUS);
		tx_queue_ok(index);
		break;
	case 'r': // master: tx header
		if (likely(lin->enabled)) {
			if (likely(count >= 5)) {
				if (likely(lin->conf.master)) {
					uint_least16_t can_id = 0;

					can_id <<= 4;
					can_id |= char_to_nibble(rx_next_char(index));
					can_id <<= 4;
					can_id |= char_to_nibble(rx_next_char(index));
					can_id <<= 4;
					can_id |= char_to_nibble(rx_next_char(index));

					// LOG("ch%u %s -> id=%x len=%u flags=%x\n", index, cmd_buffer, id, frame_len, flags);

					if (SLLIN_ID_FLAG_BUS_BREAK & can_id) {
						if (likely(sllin_board_lin_master_break(index))) {
							tx_queue(index, 'z');
							tx_queue_ok(index);
						} else {
							sllin_store_tx_queue_full_error_response(index);
						}
					} else {
						uint8_t const id = can_id & 0x3f;

						if (likely(sllin_board_lin_master_request(index, id))) {
							tx_queue(index, 'z');
							tx_queue_ok(index);
						} else {
							sllin_store_tx_queue_full_error_response(index);
						}
					}
				} else {
					LOG("ch%u refusing to transmit in slave mode\n", index);
					tx_queue_error(index);
				}
			} else {
				LOG("ch%u malformed command\n", index);
				tx_queue_error(index);
			}
		} else {
			LOG("ch%u refusing to transmit / store reponses when closed\n", index);
			tx_queue_error(index);
		}
		break;
	case 'T':
		sllin_process_eff_frame(index);
		break;
	case 'F': {
		uint8_t flags = 0;
		uint8_t rx_full = 0;
#if defined(HAVE_ATOMIC_COMPARE_EXCHANGE) && HAVE_ATOMIC_COMPARE_EXCHANGE
		// NOT supported on Cortex-M0
		rx_full = __atomic_exchange_n(&lin->rx_full, 0, __ATOMIC_ACQ_REL);
#else
		taskENTER_CRITICAL();
		rx_full = lin->rx_full;
		lin->rx_full = 0;
		taskEXIT_CRITICAL();
#endif
		if (rx_full) {
			flags |= 0x1;
		}

		if (lin->tx_full) {
			lin->tx_full = false;
			flags |= 0x2;
		}

		tx_queue(index, 'F');
		tx_queue(index, nibble_to_char(flags >> 4));
		tx_queue(index, nibble_to_char(flags));
		tx_queue_ok(index);
	} break;
	case 'V': // HW version?
		tx_queue(index, 'V');
		tx_queue(index, '0');
		tx_queue(index, '1');
		tx_queue(index, '0');
		tx_queue(index, '0');
		tx_queue_ok(index);
		break;
	case 'v': // firmware version
		tx_queue(index, 'v');
		ring_buffer_memcpy(
			lin->tx_sl_buffer,
			TU_ARRAY_SIZE(lin->tx_sl_buffer),
			&lin->tx_sl_pi,
			(uint8_t const*)SLLIN_VERSION_STR,
			sizeof(SLLIN_VERSION_STR) - 1);
		tx_queue_ok(index);
		break;
	case 'n': {
		char buffer[48];
		size_t capa_len = sizeof(buffer) - 1;

		usb_get_desc_string(4 + index, buffer, &capa_len);
		buffer[capa_len] = 0;
		LOG("ch%u name: %s\n", index, buffer);

		tx_queue(index, 'n');
		ring_buffer_memcpy(
			lin->tx_sl_buffer,
			TU_ARRAY_SIZE(lin->tx_sl_buffer),
			&lin->tx_sl_pi,
			(uint8_t const*)buffer,
			capa_len);
		tx_queue_ok(index);
	} break;
	case 'N': {
		uint32_t id = sllin_board_identifier();

		id = ((id >> 16) & 0xffff) ^ (id & 0xffff);
		tx_queue(index, 'N');
		tx_queue(index, nibble_to_char(id >> 12));
		tx_queue(index, nibble_to_char(id >> 8));
		tx_queue(index, nibble_to_char(id >> 4));
		tx_queue(index, nibble_to_char(id));
		tx_queue_ok(index);
	} break;
	case 'Z':
		if (lin->enabled) {
			LOG("ch%u refusing to change time stamp setting when enabled\n", index);
			tx_queue_error(index);
		} else if (count < 2) {
			LOG("ch%u malformed command\n", index);
			tx_queue_error(index);
		} else {
			lin->report_time_per_frame = !(rx_next_char(index) == '0');
			LOG("ch%u report time stamp per frame %u\n", index, lin->report_time_per_frame);
			tx_queue_ok(index);
		}
		break;
	case 'Y':
		if (count < 2) {
			LOG("ch%u malformed command\n", index);
			tx_queue_error(index);
		} else {
			lin->report_time_periodically = !(rx_next_char(index) == '0');
			LOG("ch%u report time stamp periodically %u\n", index, lin->report_time_periodically);
			tx_queue_ok(index);
		}
		break;
	default:
		LOG("ch%u unhandled command %c (%02X)\n", index, cmd_start_char, cmd_start_char);
		tx_queue_error(index);
		break;
	}
}


SLLIN_RAMFUNC static inline void process_rx_frame(uint8_t index, sllin_queue_element *e)
{
	struct lin *lin = &lins[index];
	uint32_t id = e->frame.id;
	uint8_t led_state = 0;

	SLLIN_DEBUG_ASSERT(e->frame.len <= 8);

	if (id & SLLIN_ID_FLAG_BUS_STATE_FLAG) {
		switch ((id & SLLIN_ID_FLAG_BUS_STATE_MASK) >> SLLIN_ID_FLAG_BUS_STATE_SHIFT) {
		case SLLIN_ID_FLAG_BUS_STATE_ASLEEP:
			led_state = SLLIN_LIN_LED_STATUS_ON_BUS_SLEEPING;
			break;
		case SLLIN_ID_FLAG_BUS_STATE_AWAKE:
			led_state = SLLIN_LIN_LED_STATUS_ON_BUS_AWAKE_PASSIVE;
			break;
		default:
			led_state = SLLIN_LIN_LED_STATUS_ERROR;
			break;
		}
	} else {
		led_state = SLLIN_LIN_LED_STATUS_ON_BUS_AWAKE_ACTIVE;
	}

	set_led(index, led_state);

	// EFF
	tx_queue(index, 'T');
	tx_queue(index, nibble_to_char(id >> 28));
	tx_queue(index, nibble_to_char(id >> 24));
	tx_queue(index, nibble_to_char(id >> 20));
	tx_queue(index, nibble_to_char(id >> 16));
	tx_queue(index, nibble_to_char(id >> 12));
	tx_queue(index, nibble_to_char(id >> 8));
	tx_queue(index, nibble_to_char(id >> 4));
	tx_queue(index, nibble_to_char(id));
	tx_queue(index, '0' + e->frame.len);

	for (unsigned i = 0, count = e->frame.len; i < count; ++i) {
		tx_queue(index, nibble_to_char(e->frame.data[i] >> 4));
		tx_queue(index, nibble_to_char(e->frame.data[i]));
	}

	if (lin->report_time_per_frame) {
		sllin_make_time_stamp_string(index, e->time_stamp_ms);
	}

	tx_queue_ok(index);
}

SLLIN_RAMFUNC static inline void process_queue(uint8_t index, sllin_queue_element *e)
{
	struct lin *lin = &lins[index];

	switch (e->type) {
	case SLLIN_QUEUE_ELEMENT_TYPE_FRAME:
		process_rx_frame(index, e);
		break;
	case SLLIN_QUEUE_ELEMENT_TYPE_TIME_STAMP: {
		if (lin->report_time_periodically) {
			tx_queue(index, 'Y');
			sllin_make_time_stamp_string(index, e->time_stamp_ms);
			tx_queue_ok(index);
		}
	} break;
	default:
		LOG("unhandled queue element type=%x\n", e->type);
		SLLIN_ASSERT(false && "unhandled queue element type");
		break;
	}
}

SLLIN_RAMFUNC static void tusb_device_task(void* param)
{
	(void) param;

	while (1) {
		LOG("tud_task\n");
		tud_task();
	}
}

SLLIN_RAMFUNC static bool process_tx_queue(uint8_t index)
{
	struct lin *lin = &lins[index];

	if (likely(lin->tx_sl_gi != lin->tx_sl_pi)) {
		sl_index_t used = lin->tx_sl_pi - lin->tx_sl_gi;
		sl_index_t gi = lin->tx_sl_gi % TU_ARRAY_SIZE(lin->tx_sl_buffer);
		sl_index_t left = TU_ARRAY_SIZE(lin->tx_sl_buffer) - gi;
		uint32_t w = 0;

		if (likely(left >= used)) {
			w = tud_cdc_n_write(index, &lin->tx_sl_buffer[gi], used);
		} else {
			w = tud_cdc_n_write(index, &lin->tx_sl_buffer[gi], left);
			w += tud_cdc_n_write(index, &lin->tx_sl_buffer[0], used - left);
		}

		lin->tx_sl_gi = lin->tx_sl_pi;

		if (unlikely(w != used)) {
			lin->tx_full = true;
			LOG("ch%u TXF\n", index);
		}

		return true;
	}

	return false;
}

SLLIN_RAMFUNC static void lin_usb_task(void* param)
{
	const uint8_t index = (uint8_t)(uintptr_t)param;
	SLLIN_ASSERT(index < TU_ARRAY_SIZE(lins));

	LOG("ch%u task start\n", index);

	struct lin *lin = &lins[index];
	bool written = false;
	bool connected = false;

	while (42) {
		bool yield = false;

		(void)ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

		for (bool done = false; !done; ) {
			done = true;

			if (tud_cdc_n_connected(index)) {
				// int count = tud_cdc_n_available(index);
				// LOG("ch%u count=%d\n", index, count);
				int c = tud_cdc_n_read_char(index);
				uint8_t gi = lin->rx_fifo_gi;
				uint8_t pi = __atomic_load_n(&lin->rx_fifo_pi, __ATOMIC_ACQUIRE);

				connected = true;

				if (-1 == c) {
					yield = true;
				} else {
					done = false;

					if (SLLIN_OK_TERMINATOR == c) {
						if (likely(lin->rx_sl_gi != lin->rx_sl_pi)) {
							// zero terminate in case we try to print the command elsewhere
							lin->rx_sl_buffer[lin->rx_sl_pi % TU_ARRAY_SIZE(lin->rx_sl_buffer)] = 0;
#if defined(SLLIN_DEBUG) && SLLIN_DEBUG
							LOG("ch%u: RX CMD: ", index);
							for (sl_index_t cmd_get_index = lin->rx_sl_gi; cmd_get_index != lin->rx_sl_pi; ++cmd_get_index) {
								sl_index_t cmd_gi = cmd_get_index % TU_ARRAY_SIZE(lin->rx_sl_buffer);
								LOG("%c", lin->rx_sl_buffer[cmd_gi]);
							}

							LOG("\n");
#endif

							sllin_process_command(index, false);
							lin->rx_sl_gi = lin->rx_sl_pi;

							if (process_tx_queue(index)) {
								if (!written) {
									xTaskNotifyGive(lin->usb_task_handle);
									written = true;
								}
							}
						} else {
							// empty command
						}
					} else {
						lin->rx_sl_buffer[lin->rx_sl_pi++ % TU_ARRAY_SIZE(lin->rx_sl_buffer)] = c;
					}
				}

				if (pi != gi) {
					uint8_t fifo_index = gi % TU_ARRAY_SIZE(lin->rx_fifo);
					sllin_queue_element *e = &lin->rx_fifo[fifo_index];

					process_queue(index, e);

					if (process_tx_queue(index)) {
						if (!written) {
							xTaskNotifyGive(lin->usb_task_handle);
							written = true;
						}
					}

					__atomic_store_n(&lin->rx_fifo_gi, gi + 1, __ATOMIC_RELEASE);

					done = false;
				}

				if (yield && written) {
					written = false;
					tud_cdc_n_write_flush(index);
					// LOG("ch%u tx flush\n", index);
				}
			} else {
				if (connected) {
					LOG("ch%u TT\n", index);

					channel_off(index);

					// can't call this if disconnected
					// tud_cdc_n_read_flush(index);
				} else {
					yield = true;
				}

				connected = false;

				// may still get pending events such as the time stamp event
				clear_rx_fifo(index);
			}
		}

		if (yield) {
			// yield to prevent this task from eating up the CPU
			// when the USB buffers are full/busy.
			yield = false;
			// taskYIELD();
			vTaskDelay(pdMS_TO_TICKS(1)); // 1ms for USB FS
			// LOG("+");
		}
	}
}

SLLIN_RAMFUNC void tud_cdc_rx_cb(uint8_t index)
{
	struct lin *lin = &lins[index];

	// LOG("ch%u tud_cdc_rx_cb\n", index);

	xTaskNotifyGive(lin->usb_task_handle);
}


SLLIN_RAMFUNC void tud_cdc_line_state_cb(uint8_t index, bool dtr, bool rts)
{
	struct lin *lin = &lins[index];

	(void) dtr;
	(void) rts;

	// LOG("ch%u tud_cdc_line_state_cb\n", index);

	xTaskNotifyGive(lin->usb_task_handle);
}

SLLIN_RAMFUNC extern void sllin_lin_task_queue(uint8_t index, sllin_queue_element const *element)
{
	struct lin *lin = &lins[index];
	uint8_t pi = __atomic_load_n(&lin->rx_fifo_pi, __ATOMIC_RELAXED);
	uint8_t gi = __atomic_load_n(&lin->rx_fifo_gi, __ATOMIC_ACQUIRE);
	uint8_t used = pi - gi;

	if (likely(used < TU_ARRAY_SIZE(lin->rx_fifo))) {
		uint8_t fifo_index = pi % TU_ARRAY_SIZE(lin->rx_fifo);

		lin->rx_fifo[fifo_index] = *element;

		__atomic_store_n(&lin->rx_fifo_pi, pi + 1, __ATOMIC_RELEASE);
	} else {
#if defined(HAVE_ATOMIC_COMPARE_EXCHANGE) && HAVE_ATOMIC_COMPARE_EXCHANGE
		__atomic_store_n(&lin->rx_full, 1, __ATOMIC_RELEASE);
#else
		taskENTER_CRITICAL();
		lin->rx_full = 1;
		taskEXIT_CRITICAL();
#endif
	}
}


SLLIN_RAMFUNC extern void sllin_lin_task_notify_def(uint8_t index, uint32_t count)
{
	struct lin *lin = &lins[index];

	for (uint32_t i = 0; i < count; ++i) {
		xTaskNotifyGive(lin->usb_task_handle);
	}
}

SLLIN_RAMFUNC extern void sllin_lin_task_notify_isr(uint8_t index, uint32_t count)
{
	struct lin *lin = &lins[index];
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if (likely(count)) {
		for (uint32_t i = 1; i < count; ++i) {
			vTaskNotifyGiveFromISR(lin->usb_task_handle, NULL);
		}

		// LOG("ch%u notify\n", index);
		vTaskNotifyGiveFromISR(lin->usb_task_handle, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}


uint16_t _sllin_time_stamp_ms;

SLLIN_RAMFUNC extern void vApplicationTickHook(void)
{
	_Static_assert(1000 == configTICK_RATE_HZ, "fix this function");

	uint16_t now = __atomic_load_n(&_sllin_time_stamp_ms, __ATOMIC_ACQUIRE);

	if (unlikely((now % 1024) == 0)) {
		sllin_queue_element e;
		e.type = SLLIN_QUEUE_ELEMENT_TYPE_TIME_STAMP;
		e.time_stamp_ms = now;

		for (uint8_t i = 0; i < TU_ARRAY_SIZE(lins); ++i) {
			sllin_lin_task_queue(i, &e);
			sllin_lin_task_notify_isr(i, 1);
		}
	}

	// update, wrap around at 1 [s] (60000 [ms])
	if (unlikely(now == 0xEA5F)) {
		__atomic_store_n(&_sllin_time_stamp_ms, 0, __ATOMIC_RELEASE);
	} else {
		__atomic_store_n(&_sllin_time_stamp_ms, now + 1, __ATOMIC_RELEASE);
	}
}


void tud_mount_cb(void)
{
	LOG("mounted\n");
	led_blink(0, 250);
}

void tud_umount_cb(void)
{
	LOG("unmounted\n");
	led_blink(0, 1000);
	channels_off();
}

void tud_suspend_cb(bool remote_wakeup_en)
{
	(void) remote_wakeup_en;
	LOG("suspend\n");
	led_blink(0, 500);
	channels_off();
}

void tud_resume_cb(void)
{
	LOG("resume\n");
	led_blink(0, 250);
}

#if CFG_TUD_DFU_RUNTIME

static inline const char* recipient_str(tusb_request_recipient_t r)
{
	switch (r) {
	case TUSB_REQ_RCPT_DEVICE:
		return "device (0)";
	case TUSB_REQ_RCPT_INTERFACE:
		return "interface (1)";
	case TUSB_REQ_RCPT_ENDPOINT:
		return "endpoint (2)";
	case TUSB_REQ_RCPT_OTHER:
		return "other (3)";
	default:
		return "???";
	}
}

static inline const char* type_str(tusb_request_type_t value)
{
	switch (value) {
	case TUSB_REQ_TYPE_STANDARD:
		return "standard (0)";
	case TUSB_REQ_TYPE_CLASS:
		return "class (1)";
	case TUSB_REQ_TYPE_VENDOR:
		return "vendor (2)";
	case TUSB_REQ_TYPE_INVALID:
		return "invalid (3)";
	default:
		return "???";
	}
}

static inline const char* dir_str(tusb_dir_t value)
{
	switch (value) {
	case TUSB_DIR_OUT:
		return "out (0)";
	case TUSB_DIR_IN:
		return "in (1)";
	default:
		return "???";
	}
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
	LOG("port=%u stage=%u\n", rhport, stage);

	switch (stage) {
	case CONTROL_STAGE_SETUP: {
		switch (request->bRequest) {
		case DFU_VENDOR_REQUEST_MICROSOFT:
			if (request->wIndex == 7) {
				// Get Microsoft OS 2.0 compatible descriptor
				LOG("send MS OS 2.0 compatible descriptor\n");
				uint16_t total_len;
				memcpy(&total_len, desc_ms_os_20 + DFU_MS_OS_20_SUBSET_HEADER_FUNCTION_LEN, 2);
				total_len = tu_le16toh(total_len);
				return tud_control_xfer(rhport, request, (void*)desc_ms_os_20, total_len);
			}
			break;
		default:
			LOG("req type 0x%02x (reci %s type %s dir %s) req 0x%02x, value 0x%04x index 0x%04x reqlen %u\n",
				request->bmRequestType,
				recipient_str(request->bmRequestType_bit.recipient),
				type_str(request->bmRequestType_bit.type),
				dir_str(request->bmRequestType_bit.direction),
				request->bRequest, request->wValue, request->wIndex,
				request->wLength);
			break;
		}
	} break;
	case CONTROL_STAGE_DATA:
	case CONTROL_STAGE_ACK:
		switch (request->bRequest) {
		case DFU_VENDOR_REQUEST_MICROSOFT:
			return true;
		default:
			break;
		}
	default:
		break;
	}

	// stall unknown request
	return false;
}

#endif // CFG_TUD_DFU_RUNTIME
