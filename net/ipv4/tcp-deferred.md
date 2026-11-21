# Deferred TCP application operations

`TIKU_KITS_NET_TCP_DEFERRED_ENABLE=1` adds application operations that queue
network output and retire connections in steps. The existing send, read and
abort entrypoints retain their synchronous behavior for normal connections.
Calls run in serialized network/service owner context, outside interrupts.

`send_deferred()` copies at most 512 bytes into the retransmission pool. The
connection must be established or in CLOSE_WAIT, have no outstanding sequence
bytes or TX segments, and have enough negotiated MSS and peer window for the
payload. Pool exhaustion and size/window refusals retain nothing. The caller
can release its input after success. Sequence accounting includes the admitted
bytes while the output request remains pending.

`read_deferred()` copies at most 512 bytes from the receive ring. Opening a
closed receive window records a pending ACK without calling the link provider.
The caller owns the copied output and retains no pointer into the receive ring.

`deferred_poll()` attempts one connection's pending data or window ACK per call.
It visits connections in round-robin order and keeps failed output pending.
A data transmission carries the current acknowledgment and window, satisfying
both pending indications. Successful output leaves the segment queued for its
peer acknowledgment and normal retransmission. Pending initial output does not
consume retransmission attempts.

The periodic handler calls `deferred_poll()` once per timer tick. A network owner
can also poll it between ticks to reduce initial-send latency without advancing
retransmission timers. This poll can invoke the link provider; its timing bound
therefore includes that provider. Application admission, deferred reads and
retirement do not invoke the link provider.

`discard_step()` first quiesces a connection and reserves its table slot. Each
following call releases at most one TX segment. It returns one while pending
and zero after releasing the slot. It sends no reset and invokes no callback.
The peer discovers the loss through its timeout or subsequent traffic.

Input and timer handling leave a reserved connection quiescent. Link-down and
normal connection cleanup preserve the reservation, including when an abort
callback starts retirement before the network handler returns. The caller must
retain its pointer and continue retirement until zero, then discard that pointer;
a later connection can reuse the released slot. RX storage belongs to the static
connection table, and its bytes are not erased by retirement.

`listener_connection(port, callback, index)` inspects one table slot without
output. It includes passive handshakes with inherited listener callbacks and
returns a borrowed pointer only for a matching active entry. A service removing
an endpoint can scan the bounded table and reserve matching entries through
`discard_step`; unrelated ports and callbacks remain untouched. Reserved entries
are absent from this lookup because their callbacks have been cleared.
