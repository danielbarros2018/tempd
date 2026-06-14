#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define TEMP_ON   55.0
#define TEMP_OFF  45.0
#define OFF_DELAY 120

#define GPIO_PIN 12

double read_temp(void)
{
    FILE *fp;
    int temp_milli;

    fp = fopen(
        "/sys/class/thermal/thermal_zone0/temp",
        "r");

    if (!fp)
    {
        perror("Erro ao abrir sensor de temperatura");
        return -1;
    }

    if (fscanf(fp, "%d", &temp_milli) != 1)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return temp_milli / 1000.0;
}

void fan_on(void)
{
    system("pinctrl set 12 dh");
}

void fan_off(void)
{
    system("pinctrl set 12 dl");
}

int main(void)
{
    int fan_is_on = 0;
    time_t below_since = 0;

    printf("Inicializando GPIO%d...\n", GPIO_PIN);

    system("pinctrl set 12 op");
    fan_off();

    while (1)
    {
        double temp = read_temp();

        if (temp < 0)
        {
            sleep(1);
            continue;
        }

        printf(
            "[%ld] Temperatura: %.1f°C | Fan: %s\n",
            time(NULL),
            temp,
            fan_is_on ? "ON" : "OFF"
        );

        /*
         * Liga imediatamente ao atingir TEMP_ON
         */
        if (temp >= TEMP_ON)
        {
            if (!fan_is_on)
            {
                printf(
                    "Temperatura %.1f°C >= %.1f°C -> LIGANDO FAN\n",
                    temp,
                    TEMP_ON
                );

                fan_on();

                fan_is_on = 1;
            }

            below_since = 0;
        }

        /*
         * Se estiver abaixo de TEMP_OFF,
         * inicia contagem para desligamento.
         */
        else if (temp < TEMP_OFF)
        {
            if (fan_is_on)
            {
                if (below_since == 0)
                {
                    below_since = time(NULL);

                    printf(
                        "Temperatura abaixo de %.1f°C. "
                        "Iniciando temporizador de %d segundos.\n",
                        TEMP_OFF,
                        OFF_DELAY
                    );
                }
                else
                {
                    time_t elapsed =
                        time(NULL) - below_since;

                    if (elapsed >= OFF_DELAY)
                    {
                        printf(
                            "Temperatura permaneceu abaixo "
                            "de %.1f°C por %d segundos. "
                            "DESLIGANDO FAN.\n",
                            TEMP_OFF,
                            OFF_DELAY
                        );

                        fan_off();

                        fan_is_on = 0;
                        below_since = 0;
                    }
                }
            }
        }

        /*
         * Entre TEMP_OFF e TEMP_ON:
         * mantém o estado atual.
         */
        else
        {
            below_since = 0;
        }

        sleep(1);
    }

    return 0;
}
