# HTTP API

When built with `-DMICROHTTPD_ENABLE=ON`, the miner exposes a local monitoring API.

The configured HTTP port lives in `config.txt`.

## Status

```text
GET /api.json
GET /api/v1/status
```

Status responses include hashrate, pool state, accepted/rejected share counts, and GPU telemetry where available.

## Configuration

Pool configuration can be updated through the API when enabled:

```text
PUT /api/v1/config/pool
```

Example body:

```json
{
  "pool_address": "pool.example.com:3333",
  "wallet_address": "RYO_WALLET_OR_LOGIN",
  "pool_password": "x"
}
```

Passwords and tokens should not be printed in status responses.

## Authentication

The HTTP server supports configured authentication. For API clients, bearer token auth is the simplest path when `http_api_token` is set:

```bash
curl -H "Authorization: Bearer TOKEN" http://127.0.0.1:9090/api/v1/status
```

Bind the API to localhost unless you have a real reason to expose it.
