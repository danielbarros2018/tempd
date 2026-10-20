
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>

#define TEMP_ON   65.0
#define TEMP_OFF  55.0
#define OFF_DELAY 30

#define GPIO_PIN 12

#define LOG_FILE "/var/log/tempd.csv"
#define PID_FILE "/run/tempd.pid"

typedef enum {
    MODE_AUTO,
    MODE_MANUAL_ON,
    MODE_MANUAL_OFF
} fan_mode_t;

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t requested_mode = MODE_AUTO;

static int fan_is_on = 0;
static time_t below_since = 0;

static double read_temp(void)
{
    FILE *fp = fopen(
        "/sys/class/thermal/thermal_zone0/temp", "r"
    );

    if (!fp) {
        perror("Erro ao abrir sensor");
        return -1;
    }

    int temp_milli;

    if (fscanf(fp, "%d", &temp_milli) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return temp_milli / 1000.0;
}

static void log_event(const char *event, double temp)
{
    FILE *fp = fopen(LOG_FILE, "a");

    if (!fp) {
        perror("Erro ao abrir log");
        return;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    char timestamp[32];

    localtime_r(&now, &tm_now);

    strftime(
        timestamp,
        sizeof(timestamp),
        "%Y-%m-%d %H:%M:%S",
        &tm_now
    );

    fprintf(
        fp,
        "%s,%.1f,%s\n",
        timestamp,
        temp,
        event
    );

    fclose(fp);
}

static int set_fan(int on, double temp)
{
    if (fan_is_on == on)
        return 0;

    char command[80];

    snprintf(
        command,
        sizeof(command),
        "pinctrl set %d %s",
        GPIO_PIN,
        on ? "dh" : "dl"
    );

    if (system(command) != 0) {
        fprintf(stderr, "Erro ao executar: %s\n", command);
        return -1;
    }

    fan_is_on = on;

    printf(
        "FAN %s | Temperatura: %.1f°C\n",
        on ? "ON" : "OFF",
        temp
    );
    fflush(stdout);

    log_event(on ? "ON" : "OFF", temp);

    return 0;
}

static void signal_handler(int sig)
{
    switch (sig) {
        case SIGUSR1:
            requested_mode = MODE_MANUAL_ON;
            break;

        case SIGUSR2:
            requested_mode = MODE_MANUAL_OFF;
            break;

        case SIGHUP:
            requested_mode = MODE_AUTO;
            break;

        case SIGTERM:
        case SIGINT:
            running = 0;
            break;
    }
}

static int install_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    int signals[] = {
        SIGUSR1, SIGUSR2, SIGHUP, SIGTERM, SIGINT
    };

    for (size_t i = 0;
         i < sizeof(signals) / sizeof(signals[0]);
         i++) {

        if (sigaction(signals[i], &sa, NULL) != 0) {
            perror("sigaction");
            return -1;
        }
    }

    return 0;
}

static int write_pid(void)
{
    FILE *fp = fopen(PID_FILE, "w");

    if (!fp) {
        perror("Erro ao criar PID");
        return -1;
    }

    fprintf(fp, "%ld\n", (long)getpid());
    fclose(fp);

    return 0;
}

static int send_command(const char *arg)
{
    int sig;

    if (strcasecmp(arg, "ON") == 0) {
        sig = SIGUSR1;
    }
    else if (strcasecmp(arg, "OFF") == 0) {
        sig = SIGUSR2;
    }
    else if (strcasecmp(arg, "AUTO") == 0) {
        sig = SIGHUP;
    }
    else {
        fprintf(
            stderr,
            "Uso: tempd [ON|OFF|AUTO]\n"
        );
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(PID_FILE, "r");

    if (!fp) {
        fprintf(
            stderr,
            "Daemon não encontrado. "
            "Verifique: systemctl status tempd\n"
        );
        return EXIT_FAILURE;
    }

    long pid;

    if (fscanf(fp, "%ld", &pid) != 1 || pid <= 1) {
        fclose(fp);
        fprintf(stderr, "PID inválido\n");
        return EXIT_FAILURE;
    }

    fclose(fp);

    if (kill((pid_t)pid, sig) != 0) {
        perror("Erro ao enviar comando");
        return EXIT_FAILURE;
    }

    printf("Comando %s enviado ao tempd (PID %ld)\n",
           arg, pid);

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    if (argc == 2)
        return send_command(argv[1]);

    if (argc != 1) {
        fprintf(stderr, "Uso: tempd [ON|OFF|AUTO]\n");
        return EXIT_FAILURE;
    }

    setvbuf(stdout, NULL, _IOLBF, 0);

    if (install_signals() != 0)
        return EXIT_FAILURE;

    if (write_pid() != 0)
        return EXIT_FAILURE;

    char command[80];

    snprintf(
        command,
        sizeof(command),
        "pinctrl set %d op",
        GPIO_PIN
    );

    if (system(command) != 0) {
        fprintf(stderr, "Erro ao configurar GPIO\n");
        unlink(PID_FILE);
        return EXIT_FAILURE;
    }

    // Estado inicial: desligado.
    snprintf(
        command,
        sizeof(command),
        "pinctrl set %d dl",
        GPIO_PIN
    );

    if (system(command) != 0) {
        fprintf(stderr, "Erro ao inicializar FAN\n");
        unlink(PID_FILE);
        return EXIT_FAILURE;
    }

    printf("tempd iniciado | PID: %ld\n", (long)getpid());

    fan_mode_t current_mode = MODE_AUTO;

    while (running) {
        double temp = read_temp();

        if (temp < 0) {
            sleep(1);
            continue;
        }

        fan_mode_t new_mode =
            (fan_mode_t)requested_mode;

        if (new_mode != current_mode) {
            current_mode = new_mode;
            below_since = 0;

            printf(
                "Modo alterado: %s\n",
                current_mode == MODE_AUTO ? "AUTO" :
                current_mode == MODE_MANUAL_ON ? "MANUAL ON" :
                "MANUAL OFF"
            );
        }

        if (current_mode == MODE_MANUAL_ON) {
            set_fan(1, temp);
        }
        else if (current_mode == MODE_MANUAL_OFF) {
            set_fan(0, temp);
        }
        else {
            if (temp >= TEMP_ON) {
                if (!fan_is_on)
                    set_fan(1, temp);

                below_since = 0;
            }
            else if (temp < TEMP_OFF) {
                if (fan_is_on) {
                    if (below_since == 0) {
                        below_since = time(NULL);
                    }
                    else if (
                        time(NULL) - below_since >= OFF_DELAY
                    ) {
                        if (set_fan(0, temp) == 0)
                            below_since = 0;
                    }
                }
            }
            else {
                below_since = 0;
            }
        }

        sleep(1);
    }

    // Desliga a FAN ao encerrar o serviço.
    double temp = read_temp();

    if (fan_is_on)
        set_fan(0, temp);

    unlink(PID_FILE);

    printf("tempd encerrado\n");

    return EXIT_SUCCESS;
}

