---
title: "Bidaw: Enhancing Key-Value Caching for Interactive LLM Serving via Bidirectional Computation-Storage Awareness"
aliases: [Bidaw]
description: "compute engine과 two-tier storage(host memory+SSD) 간 bidirectional awareness로 interactive LLM serving의 KV caching 효율을 높여 응답 지연 최대 3.58x 단축·throughput 최대 1.83x 개선"
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/llm
  - topic/kv-cache
  - topic/llm-serving
  - venue/fast
  - year/2026
---

# Bidaw: Enhancing Key-Value Caching for Interactive LLM Serving via Bidirectional Computation-Storage Awareness

> **FAST 2026** · `cluster/llm` · Source: [Bidaw - Enhancing Key-Value Caching for Interactive LLM Serving via Bidirectional Computation-Storage Awareness.pdf](<Bidaw - Enhancing Key-Value Caching for Interactive LLM Serving via Bidirectional Computation-Storage Awareness.pdf>)

**저자**: Shipeng Hu, Guangyan Zhang (교신, Tsinghua University), Yuqi Zhou (China University of Geosciences Beijing), Yaya Wei, Ziyan Zhong (China Telecom Omni-channel Operation Center), Jike Chen (Tsinghua University)

## TL;DR
Interactive LLM serving에서 multi-round 대화의 history KV tensor를 host memory + SSD의 two-tier storage에 캐싱할 때, 기존 방식은 compute engine과 storage가 서로의 사정을 모르고(mutually unaware) 동작해 KV loading이 병목이 된다. Bidaw는 (1) compute engine이 KV의 storage layer·크기를 인지해 request를 scheduling하고(I/O-aware scheduling), (2) storage가 compute engine이 만든 model answer 길이로 미래 access timing을 예측해 eviction한다(previous-answer-based eviction)는 **양방향 인지(bidirectional awareness)** 로, 응답 지연을 최대 3.58x 줄이고 throughput을 최대 1.83x 높여 모든 KV가 host memory에 있는 이상적 상한에 근접한다.

## 문제 & 동기
Interactive LLM serving(virtual companion, language learning roleplay, 고객 상담 등)에서는 사용자와 LLM이 여러 round를 주고받는다. 각 round의 computation은 이전 round들의 KV tensor를 필요로 하는데, GPU 메모리가 제한적이라 KV는 host memory(performance layer)와 SSD(capacity layer)로 이뤄진 two-tier storage에 캐싱된다. 재계산을 피하려면 모든 round의 KV를 캐싱·재로딩해야 하지만, two-tier storage에서 KV를 로딩하는 과정이 critical path에 놓여 기존 방식 대비 응답 지연이 최대 3.8x 높고 throughput이 최대 2.0x 낮다. 근본 원인은 compute engine과 two-tier storage가 **상호 무인지(mutually unaware)** 라는 점이다: (1) compute engine이 KV loading latency를 무시하고 scheduling해 I/O-induced request blocking이 발생하고, (2) storage가 자신의 과거 KV access(queuing) 정보만 쓰고 compute engine의 대화 패턴을 무시해 performance layer hit rate가 낮다(약 20%).

> [!quote]- 📄 원문 표현 (paper)
> "The root cause of such a performance gap is that in existing works, the compute engine and the two-tier storage are mutually unaware." (p.3)
>
> "loading KVs from two-tier storage in existing approaches increases serving latency by up to 3.8× and decreases throughput by up to 2.0× compared to an ideal large-memory setting on our interactive conversation workload." (Abstract, p.1)
>
> "Our interactive conversation workload reveals that there are an average of 22.4 conversation rounds with each user, and above redundant computation accounts for as high as 93.1% of the total computational workload." (p.2)

## 핵심 통찰

> [!note]- 통찰 1 — Bidirectional awareness가 핵심
> 기존 two-tier KV caching(CachedAttention, FlashGen)은 compute engine과 storage가 단방향 또는 무인지로 동작한다. Bidaw의 핵심은 양방향이다: compute→storage로는 KV의 위치/크기를 scheduling에 활용, storage←compute로는 model answer 길이를 eviction에 활용. 이 결합이 I/O blocking과 low hit rate를 동시에 해소한다.

> [!note]- 통찰 2 — Weighted reuse distance가 model answer 길이와 양의 상관
> 다음 KV access까지의 weighted reuse distance(연속 access 사이에 접근된 다른 unique KV들의 총 크기)는 이전 round에서 compute engine이 생성한 **model answer 길이와 양의 상관**을 갖는다(Spearman 0.94~0.98, p.7). 긴 답변일수록 사용자가 읽고 다음 질문을 만드는 데 시간이 걸려 다음 access가 늦어지기 때문. 이 인지를 storage가 미리 알 수 없으므로, compute engine이 storage에 알려줘야 한다.

> [!note]- 통찰 3 — Weighted reuse distance가 커도 hit 가능성은 남는다
> reuse distance가 performance layer 용량을 넘어도(promising reuse distance 구간) optimal(Belady) eviction의 hit rate는 0보다 크다. 따라서 단순히 reuse distance가 큰 KV를 무조건 evict하면 손해. ghost cache로 promising 구간을 fine-grained bucket으로 나눠 hit potential을 추정해야 한다.

> [!note]- 통찰 4 — Intermediate tensor 캐싱의 space-compute trade-off
> History 재사용에 필요한 것은 KV tensor지만, GPU computation 중 생성되는 여러 intermediate tensor들도 KV로 변환 가능하며 크기가 다르다. tensor 6(normalized activation)은 KV tensor 대비 cost efficiency(saved compute/space)가 가장 높아(51.0 vs 30.5, p.9) storage footprint를 줄이면서 더 많은 history를 host memory에 담을 수 있다. 단 MHA 기반 모델에서만 유효하고 GQA는 KV 캐싱이 낫다.

## 설계 / 메커니즘

> [!abstract]- 시스템 개요 (Figure 9, p.5)
> Bidaw = Compute Engine(Scheduler + GPU + History Cacher) ↔ Two-tier Storage(Performance Layer host memory + Capacity Layer SSD + Eviction Manager). 빨간 점선이 bidirectional awareness 경계.
> Workflow: (1) request 도착 시 KV 위치·크기 기반 scheduling, capacity layer의 KV는 performance layer로 먼저 load 후 GPU로. (2) computation 중 storage-efficient tensor를 storage에 caching. (3) computation 완료 후 model answer가 eviction manager에 전달. (4) performance layer free space가 임계치 이하면 eviction 트리거, model answer 기반으로 capacity layer로 evict (inclusive caching이라 복사본 유지해 write traffic 회피).

> [!abstract]- 메커니즘 1 — I/O-aware Request Scheduling (§3.2, p.5-6)
> **Dual queues**: KV가 performance layer에 있으면 "ready queue", capacity layer에 있으면 "preparing queue"로 분리. GPU 메모리 여유 시 ready queue만 GPU scheduling 대상. preparing queue request는 KV가 performance layer로 옮겨지면 ready queue로 promote. → 느린 capacity layer I/O가 후속 request를 block하지 않게 함.
> **Hybrid scheduling**: ready queue는 FCFS(promotion time이 아닌 원래 도착 순으로 삽입해 tail latency 방지, ready queue 선두가 GPU에 안 맞으면 skip). preparing queue는 disk-HRRN(Highest Response Ratio Next 변형): Response ratio = 1 + (Request waiting time / KV size). 작은 KV를 우선 처리하되 대기 시간으로 큰 KV의 starvation 방지.

> [!abstract]- 메커니즘 2 — Previous-answer-based Eviction (§3.3, p.6-8)
> **Step 1 (예측)**: 각 user의 최신 model answer 길이를 추적해 다음 KV access의 weighted reuse distance lower bound를 추정(통찰 2).
> **Step 2 (hit potential 추정)**: reuse distance를 small bucket(hit rate 1.0) / m개 promising bucket(ghost cache로 hit_promising(i) 추정) / extreme bucket(hit rate 0.0)으로 분할. ghost cache는 optimal(Belady) eviction을 미래 trace에 적용해 background로 운영. user별 과거 access 분포로 각 bucket 확률(prob_small, prob_promising(i), prob_extreme) 추정 후, lower bound로 불가능 bucket을 0으로 pruning·정규화.
> **Step 3 (eviction)**: Overall_potential = prob_small × 1.0 + prob_extreme × 0.0 + Σ prob_promising(i) × hit_promising(i) (Eq.2). hit potential이 가장 낮은 KV를 evict.

> [!abstract]- 메커니즘 3 — 구현 최적화 (§4, p.9)
> **Mix-grained GPU blocks**: PagedAttention의 작은 비연속 block은 CPU-GPU 대역폭을 못 살림. big block(256 tokens) + small block(16 tokens) 혼합. 길이가 알려진 history/query token은 big block, 길이 미지의 response token은 small block. big↔small 상호 분할·병합으로 fragmentation 방지.
> **Storage-efficient tensor caching**: KV tensor 대신 cost efficiency 최고인 tensor 6(storage-efficient tensor) 캐싱. 로딩 시 idle SM(30%+ idle, inference는 memory-bound)에서 low-priority CUDA stream으로 KV tensor 변환. MHA 모델(Llama, Qwen, Bloom, OPT, Baichuan)에 일반화, GQA는 예외.
> **Continuous batching** 채택, waiting request 데이터는 evict하지 않음.

## 평가

> [!success]- 실험 환경 & 워크로드 (p.10)
> - 서버: 1× 80GB A800 GPU, 200GB host memory, SSD 1.5GB/s(4× SATA SSD RAID-5), PCIe Gen 4.
> - 모델: OPT-6.7B, Qwen-7B, OPT-13B, Qwen-14B, OPT-30B.
> - 워크로드: 자체 interactive conversation workload(100만+ round, avg query 36 tokens, avg response 45 tokens, avg/median/P90 round = 22/18/45, avg conversation duration 길고 timestamp·user 정보 포함) + 공개 ShareGPT(avg 5.7 rounds, Poisson timestamp 시뮬). warm-up으로 performance layer를 history로 채움.
> - 비교: vLLM, CachedAttention, FlashGen, Optimal(모든 KV가 performance layer에서 로드되는 이상적 상한).

> [!success]- 종합 성능 (Figure 15, p.10-11)
> - OPT-13B에서 응답 지연 최대 **3.58x 단축**, 즉 SOTA 대비 최대 **83.9% 감소**.
> - throughput 최대 **1.83x 개선**(SOTA 대비 1.43~1.83x 더 높은 user arrival rate 지원, 유사 지연 하).
> - Optimal(이상적 상한)에 근접. Lossless(정확도 무손상) — request reorder만 하고 개별 request 내 computation 순서는 불변.
> - SSD 5GB/s로 올려도 baseline(FlashGen)은 15.18→20.23 users/min, Bidaw는 27.81→30.35 users/min로 여전히 우위.

> [!success]- Host memory 민감도 & ShareGPT (Figure 16/17, p.11)
> - host memory 120~200GB(GPU 메모리의 1.5~2.5x) 전 구간에서 Bidaw가 SOTA보다 우수, **1.75~2.19x** 더 높은 user arrival rate 지원.
> - ShareGPT(공개): throughput 1.40x 더 높은 user arrival rate, vLLM/CachedAttention/FlashGen 대비 응답 지연 69.8%/65.8%/56.9% 감소. (단 Poisson 시뮬 timestamp라 previous-answer eviction 효과는 줄어듦.)

> [!success]- Eviction · queuing · overhead · tail (Figure 18-22, p.11-12)
> - Performance layer miss rate: previous-answer-based가 queue-enhanced 대비 최대 **57.6%**, 일반 전략(LFU/LRU/FIFO) 대비 **69.9%** 감소(OPT-13B, 25 users/min).
> - Request queuing time: I/O-aware scheduler가 FCFS 대비 평균 **57.5%** 감소(2.45s vs 5.76s).
> - Overhead: scheduling 평균 0.62ms/op, eviction 0.35ms/op(optimal은 2.86ms, background), storage-efficient→KV 변환 수십 ms(응답 지연 대비 무시 가능).
> - Ablation(Figure 21): I/O-aware scheduling +1.58x 지연 개선, +previous-answer eviction throughput +1.25x, +storage-efficient tensor caching +1.10x.
> - Tail latency(OPT-30B, Figure 22): CachedAttention 대비 P90 52.96%, P95 49.30%, P99 47.03% 감소; FlashGen 대비 P90 66.63%, P95 62.64%, P99 56.81% 감소.

## 섹션 노트
- **§1 Introduction**: 문제 정의, redundant computation 93.1%, two-tier storage 채택 동기(RDMA memory pool은 hardware 비용 높음, vertical domain 기업은 local 배포 선호).
- **§2 Background & Motivation**: §2.1 two-tier KV caching, KV write는 critical path 아님. §2.2 million-round real workload 특성화 — Observation 1(KV가 오래 상주, 동시 캐싱 KV 대량, performance layer의 최대 3.91x), Observation 2(poor temporal locality, 80% access가 200GB 초과), Observation 3(bandwidth gap·KV 크기 차이로 loading time variation 큼, CoV 90%+). §2.3 root cause = mutually unaware.
- **§3 Design**: §3.1 system overview, §3.2 I/O-aware scheduling, §3.3 previous-answer-based eviction(§3.3.1 예측, §3.3.2 hit potential, §3.3.3 overall strategy).
- **§4 Implementation & Optimization**: mix-grained GPU blocks, storage-efficient tensor caching, continuous batching.
- **§5 Evaluation**: §5.1 overall, §5.2 memory sensitivity, §5.3 ShareGPT, §5.4 miss rate, §5.5 queuing, §5.6 overhead, §5.7 ablation, §5.8 tail.
- **§6 Related Work**: general LLM serving, lossy KV compression(orthogonal), KV caching(CachedAttention/FlashGen, HCache는 intermediate activation buffer, MeanCache는 user 간 reuse 대상 — Bidaw는 same-user multi-round 대상).

## 핵심 용어
- **KV (key-value tensor)**: attention의 key/value 텐서. history round의 KV를 재사용하면 재계산을 피할 수 있음.
- **Two-tier storage**: performance layer(host memory, 빠름/소용량) + capacity layer(SSD, 느림/대용량)의 2계층 KV 캐싱 저장소.
- **Bidirectional awareness**: compute engine과 storage가 서로의 정보(KV 위치/크기 ↔ model answer 길이)를 교환해 협력하는 Bidaw의 핵심 개념.
- **Weighted reuse distance**: 같은 KV의 연속 access 사이에 접근된 다른 unique KV들의 총 크기. temporal locality의 정량 지표.
- **I/O-aware request scheduling**: KV의 storage layer·크기를 인지해 dual queue(ready/preparing)와 disk-HRRN으로 request를 재정렬, I/O blocking 완화.
- **Previous-answer-based eviction**: 이전 round model answer 길이로 다음 access timing(weighted reuse distance)을 예측해 eviction하는 전략.
- **Ghost cache**: optimal(Belady) eviction을 과거 trace에 적용해 promising bucket별 hit rate를 추정하는 background 캐시.
- **Hit potential**: 미래를 아는 optimal 전략 하에서의 hit rate. 이 값이 가장 낮은 KV를 evict.
- **Disk-HRRN**: HRRN 변형 scheduling. Response ratio = 1 + waiting time / KV size. 작은 KV 우선 + starvation 방지.
- **Storage-efficient tensor**: KV tensor로 1-step 변환 가능하며 cost efficiency(saved compute/space)가 최고인 intermediate tensor(tensor 6, normalized activation).
- **Mix-grained GPU blocks**: big block(256 tokens, 길이 알려진 history/query) + small block(16 tokens, 길이 미지 response)을 혼합한 PagedAttention 변형 메모리 할당.
- **Inclusive caching**: capacity layer가 performance layer KV의 복사본을 유지해 eviction 시 write 회피.

## 강점 · 한계 · 열린 질문
**강점**
- Lossless(정확도 무손상)로 dropping/quantization 기반 lossy 기법과 직교, 결합 가능.
- compute·storage 양쪽을 동시에 다루는 통합적 접근으로 I/O blocking과 hit rate 두 병목을 함께 해소.
- 실제 산업 파트너(China Telecom)의 백만 round workload로 특성화·검증 — 현실성 높음.
- overhead가 매우 작고(scheduling 0.62ms, eviction 0.35ms) background 활용으로 critical path 영향 최소화.

**한계**
- storage-efficient tensor caching은 MHA 기반 모델에만 유효, GQA(최신 모델 다수)에서는 KV 캐싱으로 회귀 — 적용 범위 제약.
- previous-answer-based eviction은 실제 timestamp 의존적이라 Poisson 시뮬(ShareGPT)에서는 효과가 크게 줄어듦 — 합성 워크로드 일반화 한계.
- 단일 A800 GPU·200GB host memory 환경 위주 평가, 다중 GPU·disaggregated 환경 확장성 미검증.
- ghost cache 운영(optimal eviction 시뮬)의 메모리/연산 비용 절대치는 background라지만 대규모 동시 user에서의 scalability 정량 분석이 제한적.

**열린 질문**
- GQA 모델에서 storage-efficient tensor에 상응하는 절감 기법이 가능한가?
- multi-node/disaggregated prefill-decode 환경에서 bidirectional awareness를 어떻게 확장하는가?
- model answer 길이 외에 user 행동(읽기 속도 등)을 더 정교히 모델링하면 eviction 정확도가 얼마나 오르는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 two-tier KV caching의 근본 병목은 무엇인가?
> 답: compute engine과 two-tier storage가 서로의 정보를 모르는 mutually unaware 상태. compute engine은 KV loading latency를 무시하고 scheduling해 I/O-induced request blocking을 일으키고(긴 I/O request가 후속을 block), storage는 자신의 과거 access만 보고 compute engine의 대화 패턴을 무시해 performance layer hit rate가 약 20%로 낮다.

> [!question]- Q2. Bidaw의 "bidirectional awareness"에서 양방향이 각각 무엇을 인지하는가?
> 답: compute→storage 방향은 KV가 어느 layer에 있고 크기가 얼마인지(KV location & size)를 인지해 I/O-aware scheduling에 활용. storage←compute 방향은 compute engine이 생성한 model answer 길이를 받아 다음 KV access timing(weighted reuse distance)을 예측해 eviction에 활용.

> [!question]- Q3. I/O-aware scheduling의 dual queue와 disk-HRRN은 각각 무슨 문제를 푸는가?
> 답: dual queue(ready/preparing)는 capacity layer의 느린 I/O request를 preparing queue로 격리해 performance layer의 빠른 request가 block되지 않게 함(GPU underutilization 방지). disk-HRRN(Response ratio = 1 + waiting time/KV size)은 preparing queue에서 작은 KV를 우선 promote하되 대기 시간으로 큰 KV의 starvation을 막는다.

> [!question]- Q4. weighted reuse distance가 크면 왜 무조건 evict하면 안 되는가?
> 답: reuse distance가 performance layer 용량을 넘어도(promising reuse distance 구간) optimal(Belady) eviction의 hit rate는 0보다 크기 때문. 따라서 promising 구간을 fine-grained bucket으로 나눠 ghost cache로 hit potential을 추정하고, hit potential이 가장 낮은 KV만 evict해야 hit rate를 극대화할 수 있다.

> [!question]- Q5. model answer 길이로 미래 access를 예측할 수 있는 이유는?
> 답: 긴 답변일수록 사용자가 읽고/듣고 이해해 다음 질문을 만드는 데 시간이 더 걸려 다음 KV access가 늦어진다. 즉 weighted reuse distance lower bound가 이전 round model answer 길이와 양의 상관(Spearman 0.94~0.98)을 갖는다. 이는 human-LLM 상호작용의 본질적 특성이라 일반화 가능하다.

> [!question]- Q6. storage-efficient tensor caching은 무엇을 절약하며 어떤 비용/제약이 있는가?
> 답: KV tensor 대신 cost efficiency 최고인 intermediate tensor(tensor 6, normalized activation)를 캐싱해 storage footprint를 줄여 performance layer에 더 많은 history를 담는다. 로딩 시 KV tensor로 변환해야 하지만 idle SM(30%+)에서 low-priority CUDA stream으로 수십 ms 내 변환. 단 MHA 모델에만 유효하고 GQA에서는 KV가 더 작아 이득이 없다.

> [!question]- Q7. Bidaw가 정확도를 해치지 않는다고 주장하는 근거는?
> 답: Bidaw는 I/O-aware scheduler로 request 간 순서만 재정렬할 뿐, 개별 request 내부의 computation 순서는 바꾸지 않는다. user 1을 user 2보다 먼저 처리해도 각 user의 LLM 응답 생성 자체는 동일하므로 lossless다. lossy KV compression(dropping/quantization)과 달리 정확도 손실이 없다.

> [!question]- Q8. 핵심 정량 성과는?
> 답: OPT-13B 기준 응답 지연 최대 3.58x 단축(SOTA 대비 83.9% 감소), throughput 최대 1.83x 개선(1.43~1.83x 높은 user arrival rate). performance layer miss rate는 queue-enhanced 대비 57.6%, 일반 전략 대비 69.9% 감소. request queuing time은 FCFS 대비 57.5% 감소. 모든 KV가 host memory에 있는 ideal upper bound에 근접.

## 🔗 Connections
[[LLM Systems]] · [[FAST]] · [[2026]]

## References worth following
- **CachedAttention** (Gao et al., USENIX ATC'24, [11]): two-tier storage KV caching + queue-enhanced eviction. Bidaw의 주 baseline·전신.
- **FlashGen** (Jeong & Ahn, ASPLOS'25, [19]): inclusive caching + GPU-fit prioritized scheduling. 또 다른 주 baseline.
- **HCache** (Gao et al., EuroSys'25, [12]): KV 대신 intermediate activation을 buffer해 fast state restoration — storage-efficient tensor caching과 비교 포인트.
- **MOONCAKE** (Qin et al., FAST'25, [36]): KVCache-centric, RDMA disaggregated memory pool로 KV 캐싱 — two-tier vs memory pool 설계 대비.
- **IMPRESS** (Chen et al., FAST'25, [8]): importance-informed multi-tier prefix KV storage — lossy 계열 다층 KV 저장.
- **PagedAttention/vLLM** (Kwon et al., SOSP'23, [23]): GPU 메모리 관리 기반 — mix-grained block 최적화의 출발점.

## Personal annotations
<본인 메모 영역>
