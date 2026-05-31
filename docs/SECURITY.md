# Security Notes

## Do Not Commit Local Runtime Files

Local miner config can contain wallet addresses, pool logins, rig names, and API credentials. Keep these files out of commits:

- `config.txt`
- `pools.txt`
- `amd.txt`
- `nvidia.txt`
- `autotune.json`
- benchmark output JSON
- build directories
- release archives

## HTTP API

- Prefer binding the API to `127.0.0.1`.
- Set authentication before exposing the API outside localhost.
- Use a reverse proxy with TLS if remote access is needed.
- Treat bearer tokens like passwords.

## Pool Credentials

Many pools use `x` as a password, but some use real account credentials. Do not paste real pool credentials into docs, tests, commits, logs, or issue reports.

## Repo Hygiene

Before publishing:

```bash
git status --short
rg -n --hidden -S 'PRIVATE KEY|github token|api key|internal IP|password' . -g '!/.git'
```

If a secret ever lands in public history, rotate it. Rewriting git history helps remove obvious refs, but forks, caches, and local clones can still exist.

## License Hygiene

This repository is a GPLv3 derived work from xmr-stak. Keep `LICENSE`,
`NOTICE`, and `THIRD-PARTY-LICENSES` intact when publishing modified source
or binaries. Do not remove upstream author attribution from source files,
the startup banner, or distribution archives.
