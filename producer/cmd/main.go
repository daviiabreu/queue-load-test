package main

import (
	"context"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/davi/queue-load-test/producer/internal/handler"
	"github.com/davi/queue-load-test/producer/internal/queue"
)

func main() {
	amqpURL := getEnv("RABBITMQ_URL", "amqp://guest:guest@localhost:5672/")
	httpPort := getEnv("HTTP_PORT", "8080")

	publisher, err := queue.NewPublisher(amqpURL)
	if err != nil {
		log.Fatalf("Failed to create publisher: %v", err)
	}
	defer publisher.Close()
	log.Println("Connected to RabbitMQ")

	telemetryHandler := handler.NewTelemetryHandler(publisher)

	mux := http.NewServeMux()
	mux.HandleFunc("POST /api/telemetry", telemetryHandler.HandleTelemetry)
	mux.HandleFunc("GET /health", telemetryHandler.HandleHealth)

	server := &http.Server{
		Addr:         ":" + httpPort,
		Handler:      mux,
		ReadTimeout:  5 * time.Second,
		WriteTimeout: 10 * time.Second,
	}

	go func() {
		log.Printf("Producer listening on :%s", httpPort)
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("HTTP server error: %v", err)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit
	log.Println("Shutting down...")

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	server.Shutdown(ctx)
	log.Println("Producer stopped")
}

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
