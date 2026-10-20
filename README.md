# tempd

Daemon para **Raspberry Pi 4 com Ubuntu ARM64** que monitora a temperatura da CPU e controla automaticamente uma ventoinha de 5 V conectada ao **GPIO12**, por meio de um transistor S8050. Também permite acionamento manual pelo terminal e registra as mudanças de estado em CSV.

## Funcionamento

No modo automático:

- **Liga a ventoinha quando a CPU atinge 65°C**.
- Mantém a ventoinha ligada enquanto a temperatura não permanecer abaixo de **55°C** pelo período configurado.
- Ao detectar temperatura **inferior a 55°C**, inicia uma contagem de **30 segundos**.
- Se a temperatura continuar abaixo de 55°C durante os 30 segundos, **desliga a ventoinha**.
- Se a temperatura voltar a 55°C ou mais durante a contagem, o temporizador é cancelado.
- Entre 55°C e 65°C, mantém o estado anterior. Essa diferença de 10°C é a **histerese**, que ajuda a evitar ciclos frequentes de liga/desliga.

O daemon consulta o sensor a cada segundo. Os limites são parâmetros de controle preventivo, não limites máximos de segurança do Raspberry Pi.

### Parâmetros

No arquivo `tempd.c`:

```c
#define TEMP_ON   65.0
#define TEMP_OFF  55.0
#define OFF_DELAY 30

#define GPIO_PIN 12
#define LOG_FILE "/var/log/tempd.csv"
#define PID_FILE "/run/tempd.pid"
```

| Parâmetro | Valor | Descrição |
| --- | --- | --- |
| `TEMP_ON` | 65°C | Liga a ventoinha automaticamente |
| `TEMP_OFF` | 55°C | Temperatura abaixo da qual começa a contagem para desligar |
| `OFF_DELAY` | 30 s | Tempo contínuo abaixo de 55°C necessário para desligar |
| `GPIO_PIN` | 12 | GPIO BCM usado para acionamento |
| `LOG_FILE` | `/var/log/tempd.csv` | Arquivo com os eventos de mudança de estado |
| `PID_FILE` | `/run/tempd.pid` | Identificação do processo para os comandos manuais |

**Observação:** o código atual não implementa um tempo mínimo obrigatório de funcionamento da ventoinha. Um `OFF_DELAY` menor reduz o tempo ligado após o resfriamento, mas pode aumentar a frequência de acionamentos.

## Hardware

### Raspberry Pi

- Raspberry Pi 4.
- Ubuntu Server ARM64.

### Ventoinha

- Ventoinha de 5 V, com dois fios (positivo e negativo).
- Controle **liga/desliga**, sem controle de velocidade (PWM).

### Acionamento

A ventoinha é acionada por um transistor **S8050** conectado ao **GPIO12 (BCM)**.

```text
GPIO12 ── resistor ── base do S8050
Coletor ───────────── negativo da ventoinha
Emissor ───────────── GND
Positivo da ventoinha ── 5 V
```

Use um resistor apropriado na base e um diodo de proteção (*flyback*) em paralelo com a ventoinha, com a polaridade correta. Confirme a corrente da ventoinha e os limites do transistor antes de conectar.

## Dependências

Instale as ferramentas de compilação:

```bash
sudo apt update
sudo apt install -y build-essential
```

Verifique a disponibilidade do `pinctrl`:

```bash
command -v pinctrl
```

O daemon utiliza o utilitário `pinctrl` para alterar o GPIO; ele deve estar instalado e acessível ao serviço.

Para testar o GPIO **com o serviço parado**:

```bash
sudo systemctl stop tempd
sudo pinctrl set 12 op
sudo pinctrl set 12 dh  # Liga
sudo pinctrl set 12 dl  # Desliga
```

Quando o serviço estiver em execução, prefira os comandos `tempd ON`, `OFF` e `AUTO`, para evitar que o daemon e um comando externo disputem o estado do GPIO.

## Compilação

A partir da pasta do projeto:

```bash
gcc -Wall -Wextra -O2 tempd.c -o tempd
```

O código utiliza tanto `<string.h>` (para `memset`) quanto `<strings.h>` (para `strcasecmp`). Verifique se ambos estão incluídos no arquivo-fonte.

## Execução direta

Se não estiver executando o serviço systemd:

```bash
sudo ./tempd
```

O programa permanece em primeiro plano. Para interromper, pressione `Ctrl+C`. **Não execute uma segunda instância se o serviço já estiver ativo.**

## Instalação como serviço systemd

Instale o executável:

```bash
sudo install -m 755 tempd /usr/local/bin/tempd
```

Crie o serviço:

```bash
sudo vim /etc/systemd/system/tempd.service
```

Conteúdo:

```ini
[Unit]
Description=Raspberry Pi Temperature Fan Controller
After=multi-user.target

[Service]
Type=simple
ExecStart=/usr/local/bin/tempd
Restart=on-failure
RestartSec=5
User=root
Group=root

[Install]
WantedBy=multi-user.target
```

Habilite e inicie:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now tempd
sudo systemctl status tempd
```

O status esperado é `active (running)`.

## Controle manual da FAN

Com o daemon **em execução**, utilize:

| Comando | Efeito |
| --- | --- |
| `sudo tempd ON` | Ativa o modo manual e mantém a ventoinha ligada |
| `sudo tempd OFF` | Ativa o modo manual e mantém a ventoinha desligada |
| `sudo tempd AUTO` | Retorna ao controle automático por temperatura |

Exemplo:

```bash
sudo tempd ON
sudo tempd OFF
sudo tempd AUTO
```

Os comandos enviam sinais ao daemon usando o PID armazenado em `/run/tempd.pid`. Eles **não** criam uma segunda instância e **não** controlam diretamente o GPIO. É necessário que o serviço esteja ativo e que o usuário tenha permissão para enviar sinais ao processo.

- `ON` envia `SIGUSR1`.
- `OFF` envia `SIGUSR2`.
- `AUTO` envia `SIGHUP`.

O modo manual permanece ativo até outro comando ou até reiniciar o serviço; ao iniciar, o daemon entra no modo `AUTO`.

> **Atenção:** `sudo tempd OFF` mantém a ventoinha desligada mesmo se a temperatura subir acima de 65°C. Utilize apenas para testes supervisionados e execute `sudo tempd AUTO` ao terminar.

## Registro de eventos em CSV

O arquivo de eventos fica em:

```text
/var/log/tempd.csv
```

Cada linha contém **data e hora, temperatura da CPU e novo estado da FAN**:

```csv
2026-10-19 21:30:12,65.1,ON
2026-10-19 21:35:48,54.7,OFF
```

O arquivo é criado automaticamente **quando ocorre a primeira mudança de estado** registrada pelo daemon. Não há gravação de temperatura a cada segundo; portanto, o CSV é um histórico de **acionamentos**, não um histórico contínuo de temperatura.

Os registros são acrescentados ao final do arquivo, inclusive após reinicializações.

### Consultar os eventos

```bash
sudo tail -n 20 /var/log/tempd.csv
```

Acompanhar em tempo real:

```bash
sudo tail -f /var/log/tempd.csv
```

Contar os acionamentos `ON` registrados no arquivo inteiro:

```bash
awk -F, '$3 == "ON" {n++} END {print n+0}' /var/log/tempd.csv
```

**Nota:** o CSV não contém uma coluna que diferencie acionamentos manuais dos automáticos. O arquivo pode crescer com o tempo; considere configurar `logrotate` se mantiver o serviço por longos períodos.

## Teste de temperatura e acionamento automático

Instale o `stress-ng`:

```bash
sudo apt install -y stress-ng
```

Garanta que o controle esteja no modo automático:

```bash
sudo tempd AUTO
```

Execute uma carga de até 10 minutos nos quatro núcleos:

```bash
stress-ng --cpu 4 --cpu-load 100 --timeout 10m --metrics-brief
```

Em outro terminal, monitore a temperatura em graus Celsius:

```bash
watch -n 1 'awk "{printf \"CPU: %.1f°C\\n\", \$1/1000}" /sys/class/thermal/thermal_zone0/temp'
```

Em outro terminal, acompanhe o serviço:

```bash
sudo journalctl -u tempd -f
```

E os eventos:

```bash
sudo tail -f /var/log/tempd.csv
```

**Resultado esperado:** ao atingir 65°C, a FAN liga. Após interromper a carga (`Ctrl+C`) e a temperatura permanecer abaixo de 55°C por 30 segundos, a FAN desliga. Interrompa o teste caso a temperatura se aproxime de 80°C e a refrigeração não consiga estabilizá-la.

## Reiniciar após alterações

Depois de editar `tempd.c`, compile **antes** de parar o serviço, para não deixá-lo indisponível caso a compilação falhe:

```bash
gcc -Wall -Wextra -O2 tempd.c -o tempd
```

Se compilar sem erros, instale e reinicie:

```bash
sudo systemctl stop tempd
sudo install -m 755 tempd /usr/local/bin/tempd
sudo systemctl start tempd
sudo systemctl status tempd
```

Se modificar o arquivo `.service`, recarregue também as unidades:

```bash
sudo systemctl daemon-reload
sudo systemctl restart tempd
```

## Comandos úteis

| Operação | Comando |
| --- | --- |
| Status do serviço | `sudo systemctl status tempd` |
| Iniciar | `sudo systemctl start tempd` |
| Parar | `sudo systemctl stop tempd` |
| Reiniciar | `sudo systemctl restart tempd` |
| Habilitar no boot | `sudo systemctl enable tempd` |
| Desabilitar no boot | `sudo systemctl disable tempd` |
| Parar e desabilitar | `sudo systemctl disable --now tempd` |
| Verificar se está ativo | `systemctl is-active tempd` |
| Verificar se está habilitado | `systemctl is-enabled tempd` |
| Acompanhar journal | `sudo journalctl -u tempd -f` |
| Journal das últimas 24 horas | `sudo journalctl -u tempd --since "24 hours ago"` |
| Ligar manualmente | `sudo tempd ON` |
| Desligar manualmente | `sudo tempd OFF` |
| Retomar controle automático | `sudo tempd AUTO` |
| Últimos eventos CSV | `sudo tail -n 20 /var/log/tempd.csv` |

### Temperatura atual

O sensor retorna milésimos de grau Celsius:

```bash
cat /sys/class/thermal/thermal_zone0/temp
```

Por exemplo, `52341` significa **52,341°C**.

Para acompanhar em °C:

```bash
watch -n 1 'awk "{printf \"CPU: %.1f°C\\n\", \$1/1000}" /sys/class/thermal/thermal_zone0/temp'
```

## Atualizando pelo Git

No diretório do repositório:

```bash
git pull
gcc -Wall -Wextra -O2 tempd.c -o tempd
sudo systemctl stop tempd
sudo install -m 755 tempd /usr/local/bin/tempd
sudo systemctl start tempd
sudo systemctl status tempd
```

**Importante:** execute os comandos de instalação apenas se o `gcc` concluir sem erros. A compilação e o reinício são etapas distintas.

## Remover serviço

```bash
sudo systemctl disable --now tempd
sudo rm /etc/systemd/system/tempd.service
sudo systemctl daemon-reload
sudo rm /usr/local/bin/tempd
```

O CSV em `/var/log/tempd.csv` não é apagado por esses comandos. Remova-o separadamente apenas se não precisar mais do histórico.

## Limitações e cuidados

- O daemon usa `system("pinctrl ...")` para acionar o GPIO; a execução depende do utilitário `pinctrl`.
- O arquivo PID, por si só, **não impede múltiplas instâncias**. Execute uma única instância gerenciada pelo systemd.
- O comando manual confirma o **envio do sinal**, não necessariamente a conclusão da mudança física do GPIO; consulte o journal ou o CSV para confirmar o evento.
- No encerramento normal por `SIGTERM` ou `SIGINT`, o daemon tenta desligar a ventoinha. Um encerramento forçado (`SIGKILL`) ou uma falha abrupta pode impedir essa rotina.
- O controle manual `OFF` não deve ser usado durante testes de estresse sem supervisão.
- O arquivo CSV contém apenas eventos `ON`/`OFF`, não medições periódicas de temperatura.

## Licença

MIT License.
