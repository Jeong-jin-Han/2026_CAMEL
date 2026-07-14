# A Device-Side Execution Model for Multi-GPU Task Graphs

> **Source PDF**: [DeviceSideExecModel.pdf](DeviceSideExecModel.pdf)
> 🕸️ NodeGraph: [DeviceSideExecModel.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/Device-Side%20Execution%20Model%20-%20A%20Device-Side%20Execution%20Model%20for%20Multi-GPU%20Task%20Graphs/DeviceSideExecModel.html)
> **Authors**: Ilyas Turimbetov (Koç University), Mohamed Wahib (RIKEN Center for Computational Science), Didem Unat (Koç University)
> **Venue / Year**: ICS '25 (2025 International Conference on Supercomputing), Salt Lake City, UT, USA — June 08–11, 2025
> **arXiv / DOI**: https://doi.org/10.1145/3721145.3730426
> **Length**: 13 pages
> **Read status**: ☑ Full read (2026-07-13)
> **My reading purpose**: H3 (GPU 옆 FPGA 제어 평면 아이디어) 관련성 검증 — device-side 실행이 host 제어를 어디까지 대체하는지, multi-GPU 확장 방식이 "GPU당 독립 제어 평면" 가설과 얼마나 비슷한지 확인

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

이 논문은 **MUSTARD**라는 실행 모델을 제안한다. 목표는 여러 GPU(멀티노드 포함)에 걸친 **정적(static) task graph**를 실행할 때 host CPU를 "task graph 초기화" 한 번에만 관여시키고, 이후의 **의존성 추적(dependency tracking), 데이터 전송, occupancy 기반 부하 분산(load balancing), 커널 스케줄링/launch**를 전부 GPU 디바이스 측에서 처리하도록 만드는 것이다. 핵심 아이디어는 "graph enrichment" — 기존 CUDA Graph의 정점(vertex) 사이에 의존성 갱신(DU, dependency update), 의존성 대기(DW, dependency wait), occupancy 증감, 데이터 공유(SP, share pointer)를 담당하는 보조 정점들을 삽입해서, 런타임이 해야 할 일을 그래프 구조 자체에 인코딩하는 것이다. 멀티노드 통신은 NVSHMEM 원자적(atomic) 연산으로, 디바이스 측 커널 launch는 CUDA의 device-side graph launch 기능으로 구현했다. LU/Cholesky 분해와 cuBLAS/cuSOLVER를 예제로 사용해 StarPU, SLATE, 단일/멀티-GPU cuSOLVER, 단일-GPU CUDA Graph와 비교했고, 64-GPU 멀티노드 환경에서 SLATE 대비 평균 5.83배, 단일노드에서 최고 성능 baseline 대비 LU 평균 1.66배·Cholesky 평균 1.29배 속도향상을 보였다.

---

## Core thesis

> "MUSTARD, a multi-GPU execution model that shifts execution of static task graphs entirely to the devices, drastically reducing overhead." (Abstract, p.384)

추가 설명: 기존 멀티-GPU 런타임(StarPU, PaRSEC, Legion)이나 CUDA Graphs는 host가 커널 launch·의존성 추적·데이터 이동 결정 중 최소 하나는 계속 담당해야 했다. MUSTARD는 그래프 구조 자체를 "enrich"해서 이 모든 런타임 기능을 그래프 정점으로 흡수시키고, host는 그래프를 한 번 만들고 파티셔닝하는 초기화 단계에만 등장한다. 단, 이 모델은 명시적으로 **정적 그래프(static graph)**, 즉 그래프의 구조(어떤 태스크가 있고 어떤 의존성을 갖는지)가 실행 전에 고정되어 있는 경우로 범위를 한정한다.

---

## Why this matters to me

내 H3 가설은 "GPU 패키지 안에 작은 FPGA 제어 평면을 둬서 host가 하던 kernel 스케줄링/launch 결정을 GPU 옆에서 대신하게 하면, CUDA Graphs가 캡처하지 못하는 데이터-의존적 동적 제어(LLM decode의 EOS 판정 등)까지 host 왕복 없이 처리할 수 있지 않을까"라는 아이디어였다. 이 논문은 정확히 "host를 execution critical path에서 빼는" 문제를 다루지만, 저자들이 §2 서두에서 스스로 "This work focuses on static task graphs" (p.385)라고 범위를 명시한다. 즉 MUSTARD가 GPU 측에서 대신하는 것은 **정적으로 정해진 그래프의 스케줄링·부하분산**이지, **그래프 구조 자체가 런타임 값에 따라 바뀌는 동적 제어**가 아니다. 이건 H3가 노리던 빈틈(데이터-의존적 분기)이 이 논문의 범위 밖에 명시적으로 놓여 있다는 뜻이라, "이미 누가 풀었나?"라는 질문에 대해 "아니, 이 논문은 의도적으로 그 문제를 피해간다"는 정직한 답을 준다. 동시에 이 논문이 사용하는 메커니즘(그래프에 런타임 로직을 정점으로 삽입하는 "graph enrichment", NVSHMEM 기반 GPU 간 직접 통신, per-device 독립 스케줄러 커널)은 H3의 "GPU당 독립 제어 평면"이 소프트웨어로 이미 부분적으로 구현되어 있다면 어떤 모양일지 보여주는 참고점이 된다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.384-385 | MUSTARD 소개: host는 초기화만, 정적 그래프의 실행은 전부 GPU 측. 기여 6가지 나열 |
| 2 | Motivation and Related Work | p.385-387 | 정적 그래프로 범위 한정을 명시. CPU-managed 런타임(StarPU/PaRSEC/Legion), CUDA Graphs, persistent megakernel 세 계열을 비교 (Table 1) |
| 3 | Mustard | p.387-391 | 핵심 설계: graph enrichment, 정적 스케줄링(§3.1)과 동적 스케줄링(§3.2), device-side kernel launch, NVSHMEM 데이터 전송, occupancy 추적, GPU-side 큐/스케줄러(Algorithm 2) |
| 4 | Limitations | p.391 | 120-subgraph 상한, 메모리 상한, "멀티노드 실행은 동적 부하분산 없이만 가능"이라는 핵심 제약 |
| 5 | Evaluation | p.391-395 | 오버헤드 실험(Table 2, Fig.5), LU/Cholesky 단일노드(Table 3-4, Fig.6-7), 멀티노드(Table 5, Fig.8) |
| 6 | Conclusion | p.395 | host 개입 없이 부하분산·통신 관리를 하는 멀티-GPU task graph 실행 모델. 향후 AMD ROCm 지원, 더 많은 그래프 응용 예고 |

---

## Section notes

### §1 Introduction (p.384-385)

MUSTARD는 "the first device-side multi-GPU execution model for multi-node systems"(p.385)라고 스스로를 규정한다. Figure 1(p.385)이 핵심 motivating example: 같은 8-태스크 그래프를 (1) 오버헤드가 큰 런타임 시스템, (2) 프로그래머가 수동으로 디바이스를 배정한 CUDA Graph, (3) MUSTARD의 세 가지 방식으로 3-GPU에 스케줄링했을 때, 런타임 시스템은 오버헤드 때문에, CUDA Graph는 프로그래머가 태스크 길이를 몰라 Task 7을 GPU0(이미 바쁜 디바이스)에 배정해서 손해를 보고, MUSTARD만 온라인 스케줄링으로 Task 7을 유휴 GPU2에 배정해 총 실행시간을 8~9 timestep에서 6~7 timestep으로 줄인다. 기여는 (i) 최초의 device-side 멀티노드 실행모델, (ii) host 개입 없는 저오버헤드 동적 스케줄링·부하분산, (iii) 성능 모델/휴리스틱/커스텀 API 불필요, (iv) LU·Cholesky 분해 데모(cuBLAS/cuSOLVER 통합), (v) StarPU·SLATE·cuSOLVER·단일GPU CUDA Graph 대비 성능평가.

### §2 Motivation and Related Work (p.385-387)

> "This work focuses on static task graphs. Although dynamic graphs offer more flexibility by allowing changes in graph structure, most scientific, linear algebra and graph analytics applications can be expressed in form of a static graph." (§2, p.385)

이 문장이 논문 전체의 스코프를 정의한다. Table 1(p.386)이 StarPU, PaRSEC, Legion, OpenMP(SLATE), CUDA Graphs, partitioned CUDA Graphs, Juggler(megakernel), MUSTARD를 "Dynamic graphs" 열을 포함해 비교하는데, MUSTARD는 이 열에서 ✗(미지원)이고 "Static graphs" 열에서만 ✓다. §2.1은 CPU-managed 런타임(StarPU/PaRSEC/Legion)이 work-stealing 등으로 부하분산은 잘 하지만 커널 launch·런타임 컴포넌트 오버헤드가 크다는 점(StarPU가 GPU 전용 실행 대비 최대 13배 느림, §5.1.2 인용)을 지적한다. §2.2는 CUDA Graphs를 설명하며 "load balancing and management of inter-GPU communications between multiple devices remains contingent on the programmer's decisions and is only possible within a single node"(p.386) — 즉 CUDA Graphs 자체가 멀티노드를 지원하지 않고 부하분산도 프로그래머 책임이라는 한계를 명시한다. §2.3은 persistent megakernel 계열(Juggler 등)을 다루며, 이들은 device-side 실행을 하지만 CPU-free 특성상 register pressure·thread divergence·하드웨어 스케줄러 미지원 문제가 있고, 멀티-GPU megakernel 구현은 저자들이 아는 한 존재하지 않는다(p.387)고 밝힌다.

### §3 Mustard (p.387-391)

핵심 메커니즘은 **graph enrichment**: CUDA Graph의 원래 정점(계산 커널) 사이에 런타임 기능을 담당하는 보조 정점을 삽입한다.
- §3.1(정적 스케줄링)에서는 의존성 갱신(DU)/대기(DW) 정점이 NVSHMEM atomic으로 원격 디바이스의 의존성 카운터를 감소시키고, 데이터 전송(DT) 정점이 소스/목적지가 사전에 알려진 상태에서 cudaMemcpy(노드 내) 또는 NVSHMEM(노드 간)으로 전송을 수행한다(Figure 2, p.387). Figure 3(p.389)은 3×3 타일 LU 분해를 2개 컴퓨트 노드(각 노드 GPU 1개)에 round-robin column-wise로 파티셔닝한 뒤 DU/DW 정점이 삽입되는 구체적 예를 보여준다.
- §3.2(동적 스케줄링)는 태스크를 subgraph(A~E) 단위로 묶고, 각 subgraph 실행 전후에 occupancy 증가(⊕)/감소(⊖) 정점을 붙이며(Figure 4, p.389), 디바이스별 공유 큐(Broker queue를 NVSHMEM atomic으로 재구현)에서 dequeue한 subgraph를 device-side CUDA Graph launch로 실행한다(Algorithm 2, p.391). Occupancy 추적은 A100 기준 "108 blocks of 1024 threads"(p.390)라는 디바이스 점유 한도를 기준으로 부하분산에 사용된다.
- Device-side kernel launch는 dynamic parallelism(오버헤드 큼, divide-and-conquer 전용, 한때 deprecated) 대신 CUDA의 device-side graph launch 기능을 사용하며, 이 기능은 **디바이스당 최대 120개 subgraph**라는 하드웨어/CUDA 제약을 갖는다(p.389-390).

> "MUSTARD aims to address the current limitations of multi-GPU scheduling of static graphs by introducing an inter-node execution mechanism and demonstrate the viability of moving runtime components to the device." (§3, p.387)

### §4 Limitations (p.391)

가장 중요한 제약: **"In MUSTARD, multi-node execution is only possible without dynamic load balancing."** (p.391) — 즉 멀티노드 시나리오에서는 정적 파티셔닝만 가능하고, 노드 간 동적 부하분산은 지원하지 않는다. 동적 부하분산은 단일 GPU 메모리에 데이터가 들어가는 단일노드 상황에서만 동작한다. 그 외 120-subgraph 상한(직렬/강하게 연결된 정점을 병합해 우회 가능하지만 병렬성 손실), cudaGraph가 수백만 정점 규모에서 디바이스 메모리를 다 차지할 수 있다는 메모리 제약, cuSOLVER/cuBLAS가 스레드/블록 수를 자동 결정해 occupancy 추적 정밀도가 제한된다는 점도 명시한다.

### §5 Evaluation (p.391-395)

A100 8장 × 노드, EPYC 7742 듀얼소켓, NVLink 3.0, 최대 8노드(64 GPU) 환경. 오버헤드 실험(Table 2, p.392)에서 무연산 랜덤 그래프 기준 MUSTARD는 멀티-GPU CUDA Graph 대비 오버헤드를 2^8 정점에서 50.48배, 2^14 정점에서도 1.69배 줄였다. Figure 5a(p.392)는 4-GPU에서 StarPU가 16.95배 느려지는 반면 MUSTARD는 6.17배로, StarPU 대비 4.25배 빠르다는 것을 보여준다. LU/Cholesky 실험(Table 3-4, Figure 6-7, p.393-394)은 단일노드 2/4/8-GPU에서 MUSTARD가 StarPU·SLATE를 항상 능가하고 cuSOLVER(cuSmG/cuSmGF)와 대등하거나 우세함을 보인다. 멀티노드 실험(Table 5, Figure 8, p.394-395)에서는 최대 8노드(64 GPU), 120,000×120,000 행렬에서 MUSTARD 461.6 TFLOPS vs SLATE 55.7 vs StarPU 6.1 TFLOPS — SLATE 대비 2.81~8.28배, StarPU 대비 4.75~75.2배 속도향상. 논문은 "MUSTARD achieves efficiency close to theoretical peak of the hardware"(§6, p.395)라고 결론짓는다.

### §6 Conclusion (p.395)

> "MUSTARD is designed for executing task graphs on multi-GPU systems, prioritizing load balancing and communication management without relying on host CPU control." (§6, p.395)

향후 방향으로 AMD GPU(ROCm)에서의 MUSTARD 실증과 더 많은 응용 사례의 task graph 적용을 언급한다. 동적 그래프 지원 확장은 future work로 명시적으로 언급되지 않는다 — 이 논문은 끝까지 정적 그래프 스코프를 유지한다.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "shifts execution of static task graphs entirely to the devices" (host를 execution critical path에서 제거)
- "the CPU is used solely for task graph initialization"
- "removing the CPU from the critical path of the task graph scheduling and execution"

**Technical concepts:**
- "graph enrichment" (런타임 기능을 그래프 정점으로 인코딩)
- "dependency update (DU) / dependency wait (DW) vertex"
- "device-side CUDA Graph launch" (부모 커널에서 이미 초기화된 cudaGraph를 device 측에서 launch)
- "occupancy tracking" (블록/스레드 점유량 기반 부하분산)
- "subgraph" (device-side launch 120개 상한을 우회하기 위해 병합된 스케줄링 단위)

**Value language:**
- "reaches performance close to the hardware's theoretical peak even on smaller matrices"
- "without requiring modifications to GPU kernel code or the adoption of new runtime mechanisms or APIs"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "MUSTARD" (제품/시스템명 그대로 echo하면 이 논문을 표절하는 것처럼 보임)
> - "graph enrichment" (이 논문이 만든 정확한 조어이므로 내 아이디어 설명에 그대로 쓰면 출처 혼동 위험 — 인용할 땐 명시적으로 "MUSTARD의 graph enrichment 기법"이라고 명시)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.384 | 64-GPU 멀티노드에서 SLATE 대비 평균 **5.83배** 속도향상 | 멀티-GPU/노드 device-side 실행의 스케일링 효과 인용 시 |
| Abstract, p.384 | 단일노드 최고 baseline 대비 LU 평균 **1.66배**, Cholesky 평균 **1.29배** | 단일노드에서도 host-free 스케줄링의 이득이 있음을 보일 때 |
| §5.1.2, p.392 | 4-GPU에서 StarPU **16.95배** 슬로우다운 vs MUSTARD **6.17배** (StarPU 대비 **4.25배** 빠름) | CPU-managed 런타임의 오버헤드가 여전히 크다는 motivation 인용 |
| Table 2, p.392 | 랜덤 그래프 오버헤드: 2^8 정점에서 멀티-GPU CUDA Graph 대비 **50.48배** 개선, 2^14 정점에서 **1.69배** | 그래프가 커질수록 device-side 스케줄링 이득이 줄어든다는 (그래프 크기 의존성) 근거 |
| §4, p.391 | Device-side CUDA Graph launch는 디바이스당 **최대 120개 subgraph**로 제한 | H3 설계에서 device-side launch 메커니즘 자체의 하드웨어적 한계를 언급할 때 |
| §4, p.391 | "multi-node execution is only possible **without dynamic load balancing**" | 멀티노드 확장의 근본적 제약 — 노드 간 동적 의사결정은 여전히 미해결임을 인용 |
| §5.4, p.395 | 8노드(64 GPU), 120,000×120,000 행렬에서 MUSTARD **461.6 TFLOPS** vs SLATE 55.7 vs StarPU 6.1 TFLOPS | 대규모 멀티노드 스케일링 수치 |

---

## 🎯 Strategic anchor

> "This work focuses on static task graphs. Although dynamic graphs offer more flexibility by allowing changes in graph structure, most scientific, linear algebra and graph analytics applications can be expressed in form of a static graph." (§2, p.385)

→ **본인 활용**: 면담에서 "MUSTARD(ICS'25)는 host를 execution critical path에서 빼는 문제를 이미 풀었지만, §2에서 스스로 '정적 그래프만 다룬다'고 스코프를 한정합니다. 제 H3는 정확히 그 바깥 — 그래프 구조 자체가 런타임 데이터(EOS 판정, continuous batching, spec-decode accept/reject)에 따라 바뀌는 동적 제어를 대상으로 합니다. 즉 이 논문은 제 문제가 아직 안 풀렸다는 것을 오히려 명시적으로 확인해 줍니다"라는 방식으로 사용 가능. 또한 §4의 "multi-node execution is only possible without dynamic load balancing"(p.391)은 소프트웨어만으로 device-side 런타임을 확장할 때 노드 간 동적 의사결정이 왜 어려운지(NVSHMEM atomic 오버헤드, locality 부재, 120-subgraph 상한)를 보여주는 근거로, FPGA 기반 제어 평면이 통신 지연/원자적 갱신 오버헤드를 하드웨어로 흡수해 이 제약을 완화할 여지가 있다는 논리로 이어갈 수 있음.

---

## Connection to my research direction

| 차원 | 이 paper (MUSTARD) | 본인 방향 (H3) |
|---|---|---|
| Scope | **정적(static)** task graph — 그래프 구조는 실행 전 고정, 실행 순서/디바이스 배정만 런타임에 동적 결정 | **데이터-의존적 동적 제어** — 그래프 구조/다음 커널 자체가 런타임 값(EOS, batching 상태, accept/reject)에 의해 결정됨 |
| Mechanism | 소프트웨어: 그래프 정점에 런타임 로직을 "enrich"해 삽입 → GPU 자신의 SM/스레드가 스케줄러 커널로 실행 (device-side CUDA Graph launch, NVSHMEM atomic) | 하드웨어: GPU 패키지 내 별도 FPGA가 제어 로직을 전담 — GPU의 SM 자원을 스케줄링에 소모하지 않음 |
| Workload | 밀집 선형대수(LU, Cholesky 분해) — 태스크 종류·개수가 고정된 HPC 커널 | LLM 추론 decode 등 — 매 스텝마다 다음 동작이 데이터에 좌우되는 워크로드 |
| Host 대체 정도 | Host는 그래프 **초기화**(파티셔닝, cudaSetDevice, 최초 그래프 구성) 1회만 담당, 이후 실행 내내 배제 | (H3 목표) host는 그래프 초기화도 필요 없이, 매 스텝의 분기 결정까지 GPU 옆 제어 평면이 담당 |
| Multi-GPU 확장 | Host가 정적으로 파티셔닝 → 이후 **GPU마다 자신만의 스케줄러 커널**이 자기 큐를 dequeue하며 독립 실행, GPU 간 통신은 NVSHMEM으로 직접 (D2D) | (H3 가설) **GPU마다 독립 FPGA 제어 평면** 1개 — host의 중앙집중 없이 분산 제어라는 지향점은 유사하나, MUSTARD는 "GPU 컴퓨팅 자원 안에서" 소프트웨어 스케줄러를 돌리는 반면 H3는 "GPU 컴퓨팅 자원 밖의 전용 하드웨어"를 쓰자는 제안 |
| Open space | 노드 간 **동적** 부하분산 미지원(§4), 120-subgraph 상한, 그래프 구조 변경 불가 | 이 지점들이 H3가 채울 수 있는 후보 — 단, "GPU당 FPGA 제어 평면"이 실제로 이 제약들을 어떻게 완화하는지는 아직 미검증(가설 단계) |

MUSTARD는 내 H3가 겨냥한 "host가 매 결정마다 개입해야 하는 문제"를 이미 상당 부분 소프트웨어로 풀었지만, 딱 **정적 그래프**라는 경계선 안에서만이다. 흥미로운 점은 MUSTARD의 multi-GPU 확장 방식이 이미 "GPU마다 독립적인 제어 루프(스케줄러 커널)가 자기 큐를 처리하고, 중앙 host 병목 없이 NVSHMEM으로 GPU 간 직접 통신한다"는 점에서 H3가 상상한 "GPU당 독립 제어 평면"과 **거시적 구조는 유사**하다는 것이다. 다만 결정적 차이는 그 제어 루프가 GPU 자신의 SM/스레드 자원을 점유하는 **소프트웨어 커널**이라는 점 — H3는 이를 GPU 컴퓨팅 자원과 분리된 **전용 하드웨어(FPGA)**로 옮기자는 제안이므로, (1) 스케줄링이 GPU 컴퓨팅 처리량을 잠식하지 않고, (2) 그래프 구조 자체가 바뀌는 동적 분기까지 다룰 수 있어야 한다는 두 가지 지점에서 MUSTARD보다 더 나아가려는 시도다. 이 두 지점 모두 MUSTARD 논문이 스스로 미해결로 남긴 영역(§2 스코프 제한, §4 멀티노드 동적 부하분산 제약)과 정확히 겹친다는 점에서, MUSTARD는 H3의 경쟁작이 아니라 오히려 H3가 정직하게 서 있는 위치를 확인해 주는 참고점에 가깝다.

---

## Open questions / gaps

- [ ] MUSTARD가 정적 그래프로 스코프를 한정한 것은 설계 선택인가, 아니면 device-side graph launch(120-subgraph 상한 등) 자체가 동적 그래프 구조 변경을 근본적으로 지원하지 않기 때문인가? — CUDA 플랫폼 제약과 설계 선택의 경계가 §2/§4에서 명확히 구분되지 않음
- [ ] "멀티노드에서는 동적 부하분산 불가"(§4)의 근본 원인이 NVSHMEM atomic 지연 때문인지, 아니면 큐가 단일 디바이스에 상주해야 한다는 설계(§3.2.5)의 구조적 한계인지 — 이 지점이 H3(하드웨어 제어 평면)가 실제로 개선할 수 있는 지점인지 검증 필요
- [ ] 스케줄러 커널이 "a single GPU thread"만 점유한다고 언급하지만(§3.2.4), 전체 device-side 스케줄링·의존성 추적 인프라(DU/DW/SP/RD 정점들)가 실제 컴퓨팅 처리량에서 차지하는 비중(SM 점유율, register pressure)이 정량적으로 보고되지 않음 — H3의 "GPU 컴퓨팅 자원을 잠식하지 않는다"는 차별점을 주장하려면 이 수치가 필요
- [ ] 데이터-의존적 동적 제어(다음 커널이 이전 커널의 출력값에 좌우되는 경우)를 이 프레임워크가 얼마나 확장 가능한지에 대한 논의가 전혀 없음 — 저자들도 미래 과제로 명시하지 않음(§6)

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [7] | Juggler: A Dependence-Aware Task-Based Execution Framework for GPUs (Ben-Nun et al., IPDPS 2018) | 유일하게 존재하는 멀티-GPU가 아닌 persistent megakernel 기반 device-side 실행 사례 — H3의 "GPU 자원 내부 vs 외부 제어" 비교 기준점 |
| ☐ | [26] | The Broker Queue: A Fast, Linearizable FIFO Queue for Fine-Granular Work Distribution on the GPU (Kerbl et al., ICS 2018) | MUSTARD의 device-side 큐 구현 기반. GPU 측 동시성 큐 설계의 기본기 |
| ☐ | [40] | Ouroboros: Virtualized Queues for Dynamic Memory Management on GPUs (Winter et al., ICS 2020) | MUSTARD가 채택한 device-side 동적 메모리 할당 기법 — 동적 스케줄링 시나리오의 memory management 참고 |
| ☐ | [13] | ChARMinG: A Scalable GPU-resident Runtime System (Choi et al., IPDPS 2021) | "완전히 device-side로 이동한" 유일한 선행연구로 언급됨(§3.2) — MUSTARD와 직접 경쟁하는 접근 |
| ☐ | [38] | The Landscape of GPU-Centric Communication (Unat et al., arXiv:2409.09874, 2024) | 저자 그룹(Didem Unat)의 별도 서베이 — GPU-initiated 통신/제어 전반의 최신 지형도, H3 motivation 문헌으로 유용 |
| ☐ | [35] | A Specialized Concurrent Queue for Scheduling Irregular Workloads on GPUs (Troendle et al., ICPP 2019) | occupancy 기반 동적 스케줄링과 관련된 GPU 큐 설계 대안 |
| ☐ | [12] | Scalable Irregular Parallelism with GPUs: Getting CPUs Out of the Way (Chen et al., SC'22) | "host를 배제한다"는 동일한 모토를 가진 별도 계열 연구 — 비교 대상으로 유용 |

---

## Personal annotations

<자유 형식 메모 — user가 직접 추가하는 영역>
