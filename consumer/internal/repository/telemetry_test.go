package repository_test

import (
	"testing"

	"github.com/davi/queue-load-test/consumer/internal/repository"
)

func TestNewTelemetryRepository(t *testing.T) {
	repo := repository.NewTelemetryRepository(nil)
	if repo == nil {
		t.Fatal("expected non-nil repository")
	}
}
