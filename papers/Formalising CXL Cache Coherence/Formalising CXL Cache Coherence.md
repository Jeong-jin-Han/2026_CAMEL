---
title: "Formalising CXL Cache Coherence"
aliases: [Formalising CXL Cache Coherence, CXL.cache 형식화]
description: "CXL.cache 프로토콜을 Isabelle로 형식화해 산문 스펙의 모호함·결함(incoherence 유발 가능)을 발견하고, 수정안이 CXL 컨소시엄에 잠정 채택된 논문"
venue: ASPLOS
year: 2025
tier: stub
status: dropped-from-talks   # 2026-07-13 발표 탈락 (랩장: formal proof는 랩 방향과 다름) — 개인 정독 후보로 유지
tags:
  - paper
  - topic/cxl
  - topic/verification
  - topic/coherence
  - venue/asplos
  - year/2025
---

# Formalising CXL Cache Coherence

> **ASPLOS 2025** · `topic/cxl · verification` · arXiv: [2410.15908](https://arxiv.org/abs/2410.15908) · [ACM DL](https://dl.acm.org/doi/10.1145/3676641.3715999)

저자: Chengsong Tan, Alastair F. Donaldson, **John Wickerson** (Imperial College London — memory model 형식화 명가)

## TL;DR (stub — 정독 전, 검색 기반 요약)
CXL의 device간 cache coherence 프로토콜인 **CXL.cache**를 산문(영어) 스펙으로부터 **Isabelle 정리증명기로 형식 모델화**. 그 과정에서 스펙의 **불명확·모호·부정확한 지점 다수를 발견**(일부는 **incoherence를 유발 가능**)하고 수정안을 제시 — **거의 전부 CXL 컨소시엄이 확인 후 잠정 채택**. "Snoop-pushes-GO" 등 핵심 제약의 시나리오 검증 + coherence 성질(**SWMR**)의 **기계화된 증명(mechanised proof)** 수행.

## 왜 이 논문인가 (발표 선정 이유 — 2026-07-10 확정)
- **주제 = 랩 심장부**: CXL coherence는 CAMEL/Panmnesia가 실리콘으로 만드는 그것. 스펙 결함은 그 지반의 균열
- **방법 = 랩의 빈칸**: build & measure 문화에 없는 formal proof — 내 정체성([[연구 목표 변화 로그]] v1: coherence·consistency를 transparent·formal로) 신호
- **"실리콘 없이 실제 쓰임"**: 증명 → **산업 표준(스펙)이 고쳐짐** = impact의 다른 경로
- **오프닝 논거**: "검증(testing-DV)은 다 한다. 그런데 coherence는 상태폭발·미묘한 race 때문에 testing이 못 덮는 대표 영역 — 그래서 스펙 수준 증명이 필요했고, 실제로 구멍이 나왔다"
- 발표 프레이밍: "Isabelle 기법 소개" ✗ → **"CXL.cache 스펙에 구멍이 있었고 컨소시엄이 고쳤다"** (아키텍처 스토리, 증명은 도구로 한 슬라이드)

## 발표 서사 내 위치
[[Smart-Infinity]](실측) → [[DJFS]](CXL-SSD 위 FS 실증) → **본 논문(그 장치들의 기반 프로토콜을 증명)** → [[WOFS]](FS 시스템 전체를 증명) = **"storage를 실증에서 증명까지"** 사다리. [[Ananke]](fault-injection 실증)를 교체 — Ananke는 정독 후보로 강등 (WOFS와 역할 부분 중복).

## 관련
- 허브: [[CXL]] · [[ASPLOS]] · [[Venue Tiers]]
- 내 가설: [[H1 — 워크로드 특화로 multi-node coherence 줄이기]] · [[H2 — CXL 위에서 PGAS 재해석]] (이 논문의 형식 기법 = H1/H2 검증 도구 후보)
- 배경: [[CXL Coherence]] · [[CXL Multi-node Coherence]]
- 같은 우물 (정독 목록): HeteroGen (HPCA'22, coherence 자동 합성 — plan B였음) · Trippel "Axiomatic HW-SW Contracts" (ISCA'22) · MC² (FMCAD'24)

> [!warning] 발표 탈락 (2026-07-13)
> 랩장 피드백 *"formal proof·수학 증명 중심 논문은 랩과 결이 맞지 않다"*로 WOFS와 함께 발표 라인업에서 제외 (최종 4편 = Smart-Infinity·SkyByte·Ananke·Sparse Checkpointing). **개인 정독 후보 + 장기 formal 노선([[연구 목표 변화 로그]] v1 north star)의 핵심 논문**으로 유지. 아래 "선정 이유"는 당시 판단의 기록.

## TODO (개인 채널)
- [ ] PDF 확보·정독 (세미나 무관, 개인 pace)
- [ ] 발견된 스펙 결함 목록·SWMR 증명 구조 정리
- [ ] H1/H2 검증 도구로서의 Isabelle 접근 가능성 타진
