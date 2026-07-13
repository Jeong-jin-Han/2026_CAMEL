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
프레임 = **"storage를 실증에서 증명까지"** (v1 pivot — [[연구 목표 변화 로그]]): 실측 → crash test → 프로토콜 증명 → 시스템 증명, 증명 강도 상승순.
- [[Smart-Infinity]] — HPCA'24 · **7/9 ✅** · near-storage LLM training (real-system 실측)
- [[DJFS]] — FAST'25 · **7/23** · CMM-H(=CXL) 위 filesystem journaling (실 prototype + 1,000 crash test) — 발표 내 **CXL bridge**
- [[Formalising CXL Cache Coherence]] — ASPLOS'25 · **8/6 (랩장 피칭 예정)** · CXL.cache 스펙 형식화 — 결함 발견→컨소시엄 채택. *(Ananke 교체 — Ananke는 정독 후보로 강등)*
- [[WOFS]] — OSDI'25 · **8/20** · crash consistency **formal proof** = correct-by-construction (climax)

## By Topic (cluster 기준 편수)
- [[File System]] (18) · [[In-Storage Computing]] (13) · [[Reliability]] (8)
- [[LLM Systems]] (7) · [[KV-LSM]] (6) · [[ZNS]] (6) · [[CXL]] (5)
- [[Infra]] (4) · [[Vector Search]] (2)

## By Venue
- [[FAST]] · [[ASPLOS]] · [[ISCA]] · [[HPCA]] · [[MICRO]] · [[OSDI]] · [[SOSP]] · [[ATC]] · [[Eurosys]] · [[NSDI]]
- 🏆 [[Venue Tiers]] — 학회 tier 지도 (4대장·스토리지·OSDI/SOSP 위치 + 내 target 경로)

## By Year
- [[2026]] · [[2025]] · [[2024]] · [[2023]]

## 📚 배경 개념
- 🧭 허브: [[Concepts]] (전체 개념 index) · [[Design Principles]] (논문 읽는 고정 렌즈 — Amdahl·Locality·Parallelism·Hierarchy)
- CXL: [[CXL Overview]] · [[CXL 3.0]] · [[CXL 3.2]] · [[CXL 4.0]] · [[CXL SOTA & Roadmap]] · [[CXL Glossary]]
- 메모리 아키텍처 심화 (CXL/disaggregation/coherence — 내 연구 틀. *AT = Architecture*이지 Address Translation 아님): [[CXL Address Translation]] (주소 4종·HPA/DPA) → [[CXL Distributed Translation]] (변환 분산·control/data plane) → [[CXL Coherence]] (CXL.cache·용어) → [[CXL Multi-node Coherence]] (directory 난관·NVLink 비교) → [[PGAS]] (분할 전역 주소 공간·release consistency)
- 내 연구 가설 (검증 전): [[Hypotheses]] — [[H1 — 워크로드 특화로 multi-node coherence 줄이기|H1: 왜 워크로드 특화]] → [[H2 — CXL 위에서 PGAS 재해석|H2: 어떻게 = CXL 위 PGAS]]
- 🧭 [[연구 목표 변화 로그]] — 목표·안목이 어떻게 진화하는지 시간순 기록 (기존 목표 안 지우고 쌓음)
- 비전: [[Communication Tax]] — 병목은 compute가 아니라 communication+memory (CXL 배경 이해용)
- CXL 배경 reading (발표 focus 아님, 감 잡기용): [[LightPC]] (ISCA'22, full-system persistence) → [[TrainingCXL]] (IEEE Micro'23, CXL type-2 학습) · 발표 내 CXL 접점은 [[DJFS]] (CMM-H 위 FS, build로 증명)
- CAMEL Lab CXL 연구 계보 (리스트 밖·내 CXL 연구 배경): [[CAMEL Lab CXL 연구 계보]] — DirectCXL(뿌리)부터 Panmnesia silicon·One-Chip vision까지 시간순 flow + 빈 자리(programming model)

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
- hubs: topic 9 · venue 10 · year 4 (전부 자동 채움) · concepts(CXL) 10 · insights 3 · hypotheses 2
- CXL 계보 papers([[CAMEL Lab CXL 연구 계보]]): **17/19 정독+요약 완료** (미정독 2 = Panmnesia·One-Chip, 공개 PDF 없음)
