---
title: Paper Wiki — Home
aliases: [Home, Index]
tags: [meta/hub]
---
# CAMEL 26S Paper Wiki

SSD Internals Intensive Seminar (26S) 논문 위키. **75편 전부 정독(deep) 완료.**
다축(토픽·학회·연도)으로 navigate. 작성 규칙은 워크플로 노트 참고.

> [!tip] 시작점
> 처음이면 [[Communication Tax]](교수님 비전 문서)부터 → 발표 4편 → 토픽 hub 순으로 보면 맥락이 잡힙니다.

## 🎤 내 발표 4편 (격주 목)
- [[SkyByte]] — HPCA'25 · **7/9** · memory-semantic CXL-SSD (OS·HW co-design), 6.11×
- [[XHarvest]] — ISCA'25 · **7/23** · CXL-driven SSD 자원 harvesting
- [[Smart-Infinity]] — HPCA'24 · **8/6** · near-storage LLM training, 2.11×
- [[Sparse Checkpointing for Fast and Reliable MoE Training]] — NSDI'26 · **8/20** (교수님 참석) · MoE sparse checkpointing (MoEvement), 8×

## By Topic (cluster 기준 편수)
- [[File System]] (18) · [[In-Storage Computing]] (13) · [[Reliability]] (8)
- [[LLM Systems]] (7) · [[KV-LSM]] (6) · [[ZNS]] (6) · [[CXL]] (5)
- [[Infra]] (4) · [[Vector Search]] (2)

## By Venue
- [[FAST]] · [[ASPLOS]] · [[ISCA]] · [[HPCA]] · [[MICRO]] · [[OSDI]] · [[SOSP]] · [[ATC]] · [[Eurosys]] · [[NSDI]]

## By Year
- [[2026]] · [[2025]] · [[2024]] · [[2023]]

## 📚 배경 개념
- CXL: [[CXL Overview]] · [[CXL 3.0]] · [[CXL 3.2]] · [[CXL 4.0]] · [[CXL SOTA & Roadmap]] · [[CXL Glossary]]
- 비전: [[Communication Tax]] — 병목은 compute가 아니라 communication+memory (발표 4편 관통)

## 🔬 랩 관심사 맵 (리스트 75편 분포)
> 주의: 이건 **세미나 읽기 리스트**지 랩 publication이 아님 → 최근 SSD/storage 연구 지형을 넓게 본 것.

테마별 관련 논문 수(중복 카운트):
- **LLM/ML × storage — 24편 (≈1/3, 최대 무게중심)**
- In-storage / Near-data / PIM — 15
- File system / crash·journaling·GC — 15
- KV / LSM — 10 · Flash 신뢰성·FTL — 9
- **CXL / tiered-memory / disaggregation — 8**
- ZNS — 6 · Vector search — 2

해석: 무게중심은 **① ML×스토리지 → ② in-storage → ③ FS·신뢰성**. CXL은 수는 적지만(읽는 대상이 아니라 *만들 방향*) [[Communication Tax]]와 발표 선택(4편 중 2편 CXL)에 "지향점"으로 박혀 있음.

## 진행 현황
- deep: **75 / 75** · stub: 0 · 남은 SSD-list: 0 ✅
- hubs: topic 9 · venue 10 · year 4 (전부 자동 채움) · concepts(CXL) 9 · insights 1
