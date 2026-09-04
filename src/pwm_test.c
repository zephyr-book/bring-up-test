/**
 * @file pwm_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief PWM buzzer bring-up test driven by the potentiometer.
 *
 * @version 0.1
 * @date 29-01-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "pwm_test.h"
#include "zephyr/devicetree.h"
#include "zephyr/shell/shell_fprintf.h"

#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_types.h>
#include <zephyr/drivers/gpio.h>

static const struct pwm_dt_spec pwm_buzzer = PWM_DT_SPEC_GET(DT_ALIAS(pwm_buzzer));
static const struct adc_channel_cfg potentiometer = ADC_CHANNEL_CFG_DT(DT_NODELABEL(potentiometer));

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

int init_pwm_adc_test(const struct shell *sh)
{
	int ret;
	int32_t voltage_mv;
	uint32_t frequency, period_ns, pulse_ns;
	uint16_t adc_buffer = 0;
	struct adc_sequence sequence = {
		.buffer = &adc_buffer,
		.buffer_size = sizeof(adc_buffer),
		.calibrate = true,
		.channels = BIT(potentiometer.channel_id),
		.resolution = 12,
	};

	if (!pwm_is_ready_dt(&pwm_buzzer)) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED, "PWM device not ready");
		return -ENODEV;
	}

	if (!device_is_ready(adc_dev)) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED, "ADC device not ready");
		return -ENODEV;
	}

	ret = adc_channel_setup(adc_dev, &potentiometer);
	if (ret < 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED, "Failed to setup ADC channel");
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "PWM Test started. Press any key to stop.\n");
	shell_fprintf(sh, SHELL_NORMAL, "Reading potentiometer and adjusting buzzer frequency\n");

	while (true) {

		shell_fprintf(sh, SHELL_NORMAL, "Reading ADC...\n");
		ret = adc_read(adc_dev, &sequence);
		if (ret < 0) {
			shell_error(sh, "ADC read failed: %d", ret);
			return ret;
		}

		shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "ADC read successful: %d\n", adc_buffer);

		voltage_mv = adc_buffer;
		ret = adc_raw_to_millivolts(3300, ADC_GAIN_1, 12, &voltage_mv);
		if (ret < 0) {
			shell_error(sh, "ADC conversion failed: %d", ret);
			return ret;
		}

		shell_fprintf(sh, SHELL_NORMAL, "Raw ADC: %d, Voltage: %d mV\r", adc_buffer,
			      voltage_mv);

		/* Map voltage (0-3300mV) to frequency */
		frequency = 1440 - ((voltage_mv * 1000) / 3300);

		/* Calculate period and pulse width (50% duty cycle) */
		period_ns = NSEC_PER_SEC / frequency;
		pulse_ns = period_ns / 2;

		ret = pwm_set_dt(&pwm_buzzer, period_ns, pulse_ns);
		if (ret < 0) {
			shell_error(sh, "PWM set failed: %d", ret);
			return ret;
		}

		shell_fprintf(sh, SHELL_NORMAL, "Voltage: %d mV, Frequency: %d Hz\r", voltage_mv,
			      frequency);

		k_sleep(K_MSEC(20));
	}

	pwm_set_dt(&pwm_buzzer, 0, 0);
	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "\nPWM Test completed\n");

	return 0;
}

int cmd_test_pwm(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;

	ret = init_pwm_adc_test(sh);
	if (ret < 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED, "PWM Test failed with error: %d\n", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "PWM Test successful\n");

	return 0;
}

SHELL_SUBCMD_ADD((test), pwm, NULL, "Initialize the bringup test for PWM Module.", cmd_test_pwm, 1, 0);
