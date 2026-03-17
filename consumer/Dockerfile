FROM golang:1.26-alpine AS build
WORKDIR /app
COPY consumer/go.mod consumer/go.sum ./
RUN go mod download
COPY consumer/ .
RUN CGO_ENABLED=0 go build -o consumer ./cmd/main.go

FROM alpine:3.19
WORKDIR /app
COPY --from=build /app/consumer .
COPY migrations/ /app/migrations/
CMD ["./consumer"]
