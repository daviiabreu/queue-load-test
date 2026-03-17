package model

import (
	"errors"
	"time"
)

type TelemetryRequest struct {
	DeviceID    string  `json:"device_id"`
	Timestamp   string  `json:"timestamp"`
	SensorType  string  `json:"sensor_type"`
	ReadingType string  `json:"reading_type"`
	Value       float64 `json:"value"`
}

func (t TelemetryRequest) Validate() error {
	if t.DeviceID == "" {
		return errors.New("device_id is required")
	}
	if t.SensorType == "" {
		return errors.New("sensor_type is required")
	}
	if t.Timestamp == "" {
		return errors.New("timestamp is required")
	}
	if _, err := time.Parse(time.RFC3339, t.Timestamp); err != nil {
		return errors.New("timestamp must be valid ISO 8601 (RFC3339)")
	}
	if t.ReadingType != "analog" && t.ReadingType != "discrete" {
		return errors.New("reading_type must be 'analog' or 'discrete'")
	}
	if t.ReadingType == "discrete" && t.Value != 0 && t.Value != 1 {
		return errors.New("discrete reading value must be 0 or 1")
	}
	return nil
}
