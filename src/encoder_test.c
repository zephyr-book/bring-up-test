/**
 * @file encoder_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief PEC12R rotary encoder bring-up test (P2 only).
 *
 * @version 0.1
 * @date 03-09-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "encoder_test.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define QDEC_NODE DT_NODELABEL(encoder_qdec)

#if DT_NODE_EXISTS(QDEC_NODE)

#define POLL_INTERVAL_MS 20
#define TEST_DURATION_MS 20000

static const struct device *const qdec_dev = DEVICE_DT_GET(QDEC_NODE);
static const struct gpio_dt_spec enc_button = GPIO_DT_SPEC_GET(DT_ALIAS(enc_button), gpios);

static atomic_t enc_position;
static atomic_t enc_events;

static void encoder_callback(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type != INPUT_EV_REL || evt->code != INPUT_REL_WHEEL) {
		return;
	}

	atomic_add(&enc_position, evt->value);
	atomic_inc(&enc_events);
}

INPUT_CALLBACK_DEFINE(qdec_dev, encoder_callback, NULL);

int cmd_test_encoder(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;
	atomic_val_t last_reported;

	if (!device_is_ready(qdec_dev)) {
		shell_error(sh, "Encoder device %s not ready", qdec_dev->name);
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&enc_button)) {
		shell_error(sh, "Encoder push-switch not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&enc_button, GPIO_INPUT);
	if (ret < 0) {
		shell_error(sh, "Failed to configure encoder switch: %d", ret);
		return ret;
	}

	atomic_set(&enc_position, 0);
	atomic_set(&enc_events, 0);
	last_reported = 0;

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Encoder test started\n");
	shell_fprintf(sh, SHELL_NORMAL,
		      "Turn the shaft to move the counter; press it to finish (%d s timeout)\n",
		      TEST_DURATION_MS / 1000);

	for (int elapsed = 0; elapsed < TEST_DURATION_MS; elapsed += POLL_INTERVAL_MS) {
		atomic_val_t position = atomic_get(&enc_position);

		if (position != last_reported) {
			shell_fprintf(sh, SHELL_NORMAL, "Encoder position: %ld (%s)\n",
				      (long)position,
				      position > last_reported ? "CW" : "CCW");
			last_reported = position;
		}

		if (gpio_pin_get_dt(&enc_button) == 1) {
			shell_fprintf(sh, SHELL_NORMAL, "Encoder switch pressed\n");

			/* Crude debounce: wait for release before returning. */
			while (gpio_pin_get_dt(&enc_button) == 1) {
				k_msleep(POLL_INTERVAL_MS);
			}
			break;
		}

		k_msleep(POLL_INTERVAL_MS);
	}

	if (atomic_get(&enc_events) == 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED,
			      "No rotation detected -- check ENC_A/ENC_B wiring\n");
		return -EIO;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN,
		      "Encoder test completed (%ld events, final position %ld)\n",
		      (long)atomic_get(&enc_events), (long)atomic_get(&enc_position));
	return 0;
}

SHELL_SUBCMD_ADD((test), encoder, NULL,
		 "Initialize the bringup test for the rotary encoder (P2).", cmd_test_encoder, 1,
		 0);

#endif /* DT_NODE_EXISTS(QDEC_NODE) */
