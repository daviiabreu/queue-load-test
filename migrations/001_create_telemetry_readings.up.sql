CREATE TABLE telemetry_readings (
    id           BIGSERIAL PRIMARY KEY,
    device_id    VARCHAR(255) NOT NULL,
    timestamp    TIMESTAMPTZ  NOT NULL,
    sensor_type  VARCHAR(100) NOT NULL,
    reading_type VARCHAR(10)  NOT NULL CHECK (reading_type IN ('analog', 'discrete')),
    value        DOUBLE PRECISION NOT NULL,
    created_at   TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_telemetry_device_id ON telemetry_readings (device_id);
CREATE INDEX idx_telemetry_timestamp ON telemetry_readings (timestamp);
CREATE INDEX idx_telemetry_sensor_type ON telemetry_readings (sensor_type);
