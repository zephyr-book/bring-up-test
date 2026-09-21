/**
 * @file ir_io_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief IR emitter and receiver bring-up test.
 *
 * @version 0.1
 * @date 29-01-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "ir_io_test.h"

#include "zephyr/devicetree.h"
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/shell/shell.h>

#define SLEEP_TIME_MS 10

static const struct gpio_dt_spec ir_emitter = GPIO_DT_SPEC_GET(DT_NODELABEL(ir_emitter), gpios);
static const struct gpio_dt_spec ir_receiver = GPIO_DT_SPEC_GET(DT_NODELABEL(ir_receiver), gpios);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct gpio_dt_spec led3 = GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios);

static const struct gpio_dt_spec button_up = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);
static const struct gpio_dt_spec button_down = GPIO_DT_SPEC_GET(DT_ALIAS(button3), gpios);

#define ZBIR_TEST_DURATION_MS 15000
#define ZBIR_POLL_INTERVAL_MS 20
#define ZBIR_FRAME_GAP_MS     70
#define ZBIR_DEFAULT_BYTE     0xA5

#define ZBIR_HDR_MARK_USEC  9000
#define ZBIR_HDR_SPACE_USEC 4500

#define ZBIR_BIT_MARK_USEC   2000
#define ZBIR_BIT0_TOTAL_USEC 4000
#define ZBIR_BIT1_TOTAL_USEC 6000

#define ZBIR_BIT0_TOTAL_TOL_USEC 300
#define ZBIR_BIT1_TOTAL_TOL_USEC 300

#define ZBIR_HDR_TOTAL_USEC     (ZBIR_HDR_MARK_USEC + ZBIR_HDR_SPACE_USEC)
#define ZBIR_HDR_TOTAL_TOL_USEC 1000

#define ZBIR_EDGES_COUNT (2 + 2 * BITS_PER_BYTE + 2)

#define ZBIR_MIN_TICK(usec, tol)                                                                   \
	((((usec) - (tol)) * (int64_t)CONFIG_SYS_CLOCK_TICKS_PER_SEC) / USEC_PER_SEC)
#define ZBIR_MAX_TICK(usec, tol)                                                                   \
	((((usec) + (tol)) * (int64_t)CONFIG_SYS_CLOCK_TICKS_PER_SEC) / USEC_PER_SEC)

#define ZBIR_HDR_TOTAL_MIN_TICK  ZBIR_MIN_TICK(ZBIR_HDR_TOTAL_USEC, ZBIR_HDR_TOTAL_TOL_USEC)
#define ZBIR_HDR_TOTAL_MAX_TICK  ZBIR_MAX_TICK(ZBIR_HDR_TOTAL_USEC, ZBIR_HDR_TOTAL_TOL_USEC)
#define ZBIR_BIT0_TOTAL_MIN_TICK ZBIR_MIN_TICK(ZBIR_BIT0_TOTAL_USEC, ZBIR_BIT0_TOTAL_TOL_USEC)
#define ZBIR_BIT0_TOTAL_MAX_TICK ZBIR_MAX_TICK(ZBIR_BIT0_TOTAL_USEC, ZBIR_BIT0_TOTAL_TOL_USEC)
#define ZBIR_BIT1_TOTAL_MIN_TICK ZBIR_MIN_TICK(ZBIR_BIT1_TOTAL_USEC, ZBIR_BIT1_TOTAL_TOL_USEC)
#define ZBIR_BIT1_TOTAL_MAX_TICK ZBIR_MAX_TICK(ZBIR_BIT1_TOTAL_USEC, ZBIR_BIT1_TOTAL_TOL_USEC)

static struct gpio_callback ir_recv_cb;
static int64_t ir_edges_ticks[ZBIR_EDGES_COUNT];
static atomic_t ir_edges_count;

static inline bool zbir_within(int64_t ticks, int64_t min, int64_t max)
{
	return ticks >= min && ticks <= max;
}

static bool zbir_detect_burst(const int64_t *edges, int64_t total_min, int64_t total_max)
{
	int64_t mark = edges[1] - edges[0];
	int64_t space = edges[2] - edges[1];
	int64_t total = edges[2] - edges[0];

	if (mark <= 0 || space <= 0) {
		return false;
	}

	return zbir_within(total, total_min, total_max);
}

static void ir_recv_callback(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	int count = atomic_get(&ir_edges_count);

	if (count >= ZBIR_EDGES_COUNT) {
		return;
	}

	ir_edges_ticks[count++] = k_uptime_ticks();

	if (count == 3 &&
	    !zbir_detect_burst(ir_edges_ticks, ZBIR_HDR_TOTAL_MIN_TICK, ZBIR_HDR_TOTAL_MAX_TICK)) {
		ir_edges_ticks[0] = ir_edges_ticks[1];
		ir_edges_ticks[1] = ir_edges_ticks[2];
		count = 2;
	}

	atomic_set(&ir_edges_count, count);
}

static bool zbir_read_bit(const int64_t *edges, uint8_t index, bool *bit_out)
{
	size_t base = 2 + (2U * index);
	int64_t mark = edges[base + 1] - edges[base];
	int64_t total = edges[base + 2] - edges[base];

	if (mark <= 0 || total <= 0 || mark >= total) {
		return false;
	}

	if (zbir_within(total, ZBIR_BIT0_TOTAL_MIN_TICK, ZBIR_BIT0_TOTAL_MAX_TICK)) {
		*bit_out = false;
		return true;
	}

	if (zbir_within(total, ZBIR_BIT1_TOTAL_MIN_TICK, ZBIR_BIT1_TOTAL_MAX_TICK)) {
		*bit_out = true;
		return true;
	}

	return false;
}

static bool zbir_decode_frame(const int64_t *edges, uint8_t *byte_out)
{
	uint8_t byte = 0;
	int64_t trailing_mark;

	if (!zbir_detect_burst(edges, ZBIR_HDR_TOTAL_MIN_TICK, ZBIR_HDR_TOTAL_MAX_TICK)) {
		return false;
	}

	for (uint8_t i = 0; i < BITS_PER_BYTE; i++) {
		bool bit;

		if (!zbir_read_bit(edges, i, &bit)) {
			return false;
		}

		if (bit) {
			byte |= BIT(i);
		}
	}

	trailing_mark = edges[ZBIR_EDGES_COUNT - 1] - edges[ZBIR_EDGES_COUNT - 2];
	if (trailing_mark <= 0) {
		return false;
	}

	*byte_out = byte;
	return true;
}

static int init_leds(const struct shell *sh)
{
	int ret;

	if (!gpio_is_ready_dt(&led)) {
		shell_error(sh, "LED device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		shell_error(sh, "Failed to configure LED: %d\n", ret);
		return ret;
	}

	if (!gpio_is_ready_dt(&ir_emitter)) {
		shell_error(sh, "IR emitter device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&ir_emitter, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		shell_error(sh, "Failed to configure IR emitter: %d\n", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "IR emitter initialized\n");
	return 0;
}

static int init_inputs(const struct shell *sh)
{
	int ret;

	if (!gpio_is_ready_dt(&button_up)) {
		shell_error(sh, "Button UP device not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button_up, GPIO_INPUT);
	if (ret < 0) {
		shell_error(sh, "Failed to configure button UP: %d\n", ret);
		return ret;
	}

	if (!gpio_is_ready_dt(&button_down)) {
		shell_error(sh, "Button DOWN device not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&button_down, GPIO_INPUT);
	if (ret < 0) {
		shell_error(sh, "Failed to configure button DOWN: %d\n", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&ir_receiver, GPIO_INPUT);
	if (ret < 0) {
		shell_error(sh, "Failed to configure IR receiver: %d\n", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Inputs initialized\n");
	return 0;
}

int cmd_test_ir_io(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;
	bool previous_state_up = 0, previous_state_down = 0;

	ret = init_leds(sh);
	if (ret < 0) {
		shell_error(sh, "LED initialization failed: %d", ret);
		return ret;
	}

	ret = init_inputs(sh);
	if (ret < 0) {
		shell_error(sh, "Button initialization failed: %d", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "IR IO Test started\n");
	shell_fprintf(sh, SHELL_NORMAL,
		      "Press UP button to toggle IR emitter, DOWN to pass test\n");

	while (1) {

		int current = gpio_pin_get_dt(&button_up);

		/* Detect falling edge (button press if ACTIVE_LOW) */
		if (current == 1 && previous_state_up == 0) {
			gpio_pin_toggle_dt(&ir_emitter);
			shell_fprintf(sh, SHELL_NORMAL,
				      "Button UP pressed -> IR emitter toggled\n");
		}
		previous_state_up = current;

		if (gpio_pin_get_dt(&ir_receiver) == 1) {
			gpio_pin_set_dt(&led, 1);
		} else {
			gpio_pin_set_dt(&led, 0);
		}

		current = gpio_pin_get_dt(&button_down);
		if (current == 1 && previous_state_down == 0) {
			gpio_pin_toggle_dt(&ir_emitter);
			shell_fprintf(sh, SHELL_NORMAL, "Button DOWN pressed -> Passing test...\n");
			break;
		}
		previous_state_down = current;

		k_msleep(SLEEP_TIME_MS);
	}

	gpio_pin_set_dt(&ir_emitter, 0);
	gpio_pin_set_dt(&led, 0);

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "\nIR IO Test completed\n");

	return 0;
}

static void zbir_send_frame(uint8_t byte)
{
	gpio_pin_set_dt(&ir_emitter, 1);
	k_busy_wait(ZBIR_HDR_MARK_USEC);
	gpio_pin_set_dt(&ir_emitter, 0);
	k_busy_wait(ZBIR_HDR_SPACE_USEC);

	for (uint8_t i = 0; i < BITS_PER_BYTE; i++) {
		gpio_pin_set_dt(&ir_emitter, 1);
		k_busy_wait(ZBIR_BIT_MARK_USEC);
		gpio_pin_set_dt(&ir_emitter, 0);
		k_busy_wait((byte & BIT(i)) ? (ZBIR_BIT1_TOTAL_USEC - ZBIR_BIT_MARK_USEC)
					    : (ZBIR_BIT0_TOTAL_USEC - ZBIR_BIT_MARK_USEC));
	}

	gpio_pin_set_dt(&ir_emitter, 1);
	k_busy_wait(ZBIR_BIT_MARK_USEC);
	gpio_pin_set_dt(&ir_emitter, 0);
}

static int cmd_ir_io_send(const struct shell *sh, size_t argc, char **argv)
{
	int ret;
	uint8_t payload = ZBIR_DEFAULT_BYTE;

	if (argc > 1) {
		payload = (uint8_t)strtoul(argv[1], NULL, 0);
	}

	if (!gpio_is_ready_dt(&ir_emitter)) {
		shell_error(sh, "IR emitter device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&ir_emitter, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		shell_error(sh, "Failed to configure IR emitter: %d", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Transmitting 0x%02x for %d s...\n", payload,
		      ZBIR_TEST_DURATION_MS / 1000);

	int64_t start_ms = k_uptime_get();

	while (k_uptime_get() - start_ms < ZBIR_TEST_DURATION_MS) {
		zbir_send_frame(payload);
		k_msleep(ZBIR_FRAME_GAP_MS);
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Transmission complete\n");

	return 0;
}

#define ZBIR_PASS_BLINK_COUNT 5
#define ZBIR_PASS_BLINK_MS    150

static void zbir_blink_pass_leds(void)
{
	static const struct gpio_dt_spec *const leds[] = {&led, &led1, &led2, &led3};

	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		if (gpio_is_ready_dt(leds[i])) {
			gpio_pin_configure_dt(leds[i], GPIO_OUTPUT_INACTIVE);
		}
	}

	for (int blink = 0; blink < ZBIR_PASS_BLINK_COUNT; blink++) {
		for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
			if (gpio_is_ready_dt(leds[i])) {
				gpio_pin_set_dt(leds[i], 1);
			}
		}
		k_msleep(ZBIR_PASS_BLINK_MS);

		for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
			if (gpio_is_ready_dt(leds[i])) {
				gpio_pin_set_dt(leds[i], 0);
			}
		}
		k_msleep(ZBIR_PASS_BLINK_MS);
	}
}

static int cmd_ir_io_recv(const struct shell *sh, size_t argc, char **argv)
{
	int ret;
	uint8_t expected = ZBIR_DEFAULT_BYTE;
	uint8_t decoded;

	if (argc > 1) {
		expected = (uint8_t)strtoul(argv[1], NULL, 0);
	}

	if (!gpio_is_ready_dt(&ir_receiver)) {
		shell_error(sh, "IR receiver device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&ir_receiver, GPIO_INPUT);
	if (ret < 0) {
		shell_error(sh, "Failed to configure IR receiver: %d", ret);
		return ret;
	}

	atomic_set(&ir_edges_count, 0);

	gpio_init_callback(&ir_recv_cb, ir_recv_callback, BIT(ir_receiver.pin));
	ret = gpio_add_callback_dt(&ir_receiver, &ir_recv_cb);
	if (ret < 0) {
		shell_error(sh, "Failed to add IR receiver callback: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&ir_receiver, GPIO_INT_EDGE_BOTH);
	if (ret < 0) {
		shell_error(sh, "Failed to arm IR receiver interrupt: %d", ret);
		gpio_remove_callback_dt(&ir_receiver, &ir_recv_cb);
		return ret;
	}

	shell_fprintf(sh, SHELL_NORMAL, "Listening for 0x%02x, %d s...\n", expected,
		      ZBIR_TEST_DURATION_MS / 1000);

	ret = -ETIMEDOUT;

	for (int elapsed = 0; elapsed < ZBIR_TEST_DURATION_MS; elapsed += ZBIR_POLL_INTERVAL_MS) {
		if (atomic_get(&ir_edges_count) == ZBIR_EDGES_COUNT) {
			if (zbir_decode_frame(ir_edges_ticks, &decoded)) {
				if (decoded == expected) {
					shell_fprintf(sh, SHELL_VT100_COLOR_GREEN,
						      "PASS -- received 0x%02x\n", decoded);
					zbir_blink_pass_leds();
					ret = 0;
					break;
				}

				shell_fprintf(
					sh, SHELL_VT100_COLOR_YELLOW,
					"Decoded 0x%02x, expected 0x%02x -- still listening\n",
					decoded, expected);
			}

			atomic_set(&ir_edges_count, 0);
		}

		k_msleep(ZBIR_POLL_INTERVAL_MS);
	}

	if (ret < 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED,
			      "FAIL -- no valid 0x%02x frame received. Check alignment/distance "
			      "and that both boards use the same byte\n",
			      expected);
	}

	gpio_pin_interrupt_configure_dt(&ir_receiver, GPIO_INT_DISABLE);
	gpio_remove_callback_dt(&ir_receiver, &ir_recv_cb);

	return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ir_io,
	SHELL_CMD_ARG(send, NULL,
		      "Transmit an IR test frame repeatedly for 15s. Optional payload byte "
		      "(default 0xA5).",
		      cmd_ir_io_send, 1, 1),
	SHELL_CMD_ARG(recv, NULL,
		      "Listen for and decode IR test frames for 15s. Optional expected byte "
		      "(default 0xA5).",
		      cmd_ir_io_recv, 1, 1),
	SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((test), ir_io, &sub_ir_io, "Initialize the bringup test for IR IO Module.",
		 cmd_test_ir_io, 1, 0);
