package main

import (
	"context"
	"log"
	"os"
	"os/signal"
	"strconv"
	"syscall"

	"github.com/golang-migrate/migrate/v4"
	_ "github.com/golang-migrate/migrate/v4/database/postgres"
	_ "github.com/golang-migrate/migrate/v4/source/file"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/davi/queue-load-test/consumer/internal/queue"
	"github.com/davi/queue-load-test/consumer/internal/repository"
)

func main() {
	amqpURL := getEnv("RABBITMQ_URL", "amqp://guest:guest@localhost:5672/")
	dbURL := getEnv("DATABASE_URL", "postgres://user:password@localhost:5432/telemetry?sslmode=disable")
	workers := getEnvInt("CONSUMER_WORKERS", 5)

	// Run migrations
	log.Println("Running migrations...")
	m, err := migrate.New("file:///app/migrations", dbURL)
	if err != nil {
		log.Fatalf("Failed to create migrator: %v", err)
	}
	if err := m.Up(); err != nil && err != migrate.ErrNoChange {
		log.Fatalf("Migration failed: %v", err)
	}
	log.Println("Migrations complete")

	// Connect to PostgreSQL
	pool, err := pgxpool.New(context.Background(), dbURL)
	if err != nil {
		log.Fatalf("Failed to connect to database: %v", err)
	}
	defer pool.Close()
	log.Println("Connected to PostgreSQL")

	// Start consumer
	repo := repository.NewTelemetryRepository(pool)
	consumer, err := queue.NewConsumer(amqpURL, repo, workers)
	if err != nil {
		log.Fatalf("Failed to create consumer: %v", err)
	}
	defer consumer.Close()
	log.Printf("Consumer started with %d workers", workers)

	ctx, cancel := context.WithCancel(context.Background())

	go func() {
		quit := make(chan os.Signal, 1)
		signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
		<-quit
		log.Println("Shutting down...")
		cancel()
	}()

	if err := consumer.Start(ctx); err != nil {
		log.Fatalf("Consumer error: %v", err)
	}
	log.Println("Consumer stopped")
}

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func getEnvInt(key string, fallback int) int {
	if v := os.Getenv(key); v != "" {
		n, err := strconv.Atoi(v)
		if err != nil {
			return fallback
		}
		return n
	}
	return fallback
}
