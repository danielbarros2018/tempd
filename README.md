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

```text
GPIO12 -> Resistor -> Base do S8050

Coletor -> Negativo da ventoinha

Emissor -> GND

Positivo da ventoinha -> 5V
```

Recomenda-se utilizar um diodo flyback em paralelo com a ventoinha.

---

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

---

## Compilação

```bash
gcc temp.c -o tempd
```

---

## Execução

```bash
sudo ./tempd
```

---

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

---

## Instalação como serviço systemd

Criar o arquivo de serviço:

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
Restart=always
RestartSec=5

User=root
Group=root

[Install]
WantedBy=multi-user.target
```

Instalar o binário:

```bash
sudo cp tempd /usr/local/bin/
sudo chmod +x /usr/local/bin/tempd
```

Carregar a configuração do systemd:

```bash
sudo systemctl daemon-reload
```

Habilitar inicialização automática:

```bash
sudo systemctl enable tempd
```

Iniciar o serviço:

```bash
sudo systemctl start tempd
```

Verificar status:

```bash
sudo systemctl status tempd
```

---

## 8. Reiniciar após alterações

Sempre que o código-fonte for alterado:

### Recompilar

```bash
gcc temp.c -o tempd
```

### Instalar a nova versão

```bash
sudo cp tempd /usr/local/bin/
sudo chmod 755 /usr/local/bin/tempd
```

### Reiniciar o serviço

```bash
sudo systemctl restart tempd
```

### Verificar se voltou corretamente

```bash
sudo systemctl status tempd
```

O status deve indicar:

```text
Active: active (running)
```

---

## 9. Comandos úteis

### Verificar status do serviço

Mostra informações detalhadas do daemon.

```bash
sudo systemctl status tempd
```

### Iniciar o serviço

```bash
sudo systemctl start tempd
```

### Parar o serviço

Envia um SIGTERM ao daemon, permitindo que ele finalize corretamente e desligue a ventoinha antes de encerrar.

```bash
sudo systemctl stop tempd
```

### Reiniciar o serviço

```bash
sudo systemctl restart tempd
```

### Recarregar configuração do systemd

Necessário após alterações no arquivo:

```text
/etc/systemd/system/tempd.service
```

```bash
sudo systemctl daemon-reload
```

### Habilitar inicialização automática

```bash
sudo systemctl enable tempd
```

### Desabilitar inicialização automática

```bash
sudo systemctl disable tempd
```

### Verificar se o serviço está habilitado

```bash
systemctl is-enabled tempd
```

Resposta esperada:

```text
enabled
```

### Verificar se o serviço está em execução

```bash
systemctl is-active tempd
```

Resposta esperada:

```text
active
```

### Visualizar logs em tempo real

```bash
sudo journalctl -u tempd -f
```

### Visualizar logs das últimas 24 horas

```bash
sudo journalctl -u tempd --since "24 hours ago"
```

### Verificar temperatura atual da CPU

```bash
cat /sys/class/thermal/thermal_zone0/temp
```

Exemplo:

```text
52341
```

Equivale a:

```text
52.341°C
```

### Monitorar temperatura continuamente

```bash
watch -n 1 'cat /sys/class/thermal/thermal_zone0/temp'
```

### Testar acionamento manual da ventoinha

Configurar GPIO12 como saída:

```bash
sudo pinctrl set 12 op
```

Ligar ventoinha:

```bash
sudo pinctrl set 12 dh
```

Desligar ventoinha:

```bash
sudo pinctrl set 12 dl
```

### Gerar carga máxima na CPU

```bash
stress-ng --cpu 4 --timeout 10m
```

---

## Atualizando pelo Git

Obter alterações:

```bash
git pull
```

Recompilar:

```bash
gcc temp.c -o tempd
```

Instalar nova versão:

```bash
sudo cp tempd /usr/local/bin/
```

Reiniciar serviço:

```bash
sudo systemctl restart tempd
```

Verificar status:

```bash
sudo systemctl status tempd
```

---

## Remover serviço

```bash
sudo systemctl stop tempd
sudo systemctl disable tempd
sudo rm /etc/systemd/system/tempd.service
sudo systemctl daemon-reload
```

Remover binário:

```bash
sudo rm /usr/local/bin/tempd
```

---

## Configuração de temperatura

```c
#define TEMP_ON   55.0
#define TEMP_OFF  45.0
#define OFF_DELAY 120
```

### Significado

| Parâmetro | Valor | Descrição                              |
| --------- | ----- | -------------------------------------- |
| TEMP_ON   | 55°C  | Liga a ventoinha                       |
| TEMP_OFF  | 45°C  | Inicia a contagem para desligamento    |
| OFF_DELAY | 120 s | Tempo abaixo de 45°C antes de desligar |

Esses valores foram escolhidos para manter o Raspberry Pi 4 em uma faixa segura para operação contínua, evitando acionamentos excessivos da ventoinha.

---

## Licença

MIT License.
