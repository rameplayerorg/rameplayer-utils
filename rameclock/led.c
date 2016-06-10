#include <fcntl.h>
#include <mosquitto.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define NUM_LEDS 3

static int fd[NUM_LEDS];

static void on_connect(struct mosquitto *mosq, void *data, int status) {
	if (!status) mosquitto_subscribe(mosq, NULL, "rame/clock/alert", 1);
}

static void select_led(int index) {
	static const char *values[] = {"0", "255"};
	const char *value;
	int i;

	for (i = 0; i < NUM_LEDS; i++) {
		value = values[i == index];
		write(fd[i], value, strlen(value));
	}
}

static void on_message(struct mosquitto *mosq, void *data, const struct mosquitto_message *msg) {
	if (msg->payloadlen) select_led(*((unsigned char *) msg->payload) - '0');
}

#define BUF_SIZE 64
int main() {
	struct mosquitto *mosq;

	int i;
	char buf[BUF_SIZE];

	for (i = 0; i < NUM_LEDS; i++) {
		snprintf(buf, BUF_SIZE, "/sys/class/leds/rame:ext%d/brightness", i + 1);
		fd[i] = open(buf, O_WRONLY);
		if (fd[i] == -1) return 1;
	}
	select_led(-1);

	mosquitto_lib_init();
	mosq = mosquitto_new(NULL, true, NULL);
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_message_callback_set(mosq, on_message);
	mosquitto_connect(mosq, "localhost", 1883, 60);
	mosquitto_loop_forever(mosq, -1, 1);
}
