---
title: ASPLOS
aliases: [ASPLOS]
tags: [meta/hub, hub/venue]
---
# ASPLOS — venue hub

ACM International Conference on Architectural Support for Programming Languages and Operating Systems.

## Papers (this wiki)

- [[Nemo]] — ASPLOS 2026
- [[Formalising CXL Cache Coherence]] — ASPLOS 2025
- [[AnyKey]] — ASPLOS 2025
- [[CIPHERMATCH]] — ASPLOS 2025
- [[MaxEmbed]] — ASPLOS 2025
- [[AttAcc]] — ASPLOS 2024
- [[BaM]] — ASPLOS 2023
- [[LeaFTL]] — ASPLOS 2023

## 🏛️ 왜 여기서 accept되나 (논문별 축적)
> 각 줄 = 그 논문의 ✅ 잘한 점(원문 근거)에서만. (규약: [nodegraph_review.md](../../workflow/nodegraph_review.md) §3-0-2) · ⓡ = ref(SkyByte 참고문헌)

- [[FlatFlash]] (2019) ⓡ — 돋보인 점: FTL을 host page table에 통합해 paging 경로 자체를 제거(direct cacheline access), HW(byte MMIO)+OS(page table) 층을 관통. (p.4)
- [[TPP]] (2023) ⓡ — 돋보인 점: Linux 커널 MM(reclaim·NUMA Balancing·watermark)을 CXL tier용으로 재설계해 앱 수정 없이 커널 릴리스로 전역 배포 + Meta production 실측. (p.1)
- [[ByteFS]] (2025) ⓡ — 돋보인 점: SSD firmware를 log-structured write buffer+coalescing으로 재설계해 flash traffic을 Ext4/F2FS/NOVA/PMFS 대비 2.9×/2.1×/3.2×/2.2× 감소(FS+firmware co-design). (p.12)

### 취향 요약 (누적 관찰)
- **관찰(3편)**: 세 편 모두 **여러 계층을 함께 재설계한 cross-layer co-design**(FTL↔page table / 커널 MM↔CXL / FS↔firmware) — ASPLOS는 단일 계층 최적화보다 **계층 관통 설계**를 보상하는 경향. 근거: [[FlatFlash]]·[[TPP]]·[[ByteFS]].

## 🆕 26S v2 신규 (stub)
> 2026-07-16 리스트 개정으로 편입. PDF 미확보 — deep 승격 시 상단 목록으로 이동.
- [[A Cost-Effective Near-Storage Processing Solution for Offline Inference of Long-Context LLMs]] — ASPLOS 2026 🆕stub
- [[Hitchhike - Efficient Request Submission via Deferred Enforcement of Address Contiguity]] — ASPLOS 2026 🆕stub
- [[PACT - A Criticality-First Design for Tiered Memory]] — ASPLOS 2026 🆕stub
- [[PF-LLM - Large Language Model Hinted Hardware Prefetching]] — ASPLOS 2026 🆕stub
- [[STRAW - Stress-Aware WL-Based Read Disturbance Management for High-Density NAND Flash Memory]] — ASPLOS 2026 🆕stub
