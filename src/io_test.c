/**
 * @file io_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief LED / push-button bring-up test.
 *
 * @version 0.2
 * @date 29-01-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "io_test.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define SLEEP_TIME_MS 10

/* Addressed through the led0..led3 / button0..button3 devicetree aliases rather
 * than node labels, because the labels are revision-specific: P1 names its LEDs
 * by colour (blue_led, green_led, ...) and its buttons button1..button4, while
 * P2 names them led0..led3 and button0..button3 on entirely different pins.
 * Both revisions provide the aliases, so this file builds unchanged on each.
 *
 * P1 silkscreen mapping, for reference:
 *   led0 = blue, led1 = green, led2 = yellow, led3 = red
 *   button0 = UP, button1 = RIGHT, button2 = LEFT, button3 = DOWN
 */
static const struct gpio_dt_spec leds[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios),
};

static const struct gpio_dt_spec buttons[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(button2), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(button3), gpios),
};

static const char *const led_names[] = {"LED0", "LED1", "LED2", "LED3"};
static const char *const button_names[] = {"BTN0", "BTN1", "BTN2", "BTN3"};

BUILD_ASSERT(ARRAY_SIZE(leds) == ARRAY_SIZE(buttons),
	     "each button toggles the LED at the same index");

/* Index of the button that ends the test when held. */
#define STOP_BUTTON 0

static int init_leds(const struct shell *sh)
{
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		if (!gpio_is_ready_dt(&leds[i])) {
			shell_error(sh, "LED %s device not ready", led_names[i]);
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			shell_error(sh, "Failed to configure LED %s: %d", led_names[i], ret);
			return ret;
		}
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "LEDs initialized\n");
	return 0;
}

static int init_buttons(const struct shell *sh)
{
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
		if (!gpio_is_ready_dt(&buttons[i])) {
			shell_error(sh, "Button %s device not ready", button_names[i]);
			return -ENODEV;
		}

		ret = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
		if (ret < 0) {
			shell_error(sh, "Failed to configure button %s: %d", button_names[i],
				    ret);
			return ret;
		}
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Buttons initialized\n");
	return 0;
}

/* P2 adds a fifth, white LED driven through a low-side MOSFET (active-high,
 * unlike the four active-low indicator LEDs). P1 has no counterpart.
 */
#if DT_NODE_EXISTS(DT_ALIAS(led4))
static const struct gpio_dt_spec white_led = GPIO_DT_SPEC_GET(DT_ALIAS(led4), gpios);

static int blink_white_led(const struct shell *sh)
{
	int ret;

	if (!gpio_is_ready_dt(&white_led)) {
		shell_error(sh, "White LED device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&white_led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		shell_error(sh, "Failed to configure white LED: %d", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_NORMAL, "Blinking white LED\n");

	for (int i = 0; i < 3; i++) {
		gpio_pin_set_dt(&white_led, 1);
		k_msleep(200);
		gpio_pin_set_dt(&white_led, 0);
		k_msleep(200);
	}

	return 0;
}
#endif

int cmd_test_io(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;
	bool previous_state[ARRAY_SIZE(buttons)] = {0};

	ret = init_leds(sh);
	if (ret < 0) {
		shell_error(sh, "LED initialization failed: %d", ret);
		return ret;
	}

	ret = init_buttons(sh);
	if (ret < 0) {
		shell_error(sh, "Button initialization failed: %d", ret);
		return ret;
	}

#if DT_NODE_EXISTS(DT_ALIAS(led4))
	ret = blink_white_led(sh);
	if (ret < 0) {
		return ret;
	}
#endif

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "IO Test started\n");
	shell_fprintf(sh, SHELL_NORMAL,
		      "Press a button to toggle the LED of the same index; "
		      "hold %s for 2 s to stop\n",
		      button_names[STOP_BUTTON]);

	while (1) {
		for (size_t i = 0; i < ARRAY_SIZE(buttons); i++) {
			int current = gpio_pin_get_dt(&buttons[i]);

			/* Specs are ACTIVE_LOW, so 1 means physically pressed. */
			if (current == 1 && previous_state[i] == 0) {
				gpio_pin_toggle_dt(&leds[i]);
				shell_fprintf(sh, SHELL_NORMAL,
					      "Button %s pressed -> LED %s toggled\n",
					      button_names[i], led_names[i]);
			}

			previous_state[i] = (current == 1);
		}

		k_msleep(SLEEP_TIME_MS);

		if (previous_state[STOP_BUTTON] && gpio_pin_get_dt(&buttons[STOP_BUTTON]) == 1) {
			k_sleep(K_SECONDS(2));
			if (gpio_pin_get_dt(&buttons[STOP_BUTTON]) == 1) {
				shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "\nIO Test stopped\n");
				break;
			}
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
		gpio_pin_set_dt(&leds[i], 0);
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "\nIO Test completed\n");

	return 0;
}

SHELL_SUBCMD_ADD((test), io, NULL, "Initialize the bringup test for IO Module.", cmd_test_io, 1,
		 0);
