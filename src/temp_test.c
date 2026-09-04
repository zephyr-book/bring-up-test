/**
 * @file temp_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief TMP1075 temperature sensor bring-up test (P2 only).
 *
 * @version 0.1
 * @date 03-09-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "temp_test.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define TEMP_NODE DT_NODELABEL(tmp1075)

#if DT_NODE_EXISTS(TEMP_NODE)

#define SAMPLE_COUNT       10
#define SAMPLE_INTERVAL_MS 500

static const struct device *const temp_dev = DEVICE_DT_GET(TEMP_NODE);

#if DT_NODE_EXISTS(DT_ALIAS(temp_alert))
static const struct gpio_dt_spec temp_alert = GPIO_DT_SPEC_GET(DT_ALIAS(temp_alert), gpios);
#endif

int cmd_test_temp(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;

	if (!device_is_ready(temp_dev)) {
		shell_error(sh, "Temperature sensor %s not ready", temp_dev->name);
		return -ENODEV;
	}

#if DT_NODE_EXISTS(DT_ALIAS(temp_alert))
	ret = gpio_pin_configure_dt(&temp_alert, GPIO_INPUT);
	if (ret < 0) {
		shell_error(sh, "Failed to configure ALERT pin: %d", ret);
		return ret;
	}
#endif

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Temperature sensor %s ready\n",
		      temp_dev->name);

	for (int i = 0; i < SAMPLE_COUNT; i++) {
		struct sensor_value val;
		int64_t milli_c;

		ret = sensor_sample_fetch(temp_dev);
		if (ret < 0) {
			shell_error(sh, "Sample fetch failed: %d", ret);
			return ret;
		}

		ret = sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
		if (ret < 0) {
			shell_error(sh, "Channel read failed: %d", ret);
			return ret;
		}

		/* Printed in milli-degrees to stay clear of float formatting. */
		milli_c = sensor_value_to_milli(&val);

		shell_fprintf(sh, SHELL_NORMAL, "Temperature: %lld.%03lld C",
			      milli_c / 1000, (milli_c < 0 ? -milli_c : milli_c) % 1000);

#if DT_NODE_EXISTS(DT_ALIAS(temp_alert))
		shell_fprintf(sh, SHELL_NORMAL, "  (ALERT %s)",
			      gpio_pin_get_dt(&temp_alert) == 1 ? "asserted" : "clear");
#endif
		shell_fprintf(sh, SHELL_NORMAL, "\n");

		k_msleep(SAMPLE_INTERVAL_MS);
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Temperature sensor test completed\n");
	return 0;
}

SHELL_SUBCMD_ADD((test), temp, NULL,
		 "Initialize the bringup test for the temperature sensor (P2).", cmd_test_temp, 1,
		 0);

#endif /* DT_NODE_EXISTS(TEMP_NODE) */
