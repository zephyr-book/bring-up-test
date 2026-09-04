/**
 * @file accel_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief BMI323 IMU bring-up test (P2 only).
 *
 * @version 0.1
 * @date 03-09-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "accel_test.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define ACCEL_NODE DT_NODELABEL(bmi323)

#if DT_NODE_EXISTS(ACCEL_NODE)

#define SAMPLE_COUNT       10
#define SAMPLE_INTERVAL_MS 250
#define ODR_HZ             100

static const struct device *const accel_dev = DEVICE_DT_GET(ACCEL_NODE);

static int enable_channel(const struct shell *sh, enum sensor_channel chan, const char *name)
{
	struct sensor_value odr = {.val1 = ODR_HZ, .val2 = 0};
	int ret = sensor_attr_set(accel_dev, chan, SENSOR_ATTR_SAMPLING_FREQUENCY, &odr);

	if (ret < 0) {
		shell_error(sh, "Failed to set %s ODR: %d", name, ret);
		return ret;
	}

	return 0;
}

static void print_xyz(const struct shell *sh, const char *name, const struct sensor_value *v,
		      const char *unit)
{
	shell_fprintf(sh, SHELL_NORMAL, "%s [%s] x=%lld y=%lld z=%lld\n", name, unit,
		      sensor_value_to_milli(&v[0]), sensor_value_to_milli(&v[1]),
		      sensor_value_to_milli(&v[2]));
}

int cmd_test_accel(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;

	if (!device_is_ready(accel_dev)) {
		shell_error(sh, "IMU %s not ready", accel_dev->name);
		return -ENODEV;
	}

	ret = enable_channel(sh, SENSOR_CHAN_ACCEL_XYZ, "accelerometer");
	if (ret < 0) {
		return ret;
	}

	ret = enable_channel(sh, SENSOR_CHAN_GYRO_XYZ, "gyroscope");
	if (ret < 0) {
		return ret;
	}

	/* Give the sensor a couple of conversion periods before the first read. */
	k_msleep(50);

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "IMU %s ready at %d Hz\n", accel_dev->name,
		      ODR_HZ);
	shell_fprintf(sh, SHELL_NORMAL, "Tilt and rotate the board to see the values change\n");

	for (int i = 0; i < SAMPLE_COUNT; i++) {
		struct sensor_value accel[3];
		struct sensor_value gyro[3];

		ret = sensor_sample_fetch(accel_dev);
		if (ret < 0) {
			shell_error(sh, "Sample fetch failed: %d", ret);
			return ret;
		}

		ret = sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
		if (ret < 0) {
			shell_error(sh, "Accelerometer read failed: %d", ret);
			return ret;
		}

		print_xyz(sh, "accel", accel, "mm/s^2");

		ret = sensor_channel_get(accel_dev, SENSOR_CHAN_GYRO_XYZ, gyro);
		if (ret < 0) {
			/* Not fatal: the accelerometer half already proved the bus. */
			shell_warn(sh, "Gyroscope read failed: %d", ret);
		} else {
			print_xyz(sh, "gyro ", gyro, "mrad/s");
		}

		k_msleep(SAMPLE_INTERVAL_MS);
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "IMU test completed\n");
	return 0;
}

SHELL_SUBCMD_ADD((test), accel, NULL, "Initialize the bringup test for the IMU (P2).",
		 cmd_test_accel, 1, 0);

#endif /* DT_NODE_EXISTS(ACCEL_NODE) */
