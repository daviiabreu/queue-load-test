package queue

import (
	"context"
	"encoding/json"
	"fmt"

	amqp "github.com/rabbitmq/amqp091-go"
)

const (
	ExchangeName = "telemetry"
	QueueName    = "telemetry_readings"
	RoutingKey   = "telemetry.reading"
	DLXName      = "telemetry.dlx"
	DLQName      = "telemetry_readings_dlq"
)

type Publisher struct {
	conn    *amqp.Connection
	channel *amqp.Channel
}

func NewPublisher(amqpURL string) (*Publisher, error) {
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

	return &Publisher{conn: conn, channel: ch}, nil
}

func declareTopology(ch *amqp.Channel) error {
	// Main exchange
	if err := ch.ExchangeDeclare(ExchangeName, "direct", true, false, false, false, nil); err != nil {
		return err
	}

	// Dead-letter exchange
	if err := ch.ExchangeDeclare(DLXName, "fanout", true, false, false, false, nil); err != nil {
		return err
	}

	// Main queue with DLX argument
	args := amqp.Table{"x-dead-letter-exchange": DLXName}
	if _, err := ch.QueueDeclare(QueueName, true, false, false, false, args); err != nil {
		return err
	}

	// Bind main queue
	if err := ch.QueueBind(QueueName, RoutingKey, ExchangeName, false, nil); err != nil {
		return err
	}

	// Dead-letter queue
	if _, err := ch.QueueDeclare(DLQName, true, false, false, false, nil); err != nil {
		return err
	}

	// Bind DLQ
	if err := ch.QueueBind(DLQName, "", DLXName, false, nil); err != nil {
		return err
	}

	return nil
}

func (p *Publisher) Publish(ctx context.Context, payload any) error {
	body, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal payload: %w", err)
	}

	return p.channel.PublishWithContext(ctx,
		ExchangeName,
		RoutingKey,
		false,
		false,
		amqp.Publishing{
			ContentType: "application/json",
			Body:        body,
		},
	)
}

func (p *Publisher) IsConnected() bool {
	return p.conn != nil && !p.conn.IsClosed()
}

func (p *Publisher) Close() {
	if p.channel != nil {
		p.channel.Close()
	}
	if p.conn != nil {
		p.conn.Close()
	}
}
