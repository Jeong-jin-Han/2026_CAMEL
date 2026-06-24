---
title: LLM Systems
aliases: [LLM Systems, LLM]
tags: [meta/hub, hub/topic]
---
# LLM Systems — cluster hub

LLM/ML의 serving·training을 스토리지/시스템 관점에서 다루는 논문 묶음 (near-storage 학습, KV cache, MoE checkpoint 등).

## Papers
- [[Smart-Infinity]] — HPCA'24, near-storage LLM training 🟡
- [[Sparse Checkpointing for Fast and Reliable MoE Training]] — NSDI'26, MoE sparse checkpoint 🟡
- [[Mooncake]] — FAST'25, KVCache-centric LLM serving (disaggregation)
- [[InstAttention]] — HPCA'25, in-storage attention offloading

> Pass 1에서 나머지(Mooncake, IMPRESS, SolidAttention, Bidaw, Lincoln, MaxEmbed, AttAcc, AiF, InstAttention, In-Storage RAG 등) 추가 예정.

## 인접 토픽
[[In-Storage Computing]] · [[CXL]]
