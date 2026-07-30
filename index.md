---
title: Paper Wiki — Home
aliases: [Home, Index]
tags: [meta/hub]
---
# CAMEL 26S Paper Wiki

SSD Internals Intensive Seminar (26S) 논문 위키. 다축(토픽·학회·연도)으로 navigate. 작성 규칙은 워크플로 노트 참고.

> [!warning] 📢 리스트 전면 개정 (2026-07-16, v1 → v2) + 신규 33편 deep 완료
> advisor 판단으로 세미나 리스트가 교체됨. **현재(v2) = 80편**: 겹침 46 + 신규 34(신규 33편 **PDF 전체 정독 deep 완료** 2026-07-16 + AstriFlash는 SkyByte refs의 deep 재사용).
> - 과거(v1) 전용 32편 + 리스트 외 개인 정독 4편 → **[[26S v1 아카이브]]** (자산 보존)
> - **정독 중 PDF 실물로 정정된 것**: MegIS·Exploiting Similarity(2023→**ISCA'24**) · FairyWREN·SquirrelFS(→**ACM TOS'25 저널판**) · Five-Minute Rule(→**arXiv**, 학회 미확인) · Light-Dedup(→**ATC'23**) · cluster 3건(Flagger infra→isc, FairyWREN reliability→zns, Silo fs→reliability). ⚠️ 아래 "정정 확인 필요" 참조
> - ❗ **발표 영향**: [[Ananke]](8/6)·[[Sparse Checkpointing for Fast and Reliable MoE Training|Sparse Checkpointing]](8/20) 제외 → **재선정 필요** ([[SkyByte]] 7/23 유지)

> [!tip] 시작점
> 처음이면 [[Communication Tax]](교수님 비전 문서)부터 → 발표 4편 → 토픽 hub 순으로 보면 맥락이 잡힙니다.

## 🎤 내 발표 (격주 목 · 녹화/교수님 온라인 참관)
**필터 = top-tier · (best-paper OR 내 rule-making 정체성)** (2026-07-16 확정 — [[연구 목표 변화 로그]] v4). 발표 채널 ↔ 연구·배경 정독 채널을 **분리**([[Roofline & FLOPs]] 등 방향 논문은 정독만).
- [[Smart-Infinity]] — HPCA'24 · **7/9 ✅** · near-storage LLM training (첫 발표·앵커)
- [[PF-LLM - Large Language Model Hinted Hardware Prefetching\|PF-LLM]] — ASPLOS'26 · 🏆 **Best Paper** · LLM-hinted HW prefetching (HW-SW co-design)
- [[Five-Minute Rule 40 Years Later - A First-Principles Revisit for Modern Memory Hierarchy\|Five-Minute Rule]] — ISCA'26 · **rule-making 정체성** · 메모리 계층 결정 *규칙*의 재정식화 (Jim Gray 5분 법칙)
- [[SquirrelFS - using the Rust compiler to check file-system crash consistency\|SquirrelFS]] — OSDI'24 · **rule-making / correct-by-construction 정체성** · Rust 컴파일러로 crash consistency *규칙*을 컴파일타임 강제
> ※ 왜 이 조합: PF-LLM=best-paper(craft 학습), Five-Minute·SquirrelFS=내 아키텍처 미학("규칙을 만들고 적용")의 정통 픽. Mooncake는 타 발표자에게 배정됨.
> ⚠️ 이전 계획(SkyByte 7/23 · Ananke · Sparse · "SSD 이중역할" 프레임)은 폐기. **SkyByte가 현재 시트에 안 보임 — 드롭됐는지 확인 필요.**
> 정독-only(발표 X, 방향·배경): DJFS·WOFS·[[Formalising CXL Cache Coherence]]·Espresso·PACT·TPP·DirectCXL 등

## 🏆 리스트 안 Best Paper (읽기 우선순위 — 5편, 전부 deep 완료)
> 2026-07-16 확인 (USENIX 공식 best-papers 목록 + ASPLOS'26 awards 페이지 대조). frontmatter `award: Best Paper` 태그됨.
- [[We Ain't Afraid of No File Fragmentation - Causes and Prevention of Its Performance Impact on Modern Flash SSDs|We Ain't Afraid of No File Fragmentation]] — **FAST'24 Best Paper** (SSD 단편화 원인·예방)
- [[FastCommit - resource-efficient, performant and cost-effective file system journaling|FastCommit]] — **ATC'24 Best Paper** (ext4 fast-commit 저널링, Google)
- [[Mooncake - Trading More Storage for Less Computation - A KVCache-centric Architecture for Serving LLM Chatbot|Mooncake]] — **FAST'25 Best Paper** (KVCache-centric LLM serving)
- [[Building Efficient Data Pipelines for Large-Scale LLM Pre-Training|Building Efficient Data Pipelines]] — **OSDI'26 Best Paper** (원제 "Teaching the Old Dog New Tricks", ByteDance)
- [[PF-LLM - Large Language Model Hinted Hardware Prefetching|PF-LLM]] — **ASPLOS'26 Best Paper** (LLM-hinted HW prefetching) 🎤정진 발표
> ※ 미확정 잔여(저확률): HPCA'26·ISCA'23·SOSP'25·MICRO'25 2nd. 참고: v1에서 빠진 Ananke도 FAST'25 Best Paper였음.

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
- **현재 리스트(v2) 80편 전부 deep**: 겹침 46 + 신규 33(정독 완료) + AstriFlash(refs deep) 1 ✅ (stub 0)
- 과거(v1) 자산: deep 32편 보존 → [[26S v1 아카이브]]
- hubs: topic 9 · venue 10 · year 4 · concepts(CXL 10 + HW 2 + OS 1) · insights 3 · hypotheses 3
- CXL 계보 papers([[CAMEL Lab CXL 연구 계보]]): **17/19 정독+요약 완료** (미정독 2 = Panmnesia·One-Chip, 공개 PDF 없음)

### ⚠️ 정정 확인 필요 (정독 중 PDF가 시트와 달랐던 것)
- **SquirrelFS·FairyWREN** — 시트=OSDI'24이나 제공 PDF는 **ACM TOS 2025 저널 확장판**. (SquirrelFS 원본은 OSDI'24 유명 논문) → 세미나엔 학회판/저널판 중 무엇으로 볼지 결정
- **Five-Minute Rule** — 시트=ISCA'26이나 PDF에 학회 배너 없음(arXiv만). OBASE·WriteGuards도 arXiv/pre-camera라 venue 미확정 → 학회 프로그램 확인 권장
- **Exploiting Similarity** — PDF 실제 제목이 다름("...Opportunit**ies** of Emerging **Vision AI** Models on Hybrid Bonding", "3D" 없음)·연도 2023→2024. 폴더명은 안 바꿈(frontmatter만 정정)
- 다음 작업: ① 발표 2슬롯(8/6·8/20) 재선정 ② 위 정정 확인
