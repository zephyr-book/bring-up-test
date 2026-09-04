/**
 * @file motor_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief DC motor bring-up test.
 *
 * @version 0.1
 * @date 03-09-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "motor_test.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#if DT_NODE_EXISTS(DT_ALIAS(motor_pwm))
#define MOTOR_NODE DT_ALIAS(motor_pwm)
#else
#define MOTOR_NODE DT_ALIAS(pwm_mosfet)
#endif

#define STEP_PERCENT  10
#define STEP_DELAY_MS 400

static const struct pwm_dt_spec motor = PWM_DT_SPEC_GET(MOTOR_NODE);

static int set_duty(const struct shell *sh, uint32_t percent)
{
	uint32_t pulse = (uint32_t)((uint64_t)motor.period * percent / 100U);
	int ret = pwm_set_pulse_dt(&motor, pulse);

	if (ret < 0) {
		shell_error(sh, "Failed to set duty %u%%: %d", percent, ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_NORMAL, "Motor duty: %3u%%\r", percent);
	return 0;
}

int cmd_test_motor(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;

	if (!pwm_is_ready_dt(&motor)) {
		shell_error(sh, "Motor PWM device not ready");
		return -ENODEV;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Motor test started (period %u ns)\n",
		      motor.period);

	for (uint32_t duty = 0; duty <= 100U; duty += STEP_PERCENT) {
		ret = set_duty(sh, duty);
		if (ret < 0) {
			goto stop;
		}
		k_msleep(STEP_DELAY_MS);
	}

	for (uint32_t duty = 100U; duty > 0U; duty -= STEP_PERCENT) {
		ret = set_duty(sh, duty - STEP_PERCENT);
		if (ret < 0) {
			goto stop;
		}
		k_msleep(STEP_DELAY_MS);
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "\nMotor test completed\n");
stop:
	/* Always leave the motor off. */
	(void)pwm_set_pulse_dt(&motor, 0);
	return ret;
}

SHELL_SUBCMD_ADD((test), motor, NULL, "Initialize the bringup test for the DC motor.",
		 cmd_test_motor, 1, 0);
