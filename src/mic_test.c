/**
 * @file mic_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief Electret microphone bring-up test (P2 only).
 *
 * @version 0.1
 * @date 03-09-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include "mic_test.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#define MIC_CHANNEL_NODE DT_NODELABEL(microphone_adc)

#if DT_NODE_EXISTS(MIC_CHANNEL_NODE)

#define WINDOW_COUNT   20
#define WINDOW_SAMPLES 200
#define ADC_RESOLUTION 12
#define VREF_MV        3300

static const struct adc_channel_cfg mic_channel = ADC_CHANNEL_CFG_DT(MIC_CHANNEL_NODE);

/* The channel node is a child of the ADC controller. */
static const struct device *const adc_dev = DEVICE_DT_GET(DT_PARENT(MIC_CHANNEL_NODE));

int cmd_test_mic(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int ret;
	uint16_t sample = 0;
	uint32_t widest_pp = 0;
	struct adc_sequence sequence = {
		.buffer = &sample,
		.buffer_size = sizeof(sample),
		.channels = BIT(mic_channel.channel_id),
		.resolution = ADC_RESOLUTION,
	};

	if (!device_is_ready(adc_dev)) {
		shell_error(sh, "ADC device %s not ready", adc_dev->name);
		return -ENODEV;
	}

	ret = adc_channel_setup(adc_dev, &mic_channel);
	if (ret < 0) {
		shell_error(sh, "Failed to setup microphone ADC channel: %d", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Microphone test started\n");
	shell_fprintf(sh, SHELL_NORMAL, "Make some noise near MK1...\n");

	/* MK1 is AC-coupled, so the DC level sits near mid-rail and sound appears
	 * as deviation around it. A single reading carries no information; the
	 * amplitude over a window does.
	 */
	for (int w = 0; w < WINDOW_COUNT; w++) {
		uint16_t min = UINT16_MAX;
		uint16_t max = 0;
		uint32_t peak_to_peak;
		int32_t pp_mv;

		for (int i = 0; i < WINDOW_SAMPLES; i++) {
			ret = adc_read(adc_dev, &sequence);
			if (ret < 0) {
				shell_error(sh, "ADC read failed: %d", ret);
				return ret;
			}

			if (sample < min) {
				min = sample;
			}
			if (sample > max) {
				max = sample;
			}
		}

		peak_to_peak = (uint32_t)max - min;
		if (peak_to_peak > widest_pp) {
			widest_pp = peak_to_peak;
		}

		pp_mv = (int32_t)peak_to_peak;
		(void)adc_raw_to_millivolts(VREF_MV, ADC_GAIN_1, ADC_RESOLUTION, &pp_mv);

		shell_fprintf(sh, SHELL_NORMAL,
			      "window %2d: min=%4u max=%4u peak-to-peak=%4u (%d mV)\n", w, min,
			      max, peak_to_peak, pp_mv);
	}

	if (widest_pp == 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED,
			      "Microphone reading never changed -- channel looks dead\n");
		return -EIO;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN,
		      "Microphone test completed (widest peak-to-peak %u counts)\n", widest_pp);
	return 0;
}

SHELL_SUBCMD_ADD((test), mic, NULL, "Initialize the bringup test for the microphone (P2).",
		 cmd_test_mic, 1, 0);

#endif /* DT_NODE_EXISTS(MIC_CHANNEL_NODE) */
