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
#include <string.h>
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

static const struct gpio_dt_spec button_up = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);
static const struct gpio_dt_spec button_down = GPIO_DT_SPEC_GET(DT_ALIAS(button3), gpios);

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
	shell_fprintf(sh, SHELL_NORMAL, "Press UP button to toggle IR emitter, DOWN to pass test\n");

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
			shell_fprintf(sh, SHELL_NORMAL,
						"Button DOWN pressed -> Passing test...\n");
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

SHELL_SUBCMD_ADD((test), ir_io, NULL, "Initialize the bringup test for IR IO Module.", cmd_test_ir_io, 1, 0);
