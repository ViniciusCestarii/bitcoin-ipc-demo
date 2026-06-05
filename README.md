# bitcoin-chain-ipc

A minimal C++ client that connects to a running Bitcoin node over a Unix socket and queries the current chain height using the Cap'n Proto IPC interface.

The goal is to observe the lowest level of Bitcoin's IPC mechanism: the raw Cap'n Proto binary frames sent over a Unix domain socket, visible byte-by-byte via `strace`.

## Prerequisites

- A Bitcoin Core build from [PR #29409](https://github.com/bitcoin/bitcoin/pull/29409), which adds the multiprocess IPC support
- Cap'n Proto
- CMake

## Start Bitcoin

Bitcoin must be built from PR #29409 and started with multiprocess mode and IPC socket binding enabled:

```bash
bitcoin -m node -ipcbind=unix
```

This launches the node process and creates a Unix socket at `~/.bitcoin/node.sock`.

## Build

```bash
cmake -B build
cmake --build build
```

## Run

```bash
./build/bitcoin-chain-ipc ~/.bitcoin/node.sock
```

Expected output:

```
Chain height: <chain height>
```

## Observing the wire with strace

To see every syscall, including the raw Cap'n Proto bytes sent and received over the socket:

```bash
strace -s 9999 ./build/bitcoin-chain-ipc ~/.bitcoin/node.sock -o trace.out
```

The `-s 9999` flag raises strace's string truncation limit so the full binary payloads are printed instead of being cut off with `...`.

Look for the `writev` and `read` calls on the socket file descriptor. The binary data is the Cap'n Proto framing: a little-endian segment count word, followed by segment sizes, followed by the message body containing the RPC call and response structs.

For example, on my case the final `read` (line 129 on trace.out file) you can see the result for `getHeight` from `bitcoin-core` which is the last 8 bytes at the end:

```
\234\204\16\0\1\0\0\0
```

In hex: 9C 84 0E 00  01 00 00 00 that's word 8, the getHeight result struct.

- 9C 84 0E 00 = 0x000E849C = 951452 (little-endian Int32)
- 01 = hasResult = true

951452 was the chain height at the time of the call.
