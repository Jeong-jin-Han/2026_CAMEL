---
title: "Compute Can't Handle the Truth: Why Communication Tax Prioritizes Memory and Interconnects in Modern AI Infrastructure"
aliases: [Communication Tax, Compute Can't Handle the Truth, Jung Tech Report]
description: "정명수 교수님(Panmnesia)의 76쪽 비전 문서 — 현대 AI infra의 병목은 compute가 아니라 communication+memory이며, CXL composable + CXL-over-XLink hybrid fabric이 답이라는 framing. 우리 발표 4편을 관통하는 상위 맥락."
type: insight
venue: Panmnesia Technical Report
year: 2025
arxiv: "2507.07223"
author: Myoungsoo Jung
tier: deep
status: done
tags:
  - insight
  - professor
  - topic/communication-tax
  - topic/cxl
  - topic/disaggregation
  - topic/interconnect
  - topic/tiered-memory
  - year/2025
---

# Compute Can't Handle the Truth: Why Communication Tax Prioritizes Memory and Interconnects in Modern AI Infrastructure

> **Panmnesia Technical Report · 2025** · `insight` · arXiv [2507.07223](https://arxiv.org/abs/2507.07223) · 76 pages
> Author: **Myoungsoo Jung** (CAMEL / Panmnesia)
> Source: [Compute Can't Handle the Truth ....pdf](<Compute Can’t Handle the Truth Why Communication Tax Prioritizes Memory and Interconnects in Modern AI Infrastructure.pdf>)

> [!abstract] 이 문서를 따로 두는 이유
> 일반 학회 논문이 아니라 **교수님의 직관·연구 방향이 담긴 framing 문서**다. SSD-list의 CXL·near-storage·checkpointing 논문들이 "왜 중요한가"를 위에서 설명한다. → `papers/`가 아닌 `insights/`에 분리.

## TL;DR
현대 AI infrastructure의 진짜 병목은 **compute가 아니라 communication + memory**(= "communication tax")라는 주장. LLM 학습 시간의 **35–70%가 inter-GPU communication**이고, 모델 파라미터·KV cache·embedding이 단일 GPU 메모리를 압도한다. 해법으로 **CXL 기반 composable/modular data center**(tray 단위 자원 분리 + memory pooling/sharing)와, intra-cluster XLink + inter-cluster CXL을 결합한 **CXL-over-XLink hybrid fabric**, 그리고 **Tier-1(coherence) / Tier-2(capacity) 계층 메모리**를 제안한다. RAG·DLRM·MPI에서 RDMA 대비 **1.62×~21.10×** 개선을 silicon-proven prototype으로 입증.

## 핵심 주장 (Core thesis)
"compute가 병목"이라는 통념을 정면 부정. 병목은 데이터 이동량·메모리 자원·통신 수요로 이동했고, memory controller를 연산 유닛에서 분리(decouple)해 composable pool로 만드는 CXL이 답이라는 것.
> [!quote]- 📄 원문 표현 (paper)
> - "The central challenge in contemporary AI infrastructures is no longer purely computational but involves managing massive data transfer volumes, extensive memory resources, and intensive communication demands characteristic of modern AI workloads." (§1, p.3)

## 구조 개요 (Structure overview)
| § | 제목 | Pages | 핵심 |
|---|---|---|---|
| 1 | Introduction | p.3 | Communication tax thesis, CXL이 답 |
| 2 | RNN → Transformer → LLM | p.4-12 | AI 배경. KV cache가 GPU 메모리 30-85% |
| 3 | Scaling LLMs: Multi-Accelerator | p.13-22 | DP 35-40%·PP ~50% utilization 한계, 단일 노드 한계 |
| 4 | Leveraging CXL for Diverse Metrics | p.23-32 | CXL 1.0/2.0/3.0 진화 + Tray architecture |
| 5 | Composable CXL: Integration & Empirical | p.33-40 | JBOM·memory box·topology + RAG/DLRM/MPI 정량 평가 |
| 6 | Beyond CXL: Hybrid CXL-over-XLink | p.41-50 | XLink(UALink+NVLink) + CXL = supercluster + Tier-1/Tier-2 |
| 7 | Conclusion | p.51 | Future: orchestration, deployment scale |

## 섹션 노트 (Section notes)
- **§2 모델 배경**: RNN→attention→Transformer→MoE→LLM. inference 측 메모리 압박으로 KV cache가 GPU 메모리의 30–85% 차지.
- **§3 스케일링 한계**: DP는 All-Reduce sync로 peak의 35–40%, PP는 pipeline bubble로 ~50%만 활용. tightly-coupled CPU-GPU(고정 비율·고정 메모리)의 비유연성 → **modular, independently scalable** 설계로 응답.
- **§4 CXL 진화**: CXL 3.0의 **PBR(Port-Based Routing)** + multi-level switching이 "true composability"의 분기점 (root port당 accel 256, memory 4096). **Tray-based architecture**(memory/accelerator/compute/switch tray) = 자원 유형별 표준 HW 단위.
- **§5 실증**: JBOM vs dedicated memory box, Clos/3D-Torus/Dragonfly topology. silicon-proven prototype으로 RAG/DLRM/MPI 정량 평가(아래 표).
- **§6 CXL-over-XLink**: XLink(UALink 비-coherent+NVLink)는 **intra-cluster**(≤72 GPU), CXL은 **inter-cluster coherent fabric**. CXL.cache로 cross-cluster cache coherence. Tier-1=accelerator-local(coherence-centric), Tier-2=composable pool(capacity-oriented, embedding 등).
> [!quote]- 📄 원문 표현 (paper)
> - "KV caching can occupy between 30% and 85% of the available GPU memory" (§2.3, p.12)
> - "fault-tolerance mechanisms such as data redundancy, replication, and memory checkpointing can be implemented through software. These mechanisms enhance system stability and reliability in large-scale, multi-accelerator environments." (§6.2, p.47)

## 핵심 정량 데이터 (Citable quantitative data)
| 출처 (§/page) | 데이터 | 맥락 |
|---|---|---|
| §1, p.3 | inter-GPU communication = 학습 시간의 **35–70%** | communication tax 규모 |
| §3.1, p.13 | Llama 3 405B = **100 TB+** 총 메모리 | 대규모 모델 메모리 수요 |
| §2.3, p.12 | KV cache = GPU 메모리 **30–85%** | inference 메모리 압박 |
| §3.1 | DP utilization **35–40%**, PP **~50%** | communication-bound 비효율 |
| §5.2 Fig.31, p.36 | RAG **5.93×**(vector search **14.35×**), data movement **21.10× less** | composable CXL 입증 |
| §5.2 Fig.31 | DLRM tensor init **2.71×**, inference **3.51×** | model load 이득 |
| §5.2 Fig.31 | MPI communication overhead **5.02× less** | MPI-over-CXL preview |
> [!quote]- 📄 원문 표현 (paper)
> - "RAG ... execution time 5.93× faster (vector search 14.35×), data movement 21.10× less" (§5.2, Fig.31, p.36)
> - "Future research should focus on addressing deployment challenges at industrial scales, exploring advanced orchestration techniques, and further refining hybrid interconnect strategies." (§7, p.51)

## 핵심 용어 (Key terms)
- **Communication tax**: 연산이 아니라 통신이 성능·시간을 잠식하는 비용. 이 문서의 슬로건.
- **Composable / disaggregated architecture**: 자원(메모리·가속기·연산)을 tray 단위로 분리해 독립 확장.
- **CXL memory pooling vs sharing**: pooling(2.0) = 호스트별 할당, sharing(3.0) = 다중 호스트 동시 공유.
- **PBR (Port-Based Routing)**: CXL 3.0의 multi-level switching 라우팅 — true composability의 기반.
- **XLink**: intra-cluster 가속기 인터커넥트(UALink 비-coherent + NVLink).
- **CXL-over-XLink supercluster**: XLink(클러스터 내) + CXL(클러스터 간 coherent) 하이브리드 fabric.
- **Tier-1 / Tier-2 memory**: coherence-centric(가속기 로컬) / capacity-oriented(대용량 pool).

## 우리 발표 4편과의 연결 (왜 이 문서가 상위 맥락인가)
| 발표 논문 | 이 문서에서의 위치 |
|---|---|
| [[SkyByte]] (CXL memory-semantic SSD) | §4 CXL을 메모리 substrate로 — SSD를 그 pool의 일부로 끌어들이는 구체 사례 |
| [[XHarvest]] (CXL 자원 harvesting) | §4-5 composable/disaggregation 철학을 SSD 내부 자원으로 확장 |
| [[Smart-Infinity]] (near-storage LLM training) | §3 communication/memory 병목을 "연산을 데이터 옆으로"로 완화 |
| [[Sparse Checkpointing for Fast and Reliable MoE Training]] (MoEvement) | §6.2 p.47 "memory checkpointing through software" — 분산 학습 fault tolerance의 software 층, 이 문서가 미발전 영역으로 짚은 자리 |

→ 4편이 각각 이 framing의 한 조각(CXL substrate / 자원 분리 / near-storage / 분산 reliability)을 구현한다.

## 열린 질문 (Open questions / gaps)
- [ ] CXL-over-XLink fabric의 multi-node consistency model (snapshot? happens-before?)
- [ ] 분산 학습 checkpoint placement — Tier-1 vs Tier-2 trade-off
- [ ] §6.2 "software fault tolerance"의 구체 메커니즘 (이 문서는 언급만)
- [ ] LLM training 시나리오 — 이 문서는 RAG/DLRM/MPI만 평가, LLM 학습은 직접 안 다룸
- [ ] Tier-2(capacity CXL)에서 non-volatile media(PRAM 등) persistence 활용

## ❓ Q&A (자가 점검)
> [!question]- Q1. 이 문서가 부정하는 통념은 무엇이고, 대신 무엇을 병목으로 보는가?
> "compute가 병목"이라는 통념을 부정. 병목은 communication + memory(데이터 이동량·메모리 용량·통신 수요)라고 본다 (§1, p.3).

> [!question]- Q2. "communication tax"의 규모를 보여주는 수치는?
> LLM 학습 시간의 35–70%가 inter-GPU communication overhead (§1). DP 35-40%·PP ~50% utilization (§3.1).

> [!question]- Q3. CXL 3.0이 2.0과 결정적으로 다른 점은?
> Memory **sharing**(다중 호스트 동시 공유)과 multi-level switching + **PBR(Port-Based Routing)** 지원 → "true composability". root port당 accel 256/memory 4096 (§4.2, Table 1).

> [!question]- Q4. Tray-based architecture의 4종 tray는?
> Memory / Accelerator / Compute / CXL Switch tray. 각 tray = 특정 자원 유형 전용 표준 HW 단위 (§4.3).

> [!question]- Q5. XLink와 CXL의 역할 분담은?
> XLink = intra-cluster(클러스터 내, ≤72 GPU, sub-μs). CXL = inter-cluster coherent fabric(클러스터 간, multi-level switch, CXL.cache로 cross-cluster coherence) (§6.2).

> [!question]- Q6. Tier-1 / Tier-2 메모리의 구분 기준은?
> Tier-1 = coherence-centric(가속기 로컬, cache coherence 필요). Tier-2 = capacity-oriented(대용량 composable pool, latency 덜 민감, embedding table 등) (§6.3).

> [!question]- Q7. 분산 학습 fault tolerance / checkpointing은 이 문서에서 어떻게 다뤄지나?
> §6.2 p.47에서 "memory checkpointing은 software로 구현 가능"이라고 **언급만** 한다. 구체 메커니즘은 미발전 — [[Sparse Checkpointing for Fast and Reliable MoE Training]]이 바로 이 빈자리를 채우는 계열.

## 🔗 Connections
[[CXL]] · [[2025]]
관련 발표 4편: [[SkyByte]] · [[XHarvest]] · [[Smart-Infinity]] · [[Sparse Checkpointing for Fast and Reliable MoE Training]]

## References worth following
| Ref | Paper | 왜 |
|---|---|---|
| [7] | Llama 3 herd of models (Meta, 2024) | failure rate·MTBF 데이터 |
| [21] | ZeRO (SC 2020) | distributed training memory 최적화 표준 — [[Smart-Infinity]] baseline 계열 |
| [25] | Megatron-LM (SC 2021) | distributed training framework |
| [51] | MAD-Max: Beyond Single-Node (ISCA 2024) | single-node → distributed framing |
| [305] | Characterization of LLM development in datacenter (NSDI 2024) | datacenter LLM 학습 실측 |
| [315] | AI and Memory Wall (IEEE Micro 2024) | "memory wall" thesis |

## Personal annotations
<본인 메모 영역 — 워크플로우가 절대 덮어쓰지 않음>
