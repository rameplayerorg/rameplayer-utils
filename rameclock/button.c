#include <fcntl.h>
#include <mosquitto.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define TOPIC "rame/clock/alert"

static struct mosquitto *mosq;
static char state = '0';

static void set_state(char new) {
	state = new;
	mosquitto_publish(mosq, NULL, TOPIC, 1, &state, 1, true);
}

#define REPORT_SIZE 8
static void handle_hidraw(int dev) {
	static const char command[REPORT_SIZE] = {
		0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
	};
	char report[REPORT_SIZE];

	write(dev, command, REPORT_SIZE);

	if (read(dev, report, REPORT_SIZE) == REPORT_SIZE)
		switch (report[0]) {
		case 21:
			if (state > '0') set_state('0');
			break;
		case 22:
			if (state < '2') set_state('2');
			break;
		case 23:
			if (state == '0') set_state('1');
		}
}

int main(int argc, char **argv) {
	void (*handler)(int);

	if (argc < 3) return 1;

	if (!strcmp(argv[1], "hidraw")) handler = handle_hidraw;
	else return 1;

	int dev = open(argv[2], O_RDWR);
	if (dev == -1) return 1;

	fcntl(dev, F_SETFL, fcntl(dev, F_GETFL, 0) | O_NONBLOCK);

	mosquitto_lib_init();
	mosq = mosquitto_new(NULL, true, NULL);
	mosquitto_will_set(mosq, TOPIC, 1, &state, 1, true);
	mosquitto_connect(mosq, "localhost", 1883, 60);

	set_state(state);

	while (1) {
		handler(dev);
		mosquitto_loop(mosq, 100, 1);
	}
}
