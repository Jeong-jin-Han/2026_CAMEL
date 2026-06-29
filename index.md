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

## 🎤 내 발표 4편 (격주 목 · 4회 전부 녹화/교수님 온라인 참관)
프레임 = **feasibility-by-building**: "실물을 build해서 *가능하다*를 증명한" 논문만 (sim-optimization 제외). 증명 강도 상승순 — 실측 → crash test → fault-injection → formal proof.
- [[Smart-Infinity]] — HPCA'24 · **7/9** · near-storage LLM training (real-system 실측)
- [[DJFS]] — FAST'25 · **7/23** · CMM-H(=CXL) 위 filesystem journaling (실 prototype + 1,000 crash test) — 발표 내 **CXL bridge**
- [[Ananke]] — FAST'25 · **8/6** · transparent FS recovery (microkernel, 3만+ fault-injection)
- [[WOFS]] — OSDI'25 · **8/20** · crash consistency **formal proof** = correct-by-construction (climax)

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
- 비전: [[Communication Tax]] — 병목은 compute가 아니라 communication+memory (CXL 배경 이해용)
- CXL 배경 reading (발표 focus 아님, 감 잡기용): [[LightPC]] (ISCA'22, full-system persistence) → [[TrainingCXL]] (IEEE Micro'23, CXL type-2 학습) · 발표 내 CXL 접점은 [[DJFS]] (CMM-H 위 FS, build로 증명)

## 🔬 랩 관심사 맵 (리스트 75편 분포)
> 주의: 이건 **세미나 읽기 리스트**지 랩 publication이 아님 → 최근 SSD/storage 연구 지형을 넓게 본 것.

테마별 관련 논문 수(중복 카운트):
- **LLM/ML × storage — 24편 (≈1/3, 최대 무게중심)**
- In-storage / Near-data / PIM — 15
- File system / crash·journaling·GC — 15
- KV / LSM — 10 · Flash 신뢰성·FTL — 9
- **CXL / tiered-memory / disaggregation — 8**
- ZNS — 6 · Vector search — 2

해석: 무게중심은 **① ML×스토리지 → ② in-storage → ③ FS·신뢰성**. 내 발표 framing은 토픽이 아니라 *방법론*(feasibility-by-building) — FS·crash consistency 클러스터(③)를 골라 *증명 강도 사다리*로 묶음. CXL은 deep-invest 대신 background로 감만 잡음(발표 내 유일한 CXL 접점 = [[DJFS]]).

## 진행 현황
- deep: **75 / 75** · stub: 0 · 남은 SSD-list: 0 ✅
- hubs: topic 9 · venue 10 · year 4 (전부 자동 채움) · concepts(CXL) 9 · insights 3
