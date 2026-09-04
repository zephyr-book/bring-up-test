/**
 * @file ldr_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief LDR light sensor bring-up test.
 *
 * @version 0.1
 * @date 29-01-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "ldr_test.h"
#include <zephyr/devicetree.h>

#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_types.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>

static const struct adc_channel_cfg ldr_adc = ADC_CHANNEL_CFG_DT(DT_NODELABEL(ldr_adc));

static const struct pwm_dt_spec pwm_buzzer = PWM_DT_SPEC_GET(DT_ALIAS(pwm_buzzer));

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

static int init_ldr_adc_test(const struct shell *sh)
{
	int ret;
	uint32_t frequency, period_ns, pulse_ns;
	int32_t voltage_mv;
	uint16_t adc_buffer = 0;
	struct adc_sequence sequence = {
		.buffer = &adc_buffer,
		.buffer_size = sizeof(adc_buffer),
		.calibrate = true,
		.channels = BIT(ldr_adc.channel_id),
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

	ret = adc_channel_setup(adc_dev, &ldr_adc);
	if (ret < 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED, "Failed to setup ADC channel");
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "ADC Test initialized.\n");
	shell_fprintf(sh, SHELL_NORMAL, "Reading LDR and adjusting PWM frequency\n");

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

		frequency = 100 + (1000 * voltage_mv) / 3300;
		
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

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "\nPWM Test completed\n");

	return 0;
}


int cmd_test_ldr(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;

	ret = init_ldr_adc_test(sh);
	if (ret < 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED, "LDR ADC Test failed with error: %d\n", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "PWM Test successful\n");

	return 0;
}

SHELL_SUBCMD_ADD((test), ldr, NULL, "Initialize the bringup test for LDR Module.", cmd_test_ldr, 1, 0);
