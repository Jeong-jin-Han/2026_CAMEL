---
title: "Concepts — 배경 개념 허브"
aliases: [Concepts, 개념 허브, 배경 개념, Concept Hub]
type: hub
tags:
  - meta/hub
  - concept
---

# Concepts — 배경 개념 허브

> [!tip] 이 허브의 성격
> 발표 논문과 별개로, **논문을 읽는 데 필요한 배경 개념**을 클러스터로 모은 곳. 각 개념은 한국어 본문 + 영문 전문용어로 정리. (논문 요약은 `papers/`·`concepts/*/papers/`에, 이건 *개념* 전용)

---

## 🧭 방법론 (Methodology) — 논문을 읽는 렌즈
- [[Design Principles]] — 논문을 읽는 **고정 렌즈**(Amdahl·Locality·Parallelism·Hierarchy+Trade-off + 비-성능 렌즈). 원칙은 고정, 인스턴스만 바뀜.

## 🔌 CXL — 메모리 시스템 아키텍처 (내 연구 축)
- 기초: [[CXL Overview]] · [[CXL Glossary]] · [[CXL SOTA & Roadmap]]
- 버전: [[CXL 3.0]] · [[CXL 3.2]] · [[CXL 4.0]]
- 심화 (주소·coherence): [[CXL Address Translation]] → [[CXL Distributed Translation]] → [[CXL Coherence]] → [[CXL Multi-node Coherence]]
- programming model: [[PGAS]] (분할 전역 주소 공간 · release consistency)
- 계보: [[CAMEL Lab CXL 연구 계보]] (DirectCXL 뿌리 → Panmnesia silicon)

## ⚙️ HW / Accelerator — 가속기 배경
- [[FPGA Programmability]] — 회로가 되는 칩, 명령어는 선택사항 (고정 회로 → soft-core 스펙트럼 · Smart-Infinity updater가 명령어 안 받는 이유 · [[H3 — DockerGPU, in-GPU control plane|H3]] dispatcher 설계 축)
- [[Roofline & FLOPs]] — FLOP·FLOPs·FLOPS 구분 + 연산 밀도 $I$ + Roofline 병목 판정 (Adam update가 memory-bound라 near-data가 이기는 이유, 그래프 포함)

## 🧵 OS / Concurrency — 동시성 배경
- [[Deadlock · Livelock · Starvation]] — 세 가지 '멈춤'과 보장의 사다리 (Coffman 4조건 중 하나만 깨면 deadlock 원천 차단 · livelock-freedom = 유한성의 논리적 보장 · Venice 실례)

## 🧠 LLM Systems — 학습·추론 배경
- KV Cache: [[Efficiently Scaling Transformer Inference]] · [[Keep the Cost Down - A Review on Methods to Optimize LLM's KV Cache Consumption|Keep the Cost Down]] · [[KV Cache Optimization Strategies for Scalable and Efficient LLM Inference|KV Cache Optimization Strategies]]
- Parallelism: [[Demystifying Parallel and Distributed Deep Learning - An In-Depth Concurrency Analysis|Demystifying Parallel & Distributed DL]] · [[Model Parallelism on Distributed Infrastructure - A Literature Review from Theory to LLM Case-Studies|Model Parallelism Review]]

---

## 🔗 개념 ↔ 내 연구 방향
- **메모리 시스템 아키텍처**(박사 방향): CXL 클러스터 전체 + [[PGAS]] · 가설 [[H1 — 워크로드 특화로 multi-node coherence 줄이기|H1]] · [[H2 — CXL 위에서 PGAS 재해석|H2]]
- **논문 해석 프레임**: [[Design Principles]] — 특히 "공유 vs 전용 대역폭"(Parallelism 렌즈)이 CXL의 shared fabric vs per-device path 문제와 대응
- 허브로 돌아가기: [[index|Paper Wiki Home]]

> [!note] 관리 규칙
> - 개념 노트: 한국어 본문 + 영문 용어, `type: concept`
> - 허브/index: wikilink 사용 (Quartz가 해석) · `type: hub`
> - 새 개념 추가 시 여기 해당 클러스터에 한 줄 링크 추가
