# AegisFeed

**Protect the book. Respect the sequence.**

A deterministic C++20 market-data feed for Nasdaq TotalView-ITCH 5.0 over MoldUDP64.

It replays traffic, drops packets on purpose, recovers gaps, rebuilds the order book, and proves the final state.

`replay → drop → recover → rebuild → verify`

Built for correctness first. Nanoseconds next.

[Read the architecture →](./ARCHITECTURE.md)