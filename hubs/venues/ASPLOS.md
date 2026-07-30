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
- [[PF-LLM - Large Language Model Hinted Hardware Prefetching]] (2026) — 돋보인 점: 프리페칭 selection/degree/filtering 결정을 런타임 하드웨어 밖으로 빼내 오프라인 LLM(assembly 정독)이 생성 → ML+compiler+microarch를 관통하는 co-design, demand filtering으로 ensemble을 additive하게(최고 단일 대비 IPC 9.8%, 최고 ensemble 대비 18.9%). (Abstract p.1, §6.4 p.11)

### 취향 요약 (누적 관찰)
- **관찰(4편)**: 네 편 모두 **여러 계층을 함께 재설계한 cross-layer co-design**(FTL↔page table / 커널 MM↔CXL / FS↔firmware / **ML↔compiler↔microarch**) — ASPLOS는 단일 계층 최적화보다 **계층 관통 설계**를 보상하는 경향. 근거: [[FlatFlash]]·[[TPP]]·[[ByteFS]]·[[PF-LLM - Large Language Model Hinted Hardware Prefetching]].

## 🆕 26S v2 신규 (deep 완료)
> 2026-07-16 리스트 개정으로 편입 · PDF 전체 정독 완료(deep). venue/year는 PDF 실물 기준(시트 정정 포함).
- [[A Cost-Effective Near-Storage Processing Solution for Offline Inference of Long-Context LLMs]] — ASPLOS 2026 🆕(deep)
- [[Hitchhike - Efficient Request Submission via Deferred Enforcement of Address Contiguity]] — ASPLOS 2026 🆕(deep)
- [[PACT - A Criticality-First Design for Tiered Memory]] — ASPLOS 2026 🆕(deep)
- [[PF-LLM - Large Language Model Hinted Hardware Prefetching]] — ASPLOS 2026 🆕(deep)
- [[STRAW - Stress-Aware WL-Based Read Disturbance Management for High-Density NAND Flash Memory]] — ASPLOS 2026 🆕(deep)
