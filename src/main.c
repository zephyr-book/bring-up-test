/**
 * @file main.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief Shell entry point for the ZBook bring-up firmware.
 *
 * @version 0.2
 * @date 29-01-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "addr_led_test.h"
#include "io_test.h"
#include "ir_io_test.h"
#include "ldr_test.h"
#include "pwm_test.h"

#include "accel_test.h"
#include "encoder_test.h"
#include "hall_test.h"
#include "mic_test.h"
#include "temp_test.h"

#include <stdlib.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

SHELL_SUBCMD_SET_CREATE(test_subcmds, (test));

static int cmd_test_all(const struct shell *sh, size_t argc, char **argv)
{
	int ret;

#define RUN(_name, _handler)                                                                       \
	do {                                                                                       \
		ret = _handler(sh, argc, argv);                                                    \
		if (ret < 0) {                                                                     \
			shell_fprintf(sh, SHELL_VT100_COLOR_RED, "%s test failed: %d\n", _name,    \
				      ret);                                                        \
			return ret;                                                                \
		}                                                                                  \
	} while (0)

	RUN("LDR", cmd_test_ldr);
	RUN("IO", cmd_test_io);
	RUN("PWM", cmd_test_pwm);
	RUN("Addressable LED", cmd_test_addr_led);
	RUN("IR IO", cmd_test_ir_io);

#if DT_NODE_EXISTS(DT_NODELABEL(tmp1075))
	RUN("Temperature", cmd_test_temp);
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(bmi323))
	RUN("IMU", cmd_test_accel);
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(microphone_adc))
	RUN("Microphone", cmd_test_mic);
#endif
#if DT_NODE_EXISTS(DT_ALIAS(hall_sensor))
	RUN("Hall sensor", cmd_test_hall);
#endif
#if DT_NODE_EXISTS(DT_NODELABEL(encoder_qdec))
	RUN("Encoder", cmd_test_encoder);
#endif

#undef RUN

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "All tests passed successfully.\n");
	return 0;
}

SHELL_SUBCMD_ADD((test), all, NULL, "Run every bringup test available on this revision.",
		 cmd_test_all, 1, 0);

SHELL_CMD_REGISTER(test, &test_subcmds, "Test bringup of Zbook", NULL);

int main(void)
{
	const struct device *dev;
	uint32_t dtr = 0;

	dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (!device_is_ready(dev)) {
		return 0;
	}

	printk("ZBook bring-up firmware\n");
	printk("Board: %s\n", CONFIG_BOARD_TARGET);
	printk("Run `test -h` for the tests available on this revision.\n");

	while (!dtr) {
		uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(100));
	}
	return 0;
}
