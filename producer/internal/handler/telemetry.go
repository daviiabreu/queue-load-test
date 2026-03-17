package handler

import (
	"context"
	"encoding/json"
	"net/http"

	"github.com/davi/queue-load-test/producer/internal/model"
)

type MessagePublisher interface {
	Publish(ctx context.Context, payload any) error
	IsConnected() bool
}

type TelemetryHandler struct {
	publisher MessagePublisher
}

func NewTelemetryHandler(publisher MessagePublisher) *TelemetryHandler {
	return &TelemetryHandler{publisher: publisher}
}

func (h *TelemetryHandler) HandleTelemetry(w http.ResponseWriter, r *http.Request) {
	var req model.TelemetryRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		http.Error(w, `{"error":"invalid JSON"}`, http.StatusBadRequest)
		return
	}

	if err := req.Validate(); err != nil {
		http.Error(w, `{"error":"`+err.Error()+`"}`, http.StatusBadRequest)
		return
	}

	if err := h.publisher.Publish(r.Context(), req); err != nil {
		http.Error(w, `{"error":"failed to enqueue"}`, http.StatusInternalServerError)
		return
	}

	w.WriteHeader(http.StatusAccepted)
	w.Write([]byte(`{"status":"accepted"}`))
}

func (h *TelemetryHandler) HandleHealth(w http.ResponseWriter, r *http.Request) {
	if !h.publisher.IsConnected() {
		http.Error(w, `{"status":"unhealthy"}`, http.StatusServiceUnavailable)
		return
	}
	w.WriteHeader(http.StatusOK)
	w.Write([]byte(`{"status":"ok"}`))
}
