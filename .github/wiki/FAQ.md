# FAQ

## Q: Why is the file always 12.5 MB even with 0 entries?

A: By design. num8 optimizes for speed, not sparse storage. The fixed size enables O(1) everything.

## Q: Can I store more data per number?

A: No. num8 stores only presence/absence. One bit per number. If you need metadata, use a different engine.

## Q: Is it thread-safe?

A: No. Caller must synchronize. Single-writer, multi-reader is the recommended model.

## Q: What happens if the process crashes during flush?

A: Header CRC protects against partial writes. On next open, corrupted header is detected.

## Q: Can I use it on Linux/macOS?

A: Yes. C99, no platform-specific code in the core engine.