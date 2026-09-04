/**
 * @file addr_led_test.c
 * @author Vinicius Ramos <vinicius.ramos@edge.ufal.br>
 * @brief WS2812 addressable LED bring-up test.
 *
 * @version 0.1
 * @date 23-02-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "addr_led_test.h"

#include "zephyr/devicetree.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/shell/shell.h>

#define SLEEP_TIME_MS 10
#define SAMPLE_LED_BRIGHTNESS 0xFF
#define SAMPLE_LED_UPDATE_DELAY 50

#if DT_NODE_HAS_PROP(DT_ALIAS(led_strip), chain_length)
#define STRIP_NUM_PIXELS	DT_PROP(DT_ALIAS(led_strip), chain_length)
#else
#error Unable to determine length of LED strip
#endif

#define DELAY_TIME K_MSEC(SAMPLE_LED_UPDATE_DELAY)
#define RGB(_r, _g, _b) { .r = (_r), .g = (_g), .b = (_b) }

static const struct led_rgb colors[] = {
	RGB(SAMPLE_LED_BRIGHTNESS, 0x00, 0x00), /* red */
	RGB(0x00, SAMPLE_LED_BRIGHTNESS, 0x00), /* green */
	RGB(0x00, 0x00, SAMPLE_LED_BRIGHTNESS), /* blue */
};

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static const struct device *const led_strip = DEVICE_DT_GET(DT_ALIAS(led_strip));

static const struct gpio_dt_spec up_button = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);
static const struct gpio_dt_spec down_button = GPIO_DT_SPEC_GET(DT_ALIAS(button3), gpios);
static const struct gpio_dt_spec right_button = GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios);

static int init_buttons(const struct shell *sh)
{
	int ret;

    if (!gpio_is_ready_dt(&up_button)) {
        shell_error(sh, "Up Button device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&up_button, GPIO_INPUT);
    if (ret < 0) {
        shell_error(sh, "Failed to configure up button: %d", ret);
        return ret;
    }

    if (!gpio_is_ready_dt(&down_button)) {
        shell_error(sh, "Down Button device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&down_button, GPIO_INPUT);
    if (ret < 0) {
        shell_error(sh, "Failed to configure down button: %d", ret);
        return ret;
    }

    if (!gpio_is_ready_dt(&right_button)) {
        shell_error(sh, "Right Button device not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&right_button, GPIO_INPUT);
    if (ret < 0) {
        shell_error(sh, "Failed to configure right button: %d", ret);
        return ret;
    }

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Buttons initialized\n");
	return 0;
}

static int init_addr_led(void)
{
    if (!device_is_ready(led_strip)) {
        return -ENODEV;
    }

    return 0;
}

int cmd_test_addr_led(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret, previous_state_up = 0, previous_state_down = 0, previous_state_right = 0;
    size_t color = 0;
    bool test_passed = true;

    ret = init_addr_led();
    if (ret < 0) {
        shell_error(sh, "Addressable LED device not ready: %d", ret);
        return ret;
    }

    ret = init_buttons(sh);
	if (ret < 0) {
		shell_error(sh, "Buttons initialization failed: %d\n", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Addressable LED Test started, press the UP key if test passed, DOWN if test failed\n");

	while (1) {
        for (size_t cursor = 0; cursor < ARRAY_SIZE(pixels); cursor++) {
			memset(&pixels, 0x00, sizeof(pixels));
			memcpy(&pixels[cursor], &colors[color], sizeof(struct led_rgb));

			ret = led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
			if (ret) {
                shell_error(sh, "Couldn't update LED strip: %d\n", ret);
                return ret;
            }

			k_sleep(DELAY_TIME);
		}

        int current_up = gpio_pin_get_dt(&up_button);
        int current_down = gpio_pin_get_dt(&down_button);
        int current_right = gpio_pin_get_dt(&right_button);

        if (current_up == 1 && previous_state_up == 0) {
            shell_fprintf(sh, SHELL_NORMAL,
                        "Up button pressed -> TEST PASSED\n");
            break;
        }

        if (current_down == 1 && previous_state_down == 0) {
            shell_fprintf(sh, SHELL_NORMAL,
                        "Down button pressed -> TEST FAILED\n");
            test_passed = false;
            break;
        }

        if (current_right == 1 && previous_state_right == 0) {
            shell_fprintf(sh, SHELL_NORMAL,
                        "Right button pressed -> Change color\n");
            color = (color + 1) % ARRAY_SIZE(colors);
        }

        previous_state_up = current_up;
        previous_state_down = current_down;
        previous_state_right = current_right;
		k_msleep(SLEEP_TIME_MS);
	}

    memset(&pixels, 0x00, sizeof(pixels));
    ret = led_strip_update_rgb(led_strip, pixels, STRIP_NUM_PIXELS);
    if (ret) {
        shell_error(sh, "Couldn't update LED strip: %d\n", ret);
        return ret;
    }

    if (test_passed) {
        shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Addressable LED test passed successfully.\n");
        return 0;
    } else {
        shell_fprintf(sh, SHELL_VT100_COLOR_RED, "Addressable LED test failed.\n");
        return -1;
    }
}

SHELL_SUBCMD_ADD((test), addr_led, NULL, "Initialize the bringup test for Addressable LED Module.", cmd_test_addr_led, 1, 0);
