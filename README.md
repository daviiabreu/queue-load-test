# queue-load-test

Sistema de ingestão de telemetria industrial com processamento assíncrono via fila de mensagens. Desenvolvido em Go com RabbitMQ, PostgreSQL e testes de carga com k6.

---

## O que foi construído

Um pipeline de telemetria com dois serviços independentes:

- **Producer**: recebe leituras de sensores via HTTP, valida o payload e publica na fila. Retorna `202 Accepted` imediatamente — sem escrever no banco.
- **Consumer**: consome mensagens da fila em paralelo (worker pool) e persiste no PostgreSQL **uma a uma** — cada mensagem resulta em um `INSERT` individual, sem batching.

Essa separação é a decisão arquitetural central: o caminho crítico do producer é mínimo (parse → validar → enfileirar → responder), o que o torna extremamente difícil de sobrecarregar. O consumer, por outro lado, é o gargalo natural do sistema — a taxa de inserção no banco é limitada pelo número de workers e pela latência de cada `INSERT` individual.

---

## Arquitetura

```
Dispositivos / k6
      │
      │ POST /api/telemetry
      ▼
┌─────────────────────┐
│  Producer (:8080)   │  → valida payload → publica no RabbitMQ → 202 Accepted
└─────────────────────┘
           │
    ┌──────┴──────────────────────────┐
    │                                 │
[telemetry_readings]        [telemetry_readings_dlq]
   (fila principal)            (dead-letter queue)
    │                                 │
    │ ACK em sucesso          NACK → mensagens com falha
    ▼
┌─────────────────────┐
│  Consumer           │  → desserializa → persiste no PostgreSQL
│  (worker pool)      │
└─────────────────────┘
           │
           ▼
    ┌─────────────┐
    │ PostgreSQL  │
    │  telemetry  │
    └─────────────┘
```

**Topologia RabbitMQ:**

- Exchange direto `telemetry` → fila `telemetry_readings`
- Exchange fanout `telemetry.dlx` → fila `telemetry_readings_dlq` (dead-letter)
- Migrações de schema executadas automaticamente pelo consumer na inicialização (`golang-migrate`)

### Decisões técnicas

**Backend (Go):**

- **ACK/NACK explícito**: cada mensagem é confirmada individualmente. Se o `INSERT` falha, o consumer envia `NACK` sem requeue — a mensagem vai para a DLQ, garantindo que nenhuma leitura se perde silenciosamente
- **Prefetch = workers**: o consumer configura `Qos(prefetch_count=workers)` para que cada goroutine tenha no máximo 1 mensagem em processamento, evitando acúmulo em memória
- **Connection pool (pgxpool)**: o acesso ao PostgreSQL usa pool de conexões via `pgx`, não uma conexão única — suporta paralelismo real entre os workers
- **Topologia declarada pelo consumer**: exchanges, filas e bindings são declarados automaticamente na inicialização (`declareTopology`), garantindo que a infraestrutura existe antes de começar a consumir

**Firmware (Pico W):**

- **WiFi FSM com reconexão**: máquina de estados explícita (`DISCONNECTED → CONNECTING → CONNECTED`) com cooldown de 10 s entre tentativas. Se a conexão cai, o firmware reconecta automaticamente sem reiniciar
- **Autenticação dinâmica**: se `WIFI_PASSWORD` é vazio, usa rede aberta (`CYW43_AUTH_OPEN`); se não, usa WPA2-PSK. Permite trocar entre Wokwi e hardware real mudando apenas a config
- **Cooperative multitasking**: o loop principal chama `wifi::poll()` → `cyw43_arch_poll()` a cada iteração (~10 ms) para manter a stack lwIP/TCP ativa. Sem isso, pacotes TCP não são processados e conexões morrem por timeout
- **Sensor ultrassônico não-bloqueante**: o busy-wait do HC-SR04 inclui chamadas a `cyw43_arch_poll()` para que o WiFi continue funcional mesmo durante medições longas

---

## Estrutura do projeto

```
queue-load-test/
├── producer/
│   ├── cmd/main.go                     # entrypoint, servidor HTTP, shutdown gracioso
│   └── internal/
│       ├── handler/telemetry.go        # handlers HTTP
│       ├── model/telemetry.go          # struct TelemetryRequest + Validate()
│       └── queue/publisher.go          # publisher RabbitMQ
│
├── consumer/
│   ├── cmd/main.go                     # entrypoint, migrações, worker pool
│   └── internal/
│       ├── model/telemetry.go          # struct TelemetryReading
│       ├── queue/consumer.go           # consumer RabbitMQ com sync.WaitGroup
│       └── repository/telemetry.go     # persistência PostgreSQL
│
├── migrations/
│   ├── 001_create_telemetry_readings.up.sql
│   └── 001_create_telemetry_readings.down.sql
│
├── loadtest/scripts/
│   └── telemetry.js                    # teste de carga com 4 cenários
│
└── docker-compose.yml                  # 4 serviços com limites de recurso
```

---

## Como rodar

```bash
# Subir todos os serviços
docker compose up --build

# Verificar status
docker compose ps

# Checar saúde do producer
curl http://localhost:8080/health
```

Serviços disponíveis após a inicialização:

| Serviço      | Endereço                             |
| ------------ | ------------------------------------ |
| Producer API | http://localhost:8080                |
| RabbitMQ UI  | http://localhost:15672 (guest/guest) |
| PostgreSQL   | localhost:5432 (user/password)       |

**Enviar uma leitura manualmente:**

```bash
curl -X POST http://localhost:8080/api/telemetry \
  -H "Content-Type: application/json" \
  -d '{
    "device_id": "sensor-hub-001",
    "timestamp": "2026-03-17T12:00:00Z",
    "sensor_type": "temperature",
    "reading_type": "analog",
    "value": 23.5
  }'
# → 202 Accepted: {"status":"accepted"}
```

**Derrubar tudo (incluindo volume do banco):**

```bash
docker compose down -v
```

---

## API

### `POST /api/telemetry`

| Campo          | Tipo             | Obrigatório | Descrição                                   |
| -------------- | ---------------- | ----------- | ------------------------------------------- |
| `device_id`    | string           | sim         | Identificador do dispositivo                |
| `timestamp`    | string (RFC3339) | sim         | Timestamp da leitura                        |
| `sensor_type`  | string           | sim         | Tipo do sensor (temperatura, umidade, etc.) |
| `reading_type` | string           | sim         | `analog` ou `discrete`                      |
| `value`        | number           | sim         | Valor da leitura                            |

Respostas: `202 Accepted` · `400 Bad Request` · `500 Internal Server Error`

### `GET /health`

Retorna `200 OK` com `{"status":"ok"}` se o producer está conectado ao RabbitMQ.

---

## Variáveis de ambiente

### Producer

| Variável       | Padrão                              | Descrição        |
| -------------- | ----------------------------------- | ---------------- |
| `RABBITMQ_URL` | `amqp://guest:guest@rabbitmq:5672/` | Conexão RabbitMQ |
| `HTTP_PORT`    | `8080`                              | Porta HTTP       |

### Consumer

| Variável           | Padrão                                                             | Descrição                                   |
| ------------------ | ------------------------------------------------------------------ | ------------------------------------------- |
| `RABBITMQ_URL`     | `amqp://guest:guest@rabbitmq:5672/`                                | Conexão RabbitMQ                            |
| `DATABASE_URL`     | `postgres://user:password@postgres:5432/telemetry?sslmode=disable` | Conexão PostgreSQL                          |
| `CONSUMER_WORKERS` | `5`                                                                | Número de goroutines consumindo em paralelo |

---

## Limites de recurso (Docker Compose)

Definidos para garantir reprodutibilidade nos testes de carga:

| Serviço    | CPU  | Memória |
| ---------- | ---- | ------- |
| RabbitMQ   | 0.5  | 256 MB  |
| PostgreSQL | 0.5  | 256 MB  |
| Producer   | 0.25 | 128 MB  |
| Consumer   | 0.25 | 128 MB  |

---

## Testes

```bash
# Producer
cd producer && go test ./...

# Consumer
cd consumer && go test ./...
```

---

## Testes de carga

### Teste padrão — 4 cenários (`telemetry.js`)

```bash
k6 run loadtest/scripts/telemetry.js
```

| Cenário         | Executor              | Carga                 | Início |
| --------------- | --------------------- | --------------------- | ------ |
| `constant_load` | constant-vus          | 50 VUs por 1 min      | 0s     |
| `stress_ramp`   | ramping-vus           | 50 → 500 VUs          | 1 min  |
| `spike`         | ramping-vus           | 0 → 1.000 VUs (burst) | 4 min  |
| `flood`         | constant-arrival-rate | 2.000 req/s por 1 min | 5 min  |

**Thresholds:** `p(95) < 500ms` · `p(99) < 1000ms` · `error_rate < 1%`

**Resultado obtido:** ~371.000 requisições · 0 erros · p95 = 684µs

---

## Firmware — Pico W Telemetry

### Toolchain

| Item            | Detalhe                                                                                     |
| --------------- | ------------------------------------------------------------------------------------------- |
| Framework       | **Nativo Pico SDK** (C/C++, sem Arduino)                                                    |
| Compilador      | `arm-none-eabi-gcc` via CMake                                                               |
| Linguagem       | C++17                                                                                       |
| Board           | `pico_w`                                                                                    |
| Bibliotecas SDK | `pico_stdlib`, `pico_cyw43_arch_lwip_poll`, `hardware_adc`, `hardware_gpio`, `hardware_irq` |
| Simulação       | Wokwi (VS Code extension)                                                                   |

---

### Sensores integrados

| Sensor                 | Tipo            | Pino                     | Leitura                                        | Range esperado    |
| ---------------------- | --------------- | ------------------------ | ---------------------------------------------- | ----------------- |
| Potenciômetro          | Analógico (ADC) | GP26 (ADC0)              | Tensão proporcional ao giro                    | 0.00 – 3.30 V     |
| LDR (fotoresistor)     | Analógico (ADC) | GP27 (ADC1)              | Tensão proporcional à luminosidade             | 0.00 – 3.30 V     |
| Botão (push button)    | Digital (GPIO)  | GP16                     | Presença — nível lógico com pull-up interno    | 0 ou 1 (discreto) |
| Sensor PIR             | Digital (GPIO)  | GP17                     | Movimento — nível lógico com pull-down interno | 0 ou 1 (discreto) |
| HC-SR04 (ultrassônico) | Digital (GPIO)  | GP18 (TRIG), GP19 (ECHO) | Distância por tempo de eco                     | 2 – 400 cm        |

**Mapeamento sensor → telemetria:**

| Sensor        | `sensor_type` | `reading_type` |
| ------------- | ------------- | -------------- |
| Potenciômetro | `temperature` | `analog`       |
| LDR           | `luminosity`  | `analog`       |
| Botão         | `presence`    | `discrete`     |
| PIR           | `motion`      | `discrete`     |
| HC-SR04       | `distance`    | `analog`       |

**ADC:** resolução de 12 bits (0–4095), conversão para tensão via `raw × 3.3 / 4095`.

**Debouncing:** implementado no IRQ handler via timestamp com intervalo mínimo de 50 ms entre eventos.

**Hardware Timer:** leitura periódica dos sensores analógicos e ultrassônico via `add_repeating_timer_ms()` (hardware alarm do RP2040) a cada 1 segundo — sem bloquear o loop principal.

**DNS Cache:** o IP do backend é resolvido uma única vez na primeira requisição e reutilizado em todas as seguintes — evita overhead de DNS a cada envio.

---

### Diagrama de conexão

```mermaid
graph TD
    subgraph Sensores Digitais
        BTN["🔘 Botão<br/>(push button)"]
        PIR["👁 Sensor PIR<br/>(movimento)"]
    end

    subgraph Sensores Analógicos
        POT["🎛 Potenciômetro<br/>(temperatura)"]
        LDR["💡 LDR<br/>(luminosidade)"]
    end

    subgraph Sensor de Distância
        HCSR["📏 HC-SR04<br/>(ultrassônico)"]
    end

    subgraph PICO ["Raspberry Pi Pico W"]
        GP16["GP16<br/>pull-up"]
        GP17["GP17<br/>pull-down"]
        GP26["GP26<br/>ADC0"]
        GP27["GP27<br/>ADC1"]
        GP18["GP18<br/>TRIG"]
        GP19["GP19<br/>ECHO"]
        WIFI["CYW43 WiFi<br/>lwIP + TCP"]
    end

    subgraph Backend
        PROD["Producer :8080<br/>POST /api/telemetry"]
    end

    BTN -- "digital (IRQ)" --> GP16
    PIR -- "digital (IRQ)" --> GP17
    POT -- "analog 0–3.3V" --> GP26
    LDR -- "analog 0–3.3V" --> GP27
    HCSR -- "trigger" --> GP18
    GP19 -- "echo" --> HCSR
    WIFI -- "HTTP POST (JSON)" --> PROD
```

| Sensor        | Pino(s)          | Alimentação | Fio no Wokwi |
| ------------- | ---------------- | ----------- | ------------ |
| Botão         | GP16 + GND       | —           | verde        |
| PIR           | GP17 + 3V3 + GND | 3.3 V       | azul         |
| Potenciômetro | GP26 + 3V3 + GND | 3.3 V       | laranja      |
| LDR           | GP27 + 3V3 + GND | 3.3 V       | amarelo      |
| HC-SR04 TRIG  | GP18 + 3V3 + GND | 3.3 V       | roxo         |
| HC-SR04 ECHO  | GP19             | —           | ciano        |

---

### Estrutura do firmware

```
firmware/
├── CMakeLists.txt              # build config, libs linkadas
├── pico_sdk_import.cmake       # bootstrap do Pico SDK
├── diagram.json                # esquemático Wokwi
├── wokwi.toml                  # configuração do simulador
└── src/
    ├── core/
    │   ├── config.hpp          # constantes (pinos, rede, timers)
    │   └── main.cpp            # loop principal + hardware timer
    ├── sensors/
    │   ├── adc_sensor.cpp/hpp  # leitura ADC (pot + LDR)
    │   ├── gpio_sensor.cpp/hpp # IRQ handlers + debouncing
    │   └── ultrasonic_sensor.cpp/hpp  # HC-SR04
    └── network/
        ├── wifi_manager.cpp/hpp    # FSM WiFi (DISCONNECTED → CONNECTING → CONNECTED)
        └── http_client.cpp/hpp     # cliente HTTP customizado sobre TCP raw (lwIP)
```

---

### Compilação e gravação

#### Pré-requisitos

```bash
# macOS
brew install cmake arm-none-eabi-gcc

# Ubuntu/Debian
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi
```

#### Build

```bash
cd firmware
mkdir build && cd build
cmake ..
make -j4
```

Gera `pico_telemetry.uf2` em `firmware/build/`.

#### Gravação no Pico W (hardware real)

1. Segure o botão **BOOTSEL** e conecte o Pico W via USB
2. O Pico aparece como unidade de armazenamento
3. Copie `pico_telemetry.uf2` para a unidade
4. O Pico reinicia e executa o firmware automaticamente

#### Simulação no Wokwi

1. Instale a extensão [Wokwi for VS Code](https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode)
2. Com o backend rodando (`docker compose up --build`), inicie a simulação
3. O firmware conecta via `host.wokwi.internal` ao producer local

---

### Configuração de rede

Edite `firmware/src/core/config.hpp`:

```cpp
// WiFi
constexpr const char *WIFI_SSID     = "Wokwi-GUEST";   // nome da rede
constexpr const char *WIFI_PASSWORD  = "";               // senha (vazio = rede aberta)

// Backend (producer)
constexpr const char *BACKEND_HOST  = "host.wokwi.internal";  // ou IP do servidor
constexpr uint16_t    BACKEND_PORT  = 8080;
constexpr const char *BACKEND_PATH  = "/api/telemetry";
```

A autenticação WiFi é dinâmica: se `WIFI_PASSWORD` não for vazio, usa WPA2-PSK automaticamente. Para o Wokwi, use `"Wokwi-GUEST"` com senha vazia.

---

### Evidências de funcionamento

#### Log serial — leitura de sensores

Saída capturada via `wokwi-cli` com o backend rodando em `docker compose`:

```
=== Pico W Telemetry ===
Device: pico-w-001
Backend: host.wokwi.internal:8080/api/telemetry
[GPIO] Botao GP16 (valor: 1)
[GPIO] PIR GP17 (valor: 0)
[ULTRASONIC] Inicializado TRIG=GP18 ECHO=GP19
[WIFI] Conectando a "Wokwi-GUEST"...
[WIFI] Conectado!
[MAIN] Pronto!

[MAIN] POT: raw=0, 0.00 V
[MAIN] Enviando: {"device_id":"pico-w-001","timestamp":"2026-03-23T00:00:10Z","sensor_type":"temperature","reading_type":"analog","value":0.00}
[HTTP] DNS resolvido: host.wokwi.internal
[HTTP] 202 Accepted
[MAIN] LDR: raw=1000, 0.81 V
[MAIN] Enviando: {"device_id":"pico-w-001","timestamp":"2026-03-23T00:00:15Z","sensor_type":"luminosity","reading_type":"analog","value":0.81}
[HTTP] 202 Accepted
[MAIN] HC-SR04: 405.6 cm
[MAIN] Enviando: {"device_id":"pico-w-001","timestamp":"2026-03-23T00:00:18Z","sensor_type":"distance","reading_type":"analog","value":405.62}
```

Note que `[HTTP] DNS resolvido` aparece apenas uma vez — o IP é cacheado após a primeira resolução.

---

#### Requisições HTTP chegando no backend

```bash
$ docker compose logs producer consumer
```

```
producer-1  | 2026/03/25 01:17:19 Connected to RabbitMQ
producer-1  | 2026/03/25 01:17:19 Producer listening on :8080
consumer-1  | 2026/03/25 01:17:19 Running migrations...
consumer-1  | 2026/03/25 01:17:19 Migrations complete
consumer-1  | 2026/03/25 01:17:19 Connected to PostgreSQL
consumer-1  | 2026/03/25 01:17:19 Consumer started with 5 workers
```

---

#### Dados persistidos no PostgreSQL

```bash
$ docker compose exec postgres psql -U user -d telemetry -c \
    "SELECT device_id, sensor_type, reading_type, value, timestamp
     FROM telemetry_readings ORDER BY id DESC LIMIT 15;"
```

```
 device_id  | sensor_type | reading_type | value  |       timestamp
------------+-------------+--------------+--------+------------------------
 pico-w-001 | distance    | analog       |   15.3 | 2026-03-24 10:00:07+00
 pico-w-001 | motion      | discrete     |      0 | 2026-03-24 10:00:06+00
 pico-w-001 | presence    | discrete     |      1 | 2026-03-24 10:00:05+00
 pico-w-001 | luminosity  | analog       |   1.82 | 2026-03-24 10:00:04+00
 pico-w-001 | temperature | analog       |   23.5 | 2026-03-24 10:00:03+00
 pico-w-001 | temperature | analog       |   21.5 | 2026-03-24 10:00:01+00
 pico-w-001 | temperature | analog       |   22.5 | 2026-03-24 10:00:02+00
 pico-w-001 | temperature | analog       |      0 | 2026-03-23 00:05:08+00
 pico-w-001 | distance    | analog       | 405.66 | 2026-03-23 00:05:03+00
 pico-w-001 | luminosity  | analog       |   0.81 | 2026-03-23 00:04:58+00
 pico-w-001 | temperature | analog       |      0 | 2026-03-23 00:04:58+00
 pico-w-001 | distance    | analog       | 405.66 | 2026-03-23 00:04:55+00
 pico-w-001 | luminosity  | analog       |   0.81 | 2026-03-23 00:04:52+00
 pico-w-001 | temperature | analog       |      0 | 2026-03-23 00:04:47+00
 pico-w-001 | distance    | analog       | 405.66 | 2026-03-23 00:04:44+00
(15 rows)
```

Os dados mostram leituras enviadas pelo firmware (timestamps `2026-03-23`) e por requisições manuais (timestamps `2026-03-24`), todos persistidos pelo consumer.

---

#### Screenshots

**RabbitMQ — Overview (teste de carga ~700 msg/s):**

![RabbitMQ Overview](docs/screenshots/rabbitmq-overview.png)

Dashboard geral do RabbitMQ durante teste de carga com 100k mensagens. Pico de **715 msg/s** no publish, **729 msg/s** no deliver e consumer ack. Gráfico de mensagens enfileiradas mostra o backlog transitório (~500 msgs) sendo consumido em tempo real.

**RabbitMQ — detalhe da fila `telemetry_readings`:**

![RabbitMQ Queue Detail](docs/screenshots/rabbitmq-queue-detail.png)

Fila principal com **694 msg/s** de publish sustentado, DLX configurado (`telemetry.dlx`), 1 consumer ativo com capacidade de 29%. Mostra 202 mensagens in-flight (197 ready + 5 unacked), todas em memória.

**RabbitMQ — filas ativas:**

![RabbitMQ Queues](docs/screenshots/rabbitmq-queues.png)

Duas filas visíveis: `telemetry_readings` (com DLX configurado) e `telemetry_readings_dlq`, ambas em estado `running`.

**Wokwi — simulação com sensores conectados:**

![Wokwi Simulation](docs/screenshots/wokwi-simulation.png)

Simulação rodando no Wokwi com todos os 5 sensores conectados ao Pico W: HC-SR04 (topo), botão, PIR, potenciômetro e LDR (base). WiFi ativo, firmware enviando telemetria via HTTP para o backend.
