package handler_test

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/davi/queue-load-test/producer/internal/handler"
)

type mockPublisher struct {
	published [][]byte
	err       error
}

func (m *mockPublisher) Publish(ctx context.Context, payload any) error {
	if m.err != nil {
		return m.err
	}
	data, _ := json.Marshal(payload)
	m.published = append(m.published, data)
	return nil
}

func (m *mockPublisher) IsConnected() bool { return true }

func TestTelemetryHandler_ValidPayload(t *testing.T) {
	pub := &mockPublisher{}
	h := handler.NewTelemetryHandler(pub)

	body := `{"device_id":"dev-1","timestamp":"2026-03-17T14:30:00Z","sensor_type":"temperature","reading_type":"analog","value":23.5}`
	req := httptest.NewRequest(http.MethodPost, "/api/telemetry", bytes.NewBufferString(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	h.HandleTelemetry(w, req)

	if w.Code != http.StatusAccepted {
		t.Fatalf("expected 202, got %d", w.Code)
	}
	if len(pub.published) != 1 {
		t.Fatalf("expected 1 published message, got %d", len(pub.published))
	}
}

func TestTelemetryHandler_InvalidJSON(t *testing.T) {
	pub := &mockPublisher{}
	h := handler.NewTelemetryHandler(pub)

	req := httptest.NewRequest(http.MethodPost, "/api/telemetry", bytes.NewBufferString(`{invalid`))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	h.HandleTelemetry(w, req)

	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d", w.Code)
	}
}

func TestTelemetryHandler_ValidationError(t *testing.T) {
	pub := &mockPublisher{}
	h := handler.NewTelemetryHandler(pub)

	body := `{"device_id":"","timestamp":"2026-03-17T14:30:00Z","sensor_type":"temperature","reading_type":"analog","value":23.5}`
	req := httptest.NewRequest(http.MethodPost, "/api/telemetry", bytes.NewBufferString(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()

	h.HandleTelemetry(w, req)

	if w.Code != http.StatusBadRequest {
		t.Fatalf("expected 400, got %d", w.Code)
	}
}

func TestHealthHandler(t *testing.T) {
	pub := &mockPublisher{}
	h := handler.NewTelemetryHandler(pub)

	req := httptest.NewRequest(http.MethodGet, "/health", nil)
	w := httptest.NewRecorder()

	h.HandleHealth(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
}
