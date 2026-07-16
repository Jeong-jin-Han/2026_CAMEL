---
title: Paper Wiki — Home
aliases: [Home, Index]
tags: [meta/hub]
---
# CAMEL 26S Paper Wiki

SSD Internals Intensive Seminar (26S) 논문 위키. 다축(토픽·학회·연도)으로 navigate. 작성 규칙은 워크플로 노트 참고.

> [!warning] 📢 리스트 전면 개정 (2026-07-16, v1 → v2)
> advisor 판단으로 세미나 리스트가 교체됨. **현재(v2) = 80편**: 겹침 46편(기존 deep 노트 유지) + **신규 34편**(33 stub 생성 + AstriFlash는 SkyByte refs의 deep 노트 재사용).
> - 과거(v1) 전용 32편 + 리스트 외 개인 정독 4편 → **[[26S v1 아카이브]]** (노트·정독 자산 보존, 삭제 아님)
> - 신규 stub은 `list/26s-v2` 태그로 구분 · cluster 추정 ⚠️ 9편은 확인 필요
> - ❗ **발표 영향**: [[Ananke]](8/6)·[[Sparse Checkpointing for Fast and Reliable MoE Training|Sparse Checkpointing]](8/20)이 새 리스트에서 제외됨 → **재선정 필요** ([[SkyByte]] 7/23은 유지)

> [!tip] 시작점
> 처음이면 [[Communication Tax]](교수님 비전 문서)부터 → 발표 4편 → 토픽 hub 순으로 보면 맥락이 잡힙니다.

## 🎤 내 발표 4편 (격주 목 · 4회 전부 녹화/교수님 온라인 참관)
프레임 = **"SSD의 이중 역할(compute + persistence)이 LLM 학습을 어떻게 뒷받침하는가"** + 관통 개념 **transparent HW-SW co-design** (2026-07-13 랩장 피드백 반영 **최종 확정** — [[연구 목표 변화 로그]] v2 · 상세 기록: 2026_vault 「세미나_최종4편_선정_기록」).
- [[Smart-Infinity]] — HPCA'24 · **7/9 ✅** · LLM 학습 compute를 near-storage로 offload *(Phase 1: Compute)*
- [[SkyByte]] — HPCA'25 · **7/23** · SSD를 memory-semantic CXL로 재설계 (OS+HW co-design) *(Phase 1: Compute — v2 리스트에도 있음, 유지)*
- ~~[[Ananke]] — FAST'25 · 8/6~~ · ❗**v2 리스트 제외 → 발표 취소, 재선정 필요**
- ~~[[Sparse Checkpointing for Fast and Reliable MoE Training|Sparse Checkpointing]] — NSDI'26 · 8/20~~ · ❗**v2 리스트 제외 → 발표 취소, 재선정 필요**
- (이전 강등 이력: DJFS·WOFS·Formalising CXL — 정독 후보. 재선정 시 새 후보 신호: SquirrelFS(OSDI'24, Rust로 crash consistency 검사)·HPCA'23 secure-NVM/crash 계열·Espresso(OSDI'26, CXL JBOF))

## By Topic (cluster 기준 편수 — v2 리스트, 2026-07-16 재집계)
- [[In-Storage Computing]] (~18) · [[File System]] (~14) · [[Reliability]] (~14)
- [[Infra]] (~8) · [[LLM Systems]] (~7) · [[CXL]] (~5) · [[ZNS]] (~4) · [[KV-LSM]] (~4) · [[Vector Search]] (2)
- ※ v1 대비 이동: **ISC·Reliability(secure-NVM/crash 계열)↑, FS-SW·LLM-serving↓** — v1의 FAST 25편이 v2에선 4편 (advisor의 "FAST 제외" 방향과 정합). 신규 stub의 cluster 추정 ⚠️9편 확정 후 수치 고정

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
- CXL 배경 reading (발표 focus 아님, 감 잡기용): [[LightPC]] (ISCA'22, full-system persistence) → [[TrainingCXL]] (IEEE Micro'23, CXL type-2 학습) · 발표 내 CXL 접점은 [[SkyByte]] (memory-semantic CXL SSD, OS+HW co-design)
- CAMEL Lab CXL 연구 계보 (리스트 밖·내 CXL 연구 배경): [[CAMEL Lab CXL 연구 계보]] — DirectCXL(뿌리)부터 Panmnesia silicon·One-Chip vision까지 시간순 flow + 빈 자리(programming model)

## 🔬 랩 관심사 맵 (⚠️ v1 75편 기준 — 개정 전 분석, v2 재분석 필요)
> 주의: 이건 **세미나 읽기 리스트**지 랩 publication이 아님 → 최근 SSD/storage 연구 지형을 넓게 본 것.
> **v2 변화 관찰(잠정)**: OSDI 대폭 증가(1→12편)·FAST 급감(25→4편)·secure-NVM/crash-consistency(HPCA'23) 계열 신설 — 무게중심이 storage-SW에서 **아키텍처·신뢰성·in-storage** 쪽으로 이동.

테마별 관련 논문 수(중복 카운트):
- **LLM/ML × storage — 24편 (≈1/3, 최대 무게중심)**
- In-storage / Near-data / PIM — 15
- File system / crash·journaling·GC — 15
- KV / LSM — 10 · Flash 신뢰성·FTL — 9
- **CXL / tiered-memory / disaggregation — 8**
- ZNS — 6 · Vector search — 2

해석: 무게중심은 **① ML×스토리지 → ② in-storage → ③ FS·신뢰성**. 내 발표 framing(2026-07-13 최종) = **"SSD의 이중 역할(compute+persistence) × transparent HW-SW co-design"** — ①(Smart-Infinity·Sparse Ckpt)과 ②③(SkyByte·Ananke)을 관통. CXL은 deep-invest 대신 background로 감만 잡음(발표 내 CXL 접점 = [[SkyByte]]).

## 진행 현황 (v2 기준, 2026-07-16 개정)
- **현재 리스트(v2) 80편**: deep(겹침) 46 + AstriFlash(refs deep) 1 + **stub 33** (신규, PDF 미확보)
- 과거(v1) 자산: deep 32편 보존 → [[26S v1 아카이브]]
- hubs: topic 9 · venue 10 · year 4 · concepts(CXL 10 + HW 2 + OS 1) · insights 3 · hypotheses 3
- CXL 계보 papers([[CAMEL Lab CXL 연구 계보]]): **17/19 정독+요약 완료** (미정독 2 = Panmnesia·One-Chip, 공개 PDF 없음)
- 다음 작업: ① 발표 2슬롯(8/6·8/20) 재선정 ② 신규 33편 PDF 확보→deep 승격 ③ cluster ⚠️ 9편 확인
