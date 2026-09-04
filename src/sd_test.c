/**
 * @file sd_test.c
 * @author Gabriel Germano <gabriel.germano@edge.ufal.br>
 * @brief SD card (SPI) bring-up test.
 *
 * @version 0.1
 * @date 29-01-2026
 *
 * @copyright Copyright (c) 2026 - Centro de Inovação EDGE
 *
 */
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/fs/fs.h>
#include <ff.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <stdio.h>
#include <string.h>

#define DISK_NAME "SD"
#define MOUNT_POINT "/SD:"

static FATFS fat_fs;
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = MOUNT_POINT,
};

static int mount_sd(const struct shell *sh)
{
	int ret;

	shell_print(sh, "Mounting SD card...");

	ret = fs_mount(&mp);
	if (ret != 0) {
		shell_error(sh, "fs_mount failed (%d)", ret);
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "SD mounted at %s\n", MOUNT_POINT);
	return 0;
}

static int test_file_rw(const struct shell *sh)
{
	struct fs_file_t file;
	int ret;
	char read_buf[64];
	const char *test_str = "ZBOOK SD CARD TEST\n";

	fs_file_t_init(&file);

	shell_fprintf(sh, SHELL_NORMAL, "Opening file...");

	ret = fs_open(&file, MOUNT_POINT "/test.txt", FS_O_CREATE | FS_O_WRITE);
	if (ret < 0) {
		shell_error(sh, "fs_open (write) failed (%d)", ret);
		return ret;
	}
	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "SD create PASSED\n");


	shell_fprintf(sh, SHELL_NORMAL, "Writing to file...");
	ret = fs_write(&file, test_str, strlen(test_str));
	if (ret < 0) {
		shell_error(sh, "fs_write failed (%d)", ret);
		fs_close(&file);
		return ret;
	}
	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "SD write PASSED\n");


	fs_close(&file);

	shell_fprintf(sh, SHELL_NORMAL, "Reading file...");

	fs_file_t_init(&file);
	ret = fs_open(&file, MOUNT_POINT "/test.txt", FS_O_READ);
	if (ret < 0) {
		shell_error(sh, "fs_open (read) failed (%d)", ret);
		return ret;
	}

	ret = fs_read(&file, read_buf, sizeof(read_buf) - 1);
	if (ret < 0) {
		shell_error(sh, "fs_read failed (%d)", ret);
		fs_close(&file);
		return ret;
	}
	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "SD read PASSED\n");

	

	read_buf[ret] = '\0';

	fs_close(&file);

	shell_fprintf(sh, SHELL_NORMAL, "Cleaning up test file...\n");
	ret = fs_unlink(MOUNT_POINT "/test.txt");
	if (ret < 0) {
		shell_error(sh, "Failed to delete test file (%d)", ret);
	} else {
		shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Cleanup done\n");
	}

	shell_fprintf(sh, SHELL_NORMAL, "Read: %s\n", read_buf);

	if (strcmp(read_buf, test_str) == 0) {
		shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "SD test PASSED\n");
	} else {
		shell_fprintf(sh, SHELL_VT100_COLOR_RED, "SD test FAILED (data mismatch)\n");
		return -EIO;
	}

	return 0;
}

static int list_root_dir(const struct shell *sh)
{
	struct fs_dir_t dir;
	struct fs_dirent entry;
	int ret;

	fs_dir_t_init(&dir);

	shell_fprintf(sh, SHELL_NORMAL, "Listing root directory:\n");

	ret = fs_opendir(&dir, MOUNT_POINT);
	if (ret < 0) {
		shell_error(sh, "fs_opendir failed (%d)", ret);
		return ret;
	}

	while (1) {
		ret = fs_readdir(&dir, &entry);
		if (ret < 0) {
			shell_error(sh, "fs_readdir failed (%d)", ret);
			break;
		}

		/* End of directory */
		if (entry.name[0] == 0) {
			break;
		}

		shell_fprintf(sh, SHELL_NORMAL,
			      "[%s] %s (%zu bytes)\n",
			      (entry.type == FS_DIR_ENTRY_DIR) ? "DIR " : "FILE",
			      entry.name,
			      entry.size);
	}

	fs_closedir(&dir);

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "Directory listing done\n");
	return 0;
}

int cmd_test_sd(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int ret;

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "\nStarting SD card test...\n");

	ret = mount_sd(sh);
	if (ret < 0) {
		shell_error(sh, "Mount failed");
		return ret;
	}

	list_root_dir(sh);

	ret = test_file_rw(sh);
	if (ret < 0) {
		shell_error(sh, "File test failed");
		return ret;
	}

	shell_fprintf(sh, SHELL_VT100_COLOR_GREEN, "SD test completed successfully\n");

	return 0;
}

SHELL_SUBCMD_ADD((test), sd, NULL, "Initialize the bringup test for SD Card Module.", cmd_test_sd, 1, 0);
