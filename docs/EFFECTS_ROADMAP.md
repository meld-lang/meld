# Future Effects Roadmap

Effects to add when real-world demand requires them.

## Current (12 effects — sufficient for CLI tools and agent automation)

| Effect | Operations |
|--------|-----------|
| Console | println, print |
| FileSystem | read, write, append, exists, remove |
| Env | get, set, cwd, home |
| Process | exec, exit |
| Http | get, post, put, delete |
| IO | read-line |
| Log | write |
| Time | now-ms |
| Random | int, float, bool |
| FFI | load, call, call-float, call-string, call-void, close |
| Crypto | hash, random-bytes, uuid |
| Timer | sleep, measure |

## Planned (add when building servers/services)

| Effect | Operations | Trigger |
|--------|-----------|---------|
| Net.TCP | connect, listen, accept, send, recv, close | Web server, database driver |
| Net.UDP | send, recv, bind | DNS, metrics, game networking |
| Net.DNS | resolve | Hostname lookup |
| Signal | trap, raise | Graceful shutdown (SIGTERM/SIGINT) |
| Async | spawn, await, channel-send, channel-recv | Concurrent I/O |
| Database | query, execute, transaction | Persistent storage |
| Stream | pipe, transform, buffer, flush | Data pipelines |

## Design Notes

- All effects use hierarchical dot-paths (`Net.TCP.connect`)
- `@uses` does prefix matching (`@uses(Net)` grants all networking)
- Kernel primitives for reactor/fd/tcp already exist — just need effect wrappers
- Database effect should be generic (not tied to SQL) — support KV stores too
- Async effect depends on delimited continuations (`kernel.suspend/resume`)
