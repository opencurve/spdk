/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

static int
check_modules(char *driver_name)
{
	FILE *fd;
	const char *proc_modules = "/proc/modules";
	char buffer[256];

	fd = fopen(proc_modules, "r");
	if (!fd)
		return -1;

	while (fgets(buffer, sizeof(buffer), fd)) {
		if (strstr(buffer, driver_name) == NULL)
			continue;
		else {
			fclose(fd);
			return 0;
		}
	}
	fclose(fd);

	return -1;
}

static int
get_u32_from_file(const char *sysfs_file, uint32_t *value)
{
	FILE *f;
	char buf[BUFSIZ];

	f = fopen(sysfs_file, "r");
	if (f == NULL) {
		return -1;
	}

	if (fgets(buf, sizeof(buf), f) != NULL) {
		*value = strtoul(buf, NULL, 10);
	}

	fclose(f);

	return 0;
}

static int
get_str_from_file(const char *sysfs_file, char *buf, int len)
{
	FILE *f;

	f = fopen(sysfs_file, "r");
	if (f == NULL) {
		return -1;
	}

	if (fgets(buf, len, f) != NULL) {
		fclose(f);
		return 0;
	}

	fclose(f);
	return -1;
}

static int
put_u32_to_file(const char *sysfs_file, uint32_t value)
{
	FILE *f;
	int n;
	char buf[BUFSIZ];

	f = fopen(sysfs_file, "w");
	if (f == NULL) {
		return -1;
	}

	n = snprintf(buf, sizeof(buf), "%ul", value);
	if ((n < 0) || (n >= (int)sizeof(buf))) {
		fclose(f);
		return -1;
	}

	if (fwrite(buf, n, 1, f) == 0) {
		fclose(f);
		return -1;
	}

	fclose(f);
	return 0;
}

static int
get_u64_from_file(const char *sysfs_file, uint64_t *value)
{
	FILE *f;
	char buf[BUFSIZ];

	f = fopen(sysfs_file, "r");
	if (f == NULL) {
		return -1;
	}

	if (fgets(buf, sizeof(buf), f) != NULL) {
		*value = strtoull(buf, NULL, 10);
	}

	fclose(f);

	return 0;
}

static void
usage(char *program_name)
{
	printf("%s options\n", program_name);
	printf("\t[-h usage]\n");
	printf("\t[-n number of DMA channels]\n");
	printf("\t[-q queue depth, per DMA channel]\n");
	printf("\t[-s [n^2] transfer size, per descriptor]\n");
	printf("\t[-t total [n^2] data to tranfer, per DMA channel]\n");
}

int main(int argc, char *argv[])
{
	int op;
	int rc;
	char buf[BUFSIZ];
	uint32_t i, threads = 0;
	uint32_t ring_size, queue_depth = 0;
	uint32_t transfer_size, order = 0;
	uint64_t total_size, copied = 0;
	uint64_t elapsed_time = 0;
	char channel[1024];

	if (check_modules("ioatdma")) {
		fprintf(stderr, "Ioat driver not loaded,"
			" run `modprove -v ioatdma` first\n");
		return -1;
	}
	if (check_modules("dmaperf")) {
		fprintf(stderr, "Kernel Ioat test driver not loaded,"
			" run `insmod dmaperf.ko` in the kmod directory\n");
		return -1;
	}

	rc = get_u32_from_file("/sys/module/ioatdma/parameters/ioat_ring_alloc_order",
			       &order);
	if (rc < 0) {
		fprintf(stderr, "Cannot get default ioat queue depth\n");
		return -1;
	}
	ring_size = 1UL << order;

	while ((op = getopt(argc, argv, "h:n:q:s:t:")) != -1) {
		switch (op) {
		case 'n':
			threads = atoi(optarg);
			rc = put_u32_to_file("/sys/kernel/debug/dmaperf/dmaperf/threads", threads);
			if (rc < 0) {
				fprintf(stderr, "Cannot set dma channels\n");
				return -1;
			}
			break;
		case 'q':
			queue_depth = atoi(optarg);
			if (queue_depth > ring_size) {
				fprintf(stderr, "Max Ioat DMA ring size %d\n", ring_size);
				return -1;
			}
			rc = put_u32_to_file("/sys/kernel/debug/dmaperf/dmaperf/queue_depth", queue_depth);
			if (rc < 0) {
				fprintf(stderr, "Cannot set queue depth\n");
				return -1;
			}
			break;
		case 's':
			order = atoi(optarg);
			rc = put_u32_to_file("/sys/kernel/debug/dmaperf/dmaperf/transfer_size_order", order);
			if (rc < 0) {
				fprintf(stderr, "Cannot set descriptor transfer size order\n");
				return -1;
			}
			break;
		case 't':
			order = atoi(optarg);
			rc = put_u32_to_file("/sys/kernel/debug/dmaperf/dmaperf/total_size_order", order);
			if (rc < 0) {
				fprintf(stderr, "Cannot set channel total transfer size order\n");
				return -1;
			}
			break;
		case 'h' :
			usage(argv[0]);
			exit(0);
		default:
			usage(argv[0]);
			exit(1);
		}
	}

	/* get driver configuration */
	rc = get_u32_from_file("/sys/kernel/debug/dmaperf/dmaperf/transfer_size_order",
			       &order);
	if (rc < 0) {
		fprintf(stderr, "Cannot get channel descriptor transfer size\n");
		return -1;
	}
	transfer_size = 1UL << order;

	rc = get_u32_from_file("/sys/kernel/debug/dmaperf/dmaperf/total_size_order",
			       &order);
	if (rc < 0) {
		fprintf(stderr, "Cannot get channel total transfer size\n");
		return -1;
	}
	total_size = 1ULL << order;

	rc = get_u32_from_file("/sys/kernel/debug/dmaperf/dmaperf/threads",
			       &threads);
	if (rc < 0) {
		fprintf(stderr, "Cannot get dma channel threads\n");
		return -1;
	}

	rc = get_u32_from_file("/sys/kernel/debug/dmaperf/dmaperf/queue_depth",
			       &queue_depth);
	if (rc < 0) {
		fprintf(stderr, "Cannot get queue depth\n");
		return -1;
	}

	fprintf(stdout,
		"Total %d Channels, Queue_Depth %d, Transfer Size %d Bytes, Total Transfer Size %"PRIu64" GB\n",
		threads, queue_depth, transfer_size, total_size >> 30ULL);

	/* run the channels */
	rc = put_u32_to_file("/sys/kernel/debug/dmaperf/dmaperf/run", 1);
	if (rc < 0) {
		fprintf(stderr, "Cannot run the channels\n");
		return -1;
	}

	fprintf(stdout, "Running I/O ");
	fflush(stdout);
	/* wait all the channels to be idle */
	while (!get_str_from_file("/sys/kernel/debug/dmaperf/dmaperf/status", buf, BUFSIZ)) {
		if (strstr(buf, "idle") != NULL) {
			fprintf(stdout, "\n");
			fflush(stdout);
			sleep(1);
			break;
		}
		fprintf(stdout, ". ");
		fflush(stdout);
		sleep(1);
	}

	/* collect each channel performance data */

	for (i = 0; i < threads; i++) {
		/* total data transfer length for the DMA channel in Bytes */
		sprintf(channel, "/sys/kernel/debug/dmaperf/dmaperf/thread_%u/copied", i);
		rc = get_u64_from_file(channel, &copied);
		if (rc < 0) {
			fprintf(stderr, "Cannot get channel copied bytes\n");
			return -1;
		}
		/* time in microseconds for total data transfer length */
		sprintf(channel, "/sys/kernel/debug/dmaperf/dmaperf/thread_%u/elapsed_time", i);
		rc = get_u64_from_file(channel, &elapsed_time);
		if (rc < 0) {
			fprintf(stderr, "Cannot get channel elapsed time\n");
			return -1;
		}
		assert(elapsed_time != 0);
		fprintf(stdout, "Channel %d Performance Data %"PRIu64" MB/s\n",
			i, copied / elapsed_time);
	}

	return 0;
}
