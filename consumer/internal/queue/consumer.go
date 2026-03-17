package queue

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"sync"

	"github.com/davi/queue-load-test/consumer/internal/model"
	"github.com/davi/queue-load-test/consumer/internal/repository"
	amqp "github.com/rabbitmq/amqp091-go"
)

const (
	ExchangeName = "telemetry"
	QueueName    = "telemetry_readings"
	RoutingKey   = "telemetry.reading"
	DLXName      = "telemetry.dlx"
	DLQName      = "telemetry_readings_dlq"
)

type Consumer struct {
	conn    *amqp.Connection
	channel *amqp.Channel
	repo    *repository.TelemetryRepository
	workers int
}

func NewConsumer(amqpURL string, repo *repository.TelemetryRepository, workers int) (*Consumer, error) {
	conn, err := amqp.Dial(amqpURL)
	if err != nil {
		return nil, fmt.Errorf("failed to connect to RabbitMQ: %w", err)
	}

	ch, err := conn.Channel()
	if err != nil {
		conn.Close()
		return nil, fmt.Errorf("failed to open channel: %w", err)
	}

	if err := declareTopology(ch); err != nil {
		ch.Close()
		conn.Close()
		return nil, fmt.Errorf("failed to declare topology: %w", err)
	}

	if err := ch.Qos(workers, 0, false); err != nil {
		ch.Close()
		conn.Close()
		return nil, fmt.Errorf("failed to set QoS: %w", err)
	}

	return &Consumer{conn: conn, channel: ch, repo: repo, workers: workers}, nil
}

func declareTopology(ch *amqp.Channel) error {
	if err := ch.ExchangeDeclare(ExchangeName, "direct", true, false, false, false, nil); err != nil {
		return err
	}
	if err := ch.ExchangeDeclare(DLXName, "fanout", true, false, false, false, nil); err != nil {
		return err
	}
	args := amqp.Table{"x-dead-letter-exchange": DLXName}
	if _, err := ch.QueueDeclare(QueueName, true, false, false, false, args); err != nil {
		return err
	}
	if err := ch.QueueBind(QueueName, RoutingKey, ExchangeName, false, nil); err != nil {
		return err
	}
	if _, err := ch.QueueDeclare(DLQName, true, false, false, false, nil); err != nil {
		return err
	}
	if err := ch.QueueBind(DLQName, "", DLXName, false, nil); err != nil {
		return err
	}
	return nil
}

func (c *Consumer) Start(ctx context.Context) error {
	deliveries, err := c.channel.Consume(QueueName, "", false, false, false, false, nil)
	if err != nil {
		return fmt.Errorf("failed to start consuming: %w", err)
	}

	var wg sync.WaitGroup
	for i := 0; i < c.workers; i++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()
			for {
				select {
				case <-ctx.Done():
					return
				case msg, ok := <-deliveries:
					if !ok {
						return
					}
					c.processMessage(ctx, msg, workerID)
				}
			}
		}(i)
	}

	wg.Wait()
	return nil
}

func (c *Consumer) processMessage(ctx context.Context, msg amqp.Delivery, workerID int) {
	var reading model.TelemetryReading
	if err := json.Unmarshal(msg.Body, &reading); err != nil {
		log.Printf("[worker %d] failed to unmarshal message: %v", workerID, err)
		msg.Nack(false, false)
		return
	}

	if err := c.repo.Insert(ctx, reading); err != nil {
		log.Printf("[worker %d] failed to insert reading: %v", workerID, err)
		msg.Nack(false, false)
		return
	}

	msg.Ack(false)
}

func (c *Consumer) Close() {
	if c.channel != nil {
		c.channel.Close()
	}
	if c.conn != nil {
		c.conn.Close()
	}
}
