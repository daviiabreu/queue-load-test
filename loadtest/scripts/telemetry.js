import http from "k6/http";
import { sleep, check } from "k6";
import { Counter, Rate, Trend } from "k6/metrics";

const accepted = new Counter("telemetry_accepted");
const rejected = new Rate("telemetry_error_rate");
const latency = new Trend("telemetry_latency", true);

export const options = {
  scenarios: {
    // Steady baseline — simulates normal device reporting
    constant_load: {
      executor: "constant-vus",
      vus: 50,
      duration: "1m",
      startTime: "0s",
      tags: { scenario: "constant_load" },
    },

    // Stress ramp — find the throughput ceiling
    stress_ramp: {
      executor: "ramping-vus",
      startVUs: 50,
      stages: [
        { duration: "30s", target: 100 },
        { duration: "1m", target: 250 },
        { duration: "1m", target: 500 },
        { duration: "30s", target: 50 },
      ],
      startTime: "1m",
      tags: { scenario: "stress_ramp" },
    },

    // Spike — sudden burst simulating a plant-wide sensor event
    spike: {
      executor: "ramping-vus",
      startVUs: 0,
      stages: [
        { duration: "5s", target: 1000 },
        { duration: "30s", target: 1000 },
        { duration: "5s", target: 0 },
      ],
      startTime: "4m",
      tags: { scenario: "spike" },
    },

    // Flood — no sleep, raw maximum throughput
    flood: {
      executor: "constant-arrival-rate",
      rate: 2000,
      timeUnit: "1s",
      duration: "1m",
      preAllocatedVUs: 200,
      maxVUs: 500,
      startTime: "5m",
      tags: { scenario: "flood" },
    },
  },

  thresholds: {
    http_req_duration: ["p(95)<500", "p(99)<1000"],
    http_req_failed: ["rate<0.01"],
    checks: ["rate>0.99"],
    telemetry_error_rate: ["rate<0.01"],
  },
};

const sensorTypes = [
  { type: "temperature", reading: "analog", min: -20, max: 50 },
  { type: "humidity", reading: "analog", min: 0, max: 100 },
  { type: "vibration", reading: "analog", min: 0, max: 500 },
  { type: "luminosity", reading: "analog", min: 0, max: 10000 },
  { type: "level", reading: "analog", min: 0, max: 100 },
  { type: "presence", reading: "discrete", min: 0, max: 1 },
];

function randomDevice() {
  const id = Math.floor(Math.random() * 100);
  return `sensor-hub-${String(id).padStart(3, "0")}`;
}

function randomReading() {
  const sensor = sensorTypes[Math.floor(Math.random() * sensorTypes.length)];
  let value;
  if (sensor.reading === "discrete") {
    value = Math.round(Math.random());
  } else {
    value = sensor.min + Math.random() * (sensor.max - sensor.min);
    value = Math.round(value * 100) / 100;
  }
  return {
    device_id: randomDevice(),
    timestamp: new Date().toISOString(),
    sensor_type: sensor.type,
    reading_type: sensor.reading,
    value: value,
  };
}

export default function () {
  const scenario = __ENV.K6_SCENARIO_NAME || "unknown";
  const payload = JSON.stringify(randomReading());
  const params = {
    headers: { "Content-Type": "application/json" },
    tags: { scenario },
  };

  const start = Date.now();
  const res = http.post("http://localhost:8080/api/telemetry", payload, params);
  latency.add(Date.now() - start);

  const ok = check(res, {
    "status is 202": (r) => r.status === 202,
  });

  if (ok) {
    accepted.add(1);
  } else {
    rejected.add(1);
  }

  if (scenario !== "flood") {
    sleep(Math.random() * 0.4 + 0.1);
  }
}
