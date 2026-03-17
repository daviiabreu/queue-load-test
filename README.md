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
# → 202 Accepted
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

Retorna `200 OK` com `{"status":"healthy"}` se o producer está conectado ao RabbitMQ.

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
