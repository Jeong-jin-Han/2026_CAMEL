# Demystifying Parallel and Distributed Deep Learning - An In-Depth Concurrency Analysis

> **Source PDF**: [Demystifying Parallel and Distributed Deep Learning - An In-Depth Concurrency Analysis.pdf](Demystifying Parallel and Distributed Deep Learning - An In-Depth Concurrency Analysis.pdf)
> **NodeGraph**: [Demystifying.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/concepts/LLM/Parallelism/Demystifying%20Parallel%20and%20Distributed%20Deep%20Learning%20-%20An%20In-Depth%20Concurrency%20Analysis/Demystifying.html)
> **Authors**: Tal Ben-Nun, Torsten Hoefler (ETH Zurich, SPCL)
> **Venue / Year**: ACM Computing Surveys (arXiv preprint, 2018)
> **arXiv / DOI**: arXiv:1802.09941v2
> **Length**: 47 pages
> **Read status**: ☑ Full read (2026-07-06)
> **My reading purpose**: 배경 지식 확보 — LLM/DNN 분산학습의 병렬화 전략(data/model/pipeline parallelism)과 통신 패턴을 체계적으로 이해해서, 이후 memory-system architecture(CXL, disaggregation) 연구가 왜 이 computation-communication tradeoff 안에서 의미를 갖는지 근거를 마련하기 위함.

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

---

## TL;DR

이 논문은 딥러닝 학습·추론 전 과정에 존재하는 concurrency를 하나의 이론적 틀(Work-Depth 모델)로 꿰어 정리한 서베이다. 단일 operator(convolution, FC layer 등) 수준의 병렬화부터, 네트워크 전체를 나누는 data/model/pipeline parallelism, 그리고 여러 머신에 걸친 분산 학습(synchronization 방식, parameter server vs decentralized allreduce, gradient/parameter compression, model consolidation, 심지어 architecture search의 병렬화까지)을 240편의 논문을 리뷰하며 체계적으로 분류한다. 저자들은 "정확도(generalization)를 크게 해치지 않으면서 얼마나 concurrency를 짜낼 수 있는가"라는 질문을 축으로 각 접근법의 trade-off를 Work-Depth 모델(W: 총 작업량, D: critical path 길이)로 정량화한다.

---

## Core thesis

> "The world of deep learning is brimming with concurrency. Nearly every aspect of training, from the computation of a convolution to the meta-optimization of DNN architectures, is inherently parallel." (§8, p.32)

추가 설명: 저자들은 DNN의 거의 모든 계산 단계(개별 operator 실행, 네트워크 전체 forward/backward, 여러 노드에 걸친 학습, 심지어 hyper-parameter·architecture search)가 원리적으로 병렬 가능함을 보이고, 설령 일부 단계가 순차적(synchronous)이더라도 "정확도를 크게 해치지 않는 선"에서 synchronization 요구조건을 완화(relax)하면 concurrency를 늘릴 수 있다고 주장한다. 즉 이 논문의 중심 프레임은 "correctness/consistency vs. concurrency"의 tradeoff이며, 이는 §3(generalization vs. utilization)과 §7.1(model consistency spectrum)에서 반복적으로 등장한다.

---

## Why this matters to me

내 연구 방향(memory-system architecture — CXL, memory disaggregation, cache coherence, tiered memory)은 LLM 학습 자체가 아니라 그 밑에 깔린 "여러 device/node 사이의 데이터 이동과 동기화 비용" 문제를 다룬다. 이 논문은 그 문제가 딥러닝 맥락에서 어떻게 정식화되는지 보여주는 foundational survey다. 특히 §2.5의 allreduce 통신 비용 분석(LogP 모델 기반 latency-bandwidth tradeoff)과 §7 전체(centralization, compression, staleness)는 "computation과 communication 사이의 비대칭이 분산 시스템의 근본 병목"이라는 사실을 명시적으로 보여준다 — 이는 내가 CXL/coherence 연구에서 다루는 "interconnect가 memory access의 병목이 되는 지점"과 동일한 구조의 문제다. 다만 이 논문은 그 통신 계층 자체(interconnect 기술, 메모리 계층)를 black box(LogP의 α, β 파라미터)로만 다루기 때문에, 내 연구는 바로 그 black box 안쪽 — 즉 memory/interconnect 하드웨어 자체를 어떻게 설계해야 이 병목을 완화할 수 있는가 — 를 파고드는 위치에 있다고 정리할 수 있다. 억지로 LLM 연구와 직접 연결하기보다는, "분산 시스템에서 통신 비용이 왜, 어떻게 병목이 되는지"에 대한 배경 지식으로 자리매김하는 것이 정직한 평가다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction (+ Related Surveys, Scope) | p.1-3 | 240편 리뷰, 1984년까지 거슬러 추적. 병렬성이 왜 DNN 학습의 필수 요소가 됐는지 개관 |
| 2 | Terminology and Algorithms | p.3-9 | Supervised learning, SGD, weight update rules(Table 3), 병렬 컴퓨터 아키텍처 트렌드(Fig 3, 4), Work-Depth 모델과 allreduce 통신 하한 |
| 3 | The Efficiency Tradeoff: Generalization vs. Utilization | p.9-10 | Minibatch 크기가 정확도와 하드웨어 효율의 trade-off 축임을 이론(descent lemma)·실험(Fig 6b)으로 증명 |
| 4 | Deep Neural Networks | p.10-14 | Neuron/FC/Conv/RNN(LSTM, GRU) operator 정의, backpropagation, Work-Depth 특성(Table 4), 5개 대표 네트워크(Table 5) |
| 5 | Concurrency in Operators | p.14-18 | Performance modeling, FC=GEMM, Convolution 4가지 방법(direct, im2col, FFT, Winograd, Table 6), RNN 최적화(fusion, persistent RNN) |
| 6 | Concurrency in Networks | p.18-22 | Data parallelism, Model parallelism(LCN, TreeNets), Pipelining(DSN), Hybrid parallelism(DistBelief, Project Adam) |
| 7 | Concurrency in Training | p.22-32 | Model consistency(sync/stale-sync/async, Fig 20), Centralization(PS vs decentralized, Fig 21), Compression(quantization/sparsification), Model consolidation(ensemble/distillation/averaging), Optimization/Architecture search |
| 8 | Concluding Remarks | p.32-33 | Concurrency가 DNN 전 영역에 편재함을 재확인, 향후 방향(compiler-level whole-DNN 최적화, elastic training, multi-purpose networks) 제시 |
| A | Analysis of Influential CNNs | p.39-41 | LeNet/AlexNet/GoogLeNet/ResNet/DenseNet의 W/D 분석 및 트렌드(실험기→성장기→자원절약기) |
| B | DNN Layer Computation Formulas | p.41-42 | Activation/FC/Conv/Pooling/BN의 forward·backward 수식 |
| C | Convolution Computation Analysis | p.43-47 | Direct/im2col/FFT/Winograd의 상세 W-D 유도 (Algorithm 3-6) |

---

## Section notes

### §1 Introduction (p.1-3)

DNN이 여러 응용 분야(이미지 분류, 음성 인식, 자율주행, 게임 등)에서 성공한 배경에는 "계산 자원의 병렬성을 활용할 수 있었기 때문"이라는 역사적 사실이 있다고 짚는다. 데이터셋과 모델 규모가 커지면서 계산·메모리 요구가 비례해서 증가하고, 이를 감당하려면 결국 HPC 클러스터가 필요하다는 문제의식에서 출발한다(p.1). 저자들은 240편의 논문을 1984년까지 거슬러 recursively 추적했다(p.3, Table 1: cs.AI/cs.CV arXiv 논문 수가 2012-2017 사이 매년 급증).

### §2 Terminology and Algorithms (p.3-9)

Supervised learning의 정식화(loss function, SGD, Algorithm 1)와 minibatch SGD(Algorithm 2)를 정리한 뒤, 병렬 컴퓨터 아키텍처(single-machine vs multi-machine)와 통신 인프라 트렌드를 통계로 보여준다. 240편 중 147편이 하드웨어 세팅을 명시했고, GPU가 2013년 이후 지배적이 되었다(Fig 3a, p.7). 73편이 multi-node 분산학습을 사용했고, 그중 55편이 MPI를 통신 계층으로 사용해 2016년 이후 MPI가 사실상 표준이 되었음을 보인다(Fig 4b, p.8). 가장 중요한 이론적 도구는 **Work-Depth (W-D) 모델**이다: DAG의 총 작업량 W와 critical path 길이 D로 병렬성을 특징짓고, p개 프로세서에서의 실행시간은 `min{W/p, D} ≤ T_p ≤ O(W/p + D)`로 bound된다(p.8). Allreduce 통신을 LogP 모델(단순화된 α-β 모델: L=latency, G=per-byte cost)로 분석하며, 하한 `T_r ≥ Llog2(P) + γmG(P-1)/P`을 제시하고 tree/butterfly/pipeline/Rabenseifner's algorithm 등 구체 알고리즘의 성능을 비교한다(p.9).

> "Due to the relatively low bandwidth between the machines (compared to local memory bandwidths), this operation [allreduce] is often most critical for distributed learning." (§2.5, p.9)

### §3 The Efficiency Tradeoff: Generalization vs. Utilization (p.9-10)

Minibatch 크기가 너무 작으면(region A) concurrency를 활용하지 못하고, 너무 크면(region C) 정확도(generalization)가 나빠짐을 보인다(Fig 6a). Descent lemma를 이용해 큰 minibatch가 gradient variance를 늘려 수렴을 저해할 수 있음을 수식으로 증명한다(p.9). 이 tradeoff는 이후 §6.1(data parallelism의 minibatch 확장 한계)의 이론적 근거가 된다.

### §4 Deep Neural Networks (p.10-14)

Neuron, feed-forward operator(FC, convolution), pooling, batch normalization, recurrent operator(RNN, LSTM, GRU)의 수식적 정의와 각각의 Work-Depth 특성(Table 4, p.13)을 정리한다. 대부분의 operator는 W는 파라미터에 선형적으로 비례하지만 D는 로그 스케일(`O(log N)`, `O(log C_in)` 등)이라는 결과가 핵심이며, 이는 "DNN 계산은 본질적으로 매우 parallel-friendly하다"는 이 논문 전체 주장의 수학적 근거다. Backpropagation은 forward evaluation 후 chain rule로 `∇x`, `∇w`를 역순으로 계산하는 과정으로 설명된다(Fig 10, p.13).

### §5 Concurrency in Operators (p.14-18)

Convolution 계산의 4가지 방법(direct, im2col/GEMM, FFT, Winograd)을 Work-Depth로 비교한다(Table 6, p.17). FFT는 커널이 클수록 유리해 GEMM 대비 최대 16배 성능을 보이고(p.16), Winograd는 작은 커널(3×3)에 최적화된 현재 주류 방식이다. 데이터 레이아웃(NCHW vs CHWN) 변경만으로도 최대 27.9배 단일 operator 성능 향상, AlexNet 전체로는 5.6배 향상이 보고된다(Li et al., p.17). RNN 최적화로는 GEMM 연산 fusion을 통한 ~11배 성능 향상(Appleyard et al., p.17-18)과, 작은 minibatch·긴 시퀀스에 특화된 Persistent RNN이 최대 ~30배 speedup을 달성한다(p.18).

### §6 Concurrency in Networks (p.18-22)

세 가지 병렬화 전략을 구분한다(Fig 14, p.19): **Data parallelism**(minibatch를 여러 프로세서로 분할, allreduce로 gradient 취합), **Model parallelism**(레이어 내부 뉴런을 분할, 매 레이어마다 통신 필요), **Pipelining**(레이어를 depth 방향으로 분할). Data parallelism의 확장은 batch normalization의 동기화 요구가 병목이며(p.19), minibatch 크기를 8k(Goyal et al.), 32k(You et al.), 심지어 64k까지 늘려도 정확도 손실을 크게 겪지 않는 최근 연구들이 소개된다(p.19). Model parallelism은 FC layer에서 all-to-all 통신 비용이 크다는 한계가 있고(p.20), Locally Connected Networks(LCN)로 이를 완화한 사례가 5,000 CPU 노드 규모의 CNN을 3-노드 GPU 클러스터로 능가한 결과를 제시한다(Coates et al., p.21). Hybrid parallelism(DistBelief, Project Adam)은 data+model+pipeline을 결합해 4 GPU에서 최대 6.25배 speedup, 1% 미만 정확도 손실을 달성한다(p.22).

### §7 Concurrency in Training (p.22-32)

가장 방대한 섹션. **Model consistency**를 synchronous → stale-synchronous(SSP) → asynchronous(HOGWILD)의 스펙트럼으로 분류하고(Fig 20, 23), 각 방식의 수렴 보장 조건(atomic write, Lipschitz 연속성, bounded staleness)을 정리한다(p.23-24). **Centralization**은 parameter server(PS, 중앙집중) vs decentralized allreduce로 나뉘며, sharded/hierarchical PS(DistBelief, Rudra, Project Adam)로 병목을 완화하는 구조를 설명한다(Fig 21, p.25). **Compression**은 quantization(FP32→FP16→binary/ternary, stochastic rounding)과 sparsification(top-k gradient만 전송)으로 나뉘며, gradient sparsification은 최대 54배 speedup, 846-2871배 압축률을 보고한다(p.27-28). **Model consolidation**(ensemble learning, knowledge distillation, model averaging, EASGD)은 극단적으로 통신을 줄이는 방법이다(§7.4, p.28-30). 마지막으로 hyper-parameter search와 neural architecture search(NAS, SMBO, RL, evolutionary algorithms)도 병렬화 가능한 meta-optimization으로 다룬다(§7.5, p.30-32).

### §8 Concluding Remarks (p.32-33)

DNN의 거의 모든 단계가 병렬 가능하다는 core thesis를 재확인하며, 향후 방향으로 (1) whole-DNN 컴파일 최적화(TensorFlow XLA, TVM 등, 최대 4배 speedup), (2) elastic/cloud 기반 학습과 자동 architecture search의 결합, (3) multi-purpose network를 향한 AGI 지향 연구를 제시한다(p.33).

### Appendix A-C (p.39-47)

LeNet부터 DenseNet까지 5개 네트워크의 W/D를 직접 계산하며(예: LeNet W=665,832, D=41, p.39), Convolution의 4가지 계산법(Direct, im2col, FFT, Winograd)에 대한 완전한 알고리즘·복잡도 유도를 제공한다(Algorithm 3-6, p.43-47). 이는 본문 Table 6의 근거 자료다.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "concurrency and average parallelism using the Work-Depth model"
- "the efficiency tradeoff between generalization and utilization"
- "model consistency spectrum" (synchronous ↔ inconsistent)

**Technical concepts:**
- "Work-Depth (W-D) model" (W: work, D: depth/critical path)
- "data parallelism / model parallelism / pipelining / hybrid parallelism"
- "parameter server vs. decentralized allreduce"
- "staleness" / "stale-synchronous parallelism (SSP)"
- "gradient/parameter compression (quantization, sparsification)"

**Value language:**
- "computational intensity and memory demands of deep learning increase proportionally"
- "network communication remains generally slower than intra-machine communication"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "Demystifying ... An In-Depth Concurrency Analysis" (제목 그대로)
> - "The world of deep learning is brimming with concurrency" (저자 특유의 수사)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §2, p.3, Table 1 | cs.AI 논문 수 2012년 1,081건 → 2017년 2,790건 (cs.CV는 577→5,693건) | 딥러닝/ML 연구 규모의 폭발적 성장을 뒷받침하는 수치 |
| §2.3, p.7 | 리뷰 240편 중 147편이 하드웨어 세팅 명시, GPU가 2013년 이후 지배적 | 가속기 중심 컴퓨팅으로의 전환 시점 인용 |
| §2.3, p.7 | 73편이 multi-node 분산 세팅 사용, 2015년 이후 급증 | 분산 학습의 최근 필수화 근거 |
| §2.4, p.8 | Multi-node 논문 중 55편이 통신 계층 명시, 2016년 이후 MPI가 사실상 표준 | HPC 기법의 딥러닝 도입 시점 인용 |
| §5.3, p.16 | FFT 기반 convolution, 큰 커널에서 GEMM 대비 최대 16배 성능 | 알고리즘 선택이 성능에 미치는 영향 예시 |
| §5.3, p.17 | 데이터 레이아웃 변경(NCHW→CHWN)만으로 단일 operator 최대 27.9배, AlexNet 전체 5.6배 speedup | 메모리 접근 패턴의 성능 영향력 인용 |
| §5.4, p.18 | Persistent RNN, 작은 minibatch에서 최대 ~30배 speedup | 메모리 상주(register-resident) 최적화 사례 |
| §6.1.1, p.19 | Minibatch 크기를 8k/32k/64k까지 확장해도 정확도 큰 손실 없음 | Large-batch 학습의 스케일링 한계 인용 |
| §6.4, p.22 | Hybrid parallelism(data+model), 8 GPU에서 최대 6.25배 speedup, 1% 미만 정확도 손실 | 병렬화 전략 조합의 효과 |
| §7.3.2, p.27-28 | Gradient sparsification, 80노드 기준 최대 54배 speedup, 846-2871배 압축률(최대 1.8% 오차 증가) | 통신 압축의 효과 규모 인용 |
| §7.3.2, p.28 | SparCML, Ethernet 상에서 CNTK 대비 20배 이상 speedup | 통신 계층 최적화의 임팩트 |
| §2.5, p.9 | Allreduce 통신 시간 하한: `T_r ≥ Llog2(P) + γmG(P-1)/P` | 분산 시스템 통신 병목의 이론적 근거로 인용 가능 |

---

## 🎯 Strategic anchor

> "In multi-machine environments, these tables are distributed across the machines which participate in the overall reduction operation. Due to the relatively low bandwidth between the machines (compared to local memory bandwidths), this operation is often most critical for distributed learning." (§2.5, p.9)

→ **본인 활용**: 이 문장은 분산 딥러닝의 성능 병목이 "노드 간 대역폭이 로컬 메모리 대역폭보다 훨씬 낮다"는 사실 그 자체에서 발생함을 명시적으로 못 박는다. 면담·자소서에서 "왜 memory/interconnect 아키텍처(CXL 등)가 중요한가"를 설명할 때, 이 논문이 순수 알고리즘·시스템 관점에서도 결국 memory-bandwidth 격차를 병목으로 지목한다는 점을 인용해 "내 연구(memory-system architecture)가 이 병목을 하드웨어 레벨에서 직접 겨냥한다"는 논리로 연결할 수 있다.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | 단일 operator부터 분산 학습까지 concurrency 전반을 다루는 서베이 (2018년 시점) | Memory-system architecture — CXL, memory disaggregation, cache coherence, tiered memory |
| Mechanism | Allreduce/parameter server 등 통신-감소 알고리즘, LogP 모델 기반 통신 비용 분석(α-β를 상수로 취급) | Interconnect(CXL) 기반 memory pooling, 계층 간 coherence protocol — LogP의 α, β 자체를 하드웨어로 낮추는 접근 |
| Workload | CNN/RNN 중심 (2018년 이전 자료가 대부분); 원리는 LLM 시대에도 유효 | Memory-intensive workload 전반(HPC, LLM 포함) — memory bandwidth/capacity가 병목인 시스템 |
| Open space | 통신을 추상적 latency-bandwidth 모델로만 다룸; 메모리 계층·interconnect 기술 자체는 black box | CXL로 그 black box를 실제로 열어, memory-side에서 병목을 해소하려는 시도 |

이 논문은 "언제, 왜 통신이 병목이 되는가"를 알고리즘·시스템 관점에서 정량화하지만, 그 통신이 물리적으로 어떤 인터커넥트/메모리 계층을 통해 이뤄지는지는 다루지 않는다. 내 연구는 바로 그 지점 — CXL 같은 새로운 interconnect가 메모리 대역폭·용량·coherence의 물리적 한계를 어떻게 재정의하는가 — 를 파고드는 것으로, 이 논문과는 scope가 다르지만 이 논문이 제시하는 "통신이 분산 학습의 근본 제약"이라는 motivation을 그대로 물려받는 위치에 있다.

---

## Open questions / gaps

- [ ] 2018년 시점 서베이라 CXL(2019년 표준 발표) 자체를 전혀 다루지 않음 — memory disaggregation, pooled memory, multi-node cache coherence는 논의 범위 밖
- [ ] 통신 비용을 LogP의 α(latency), β(bandwidth) 상수로만 추상화하고, 실제 interconnect 하드웨어(topology, coherence protocol, near-memory processing)의 설계 여지는 다루지 않음
- [ ] GPU 메모리 용량 자체가 병목이 되는 상황(오늘날 LLM 학습에서의 memory-capacity wall)은 다루지 않음 — 이 논문의 하드웨어 트렌드 분석(Fig 3, 4)은 2017년까지만 커버
- [ ] Heterogeneous memory tier(HBM vs DRAM vs storage/CXL-attached memory) 간 데이터 배치 문제는 논의되지 않음 — tiered memory 연구가 채울 수 있는 공백
- [ ] Parameter server/allreduce 논의는 파라미터가 각 노드 로컬 메모리에 항상 들어간다고 암묵적으로 가정 — capacity-bound 분산 학습(메모리 부족 시나리오)은 스코프 밖

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [53] | Culler et al., "LogP: Towards a Realistic Model of Parallel Computation" (1993) | 이 논문 통신 비용 분석의 기반 모델. Interconnect latency/bandwidth tradeoff를 다루는 내 연구의 이론적 출발점으로 재확인 가치 |
| ☐ | [101] | Hoefler & Moor, "Energy, Memory, and Runtime Tradeoffs for Implementing Collective Communication Operations" (2014) | 제목 자체가 memory-communication tradeoff를 정면으로 다룸 — 직접적 관련 |
| ☐ | [198] | Rabenseifner, 최적화된 allreduce 알고리즘 (reduce-scatter + gather) | Bandwidth-optimal collective의 baseline. CXL 기반 collective 재설계 논의 시 비교 기준 |
| ☐ | [60] | Demmel & Dinh, convolution/pooling의 통신 하한(lower bounds) | Communication-avoiding algorithm 이론 — memory-bound 시스템 분석에 방법론적으로 참고 가능 |
| ☐ | [52] | Cui et al., "GeePS: Scalable Deep Learning on Distributed GPUs with a GPU-specialized Parameter Server" | CPU-GPU 메모리 관리 컴포넌트를 명시적으로 다룸 — memory-system 설계 관점에서 직접 관련 |
| ☐ | [107] | Hsieh et al., "Gaia: Geo-distributed Machine Learning Approaching LAN Speeds" | 지리적으로 분산된 노드 간 통신 병목을 다룸 — memory/interconnect disaggregation 논의와 구조적으로 유사 |
| ☐ | [206] | Renggli et al., "SparCML: High-Performance Sparse Communication for Machine Learning" | 통신 계층 최적화의 실제 시스템 구현 사례 |
| ☐ | [77] | Gholami et al., "Integrated Model, Batch, and Domain Parallelism in Training Neural Networks" | 1.5D 행렬곱 알고리즘 기반 통신 비용 분석 — communication-avoiding parallel algorithm 설계 방법론 참고 |

---

## Personal annotations

<자유 형식 메모 영역. 이후 본인이 채워 넣을 것.>
