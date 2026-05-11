#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_CELLS 32
#define MAX_FRAME 263

static int g_debug = 0;
static const char *g_csv_path = NULL;
static int g_print_output = 1;
static int g_bms_line_output = 0;
static int g_module_id = 1;
static long g_period_ms = 1000;
static volatile sig_atomic_t g_stop = 0;

typedef struct {
	float voltage;
	float current;
	uint32_t soc_raw;
	uint16_t status;
	uint16_t cells[MAX_CELLS];
	int cell_count;
} TinyBMSData;

static void usage(const char *program_name)
{
	printf("Usage: %s [OPTIONS] COMMAND\n", program_name);
	printf("\nOptions:\n");
	printf("  -d, --device PATH     USB-UART device path, default: /dev/ttyUSB0\n");
	printf("  -h, --help            show this help\n");
	printf("      --debug           show TX/RX debug output\n");
	printf("  -c, --csv PATH        append read result to CSV file\n");
	printf("      --no-print        do not print human-readable result\n");
	printf("      --bms-line        print one Java ingest line: BMS,module,V,A,SOC,status,cells\n");
	printf("      --module ID       module id for --bms-line, default: 1\n");
	printf("      --period-ms MS    monitor interval, default: 1000\n");
	printf("\nCommands:\n");
	printf("  read                  read one TinyBMS data packet\n");
	printf("  monitor               read cyclically until Ctrl+C\n");
	printf("  reset                 reset BMS\n");
	printf("  clear-events          clear BMS events\n");
	printf("  clear-statistics      clear BMS statistics\n");
}

static uint16_t le16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] |
		((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) |
		((uint32_t)p[3] << 24);
}

static void handle_sigint(int sig)
{
	(void)sig;
	g_stop = 1;
}

static void sleep_ms(long ms)
{
	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
	}
}

static void wait_ms_interruptible(long ms)
{
	long waited = 0;
	while (!g_stop && waited < ms) {
		sleep_ms(10);
		waited += 10;
	}
}

static uint16_t crc16_modbus(const uint8_t *data, int len)
{
	uint16_t crc = 0xFFFF;
	for (int i = 0; i < len; i++) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xA001;
			} else {
				crc >>= 1;
			}
		}
	}
	return crc;
}

static int check_crc(const uint8_t *frame, int len)
{
	if (len < 4) {
		return -1;
	}

	uint16_t got = le16(frame + len - 2);
	uint16_t expected = crc16_modbus(frame, len - 2);
	if (got != expected) {
		fprintf(stderr, "CRC error: got=0x%04X expected=0x%04X\n", got, expected);
		return -1;
	}
	return 0;
}

static int send_cmd(int fd, const uint8_t *payload, int len)
{
	uint8_t frame[256];
	if (len < 0 || len > (int)sizeof(frame) - 2) {
		fprintf(stderr, "Payload too large: %d\n", len);
		return -1;
	}

	memcpy(frame, payload, (size_t)len);
	uint16_t crc = crc16_modbus(payload, len);
	frame[len] = (uint8_t)(crc & 0xFF);
	frame[len + 1] = (uint8_t)(crc >> 8);
	int frame_len = len + 2;

	if (g_debug) {
		fprintf(stderr, "TX:");
		for (int i = 0; i < frame_len; i++) {
			fprintf(stderr, " %02X", frame[i]);
		}
		fprintf(stderr, "\n");
	}

	tcflush(fd, TCIFLUSH);
	int written = (int)write(fd, frame, (size_t)frame_len);
	tcdrain(fd);
	if (written != frame_len) {
		fprintf(stderr, "write failed, written=%d expected=%d\n", written, frame_len);
		return -1;
	}
	return 0;
}

static int read_bytes(int fd, uint8_t *buf, int len, double timeout_sec)
{
	int got = 0;
	struct timespec start;
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &start);
	while (got < len && !g_stop) {
		int n = (int)read(fd, buf + got, (size_t)(len - got));
		if (n > 0) {
			got += n;
		} else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
			fprintf(stderr, "Read failed: %s\n", strerror(errno));
			return -1;
		}

		clock_gettime(CLOCK_MONOTONIC, &now);
		double elapsed = (double)(now.tv_sec - start.tv_sec) +
			(double)(now.tv_nsec - start.tv_nsec) / 1000000000.0;
		if (elapsed >= timeout_sec) {
			break;
		}
		sleep_ms(1);
	}

	if (g_debug) {
		fprintf(stderr, "RX:");
		for (int i = 0; i < got; i++) {
			fprintf(stderr, " %02X", buf[i]);
		}
		fprintf(stderr, "\n");
	}
	return got;
}

static int read_float_cmd(int fd, uint8_t command, float *value)
{
	uint8_t request[2] = {0xAA, command};
	uint8_t response[8];

	for (int attempt = 1; attempt <= 3; attempt++) {
		if (send_cmd(fd, request, 2) != 0) {
			return -1;
		}
		int received = read_bytes(fd, response, 8, 2.0);
		if (received != 8 || check_crc(response, 8) != 0 ||
			response[0] != 0xAA || response[1] != command) {
			sleep_ms(100);
			continue;
		}

		uint32_t raw = le32(response + 2);
		memcpy(value, &raw, sizeof(*value));
		return 0;
	}
	return -1;
}

static int reset_clear_cmd(int fd, uint8_t command)
{
	uint8_t request[3] = {0xAA, 0x02, command};
	uint8_t response[5];

	for (int attempt = 1; attempt <= 3; attempt++) {
		if (send_cmd(fd, request, 3) != 0) {
			return -1;
		}
		int received = read_bytes(fd, response, 5, 2.0);
		if (received != 5 || check_crc(response, 5) != 0 ||
			response[0] != 0xAA || response[1] != 0x01 || response[2] != 0x02) {
			sleep_ms(100);
			continue;
		}
		return 0;
	}
	return -1;
}

static int read_u32_cmd(int fd, uint8_t command, uint32_t *value)
{
	uint8_t request[2] = {0xAA, command};
	uint8_t response[8];

	for (int attempt = 1; attempt <= 3; attempt++) {
		if (send_cmd(fd, request, 2) != 0) {
			return -1;
		}
		int received = read_bytes(fd, response, 8, 2.0);
		if (received != 8 || check_crc(response, 8) != 0 ||
			response[0] != 0xAA || response[1] != command) {
			sleep_ms(100);
			continue;
		}
		*value = le32(response + 2);
		return 0;
	}
	return -1;
}

static int read_u16_cmd(int fd, uint8_t command, uint16_t *value)
{
	uint8_t request[2] = {0xAA, command};
	uint8_t response[6];

	for (int attempt = 1; attempt <= 3; attempt++) {
		if (send_cmd(fd, request, 2) != 0) {
			return -1;
		}
		int received = read_bytes(fd, response, 6, 2.0);
		if (received != 6 || check_crc(response, 6) != 0 ||
			response[0] != 0xAA || response[1] != command) {
			sleep_ms(100);
			continue;
		}
		*value = le16(response + 2);
		return 0;
	}
	return -1;
}

static int read_variable_response(int fd, uint8_t command, uint8_t *payload, int payload_cap, int *payload_len)
{
	uint8_t frame[MAX_FRAME];
	int received = read_bytes(fd, frame, 3, 2.0);
	if (received != 3) {
		return -1;
	}
	if (frame[0] != 0xAA || frame[1] != command) {
		fprintf(stderr, "Unexpected response header: 0x%02X 0x%02X\n", frame[0], frame[1]);
		return -1;
	}

	int len = frame[2];
	if (len > payload_cap || len + 5 > MAX_FRAME) {
		fprintf(stderr, "Payload too large: %d\n", len);
		return -1;
	}

	received = read_bytes(fd, frame + 3, len + 2, 2.0);
	if (received != len + 2 || check_crc(frame, len + 5) != 0) {
		return -1;
	}

	memcpy(payload, frame + 3, (size_t)len);
	*payload_len = len;
	return 0;
}

static int read_cell_voltages(int fd, TinyBMSData *data)
{
	uint8_t request[2] = {0xAA, 0x1C};
	uint8_t payload[128];
	int payload_len = 0;

	for (int attempt = 1; attempt <= 3; attempt++) {
		if (send_cmd(fd, request, 2) != 0) {
			return -1;
		}
		if (read_variable_response(fd, 0x1C, payload, (int)sizeof(payload), &payload_len) != 0) {
			sleep_ms(100);
			continue;
		}

		int cell_count = payload_len / 2;
		if (cell_count > MAX_CELLS) {
			cell_count = MAX_CELLS;
		}
		data->cell_count = cell_count;
		for (int i = 0; i < cell_count; i++) {
			data->cells[i] = le16(payload + i * 2);
		}
		return 0;
	}
	return -1;
}

static const char *status_name(uint16_t status)
{
	switch (status) {
	case 0x91: return "Charging";
	case 0x92: return "Fully charged";
	case 0x93: return "Discharging";
	case 0x96: return "Regeneration";
	case 0x97: return "Idle";
	case 0x9B: return "Fault";
	default: return "Unknown";
	}
}

static void print_packet(const TinyBMSData *data)
{
	printf("pack_voltage_v = %.3f\n", data->voltage);
	printf("pack_current_a = %.3f\n", data->current);
	printf("soc_percent = %.6f\n", (double)data->soc_raw / 1000000.0);
	printf("status = 0x%04X %s\n", data->status, status_name(data->status));
	printf("cell_count = %d\n", data->cell_count);
	for (int i = 0; i < data->cell_count; i++) {
		printf("cell_%02d_v = %.4f\n", i + 1, (double)data->cells[i] / 10000.0);
	}
	fflush(stdout);
}

static void print_bms_line(const TinyBMSData *data)
{
	printf("BMS,%d,%.3f,%.3f,%u,%u",
		g_module_id,
		data->voltage,
		data->current,
		data->soc_raw,
		(unsigned int)data->status);
	for (int i = 0; i < data->cell_count; i++) {
		int cell_mv = (data->cells[i] + 5) / 10;
		printf(",%d", cell_mv);
	}
	printf("\n");
	fflush(stdout);
}

static int csv_needs_header(const char *path)
{
	struct stat st;
	return stat(path, &st) != 0 || st.st_size == 0;
}

static void csv_timestamp(char *buf, int len)
{
	time_t now = time(NULL);
	struct tm tm_now;
	localtime_r(&now, &tm_now);
	strftime(buf, (size_t)len, "%Y-%m-%d %H:%M:%S", &tm_now);
}

static int write_packet_csv(const char *path, const TinyBMSData *data)
{
	int header = csv_needs_header(path);
	FILE *f = fopen(path, "a");
	if (f == NULL) {
		fprintf(stderr, "Cannot open CSV file %s: %s\n", path, strerror(errno));
		return -1;
	}

	if (header) {
		fprintf(f, "timestamp,pack_voltage_v,current_a,soc_percent,status_hex,status_name,cell_count");
		for (int i = 0; i < data->cell_count; i++) {
			fprintf(f, ",cell_%02d_v", i + 1);
		}
		fprintf(f, "\n");
	}

	char ts[32];
	csv_timestamp(ts, (int)sizeof(ts));
	fprintf(f, "%s,%.3f,%.3f,%.6f,0x%04X,%s,%d",
		ts,
		data->voltage,
		data->current,
		(double)data->soc_raw / 1000000.0,
		data->status,
		status_name(data->status),
		data->cell_count);
	for (int i = 0; i < data->cell_count; i++) {
		fprintf(f, ",%.4f", (double)data->cells[i] / 10000.0);
	}
	fprintf(f, "\n");
	fclose(f);
	return 0;
}

static int read_packet(int fd, TinyBMSData *data)
{
	memset(data, 0, sizeof(*data));
	if (read_float_cmd(fd, 0x14, &data->voltage) != 0) return -1;
	if (read_float_cmd(fd, 0x15, &data->current) != 0) return -1;
	if (read_u32_cmd(fd, 0x1A, &data->soc_raw) != 0) return -1;
	if (read_u16_cmd(fd, 0x18, &data->status) != 0) return -1;
	if (read_cell_voltages(fd, data) != 0) return -1;
	return 0;
}

static int handle_packet(const TinyBMSData *data)
{
	if (g_print_output) {
		print_packet(data);
	}
	if (g_bms_line_output) {
		print_bms_line(data);
	}
	if (g_csv_path != NULL && write_packet_csv(g_csv_path, data) != 0) {
		return -1;
	}
	return 0;
}

static int serial_open(const char *device)
{
	int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", device, strerror(errno));
		return -1;
	}

	struct termios tty;
	if (tcgetattr(fd, &tty) != 0) {
		fprintf(stderr, "Cannot read serial settings: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	cfmakeraw(&tty);
	tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
	tty.c_cflag |= CS8 | CLOCAL | CREAD;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;

	cfsetispeed(&tty, B115200);
	cfsetospeed(&tty, B115200);

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		fprintf(stderr, "Cannot apply serial settings: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	tcflush(fd, TCIOFLUSH);
	if (g_debug) {
		fprintf(stderr, "Opened and configured %s, fd=%d\n", device, fd);
	}
	return fd;
}

static int parse_int_arg(const char *text, int fallback)
{
	char *end = NULL;
	long value = strtol(text, &end, 10);
	if (end == text || *end != '\0') {
		return fallback;
	}
	return (int)value;
}

int main(int argc, char **argv)
{
	const char *device = "/dev/ttyUSB0";
	const char *command = "read";
	int arg = 1;

	while (arg < argc) {
		if (strcmp(argv[arg], "-d") == 0 || strcmp(argv[arg], "--device") == 0) {
			if (arg + 1 >= argc) {
				fprintf(stderr, "Error: %s needs device path\n\n", argv[arg]);
				usage(argv[0]);
				return 1;
			}
			device = argv[arg + 1];
			arg += 2;
		} else if (strcmp(argv[arg], "-h") == 0 || strcmp(argv[arg], "--help") == 0) {
			usage(argv[0]);
			return 0;
		} else if (strcmp(argv[arg], "--debug") == 0) {
			g_debug = 1;
			arg++;
		} else if (strcmp(argv[arg], "-c") == 0 || strcmp(argv[arg], "--csv") == 0) {
			if (arg + 1 >= argc) {
				fprintf(stderr, "Error: %s needs file path\n\n", argv[arg]);
				usage(argv[0]);
				return 1;
			}
			g_csv_path = argv[arg + 1];
			arg += 2;
		} else if (strcmp(argv[arg], "--no-print") == 0) {
			g_print_output = 0;
			arg++;
		} else if (strcmp(argv[arg], "--bms-line") == 0) {
			g_bms_line_output = 1;
			arg++;
		} else if (strcmp(argv[arg], "--module") == 0) {
			if (arg + 1 >= argc) {
				fprintf(stderr, "Error: %s needs module id\n\n", argv[arg]);
				usage(argv[0]);
				return 1;
			}
			g_module_id = parse_int_arg(argv[arg + 1], 1);
			if (g_module_id < 1 || g_module_id > 4) {
				fprintf(stderr, "Error: module id must be 1..4\n");
				return 1;
			}
			arg += 2;
		} else if (strcmp(argv[arg], "--period-ms") == 0) {
			if (arg + 1 >= argc) {
				fprintf(stderr, "Error: %s needs milliseconds\n\n", argv[arg]);
				usage(argv[0]);
				return 1;
			}
			g_period_ms = parse_int_arg(argv[arg + 1], 1000);
			if (g_period_ms < 100) {
				g_period_ms = 100;
			}
			arg += 2;
		} else {
			command = argv[arg];
			arg++;
		}
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_sigint;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) != 0 || sigaction(SIGTERM, &sa, NULL) != 0) {
		fprintf(stderr, "Cannot set signal handler: %s\n", strerror(errno));
		return 1;
	}

	int fd = serial_open(device);
	if (fd < 0) {
		return 1;
	}
	sleep_ms(100);

	if (strcmp(command, "read") == 0) {
		TinyBMSData data;
		if (read_packet(fd, &data) != 0 || handle_packet(&data) != 0) {
			fprintf(stderr, "Cannot read packet\n");
			close(fd);
			return 1;
		}
	} else if (strcmp(command, "monitor") == 0) {
		while (!g_stop) {
			TinyBMSData data;
			if (read_packet(fd, &data) == 0) {
				if (handle_packet(&data) != 0) {
					close(fd);
					return 1;
				}
			} else {
				fprintf(stderr, "Cannot read packet\n");
			}
			wait_ms_interruptible(g_period_ms);
		}
		fprintf(stderr, "ok=stopped\n");
	} else if (strcmp(command, "reset") == 0) {
		if (reset_clear_cmd(fd, 0x05) != 0) {
			fprintf(stderr, "Cannot reset BMS\n");
			close(fd);
			return 1;
		}
		printf("ok=reset\n");
	} else if (strcmp(command, "clear-events") == 0) {
		if (reset_clear_cmd(fd, 0x01) != 0) {
			fprintf(stderr, "Cannot clear events\n");
			close(fd);
			return 1;
		}
		printf("ok=clear-events\n");
	} else if (strcmp(command, "clear-statistics") == 0) {
		if (reset_clear_cmd(fd, 0x02) != 0) {
			fprintf(stderr, "Cannot clear statistics\n");
			close(fd);
			return 1;
		}
		printf("ok=clear-statistics\n");
	} else {
		fprintf(stderr, "Unknown command: %s\n", command);
		close(fd);
		return 1;
	}

	close(fd);
	return 0;
}
