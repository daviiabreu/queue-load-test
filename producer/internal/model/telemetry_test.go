package model_test

import (
	"testing"
	"time"

	"github.com/davi/queue-load-test/producer/internal/model"
)

func validTelemetry() model.TelemetryRequest {
	return model.TelemetryRequest{
		DeviceID:    "sensor-hub-042",
		Timestamp:   time.Now().UTC().Format(time.RFC3339),
		SensorType:  "temperature",
		ReadingType: "analog",
		Value:       23.5,
	}
}

func TestValidate_ValidAnalog(t *testing.T) {
	req := validTelemetry()
	if err := req.Validate(); err != nil {
		t.Fatalf("expected no error, got %v", err)
	}
}

func TestValidate_ValidDiscrete(t *testing.T) {
	req := validTelemetry()
	req.ReadingType = "discrete"
	req.Value = 1
	if err := req.Validate(); err != nil {
		t.Fatalf("expected no error, got %v", err)
	}
}

func TestValidate_MissingDeviceID(t *testing.T) {
	req := validTelemetry()
	req.DeviceID = ""
	if err := req.Validate(); err == nil {
		t.Fatal("expected error for missing device_id")
	}
}

func TestValidate_MissingSensorType(t *testing.T) {
	req := validTelemetry()
	req.SensorType = ""
	if err := req.Validate(); err == nil {
		t.Fatal("expected error for missing sensor_type")
	}
}

func TestValidate_InvalidReadingType(t *testing.T) {
	req := validTelemetry()
	req.ReadingType = "invalid"
	if err := req.Validate(); err == nil {
		t.Fatal("expected error for invalid reading_type")
	}
}

func TestValidate_DiscreteValueNotBinary(t *testing.T) {
	req := validTelemetry()
	req.ReadingType = "discrete"
	req.Value = 5.0
	if err := req.Validate(); err == nil {
		t.Fatal("expected error for non-binary discrete value")
	}
}

func TestValidate_MissingTimestamp(t *testing.T) {
	req := validTelemetry()
	req.Timestamp = ""
	if err := req.Validate(); err == nil {
		t.Fatal("expected error for missing timestamp")
	}
}

func TestValidate_InvalidTimestamp(t *testing.T) {
	req := validTelemetry()
	req.Timestamp = "not-a-date"
	if err := req.Validate(); err == nil {
		t.Fatal("expected error for invalid timestamp")
	}
}
