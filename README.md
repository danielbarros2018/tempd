# tempd

Daemon simples para Raspberry Pi 4 que monitora a temperatura da CPU e controla uma ventoinha conectada ao GPIO12.

## Funcionamento

* Liga a ventoinha quando a temperatura da CPU atingir **55°C**.
* Mantém a ventoinha ligada enquanto a temperatura estiver acima de **45°C**.
* Quando a temperatura ficar abaixo de **45°C**, inicia uma contagem de **120 segundos**.
* Se permanecer abaixo de **45°C** durante todo esse período, desliga a ventoinha.

## Hardware

### Raspberry Pi

* Raspberry Pi 4
* Ubuntu ARM64

### Ventoinha

Ventoinha 5V de 2 fios.

### Acionamento

A ventoinha é acionada através de um transistor S8050 conectado ao GPIO12.

Exemplo simplificado:

GPIO12 -> Resistor -> Base do S8050

Coletor -> Negativo da ventoinha

Emissor -> GND

Positivo da ventoinha -> 5V

Recomenda-se utilizar um diodo flyback em paralelo com a ventoinha.

## Dependências

Instalar ferramentas de compilação:

```bash
sudo apt update
sudo apt install build-essential
```

Verificar se o utilitário `pinctrl` está disponível:

```bash
which pinctrl
```

Teste manual do GPIO:

```bash
sudo pinctrl set 12 op
sudo pinctrl set 12 dh
sudo pinctrl set 12 dl
```

## Compilação

```bash
gcc temp.c -o tempd
```

## Execução

```bash
sudo ./tempd
```

## Teste de temperatura

Instalar stress-ng:

```bash
sudo apt install stress-ng
```

Executar carga máxima:

```bash
stress-ng --cpu 4 --timeout 10m
```

Monitorar temperatura:

```bash
watch -n 1 'cat /sys/class/thermal/thermal_zone0/temp'
```

## Serviço systemd

Criar:

```bash
sudo vim /etc/systemd/system/tempd.service
```

Conteúdo:

```ini
[Unit]
Description=Temperature Fan Controller
After=multi-user.target

[Service]
Type=simple
ExecStart=/usr/local/bin/tempd
Restart=always
User=root

[Install]
WantedBy=multi-user.target
```

Instalar binário:

```bash
sudo cp tempd /usr/local/bin/
sudo chmod +x /usr/local/bin/tempd
```

Ativar:

```bash
sudo systemctl daemon-reload
sudo systemctl enable tempd
sudo systemctl start tempd
```

Verificar status:

```bash
sudo systemctl status tempd
```

Logs:

```bash
journalctl -u tempd -f
```

## Remover serviço

```bash
sudo systemctl stop tempd
sudo systemctl disable tempd
sudo rm /etc/systemd/system/tempd.service
sudo systemctl daemon-reload
```

## Temperaturas utilizadas

```c
#define TEMP_ON   55.0
#define TEMP_OFF  45.0
#define OFF_DELAY 120
```

Esses valores foram escolhidos para manter o Raspberry Pi 4 em uma faixa de temperatura adequada para operação contínua sem acionar a ventoinha desnecessariamente.

