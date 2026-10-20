# Gateway radio transport

The gateway uses normal IEEE 802.15.4 L2 and a radio-bound `AF_PACKET` /
`SOCK_RAW` socket, not global `IEEE802154_RAW_MODE`. The socket is bound to the
interface belonging to `zephyr,ieee802154`, never a hard-coded interface index.
The application no longer overrides `net_recv_data()`.

`lib/radio` and its public headers are unchanged. Both directions use complete
MAC frames without FCS:

```c
/* sock is opened once with radio_socket_open(&sock, radio_iface). */
int len = sensor_frame_encode(psdu, sizeof(psdu), &cfg, &reading);
if (len > 0) {
        len = net_send_raw(&sock, psdu, len);
}

len = radio_socket_recv(&sock, psdu, sizeof(psdu), 0);
if (len > 0) {
        int ret = sensor_frame_decode(psdu, len, &reading);
        /* Handle ret and reading. */
}
```

The transport lives in `gateway/src/radio_socket.{c,h}`. Its calls return a
byte count or negative errno; setup returns zero or negative errno. TX uses
`sendto()` with an explicit radio destination interface and does not wait for
buffer availability. A successful send means accepted by the network stack,
not confirmed delivery over the air. It never retries a partial packet.
Frames are limited to 125 bytes excluding the two-byte hardware FCS.
RX rejects truncation rather than decoding a partial frame.

One RX consumer and a separate TX thread may share the same socket. Blocking
RX polls before a nonblocking receive: this avoids holding Zephyr's packet
socket fd lock while idle, which would stall TX. Close only after both users
stop. Initialize/open each transport once; do not reopen a live descriptor.

The bring-up image still only prints received readings: no unsolicited TX has
been added. Decode/print runs on an application thread; the socket queue uses
the finite network packet/buffer pools. Slow console output can exhaust those
pools and lose frames; this is not a reliable forwarding queue. RSSI/LQI are
not exposed by this adapter.

## Hardware-free validation

```sh
west build -b samr21_xpro gateway -d build-gateway-socket
west build -b native_sim/native/64 tests/net/radio_socket \
  -d build-radio-socket-test -- -DZEPHYR_TOOLCHAIN_VARIANT=host
set -o pipefail
build-radio-socket-test/zephyr/zephyr.exe 2>&1 | tee /tmp/ufsm292_socket_local_01.log
```

The simulator uses real Zephyr sockets/L2 and fake radio/Ethernet devices. It
checks codec TX bytes at the driver boundary, RX round-trip, interface isolation
(including Ethernet exclusion), truncation, invalid input, and TX while RX waits
on the same fd. A separate synthetic fragmented RX case bypasses IEEE802154 L2
(which requires contiguous RX frames) to check socket copying independently.

Validated against Zephyr 4.4.99, commit `c605ea46e15f`. The gateway explicitly
disables promiscuous reception and requires successful PAN filtering before
starting RX. Radio delivery with filtering enabled still needs hardware validation.
Ethernet/IP operation is not validated by these tests.

The RF2xx driver in that Zephyr revision lacks an upper bound on received PHY
lengths. A captured gateway fault had a 157-byte PHY length; the driver copied
162 bytes into its 132-byte local buffer and corrupted the radio thread's return
address. Disabling promiscuous mode reduces exposure but cannot replace bounds
checking before the driver reads the frame.

`gateway/patches/rf2xx-rx-length.patch` holds that fix. It is **not** applied to
the workspace checkout, which is left clean at `c605ea46e15f`, so apply it before
building the gateway:

```sh
patch -d "$(west list zephyr -f '{abspath}')" -p1 \
  < gateway/patches/rf2xx-rx-length.patch
```

The patch rejects PHY lengths greater than 127 before the SPI frame read, in
both filtered and promiscuous modes. The existing filtered minimum-length check
is retained. It adds no RAM or stack allocation.

A host-only reproducer for the overflow, with Zephyr worktrees for both the
unpatched and patched driver, lives in `~/workspace/radio`.

## Application threads

`main()` configures the radio/socket, then runs the blocking HTTP loop on its
existing 2048-byte stack. Radio setup failure does not prevent HTTP startup.
`radio_rx_id` uses a 2048-byte stack at priority 5 and waits on `rx_ready`
until radio setup succeeds. There is no separate gateway or HTTP thread.

Ethernet setup (`eth_setup()`) runs at application initialization: DHCP start
plus a net-mgmt callback for carrier on/off and IPv4 address events.
Bounded delayed work logs gateway statistics every five seconds and Ethernet
carrier/admin status and current IPv4 address every ten seconds (`ip=none`
when no preferred address exists); network I/O never runs on the system
workqueue. TCP retains its own Zephyr workqueue.

The KSZ8851 worker acknowledges an RX interrupt before reading its frame-count
snapshot, then consumes every header/frame in that snapshot before acknowledging
another RX interrupt. It does not reinterpret a cached count on unrelated polls.
Manual frame release (`RXQCR.RRXEF`) is polled to completion before another
RX/TX DMA operation, as required by the controller's self-clearing release bit.

The RX loop receives a complete frame, decodes a `sensor_reading`, then calls
`handle_sensor_reading()`. That handler updates the table under
`gateway_memory_lock`, then prints outside the lock. The HTTP accessors copy
state under the same lock, releasing it before JSON encoding or network I/O.
Keep application processing bounded so reception can continue.

## RAM budget

The memory table holds the latest reading for up to four distinct nodes,
using 128 bytes on SAM R21. Slots are searched by node ID, so IDs 0 through
255 remain valid. A new node is rejected with `-ENOSPC` when all slots are
occupied; existing nodes can still update. Stale entries retain their slots
until reboot or explicit table reset.

The HTTP loop uses a shared 515-byte JSON response buffer and a 128-byte input
chunk. Sensor encoding uses a 128-byte whole-table snapshot on main's stack.
There is no application heap allocation. Shared network pools use eight RX
and eight TX buffers with 256-byte payloads; packet counts are eight RX and
two TX. Preserve sufficient fragments for full Ethernet frames.
The KSZ8851 shares a 1528-byte standard-frame scratch buffer between RX and TX;
unsupported jumbo frames are released without copying them into a jumbo buffer.

`NET_MAX_CONTEXTS=4` covers the radio socket, HTTP listener, current client and
one overlapping close/queued/overflow client. `NET_MAX_CONN=5` also covers
DHCPv4's UDP handler: it registers a connection without owning a socket context.
Both pools must accommodate close-handshake overlap; sizing only contexts still
causes incoming connections to be reset when the registration pool is full.
`MAIN_STACK_SIZE=2048`, `ISR_STACK_SIZE=1024`, `NET_IPV4_MAX_ROUTES=2`,
`LOG_BUFFER_SIZE=512`. The SAM R21 build uses 32244/32768 bytes of RAM (98.40%)
and 127280/262144 bytes of flash (48.55%), leaving 524 bytes outside the
statically reserved storage. No additional thread or application heap is used.

An on-board smoke ran 100 back-to-back curls across 200/404/405 responses with
no transport failures, then verified overload rejection and 20 recovery requests.
Afterward both client contexts were free and only the TCP listener remained.
Untouched stack bytes after the exercised paths were main 340, Ethernet 384,
TCP 728, logger 212 and radio 1288; all measured stack sentinels were intact.
These measurements do not establish worst-case concurrent radio/Ethernet use.

## HTTP API

`gateway/src/http_server.c` runs one client at a time on main, using Zephyr's
incremental HTTP parser. The listener binds all IPv4 addresses on
`CONFIG_GATEWAY_HTTP_PORT` (default 80); it can bind before DHCP supplies an
address. Each response includes `Content-Length`, `application/json`, and
`Connection: close`. Keep-alive and pipelining are not supported.

After serving a response, main waits up to one second for the peer to close,
discarding any additional data without interpreting another request. Curl can
finish from `Content-Length` and close first, so the gateway performs a passive
TCP close rather than retaining a `TIME_WAIT` context for every normal request.
A peer waiting for EOF still receives an active close after that bounded grace
period. TCP's 1500 ms `TIME_WAIT` protection remains enabled for active closes.

- `GET /sensors`: 200 with a JSON array of all stored readings, including stale
  entries. Fields: `node_id`, `light`, `temp_c`, `accel`, `last_seen_ms`.
  Temperature is Celsius; `last_seen_ms` is gateway uptime at reception.
- `GET /health`: 200 with `status`, 64-bit `uptime_ms`, and `online_nodes`.
  `status` is `ok` after radio startup succeeds, otherwise `degraded`; it is
  not a promise of Ethernet health or continued radio delivery. Online age
  uses `CONFIG_GATEWAY_NODE_ONLINE_TIMEOUT_MS`, default 30000 ms.
- `POST /reset`: 202 with `{"status":"restarting"}`. After successful response
  queuing, delayed work requests a cold reboot after 100 ms. This is not
  proof of client receipt or TCP acknowledgment. An incomplete or malformed
  request does not trigger a reboot. Reboot discards the RAM table.

Other methods on a known route return 405 with `Allow`; unknown exact paths
return 404. Query strings are not matched. Malformed requests return 400;
requests exceeding 2048 bytes return 413. Paths must fit 31 bytes.
Each request has a ten-second reception deadline and sends use a five-second
timeout. While the primary request is incomplete, main also polls the listener.
An extra accepted connection is rejected with HTTP 500, `{"error":"busy"}` and
`Connection: close`, with a 250 ms send timeout and the bounded peer-close grace.
This is best-effort admission shedding, not another normal client slot: full
TCP/context/registration pools can reject a connection before HTTP is possible.
The listener is not polled while main is sending a normal response or waiting
for its peer to close. Radio RX remains on its separate thread.
Request bodies are parsed and discarded; these routes need no body.

Each completely parsed request emits an info-level serial log through the existing
gateway logger, for example `gateway: http: GET /health`. This includes complete
requests that later receive 404 or 405, but not incomplete or malformed requests.
It adds no static RAM, thread, or queue; deferred log records share the existing
512-byte log buffer. A logged request is not proof that its response was delivered.

`CONFIG_NET_CONTEXT_SNDTIMEO=y` is required for `SO_SNDTIMEO` on Zephyr's
native sockets. Without it, timeout setup fails with `ENOPROTOOPT` and the
handler closes the connection before reading the request (curl reports
`Empty reply from server`). Host-offloaded sockets do not expose this
configuration dependency. A native TCP loopback smoke reproduced the failure,
then verified timeout setup and a complete `/sensors` 200 response after enabling it.

There is no TLS or authentication. Anyone with access to the listener can
read sensor data or reboot the gateway. Use only on a trusted network.

```sh
curl -i http://GATEWAY_IP/sensors
curl -i http://GATEWAY_IP/health
curl -i -X POST http://GATEWAY_IP/reset
```

Hardware-free smoke validation used native_sim host-offloaded sockets on port
8080 with the real HTTP loop, encoder, memory helpers and gateway state
accessors. Curl verified both GET responses, wrong-method 405s, unknown-path
404, stale-node health, and reset's 202 response followed by the simulator's
`sys_arch_reboot` exit. Fragmented requests, malformed input, oversized reset
requests and ignoring a pipelined reset were also exercised. This does not
validate the SAM R21 Ethernet driver, native TCP stack, or physical reboot.

## Request data helpers

`gateway_memory_get_all()` copies valid entries into a caller-owned array and
returns the entry count. Allocate `GATEWAY_MEMORY_MAX_NODES` entries (128 bytes
on SAM R21) for a complete snapshot. A smaller array returns `-ENOSPC` without
writing a partial result. Entries are in slot order, not sorted by node ID;
stale readings remain in the snapshot for `/sensors`.

`gateway_memory_get_stats()` walks all slots and returns `valid_nodes` and
`online_nodes` for `/health`. Pass gateway uptime from `k_uptime_get_32()` and
an online timeout chosen from the expected sensor reporting interval. A node
is online when the elapsed time since its last reception is less than or equal
to that timeout. Receiving another reading makes a stale node online again;
the helper does not remove stale entries or free their slots.

Both helpers allocate no heap memory. Serialize their calls with updates using
the same lock, then encode/send the copied data after releasing that lock.
Unsigned 32-bit elapsed time works across an uptime wrap when reception age
is less than 2^32 milliseconds (about 49.7 days). Longer retained ages require
wider timestamps or an explicit expiration policy to avoid wrap ambiguity.
The single-node `gateway_memory_get()` API is unchanged.

The five-second cumulative gateway log includes `stored` (valid table entries)
and `active` (online nodes). `CONFIG_GATEWAY_NODE_ONLINE_TIMEOUT_MS` defaults to
30000 ms and controls the active check. Table updates and statistics reads use
`gateway_memory_lock`; future request handlers must use that same lock. Console
output happens after releasing it, so printing does not hold up table readers.
