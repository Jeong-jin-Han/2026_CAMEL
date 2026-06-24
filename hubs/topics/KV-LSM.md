---
title: KV-LSM
aliases: [KV-LSM, KV Store, LSM]
tags: [meta/hub, hub/topic]
---
# KV-LSM — cluster hub

Key-value store · LSM-tree · object cache 계열 (flash/SSD 상의 KV 데이터 관리).

## Papers
- [[Nemo]] — ASPLOS'26, tiny object cache, low write amplification
- [[HotRAP]] — ATC'25, hot record retention/promotion for LSM-trees (tiered storage)
- [[AegonKV]] — FAST'25, KV-separated LSM with SmartSSD GC offloading

> Pass 1에서 나머지(HotRAP, AegonKV, AnyKey, Holistic LSM, ELECT 등) 추가 예정.

## 인접 토픽
[[File System]] · [[In-Storage Computing]]
