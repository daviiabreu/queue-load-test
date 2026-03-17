FROM golang:1.26-alpine AS build
WORKDIR /app
COPY producer/go.mod producer/go.sum ./
RUN go mod download
COPY producer/ .
RUN CGO_ENABLED=0 go build -o producer ./cmd/main.go

FROM alpine:3.19
RUN apk add --no-cache curl
WORKDIR /app
COPY --from=build /app/producer .
EXPOSE 8080
CMD ["./producer"]
