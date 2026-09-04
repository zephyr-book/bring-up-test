/**
 * @file hall_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief Hall sensor bring-up test (P2 only).
 *
 * @version 0.1
 * @date 03-09-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "hall_test.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define HALL_NODE DT_ALIAS(hall_sensor)

#if DT_NODE_EXISTS(HALL_NODE)

#define POLL_INTERVAL_MS 20
#define TEST_DURATION_MS 15000

static const struct gpio_dt_spec hall = GPIO_DT_SPEC_GET(HALL_NODE, gpios);

int cmd_test_hall(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;
	int transitions = 0;
	int previous;

	if (!gpio_is_ready_dt(&hall)) {
		shell_error(sh, "Hall sensor GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&hall, GPIO_INPUT);
	if (ret < 0) {
		shell_error(sh, "Failed to configure hall sensor: %d", ret);
		return ret;
	}

	previous = gpio_pin_get_dt(&hall);
	if (previous < 0) {
		shell_error(sh, "Failed to read hall sensor: %d", previous);
		return previous;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Hall sensor test started\n");
	shell_fprintf(sh, SHELL_NORMAL,
		      "Move a magnet near the sensor. Initial state: %s. Running %d s...\n",
		      previous ? "DETECTED" : "idle", TEST_DURATION_MS / 1000);

	for (int elapsed = 0; elapsed < TEST_DURATION_MS; elapsed += POLL_INTERVAL_MS) {
		int current = gpio_pin_get_dt(&hall);

		if (current < 0) {
			shell_error(sh, "Failed to read hall sensor: %d", current);
			return current;
		}

		if (current != previous) {
			transitions++;
			shell_fprintf(sh, SHELL_NORMAL, "Hall sensor -> %s\n",
				      current ? "DETECTED" : "idle");
			previous = current;
		}

		k_msleep(POLL_INTERVAL_MS);
	}

	if (transitions == 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED,
			      "No transitions seen -- sensor stuck at %s\n",
			      previous ? "DETECTED" : "idle");
		return -EIO;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN,
		      "Hall sensor test passed (%d transitions)\n", transitions);
	return 0;
}

SHELL_SUBCMD_ADD((test), hall, NULL, "Initialize the bringup test for the Hall sensor (P2).",
		 cmd_test_hall, 1, 0);

#endif /* DT_NODE_EXISTS(HALL_NODE) */
