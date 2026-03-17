package model

import "time"

type TelemetryReading struct {
	DeviceID    string  `json:"device_id"`
	Timestamp   string  `json:"timestamp"`
	SensorType  string  `json:"sensor_type"`
	ReadingType string  `json:"reading_type"`
	Value       float64 `json:"value"`
}

func (t TelemetryReading) ParsedTimestamp() (time.Time, error) {
	return time.Parse(time.RFC3339, t.Timestamp)
}
