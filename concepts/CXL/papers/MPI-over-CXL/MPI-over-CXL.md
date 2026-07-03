---
title: "MPI-over-CXL: Enhancing Communication Efficiency in Distributed HPC Systems"
aliases: [MPI-over-CXL, MPI over CXL]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# MPI-over-CXL: Enhancing Communication Efficiency in Distributed HPC Systems

> **Source PDF**: [MPI-over-CXL.pdf](MPI-over-CXL.pdf)
> **Authors**: Miryeong Kwon, Donghyun Gouk, Hyein Woo, Junhee Kim, Jinwoo Baek, Kyungkuk Nam, Sangyoon Ji, Jiseon Kim, Hanyeoreum Bae, Junhyeok Jang, Hyunwoo You, Junseok Moon, Myoungsoo Jung — **Panmnesia, Inc.**
> **Venue / Year**: SPICE workshop @ MICRO 2025
> **arXiv / DOI**: arXiv:2510.14622v1 [cs.DC], 16 Oct 2025
> **Length**: 7 pages (5 pages 본문 + references)
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL/Panmnesia CXL 계보 Phase 4(2025)의 HPC 통신 편. 특히 **"기존 MPI를 CXL 위에 포팅"한 논문**임을 확인하고, 내가 노리는 **PGAS-over-CXL programming model의 빈 자리**를 이 논문 대비로 정확히 규정하기 위함.

---

## 📋 목차

- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary (for own writing)](#key-vocabulary-for-own-writing)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

**계보**: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md) · Phase 4 (2025) · HPC 통신

---

## TL;DR

전통적 MPI는 각 프로세서가 독립된 주소 공간을 가지므로 **explicit memory-copy**로 데이터를 주고받고, 대부분 RDMA 위에서 동작해 초기화·버퍼 관리 오버헤드가 크다. 이 논문은 CXL 3.0의 **cache-coherent 멀티-호스트 shared memory**를 이용해, MPI의 message queue와 데이터 버퍼를 **shared memory로 이전**하고 모든 프로세스의 **동일 virtual address**에 매핑함으로써, 데이터 복사 대신 **pointer 교환**만으로 통신하는 `MPI-over-CXL`을 제안한다. 4nm로 검증된 **CXL 3.2 controller** + **FPGA 4-host/4-switch/4-expander RTL 에뮬레이션**(1TB coherent shared memory) + custom kernel driver(~2,352 LoC)로 프로토타입을 구현했고, CXL 2.0 기반 DSM baseline 대비 **최대 1.6× E2E 실행 속도, 평균 8.4× 통신 속도** 향상을 보인다. 핵심은 새로운 programming model이 아니라 **기존 MPI semantics를 CXL shared memory 위로 포팅**한 것이다.

---

## Core thesis

> "we propose *MPI-over-CXL*, a novel paradigm that transforms traditional memory-copy-based MPI into memory-sharing-based MPI using CXL technology. MPI-over-CXL maps shared memory spaces onto identical virtual addresses across all processors, enabling simple pointer exchanges rather than actual data transmission." (§I.A, p.1)

추가 설명: MPI의 **인터페이스·의미론(send/recv, rank, communicator, barrier)은 그대로 유지**하되, 그 아래 데이터 이동 메커니즘을 "copy → shared-memory pointer passing"으로 교체한다. 즉 이 논문의 "novel paradigm"은 **transport-layer 교체**이지 프로그래밍 모델 교체가 아니다. Message queue를 local memory에서 shared memory로 옮기고(§III.A, p.3), 첫 접근 시 OS가 page fault를 CXL expander 물리 페이지로 해소해 모든 rank에 **일관된 VA 매핑**을 보장한다(§III.B, p.3).

---

## Why this matters to me

내 박사 방향(메모리 시스템 아키텍처: **PGAS-over-CXL**, multi-node coherence)에서 이 논문은 **가장 가까운 선행연구이자, 내가 노리는 빈 자리를 역으로 드러내는 대조군**이다. 이들은 CXL 3.0 shared memory 위에 "모든 프로세스 동일 VA에 매핑된 global address space" 하부구조를 이미 만들어 놓고도, 그 위에 **MPI의 copy-semantics를 에뮬레이션**하는 데 그쳤다(message queue·pointer enqueue·barrier로 send/recv 재현). 다시 말해 **PGAS의 substrate는 깔았지만 PGAS programming model은 노출하지 않았다.** 이 지점이 정확히 "MPI 포팅이 아니라 CXL-native shared-address programming model을 새로 설계한다"는 내 연구의 차별화 포인트다. 이 논문을 읽어야 "왜 MPI 포팅으로는 부족한가 / 왜 native PGAS가 필요한가"를 이들의 구현 디테일(barrier로 async를 sync-wrap, in-order 강제)로 구체적으로 논증할 수 있다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| I | Introduction (+ I.A MPI data movement patterns) | p.1–2 | MPI의 copy/RDMA 오버헤드 → CXL pointer 교환으로 대체. 최대 1.6× E2E, 평균 8.4× 통신 |
| II | Background | p.2 | Point-to-point vs collective; CXL.io/.cache/.mem; CXL 1.0→3.0 진화; CXL 3.0 구현 난점(multi-path, 멀티호스트 coherence 지연) |
| III | Design of Scalable MPI Communication | p.2–4 | Shared message queue + shared memory mgmt; 동일 VA 매핑; metadata(ref-count 등); 동기화(atomic/lock/barrier); dynamic polling; async(Isend/Irecv) 처리 |
| IV | Prototype Implementation & Evaluation | p.4–5 | CXL 3.2 controller(4nm), FPGA 4-host/4-switch/4-expander(1TB), ~2,352 LoC driver; Graph500/NAS.IS/Tealeaf/LBM; CXL 2.0 DSM baseline 대비 speedup, strong scaling |
| V | Conclusion & Future Work | p.5 | 더 큰 클러스터 평가·추가 최적화가 future work |

---

## Section notes

### §I Introduction (p.1–2)

전통 MPI의 병목을 두 겹으로 규정한다: (1) 각 프로세서가 **독립 주소공간**이라 memory-copy가 불가피 → latency·중복 저장, (2) 대부분 **RDMA** 사용 → 복잡한 초기화와 통신 오버헤드. CXL은 하드웨어 cache coherency로 멀티-호스트 메모리 공유를 제공하므로 "데이터 대신 pointer만 전송"이 가능하고 RDMA의 초기화·소프트웨어 개입을 제거한다. 특히 **sparse workload의 irregular access**에서 RDMA-MPI는 data reformatting이 필요하지만 MPI-over-CXL은 native data structure에 직접 접근한다고 주장(§I.A, p.1).

> "achieving up to 1.6× faster overall execution and, on average, 8.4× faster communication compared to traditional memory-copy-based MPI implementations." (§I.A, p.1–2)

### §II Background (p.2)

MPI의 두 통신 유형(point-to-point / collective)과 그 동기화 오버헤드가 프로세서 수에 따라 심화됨을 정리. CXL 파트에서 세 프로토콜(**CXL.io / CXL.cache / CXL.mem**)과 버전 진화를 설명: CXL 1.0 single-host 메모리 확장 → 2.0 memory pooling → **3.0 멀티-호스트 동시 shared memory access**. 중요하게 CXL 3.0의 **구현 난점**을 솔직히 나열한다: multi-level switch에서의 multi-path response routing, 멀티-호스트 coherence의 다중 round-trip 지연, 그리고 **"standardized methods for managing device coherent memory regions ... remain undefined"** — 이 미정의 영역이 MPI-over-CXL 같은 솔루션의 필요성을 부각한다고 말한다(§II.A, p.2).

### §III Design (p.2–4)

두 핵심 컴포넌트: **① shared message queue**(rank 간 pointer 연산으로 직접 교환, 중간 버퍼 제거), **② shared memory management**(CXL expander가 shared buffer 할당·유지). Sender는 shared queue에 직접 write, receiver는 poll하여 직접 read. 교환되는 pointer는 항상 sender local buffer가 아닌 **shared region**을 가리켜, 양측이 자기 local buffer를 독립적으로 재사용 가능(§III.A, p.3).

- **Memory handling / VA 매핑**(§III.B, p.3): 각 rank가 expander shared region을 자기 VA로 매핑하되 **모든 프로세스에서 동일 VA로 일관 매핑**. 첫 접근 시 OS가 page fault를 CXL expander 물리페이지 할당으로 해소.
- **Queue & data structure**(§III.B, p.3): local이던 MPI message queue를 shared region으로 이전. Sender는 데이터를 shared memory에 놓고 **해당 pointer를 receiver queue에 삽입**. Device-resident metadata(region id, page mapping, **reference count**)로 상태 추적(Fig.3, p.3). **작은 데이터는 pointer 교환 없이 dedicated buffer에 직접 배치** — MPI **eager mode**와 유사하되 copy는 제거.
- **Communication & synchronization**(§III.C, p.4): message queue entry = pointer + status flag. 소비 후 flag 갱신으로 재사용. **atomic operation·lock·MPI barrier**로 in-order send/recv 강제하여 buffer pollution(소비 전 덮어쓰기/쓰기 전 읽기) 방지. CXL의 cache coherence가 데이터 가시성 보장.
- **Dynamic polling**(§III.C, p.4): 트래픽에 따라 polling 빈도 조정 + 하드웨어 interrupt(event-driven). Queue가 첫 접근 후 캐시되어 이후 read는 local, 타 프로세스 수정 시 **hardware cache invalidation**으로 coherence 유지.
- **Async 처리**(§III.C, p.4): `MPI_Isend`를 **내부적으로 synchronous로 wrap**하여 sender가 barrier에서 전달 완료를 대기. 전통 distributed-memory MPI와 달리 matching receive 없이도 "데이터가 shared memory에 쓰이고 pointer가 enqueue되면" send 완료 → CXL coherence가 가시성 보장하므로 classical deadlock 없음.

> "an MPI_Isend operation is internally wrapped as synchronous, where the sender explicitly waits at a barrier to confirm message delivery completion, ensuring correct message ordering and eliminating potential race conditions." (§III.C, p.4)

### §IV Prototype & Evaluation (p.4–5)

- **HW**: PCIe 6.0 PMA + CXL-specific PCS + link/transaction layer를 통합한 **CXL 3.2 controller, 4nm 공정 검증**(Fig.5a). System 평가는 **4 host × 4 memory expander × 4 FPGA CXL switch**의 RTL-emulated 환경(Fig.5b). 각 host는 custom dual-core RISC-V(CXL 3.x + root port). Expander는 **8× DDR4로 1TB coherent shared memory** 제공.
- **SW**: RISC-V Linux용 **custom kernel driver ~2,352 LoC**(shared memory·VA·동기화 관리) + user-space MPI-over-CXL library(기존 MPI 앱에 minimal adaptation).
- **Baseline**: **CXL 2.0 기반 DSM(cache coherence 없음)** [50]. PCIe Gen 6·ConnectX-8 반영.
- **Workloads**: Graph500(comm-bound, point-to-point 지배), NAS.IS(collective 부담 큼), Tealeaf·LBM(computation-intensive).
- **결과**(Fig.6, p.4): 실행 speedup 1.1×–1.6×(Graph500 1.6×, NAS.IS 1.5×, Tealeaf 1.2×, LBM 1.1×); 통신 speedup 평균 8.4×, **Tealeaf 12.3×**(NAS.IS 8.2×, Graph500 6.9×, LBM 6.1×).
- **Strong scaling**(Fig.7, p.5): 4,096 node에서 Tealeaf·LBM ~**2.25×**, Graph500 ~**4.7×**, NAS.IS ~**5.9×**. Comm-intensive는 초기 스케일에서 급격히 상승 후 포화, comp-intensive는 프로세스 증가로 통신 비중 커질수록 이득이 점차 발현.

### §V Conclusion & Future Work (p.5)

DSM 대비 통신 오버헤드 감소를 재확인. **Future work = 더 큰 클러스터 평가로 scalability 확인 + 다양한 HPC 앱에 대한 추가 최적화.** (새 programming model 확장은 언급 없음 — 이 논문의 지향은 어디까지나 MPI 성능 개선.)

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "memory-copy-based MPI → memory-sharing-based MPI"
- "pointer-based communication without explicit data transfers"
- "cache-coherent shared memory across multiple hosts"

**Technical concepts:**
- "shared message queues" / "relocating MPI's message queues ... to shared memory"
- "consistently mapped to identical virtual addresses across all participating processes"
- "device-resident metadata (region identifiers, page mappings, reference counts)"
- "hardware-triggered cache invalidations ensure coherence"
- "internally wrapped as synchronous" (async Isend/Irecv 처리)

**Value language:**
- "eliminating redundant copying operations"
- "direct access to the native data structure" (sparse/irregular workload 대비)
- "these advantages become more pronounced with increased processor counts"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 모방으로 보임):
> - "MPI-over-CXL" — 이 논문 고유 명칭. 내 연구를 "PGAS-over-CXL"로 명명할 때 형태를 그대로 베끼지 말 것(내 것은 "-over-CXL" 접미어 대신 native model 강조).
> - "transforms traditional memory-copy-based MPI into memory-sharing-based MPI" — 이 논문 정체성 문장. 그대로 쓰면 이 논문 요약처럼 읽힘.
> - "up to 1.6× / on average 8.4×" 세트 — 이들의 결과 수치. 내 motivation에서 인용할 땐 반드시 출처(§I, p.1) 병기.

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §I.A, p.1–2 | "up to 1.6× faster overall execution and, on average, 8.4× faster communication" | CXL shared memory가 MPI copy/RDMA 오버헤드를 제거할 때의 상한 이득 — motivation 근거 |
| §IV / Fig.6, p.4 | 통신 speedup: Tealeaf **12.3×**, NAS.IS 8.2×, Graph500 6.9×, LBM 6.1× | "통신-집약 구간에서 pointer passing 이득이 극대화"의 정량 예시 |
| §IV / Fig.7, p.5 | 4,096 node: Graph500 ~4.7×, NAS.IS ~5.9×, Tealeaf·LBM ~2.25× | **scale-out 시 이득 증가** — multi-node로 갈수록 통신 병목이 커진다는 내 주장 뒷받침 |
| §IV, p.4 | CXL 3.2 controller **4nm 공정 검증**; 4 host×4 switch×4 expander; **1TB** coherent shared memory(8× DDR4) | "실물 실리콘+FPGA로 feasibility 입증" — feasibility-by-building narrative에 활용 |
| §IV, p.4 | custom kernel driver **~2,352 LoC** (RISC-V Linux) | OS/kernel 계층 개입 규모의 레퍼런스 값 |
| §IV, p.5 | baseline = **CXL 2.0 DSM without cache coherence** [50] | 비교 baseline이 "coherence 없는 DSM"임 — 내가 baseline 선택 논할 때 대조점 |

---

## 🎯 Strategic anchor

> "MPI-over-CXL maps shared memory spaces onto identical virtual addresses across all processors, enabling simple pointer exchanges rather than actual data transmission. By relocating MPI's message queues from local processor memory to shared memory and exchanging pointers referencing data instead of transferring data itself, we substantially reduce memory access latency and data copying overhead" (§I.A, p.1)

→ **본인 활용**: 면담·자소서에서 — "이 논문은 **모든 프로세스를 동일 VA에 매핑한 global shared address space라는 PGAS substrate를 이미 구축**했음에도, 그 위에 **MPI의 message queue와 send/recv semantics를 재현**하는 데 그쳤습니다(§I.A, p.1; §III.A, p.3). 즉 PGAS를 깔아놓고 MPI를 흉내 낸 셈입니다. 저는 이 substrate를 **MPI 에뮬레이션 없이 CXL-native shared-address programming model(PGAS-over-CXL)로 직접 노출**하려 합니다 — barrier로 async를 sync-wrap하는(§III.C, p.4) 이들의 우회 없이, coherence를 프로그래밍 모델의 1급 시맨틱으로." 이 한 문장이 내 "빈 자리"를 이 논문의 가장 강한 근거로 지목한다.

---

## Connection to my research direction

| 차원 | 이 paper (MPI-over-CXL) | 본인 방향 (PGAS-over-CXL / multi-node coherence) |
|---|---|---|
| Scope | 기존 **MPI 성능 개선** (transport-layer 교체) | **새 programming model** 설계 (CXL-native shared-address) |
| Programming model | MPI 유지 — send/recv/rank/communicator/barrier 그대로 | PGAS — global address space를 1급 추상으로 노출, message-passing 제거 |
| Mechanism | Shared message queue + pointer enqueue, async를 barrier로 sync-wrap | Coherence-aware direct load/store, 명시적 queue·barrier 우회 지향 |
| Coherence 위치 | CXL 하드웨어 coherence를 "보조"로 이용(visibility 보장용) | Coherence를 **프로그래밍 모델·메모리 컨시스턴시의 1급 시맨틱**으로 |
| Workload | HPC MPI 앱(Graph500/NAS.IS/Tealeaf/LBM) | 동일 HPC + irregular/sparse에서 native shared-memory 알고리즘 재작성 |
| Open space | "MPI를 그대로 두고 빠르게" — 모델 혁신은 없음 | "MPI를 벗어나 CXL에 맞는 모델을" — 이 논문이 비운 자리 |

이 논문은 내 방향의 **하부 인프라(coherent shared memory + 동일 VA 매핑 + expander metadata)를 실물로 증명**해 준다는 점에서 강력한 우군이다. 그러나 프로그래밍 모델 관점에선 **정반대의 선택**을 했다: 이미 shared address space를 만들고도 MPI copy-semantics를 재현하느라 barrier·in-order 강제·sync-wrap 같은 **불필요한 직렬화**를 도입한다(§III.C, p.4). 내 연구는 바로 그 재현 계층을 걷어내고, CXL coherence를 언어·런타임 수준의 shared-memory consistency로 직접 노출하는 **PGAS-over-CXL의 빈 자리**를 채운다. 즉 이 논문은 "feasibility는 됐다, 그러나 model은 남았다"를 증명하는 발판이다.

---

## Open questions / gaps

이 paper가 다루지 않은 / 미해결로 남긴 영역 — 내 연구의 잠재적 위치.

- [ ] **Programming model 혁신 부재**: shared address space substrate를 만들고도 PGAS/one-sided 모델을 노출하지 않고 MPI를 에뮬레이션만 함 → PGAS-over-CXL의 빈 자리.
- [ ] **async를 barrier로 sync-wrap**(§III.C, p.4): 진짜 비동기·overlap을 포기 → coherence-native async completion 모델은 미탐구.
- [ ] **Consistency model 미정의**: "cache coherence가 visibility 보장"이라고만 함. Memory consistency(ordering/atomicity) 시맨틱을 프로그래밍 모델로 형식화하지 않음.
- [ ] **Coherence 확장성 비용 미측정**: §II가 인정한 멀티-호스트 coherence의 multi round-trip 지연·multi-path routing을 정량 분석하지 않음(성능만 보고, coherence traffic 자체는 미측정).
- [ ] **Baseline이 CXL 2.0 DSM(coherence 없음)** [50]: RDMA/InfiniBand MPI 실측 대비, 최신 하드웨어 MPI와의 직접 비교는 약함.
- [ ] **Fault tolerance / partial failure**: shared memory 기반 통신의 장애 격리 미논의(ref [20]이 다루는 영역인데 본 논문은 언급만).
- [ ] **Collective 최적화 부재**: point-to-point 중심. Shared-memory 위 collective 알고리즘 재설계(SHARP 류)는 미탐구.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [39] | Gouk et al., "DirectCXL: Direct access, High-Performance memory disaggregation," USENIX ATC 2022 | CAMEL/Panmnesia 계보의 뿌리. 내 계보 이해의 기준점 |
| ☐ | [17] | Gouk, Kwon, Bae, Lee, Jung, "Memory Pooling With CXL," IEEE Micro 2023 | 같은 그룹 pooling 편 — Phase 계보 연결 |
| ☐ | [21] | Hoefler et al., "Leveraging MPI's One-Sided Communication Interface for Shared-Memory Programming," EuroMPI 2012 | MPI one-sided ↔ PGAS 접점. 내 모델 차별화의 직접 선행 |
| ☐ | [22] | Hammond, Ghosh, Chapman, "Implementing OpenSHMEM Using MPI-3 One-Sided," OpenSHMEM 2014 | **OpenSHMEM = 대표 PGAS**. MPI 위 PGAS 구현 사례 → 내 PGAS-over-CXL 대비군 |
| ☐ | [19] | Huang et al., "Txcocket: cross-node data transmission via CXL-based shared memory," CCF THPC 2025 | CXL shared memory 통신의 동시대 경쟁 연구 |
| ☐ | [20] | Zhang et al., "Partial failure resilient memory management for CXL-based DSM," SOSP 2023 | fault tolerance gap 채우는 레퍼런스 |
| ☐ | [50] | Cai et al., "Efficient distributed memory management with RDMA and caching," VLDB 2018 | 본 논문의 baseline DSM — 비교 기준 이해 |
| ☐ | [12] | Hoefler et al., "Remote memory access programming in MPI-3," ACM TOPC 2015 | MPI RMA 모델의 표준 참조 |

---

## Personal annotations

<자유 형식 메모 — user 전용 영역.>

- (초기 생성 시 메모) 이 논문의 저자 소속이 stub에는 "SPICE workshop @ MICRO 2025"로만 있었는데, PDF 표지 기준 **소속은 Panmnesia, Inc.** (Jung 교수 회사). 내 [CAMEL lab/company split](../../../..) 메모대로 "disaggregation류는 회사"라는 구분과 정합 — MPI-over-CXL도 회사(Panmnesia) 산출물.
- 핵심 인상: **"PGAS를 깔고 MPI를 흉내 냈다"**가 이 논문에 대한 내 한 줄 평. Strategic anchor가 그걸 정확히 집는다.
