---
title: "Failure Tolerant Training With Persistent Memory Disaggregation Over CXL (TrainingCXL)"
aliases: [TrainingCXL, Failure Tolerant Training CXL, TRAININGCXL]
description: "정명수 교수님 랩(KAIST·Panmnesia)의 IEEE Micro 2023 논문 — CXL type-2로 PMEM과 GPU를 하나의 cache-coherent domain에 묶어 대규모 추천모델(DLRM)을 disaggregated memory pool에서 학습하고, batch-aware checkpoint + relaxation으로 fault tolerance를 critical path 밖으로 빼냄. 5.2× 학습 성능, 76% 에너지 절감."
venue: IEEE Micro
year: 2023
doi: "10.1109/MM.2023.3237548"
type: insight
tags: [insight, insight/professor, cluster/cxl, topic-cxl, topic-checkpointing, topic-near-data, topic-llm-training]
---
# Failure Tolerant Training With Persistent Memory Disaggregation Over CXL

> **Source PDF**: [Failure Tolerant Training With Persistent Memory Disaggregation Over CXL.pdf](<Failure Tolerant Training With Persistent Memory Disaggregation Over CXL.pdf>)
> **Authors**: Miryeong Kwon, Junhyeok Jang, Hanjin Choi, Sangwon Lee, **Myoungsoo Jung** (KAIST and Panmnesia)
> **Venue / Year**: IEEE Micro, vol. 43, March/April 2023 (Theme: Emerging System Interconnects)
> **DOI**: 10.1109/MM.2023.3237548
> **Length**: 10 pages
> **Read status**: ☑ Full read (2026-06-28)
> **My reading purpose**: 교수님 본인 연구. 내 발표 4편(특히 Smart-Infinity의 near-storage training, Sparse Checkpointing의 MoE checkpointing)의 뿌리가 되는 "CXL type-2 near-data + checkpointing" 원형을 이해하기 위해. CXL이 왜 SSD/PMEM과 GPU를 묶는 substrate인지의 출발점.

---

## 📋 목차

- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary](#key-vocabulary)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR

production-level 추천모델(RM/DLRM)은 embedding table이 수십 TB~PB에 달해 local memory에 못 담고, 며칠~몇 주 학습하므로 주기적 checkpoint가 필수인데 그 checkpoint가 성능 병목이다. 기존 SSD 기반 memory expansion은 embedding lookup이 만드는 작고 random한 read에 약하고, SSD write가 느려 checkpoint 용도로 못 쓴다. **TrainingCXL**은 PMEM 기반 expander(**CXL-MEM**)와 GPU(**CXL-GPU**)를 둘 다 **CXL type-2 device**로 만들어 하나의 cache-coherent domain에 통합하고, embedding 연산·checkpoint logic을 PMEM 근처(near-data)에 둔다. 여기에 **batch-aware checkpoint**(undo log를 background로)와 **relaxation**(embedding lookup·checkpoint 순서 완화로 RAW·checkpoint overhead 제거)을 더해 fault tolerance를 학습 critical path 밖으로 뺀다. 결과: 최신 PMEM 기반 RM 시스템 대비 **5.2× 학습 성능, 76% 에너지 절감** (p.66, p.74).

---

## Core thesis

> "We propose TrainingCXL that can efficiently process large-scale RM datasets in the underlying memory pool, disaggregated over Compute Express Link (CXL). TrainingCXL makes deep learning training fault tolerant without imposing the checkpointing overhead as well." (p.67)

PMEM의 **nonvolatility**를 살리면 별도 checkpoint storage 없이 학습 그 자리에서 persistence를 얻을 수 있고, CXL **type-2**가 주는 cache coherence + near-data computing이 GPU↔memory 데이터 이동의 software overhead를 없앤다. 핵심은 "PMEM의 느린 write를 어떻게 학습 critical path 밖으로 숨기느냐"이며, 그 답이 RM 학습의 고유 특성(다음 batch의 embedding 구조를 미리 안다, embedding 연산이 add/sub 기반이라 commutative)을 이용한 batch-aware checkpoint와 relaxation이다.

---

## Why this matters to me

이건 교수님(Panmnesia CEO) 본인 그룹의 연구이고, 내 발표 4편을 관통하는 **"CXL로 storage/memory를 compute에 cache-coherent하게 붙여 near-data로 처리한다"**는 세계관의 초기 형태다. 특히:
- **Smart-Infinity**(near-storage LLM training)와 같은 "학습을 storage/memory 근처로 내린다"는 motivation을 공유.
- **Sparse Checkpointing for MoE**(NSDI'26)와 같은 "checkpoint를 어떻게 싸게/critical path 밖으로 빼느냐" 문제의식의 원조 격.
- CXL type-2(computing+memory, DCOH)가 왜 단순 memory expander(type-3)보다 강력한지를 구체 사례로 보여줌 → SkyByte/XHarvest의 CXL-SSD 논의의 배경.

즉 이 논문은 [Communication Tax 비전 문서](<../Compute Can’t Handle the Truth Why Communication Tax Prioritizes Memory and Interconnects in Modern AI Infrastructure/Compute Can’t Handle the Truth Why Communication Tax Prioritizes Memory and Interconnects in Modern AI Infrastructure.md>)의 "병목은 compute가 아니라 memory+interconnect"를 추천모델 학습이라는 구체 워크로드에서 입증한 사례다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Intro | 동기 | p.66–67 | RM은 거대 embedding + 잦은 checkpoint → SSD expansion은 random read·느린 write로 한계 |
| Contributions | 3대 기여 | p.67 | CXL memory expansion / batch-aware checkpoint / lookup·checkpoint relaxation |
| Background | RM·CXL | p.67–68 | DLRM(sparse=embedding, dense=MLP), CXL type-1/2/3·subprotocol·DCOH |
| PMEM Disaggregation | 시스템 구조 | p.68–69 | CXL-GPU+CXL-MEM 둘 다 type-2, CXL-MEM frontend(컴퓨팅·checkpoint logic)+backend(PMEM) |
| Auto data movement | CXL.cache 활용 | p.69–70 | DCOH cacheline flush로 SW(cudaMemcpy) 없이 데이터 이동 |
| Failure tolerance | checkpoint | p.70 | data/log region 분리, embedding·MLP logging, undo log를 background |
| Relaxation | 성능 회복 | p.71 | RAW 회피(commutative), MLP는 매 batch checkpoint 불필요 |
| Evaluation | 평가 | p.71–74 | FPGA 프로토타입, 5.2× 성능 / 76% 에너지 |
| Related/Conclusion | 비교 | p.74 | ISP·near-memory 대비 persistency+HW 이동+CXL3.0 확장성 |

---

## Section notes

### Introduction & 동기 (p.66–67)

production RM은 정확도 유지를 위해 거대 embedding(transformer보다 큼)을 쓰고, 수십 TB/PB memory를 소비한다(p.66). 며칠~몇 주 학습하므로 fault tolerance(주기적 checkpoint)가 필수이지만, checkpoint는 여러 컴퓨팅 도메인에서 성능 병목으로 알려져 있다(p.66). SSD로 host memory를 확장하는 접근은 embedding table은 담을 수 있으나 **embedding lookup이 작은 random read를 만드는 반면 SSD는 bulk I/O에 최적화**되어 심한 성능 저하를 겪는다(p.66). 게다가 SSD write latency는 일반 memory 연산보다 수 order 느리고 GC 같은 internal task까지 유발해, 기존 RM은 SSD를 오직 memory expansion 용도로만 쓴다(p.66–67).

### Contributions (p.67)

1. **Intelligent CXL memory expansion**: 여러 PMEM 모듈을 가진 nonvolatile expander **CXL-MEM**을 CXL 3.0의 type-2로 구성하고, GPU에 type-2의 **DCOH(device coherent) agent**를 붙여 **CXL-GPU**를 만든다. 둘이 같은 cache-coherent domain이라 host CPU의 software 개입 없이 embedding이 오간다. CXL-MEM은 type-2 endpoint controller와 함께 간단한 computing·checkpointing logic을 PMEM 근처에 둔다.
2. **Batch-aware checkpoint**: 학습 latency를 늘리는 batch 끝의 model/embedding update를 가리기 위해, **다음 batch의 embedding 구조를 미리 안다**는 점을 이용해 undo logging을 background로 수행(학습과 병렬로 CXL-MEM에 log).
3. **Embedding lookup & checkpoint relaxation**: 인접 batch 사이 embedding update→lookup이 만드는 **RAW(read-after-write)** 충돌과 checkpoint overhead를 없애기 위해 lookup·checkpoint 순서를 재배치해 CXL-GPU와 CXL-MEM 연산을 완전히 overlap.

### Background — RM training & CXL (p.67–68)

DLRM(Meta)은 sparse feature(categorical→embedding 연산)와 dense feature(continuous→bottom-MLP)를 함께 쓴다(p.67). 전통적으로 bottom-MLP는 GPU, embedding 연산(table lookup/update)은 host CPU에서 처리하고, feature interaction 후 top-MLP가 FWP/BWP로 학습한다. 모든 update된 parameter·embedding은 매 batch 끝에 storage로 checkpoint된다(p.68).

CXL은 cache-coherent하게 heterogeneous device가 대규모 memory를 공유하는 open standard(p.68). 구성요소는 host CPU·switch·device 셋이고, root complex당 최대 **4,095개 device**, 통합 주소공간 **HPA**, 세 subprotocol(CXL.io/cache/mem). type-1(io+cache), type-3(io+mem), **type-2는 셋 다** 권장 — backend에 computing+memory를 모두 두는 형태이며 내부 cache의 cacheline 상태를 **DCOH engine**이 추적해 coherence를 보장(p.68).

### PMEM Disaggregation & System Architecture (p.68–69)

CPU/GPU에서 memory를 분리(disaggregate)해 CXL로 한 시스템에 통합. PMEM은 DRAM과 비슷한 성능 + 대용량 + nonvolatility를 제공한다(p.68). 시스템은 **CXL-GPU**와 **CXL-MEM** 두 type-2 device가 switch로 host CPU에 연결된 형태(p.69). type-2라 서로의 내부 memory를 노출 가능. CXL-MEM이 embedding 연산을 전담하므로 host CPU는 학습 SW(PyTorch/TensorFlow)만 돌린다.

CXL-MEM 내부(p.69): **backend** = 여러 PMEM + memory controller(대규모 embedding table·checkpoint를 위한 data parallelism). **frontend** = ① 세 subprotocol을 구현한 CXL controller, ② embedding lookup/update를 처리하는 computing logic, ③ checkpoint를 자동 생성하는 checkpointing logic. host CPU는 MMIO register에 embedding vector 길이·learning rate, MLP parameter의 주소·크기를 세팅한다.

### CXL-based automatic data movement (p.69–70)

기존 SW 기반 이동은 cudaStreamSynchronize로 완료를 알리고 cudaMemcpy로 복사 → 무시 못 할 SW overhead(p.69). TrainingCXL은 **CXL.cache**로 데이터를 옮긴다: 데이터를 "쓰일 곳"에 저장하고, DCOH가 해당 cacheline을 flush(p.70). 예) feature interaction의 입력(reduced embedding)을 CXL-GPU device memory에 두고 CXL-MEM 내부 cache에 caching → CXL-MEM의 DCOH가 flush. 동기화는 "입력 데이터가 다 준비됐는지"만 확인하면 됨.

### Failure tolerance management (p.70)

기존 SSD 기반은 **redo log**(매 epoch 끝에 영구 저장). TrainingCXL은 **undo log** 방식의 **batch-aware checkpoint** — "업데이트될 embedding index를 batch 완료 전에 sparse feature로 미리 안다"는 특성을 이용, CXL-MEM의 idle time에 embedding/MLP log를 미리 저장(p.70). CXL-MEM 공간을 **data region**과 **log region**으로 나눠, 전원 장애 시 log region으로 RM model 복구.
- **Embedding logging**: sparse feature로 index 참조 → data→log로 embedding 복사 → persistent flag=true → 그 후 data region을 직접 update. update 중 전원 장애가 나도 flag가 서 있으면 그 batch부터 재개(p.70).
- **MLP logging**: bottom/top-MLP parameter는 CXL-GPU에 있으므로, checkpointing logic이 MMIO의 주소·크기로 CXL.cache 요청 → 전송된 parameter를 log region에 저장 → 다 받으면 flag=true. embedding·MLP log 둘 다 flag가 서면 이전 batch의 옛 checkpoint 삭제(p.70).

### Relaxation of failure-tolerant training (p.71)

- **Relaxed embedding lookup**: PMEM read는 DRAM과 비슷하나 같은 물리 layout에 write 직후 read하면 느려지는 **RAW**가 Nth batch update와 (N+1)th batch lookup 사이에 발생. Kwon & Rhu 분석상 **embedding vector의 80%가 연속 batch에서 학습**되어 RAW가 잦다(p.71). embedding lookup/update가 add/sub 기반이라 **addition의 commutative property**로 의존성을 완화 — (N+1)th lookup을 두 단계로 분리(Nth batch table로 먼저 lookup, gradient 준비되면 reduced embedding update).
- **Relaxed batch-aware checkpoint**: embedding·MLP log의 batch 번호 차이가 수백이어도 **정확도 저하가 business needs(0.01%) 이내**(p.71). 따라서 MLP는 매 batch checkpoint 불필요 → MLP logging을 여러 batch에 걸쳐 schedule. CXL-GPU가 top-MLP를 끝낼 때만 CXL.cache 요청에 응답하게 해 logging을 멈춤.

### Evaluation (p.71–74)

**프로토타입**: CXL-enabled RISC-V host CPU·CXL switch·CXL-GPU·CXL-MEM을 Xilinx Alveo U200 + 커스텀 FPGA로 구현(p.71). CXL-MEM은 CXL 3.0 controller·MC 4개·computing/checkpoint logic 포함. CXL-GPU는 Vortex(RISC-V GPGPU)를 포팅하고 MLP cycle은 RTX 3090에서 추출해 emulate(p.72). PMEM latency는 DRAM 지연으로 emulate.
**구성**: 매체별 SSD/PMEM/PCIe(near-data 가능) + TrainingCXL 단계별 CXL-D(스케줄링 없음)/CXL-B(batch-aware)/CXL(relaxation 포함). 모델 RM1–RM4(RM1·RM2 embedding-intensive, RM3·RM4 MLP-intensive)(p.72).
**결과**:
- embedding-intensive(RM1/RM2)에서 PMEM이 SSD 대비 **949× 빠른 학습**(checkpoint 제외, byte-addressable + 빠른 read/write)(p.73).
- **CXL-D가 PCIe 대비 23% 학습시간 감소** — SW overhead(cudaStreamSynchronize/cudaMemcpy) 제거 + checkpoint logic이 CXL.cache로 MLP parameter 직접 검사(p.73).
- CXL-B가 checkpoint를 CXL-GPU 연산과 overlap, **CXL이 CXL-B 대비 추가 14% 감소**(checkpoint·RAW overhead 제거)(p.73).
- 에너지: embedding-intensive **RM2가 DRAM 대비 91% 절감**, MLP-intensive **RM4가 PMEM 대비 62% 절감**(p.73). 절감폭은 CXL-MEM이 학습시간을 얼마나 줄이느냐에 비례.
- 종합 **5.2× 학습 성능 향상, 76% 에너지 절감**(p.74).

device 특성(Table 2, p.72, DRAM 정규화): PMEM latency read 3× / write 7×, BW read 0.6× / write 0.1×; SSD read latency 165×.

### Related work & Conclusion (p.74)

- **ISP**: RecSSD(embedding lookup을 ISP), RM-SSD(추천 전체를 storage로 offload). TrainingCXL은 **data persistency 보장 + 모든 데이터 이동을 HW가 SW 개입 없이** 수행하는 점에서 차별.
- **Near-memory**: RecNMP(DDR4 인터페이스 → CPU 수에 memory 확장 묶임), TensorDIMM(NVSwitch pooling → multitier 확장 한계). TrainingCXL은 **CXL 3.0 multilevel switching**으로 쉽게 확장, on-card computing이 memory media 용량에 안 묶임.

---

## Key vocabulary

**Thesis / framing:**
- "fault tolerant without imposing the checkpointing overhead"
- "disaggregated memory pool over Compute Express Link"
- "single cache-coherent domain"

**Technical concepts:**
- "CXL type-2 device" / "DCOH (device coherent) agent"
- "near-data computing/checkpointing logic"
- "batch-aware checkpoint (undo log)"
- "relaxed embedding lookup" / "RAW (read-after-write) conflict"
- "data region / log region"

**Value language:**
- "take checkpointing off the critical path"
- "without any software intervention running on the host CPU"

> ⚠ **피해야 할 어휘** (TrainingCXL-signature, echo 시 모방으로 보임):
> - "TrainingCXL" 자체 명칭
> - "batch-aware checkpoint" (이 논문 고유 명명 — 인용 시 출처 명시)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.66 | "5.2× training performance improvement and 76% energy savings" | CXL near-data + checkpoint 최적화의 효과 한 줄 |
| Intro, p.66 | production RM이 "tens of terabyte/petabyte memory" 소비 | 추천모델 memory 압박 motivation |
| p.73 | embedding-intensive에서 PMEM이 SSD 대비 "949× faster" | byte-addressable PMEM의 embedding 우위 |
| p.73 | CXL-D가 PCIe 대비 "23%" 학습시간 감소 | 자동 데이터 이동(SW overhead 제거) 효과 |
| p.73 | RM2 "91%" (vs DRAM) / RM4 "62%" (vs PMEM) 에너지 절감 | 워크로드 특성별 에너지 효과 |
| Table 2, p.72 | PMEM write latency 7× / write BW 0.1× (vs DRAM) | PMEM write가 왜 critical path 문제인지 수치 근거 |

---

## 🎯 Strategic anchor

> "TrainingCXL forms a nonvolatile memory expander (CXL-MEM) ... as a type-2 of CXL 3.0. ... Since TrainingCXL integrates CXL-MEM and CXL-GPU into the same cache coherent domain, all the input/output embeddings are exchanged between those two without any software intervention running on the host CPU." (§Intelligent CXL memory expansion, p.67)

→ **본인 활용**: "교수님 그룹은 CXL type-2의 cache-coherence를 써서 storage/memory와 accelerator를 SW 개입 없이 묶는다"는 핵심 thesis를 한 문장으로. 내 발표(Smart-Infinity의 near-storage, SkyByte의 memory-semantic CXL-SSD)가 이 계보 위에 있음을 면담에서 연결 가능.

---

## Connection to my research direction

| 차원 | 이 paper (TrainingCXL) | 발표/관심 방향 |
|---|---|---|
| Scope | 추천모델(DLRM) 학습 | LLM/MoE 학습·서빙 + SSD internals |
| Mechanism | CXL type-2 PMEM expander + near-data checkpoint | near-storage compute, CXL-SSD, MoE sparse checkpoint |
| Persistence | PMEM nonvolatility로 checkpoint를 학습 그 자리에서 | Sparse Checkpointing(NSDI'26)의 critical-path-off checkpoint |
| Open space | DLRM에 특화된 batch 구조 활용 | LLM/MoE의 다른 sparsity·checkpoint 구조로 확장 |

TrainingCXL은 "추천모델 + PMEM"이라는 2023년 시점의 구체화이고, 내 발표 라인은 같은 아이디어(near-data + checkpoint relaxation)를 **LLM/MoE + 최신 CXL/SSD**로 옮긴 것에 가깝다. 즉 motivation과 mechanism은 공유하되 워크로드와 매체가 진화한 관계.

---

## Open questions / gaps

- [ ] batch-aware checkpoint의 "다음 batch embedding 구조를 미리 안다"는 가정이 LLM/MoE(동적 routing)에서도 성립하는가? → Sparse Checkpointing의 MoEvement과 비교 포인트
- [ ] PMEM(Optane) 단종 이후, 같은 설계를 CXL type-3 + 다른 nonvolatile 매체로 어떻게 이식하나
- [ ] FPGA emulation(Vortex GPU, PMEM latency 모사) 기반 결과의 실제 ASIC/상용 환경 일반화 정도
- [ ] 4,095 device 규모로 확장 시 DCOH coherence traffic의 scalability

---

## References worth following up

| 상태 | Ref | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [3] | Eisenman et al., "Check-N-Run", NSDI 2022 | RM checkpointing의 baseline — 내 Sparse Checkpointing 발표와 직접 비교 |
| ☐ | [10] | Kwon & Rhu, "Training personalized recommendation ... Look forward not backwards", ISCA 2022 | "80% embedding 연속 batch 학습" 근거 + relaxation 아이디어 출처 |
| ☐ | [15] | Sun et al., "RM-SSD", HPCA 2022 | in-storage 추천 추론 — ISC 클러스터와 연결 |
| ☐ | [17] | Kwon et al., "TensorDIMM", MICRO 2019 | near-memory embedding의 대표 비교 대상 |
| ☐ | [7] | CXL Consortium, "CXL 3.0 Specification", 2022 | type-2/DCOH/4095 device 근거 사양 (→ concepts/CXL) |

---

## Personal annotations

<자유 메모 영역 — 직접 추가>

- (메모) 같은 저자군의 [LightPC](<../LightPC/LightPC.md>)는 "PMEM persistence를 OS·HW co-design으로 가볍게"라는 한 단계 더 근본적인 질문을 다룸. TrainingCXL이 "추천모델 학습"이라는 응용이라면 LightPC는 "full-system persistence"라는 기반.
