package repository

import (
	"context"
	"fmt"

	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/davi/queue-load-test/consumer/internal/model"
)

type TelemetryRepository struct {
	pool *pgxpool.Pool
}

func NewTelemetryRepository(pool *pgxpool.Pool) *TelemetryRepository {
	return &TelemetryRepository{pool: pool}
}

func (r *TelemetryRepository) Insert(ctx context.Context, reading model.TelemetryReading) error {
	ts, err := reading.ParsedTimestamp()
	if err != nil {
		return fmt.Errorf("invalid timestamp: %w", err)
	}

	_, err = r.pool.Exec(ctx,
		`INSERT INTO telemetry_readings (device_id, timestamp, sensor_type, reading_type, value)
		 VALUES ($1, $2, $3, $4, $5)`,
		reading.DeviceID, ts, reading.SensorType, reading.ReadingType, reading.Value,
	)
	if err != nil {
		return fmt.Errorf("failed to insert telemetry reading: %w", err)
	}
	return nil
}
