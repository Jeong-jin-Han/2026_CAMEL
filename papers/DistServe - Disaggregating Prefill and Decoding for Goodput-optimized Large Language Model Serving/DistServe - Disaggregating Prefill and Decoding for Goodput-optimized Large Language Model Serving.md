# DistServe: Disaggregating Prefill and Decoding for Goodput-optimized Large Language Model Serving

> **Source PDF**: [DistServe - Disaggregating Prefill and Decoding for Goodput-optimized Large Language Model Serving.pdf](DistServe%20-%20Disaggregating%20Prefill%20and%20Decoding%20for%20Goodput-optimized%20Large%20Language%20Model%20Serving.pdf)
> **NodeGraph**: [DistServe.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/DistServe%20-%20Disaggregating%20Prefill%20and%20Decoding%20for%20Goodput-optimized%20Large%20Language%20Model%20Serving/DistServe.html)
> **Authors**: Yinmin Zhong, Shengyu Liu, Jianbo Hu, Xuanzhe Liu, Xin Jin (Peking University), Junda Chen, Hao Zhang (UC San Diego), Yibo Zhu (StepFun)
> **Venue / Year**: OSDI 2024 (18th USENIX Symposium on Operating Systems Design and Implementation), pp.193-211
> **arXiv / DOI**: arXiv:2401.09670
> **Length**: 19 pages
> **Read status**: ☑ Full read (2026-07-13)
> **My reading purpose**: [[Prefill vs Decode]]에서 이미 다룬 "prefill/decode 분리 서빙"의 원조격 논문을 직접 정독 — [[SwiftSpec]]·[[Mooncake]]가 공통으로 인용하는 배경, 그리고 disaggregation 시 통신 오버헤드를 인터커넥트 토폴로지(NVLink vs cross-node)로 관리하는 방식이 CXL fabric 배치 문제와 얼마나 닮았는지 확인

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

기존 LLM 서빙 시스템은 prefill(prompt 전체를 병렬 처리해 첫 토큰을 만드는 단계)과 decode(토큰을 하나씩 순차 생성하는 단계)를 같은 GPU에 colocate하고 배치를 섞어서(continuous batching) 처리한다. DistServe는 이 둘을 **서로 다른 GPU("instance")로 물리적으로 분리(disaggregate)**해서, (1) prefill이 decode를 지연시키고 decode가 prefill을 지연시키는 **상호 간섭(interference)**을 완전히 제거하고, (2) 각 단계에 맞는 **자원 배분·병렬화 전략을 독립적으로 최적화**할 수 있게 한다. 목표 지표는 처리량(throughput)이 아니라 **per-GPU goodput**(TTFT·TPOT 두 SLO를 동시에 만족시키면서 서빙 가능한 최대 요청률/GPU)이다. DistServe는 이 goodput을 최대화하는 배치(placement) — 각 단계에 GPU 몇 대·어떤 병렬화(intra-op/inter-op)를 줄지, 그리고 클러스터의 노드 간 대역폭 제약까지 고려해 어디에 배치할지 — 를 시뮬레이터 기반 탐색으로 자동으로 찾는다. 결과적으로 vLLM 대비 최대 **7.4배** 더 많은 요청 또는 **12.6배** 더 엄격한 SLO를 90% 이상 요청에서 만족시키며 서빙할 수 있다(Abstract, p.193).

---

## Core thesis

> "We overcome these challenges [...] to disaggregate the prefill and decoding phases of LLM inference, assigning them to separate GPUs. Our approach has two benefits. First, operating each phase independently on different GPUs eliminates prefill-decoding interference. Second, it allows to scale each phase independently with tailored resource allocation and model parallelism strategies to meet their specific latency requirements." (§1, p.194)

추가 설명: 기존 시스템(vLLM 등)은 prefill과 decode를 같은 GPU 배치(batch) 안에서 섞어 처리해 전체 throughput은 극대화하지만, 그 결과 개별 요청의 TTFT·TPOT라는 서로 다른 지연시간 요구를 동시에 만족시키기 어렵다. DistServe는 "두 단계를 다른 GPU로 쪼갠다"는 단순한 아이디어 하나로 간섭 문제와 자원 배분 결합(coupling) 문제를 동시에 풀고, 그 대가로 생기는 GPU 간 KV cache 전송 오버헤드는 클러스터의 대역폭 토폴로지(같은 노드의 NVLink vs 노드 간 낮은 대역폭)를 인식하는 배치 알고리즘으로 무시할 수준까지 줄인다.

---

## Why this matters to me

이 논문은 [[Prefill vs Decode]] 노트에서 이미 다룬 "prefill/decode 비대칭성 때문에 아예 GPU를 분리한다"는 아이디어의 원조 격 논문이라, 다른 여러 논문(SwiftSpec, Mooncake)이 인용하는 공통 배경을 직접 확인하는 의미가 있다. 하지만 내 연구 방향(메모리 시스템 아키텍처, CXL, multi-node coherence) 관점에서 더 흥미로운 지점은 **§4.2 "Placement for Low Node-Affinity Cluster"**다 — DistServe는 prefill instance와 decode instance 사이에서 KV cache라는 "상태"를 전송해야 하는데, 이 전송이 노드 내 NVLink(고대역폭)를 타는지 노드 간 네트워크(저대역폭)를 타는지에 따라 배치 전략 자체를 완전히 다르게 짠다(§4.1 vs §4.2, Algorithm 1 vs Algorithm 2). 이건 내가 CXL multi-node coherence에서 고민하는 "거리·인터커넥트 등급에 따라 동기화/전송 전략을 다르게 특화한다"는 가설(H1)과 정확히 같은 모양의 문제를, GPU 클러스터 스케줄링 레벨에서 이미 실전 배포까지 간 사례로 보여준다. 다만 DistServe는 이 문제를 **소프트웨어 placement 알고리즘**(어디에 어떤 instance를 배치할지 결정하는 오프라인 탐색)으로 풀지, 하드웨어/OS 레벨의 coherence 프로토콜로 풀지 않는다는 점이 내 연구와의 결정적 차이다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.194-195 | Colocation의 prefill-decoding 간섭 문제 제기, disaggregation 제안 |
| 2 | Background and Motivation | p.195-196 | LLM inference 특성(prefill=compute-bound, decode=memory-bound), 기존 batching 최적화의 한계 |
| 3 | Tradeoff Analysis | p.196-199 | Prefill/decode 각각의 batching·parallelism 분석, M/D/1 큐 모델로 TTFT 정량화 |
| 4 | Method | p.199-201 | High/Low node-affinity 클러스터용 두 placement 알고리즘, 온라인 스케줄링(pull 기반 KV 전송, replanning) |
| 5 | Implementation | p.200 | Python 6.5K줄 + C++/CUDA 8.1K줄, Ray 기반 실행 엔진, NCCL/CudaMemcpy 전송 |
| 6 | Evaluation | p.201-204 | vLLM·DeepSpeed-MII 대비 7.4배 rate / 12.6배 SLO, latency breakdown, ablation, 알고리즘 실행시간 |
| 7 | Discussion | p.204-205 | Throughput-optimized/resource-constrained/long-context 시나리오에서의 한계, resource disaggregation 문헌과의 관계 |
| 8 | Related Work | p.205 | Inference serving, goodput-optimized 시스템, resource disaggregation 계보 |
| 9 | Conclusion | p.205 | Disaggregation이 지연시간 중심 LLM 서빙의 핵심 전략이라는 결론 |
| A | Latency Model (Appendix) | p.209 | Prefill/decode GEMM의 FLOPs·메모리 접근 기반 analytical latency model |

---

## Section notes

### §1 Introduction (p.194-195)

Figure 1의 관찰이 논문 전체의 motivation이다: 13B 모델을 A100 GPU 하나에서 기존 시스템으로 서빙할 때, prefill만 따로 서빙하면 TTFT SLO를 만족하는 rate가 훨씬 높고, decode만 따로 서빙해도 TPOT SLO를 만족하는 rate가 훨씬 높다 — 그런데 **둘을 같이 배치하는 순간 goodput이 1.6 rps로 뚝 떨어진다** (분리하면 prefill 5.6 rps + decode 10 rps 조합으로 사실상 2.1배 개선). 저자들은 이 gap의 원인을 "colocation이 두 단계의 매우 다른 계산 특성과 지연시간 요구를 강제로 공유 자원 위에 결합시키기 때문"이라고 규정한다.

### §2 Background and Motivation (p.195-196)

핵심 구분: prefill은 prompt 전체 토큰을 한 스텝에 병렬 처리(compute-bound 경향, 특히 긴 prompt일 때), decode는 매 스텝 새 토큰 1개씩 생성하며 이전 모든 토큰의 KV cache를 다시 읽어야 해서(memory-bandwidth-bound) 근본적으로 다른 연산 패턴을 가진다. §2.3에서 두 가지 핵심 문제를 짚는다: (1) **prefill-decoding interference** — Figure 2가 정량적으로 보여주는데, decoding batch에 prefill job 하나만 추가해도 TPOT가 크게 늘어난다. (2) **resource and parallelism coupling** — 두 단계가 같은 GPU 자원·병렬화 설정을 공유해야 해서, 어느 한쪽에 맞추면 다른 쪽이 over-provision된다. Chunked-prefill(piggyback)이 완화책으로 언급되지만 "trades TTFT for TPOT and cannot eliminate the interference" (§2.3, p.195)라고 명시적으로 한계를 짚는다.

### §3 Tradeoff Analysis (p.196-199)

Disaggregation 이후 prefill/decode를 독립적으로 분석할 수 있다는 게 핵심 이점이다. §3.1은 prefill instance를 **M/D/1 큐 모델**로 formalize해서 $Avg\_TTFT = D + \frac{RD^2}{2(1-RD)}$ (Eq.1, p.197) 같은 닫힌 형태 수식을 유도하고, intra-op parallelism과 inter-op parallelism 중 어느 쪽이 유리한지 요청률(R)에 따라 갈린다는 걸 보인다(저율에선 intra-op, 고율에선 inter-op). §3.2는 decode instance가 memory-bandwidth-bound라 배칭이 핵심이며, disaggregation 덕분에 decode 전용 GPU에 여러 prefill instance의 출력을 몰아서 큰 배치를 만들 수 있다는 점을 강조한다. §3.3 "Practical Problems"에서 KV cache 전송 비용을 구체적으로 계산한다 — OPT-66B, 512토큰 요청 하나의 KV cache가 약 1.13GB, 10 rps에서 초당 11.3GB(90Gbps 상당)를 전송해야 하는데, **NVLink(600GB/s)에서는 무시할 수준이지만 cross-node 대역폭이 제한적이면 문제가 된다**는 점을 명시(p.198) — 이게 §4의 두 알고리즘으로 이어지는 다리다.

### §4 Method (p.199-201)

> "The key insight is that KV cache transfer occurs exclusively between corresponding layers of prefill and decoding instances. Leveraging inter-op parallelism, we group layers into stages [...] with each segment maintaining one specific inter-op stage. By colocating prefill and decoding segments of the same stage within a single node, we force the transfer of intermediate states to occur only via NVLINK." (§4.2, p.199)

**Algorithm 1 (High Node-Affinity, §4.1)**: cross-node 대역폭이 충분한(Infiniband 등) 클러스터용. Prefill/decode instance의 병렬화(intra-op × inter-op)를 독립적으로 최적화해 phase-level optimal per-GPU goodput을 찾고, 그 뒤 목표 traffic rate를 만족할 때까지 replication한다. 시뮬레이터(`simu_prefill`/`simu_decode`)로 SLO attainment를 추정해 탐색하며, 복잡도는 $O(NM^2)$(N=인스턴스당 노드 수 제한, M=노드당 GPU 수)이고 실행시간은 1.3분 이내(§6.5).

**Algorithm 2 (Low Node-Affinity, §4.2)**: NVLink 내부 대역폭만 믿을 수 있는 일반적인 클러스터용(이 논문의 실제 테스트베드가 이 경우, cross-node 25Gbps). Prefill/decode의 대응하는 inter-op stage를 **같은 노드**에 colocate시켜서 KV cache 전송이 항상 NVLink를 타도록 강제하는 게 핵심 아이디어. 이는 명시적으로 인터커넥트 토폴로지를 배치 제약 조건으로 끌어들이는 설계다.

§4.3 "Online scheduling"은 세 가지 실전 대응책을 다룬다: pipeline bubble을 줄이기 위한 prompt-length-aware batching, burst 대응을 위한 **KV cache "pull" 방식**(decode instance가 필요할 때 prefill instance에서 가져오는 방식, push보다 메모리 오버플로우에 안전), 그리고 워크로드 패턴 변화를 감지해 배치를 재탐색하는 **periodic replanning**.

### §5 Implementation (p.200)

Figure 6이 런타임 아키텍처: 중앙 controller가 요청을 받아 큐가 가장 짧은 prefill instance로, 이후 가장 덜 로드된 decode instance로 dispatch(단순 FCFS). Placement 알고리즘·프론트엔드·오케스트레이션은 Python 6.5K줄, 병렬 실행 엔진은 C++/CUDA 8.1K줄. Ray actor로 GPU worker를 구현하고, cross-node는 NCCL, intra-node는 비동기 CudaMemcpy로 KV cache를 전송해 GPU 연산을 블로킹하지 않는다. FlashAttention·PagedAttention·continuous batching 등 기존 최적화도 통합.

### §6 Evaluation (p.201-204)

vLLM·DeepSpeed-MII 대비 chatbot(OPT-13B/66B/175B, ShareGPT), code completion(OPT-66B, HumanEval), summarization(OPT-66B, LongBench) 세 워크로드에서 비교. ShareGPT에서 vLLM 대비 2.0×-4.6× 높은 rate(§6.2, p.202). §6.3 latency breakdown에서 OPT-175B의 KV cache transmission이 전체 latency의 **0.1% 미만**임을 보여, "통신 오버헤드가 무시할 만하다"는 §3.3의 주장을 실측으로 검증한다(95% 요청이 30ms 미만 전송 지연, Figure 10). §6.4 ablation은 disaggregation 자체의 기여(vLLM++는 병렬화만 최적화해도 vLLM과 동일 — 즉 병렬화 튜닝만으론 안 되고 disaggregation이 핵심)와 placement 알고리즘의 기여(DistServe-High가 제약이 적어 DistServe-Low보다 더 나은 배치를 찾음)를 분리해서 보여준다.

### §7 Discussion (p.204-205)

저자들이 스스로 한계를 짚는다: throughput-optimized(오프라인, latency 안 민감) 시나리오엔 chunked-prefill 쪽이 나을 수 있고, resource-constrained(GPU 한두 개) 환경에선 disaggregation할 여지가 없어 non-disaggregated 시스템이 더 나을 수 있으며, long-context(1M 토큰급)에서는 prefill이 quadratic하게 커지므로 disaggregation 효과가 오히려 더 유효해질 것이라 전망한다. §7 "Resource disaggregation" 문단에서 LegoOS·Mira 등 **자원 분리(compute/memory pooling) 시스템 문헌과의 관계**를 명시적으로 언급하며 "DistServe shares the concept by disaggregating its system components"라고 스스로 위치를 규정한다(p.205) — 다만 이건 GPU 인스턴스 단위의 분리지, 메모리 하드웨어 자체의 disaggregation(CXL류)은 아니다.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "per-GPU goodput" — throughput이 아니라 SLO를 만족하는 최대 요청률이라는 목표 지표
- "prefill-decoding interference" — colocation이 유발하는 상호 지연

**Technical concepts:**
- TTFT(time to first token) / TPOT(time per output token)
- instance (한 벌의 model weight 복사본을 관리하는 자원 단위) / prefill instance / decoding instance
- High/Low node-affinity placement
- KV cache "pull" 방식 (vs push)
- Instance segment (inter-op stage 단위로 나눈 배치 단위)

**Value language:**
- "goodput-optimized" (throughput-optimized와 대비)
- "orthogonal to model parallelism for training" (§7) — 서빙과 학습의 parallelism 문헌이 별개임을 명시

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "DistServe" 자체, "7.4×/12.6×"라는 headline 수치는 이 논문 고유의 claim이므로 내 글에서 인용 맥락 없이 그대로 반복하면 표절처럼 보임

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.193 | vLLM 대비 최대 7.4× 요청 수 또는 12.6× 더 엄격한 SLO | LLM 서빙에서 disaggregation의 효과 인용 시 |
| §1, p.194, Fig.1 | colocation 시 goodput 1.6 rps → disaggregation 시 prefill 5.6 rps/decode 10 rps 조합(2.1배) | Motivation 수치로 인용 |
| §3.3, p.198 | OPT-66B 512토큰 요청의 KV cache ≈ 1.13GB, 10 rps에서 초당 11.3GB(≈90Gbps) 전송 필요 | KV cache 전송량의 구체적 스케일 인용 시 |
| §6.3, p.203, Fig.10 | OPT-175B에서 KV cache transmission이 전체 latency의 <0.1%, 95% 요청이 전송지연 <30ms | "통신 오버헤드는 관리하면 무시할 수준"이라는 주장의 실측 근거 |
| §4, p.199 | Placement 알고리즘 실행시간, 최대 32 GPU 설정에서 1.3분 이내(Algorithm 1) | 오프라인 탐색 비용이 실용적이라는 근거 |

---

## 🎯 Strategic anchor

> "The key insight is that KV cache transfer occurs exclusively between corresponding layers of prefill and decoding instances. [...] By colocating prefill and decoding segments of the same stage within a single node, we force the transfer of intermediate states to occur only via NVLINK." (§4.2, p.199)

→ **본인 활용**: 면담에서 "GPU 서빙 스케줄러가 이미 인터커넥트 등급(NVLink vs 저대역폭 네트워크)에 따라 배치 전략을 완전히 다르게 짜는 사례가 실전 배포까지 갔다"는 근거로 인용 가능. 내가 CXL multi-node coherence에서 "거리·인터커넥트 등급별로 동기화 전략을 특화한다"는 방향을 이야기할 때, "DistServe는 이걸 소프트웨어 placement 레벨에서 하는데, 내 질문은 이걸 하드웨어/OS coherence 프로토콜 레벨로 내리면 무엇이 달라지는가"라는 대비로 쓸 수 있음.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---:|---|
| Scope | 8-32 GPU 클러스터 내 prefill/decode instance 배치 | Multi-node CXL 메모리/컴퓨트 disaggregation |
| Mechanism | 오프라인 시뮬레이터 기반 placement 탐색(Algorithm 1/2) + pull 기반 KV 전송 | HW/OS 레벨 coherence protocol의 워크로드별 특화 (H1) |
| "분리" 대상 | GPU instance(연산 단위) 자체를 물리적으로 분리 | 메모리 자체를 컴퓨트로부터 분리(disaggregate) |
| 인터커넥트 인식 | 배치 알고리즘이 NVLink vs cross-node 대역폭을 명시적 제약으로 사용(Algorithm 2) | Multi-node coherence 비용이 노드 간 거리에 어떻게 스케일하는지가 핵심 질문 |
| Open space | GPU 메모리 자체가 부족한 상황(resource-constrained, §7)엔 disaggregation이 오히려 불리하다고 인정 — 이건 CXL처럼 메모리 용량 자체를 늘리는 접근과는 정반대 축의 해법 | 메모리 용량 확장이 이런 resource-constrained 시나리오의 제약을 완화할 수 있는지가 흥미로운 교집합 |

DistServe는 "compute 인스턴스를 역할별로 쪼갠다"는 축의 disaggregation이고, 내가 보는 CXL 방향은 "메모리를 compute로부터 쪼갠다"는 다른 축이다. 그런데 §3.3의 KV cache 전송 costing과 §4.2의 노드-어피니티 인식 배치는, 내가 multi-node CXL coherence에서 풀려는 "거리별 비용을 어떻게 배치/스케줄링 결정에 반영하는가" 문제와 구조적으로 동형이다. 다만 DistServe는 이 문제를 **오프라인·중앙집중식 시뮬레이터 탐색**으로 풀고, 런타임에는 정적인 배치를 그대로 쓴다(§4.3의 replanning도 "패턴이 바뀌면 몇 분 걸려 재탐색"하는 수준) — 내가 관심 있는 "런타임에 동적으로, 그것도 하드웨어/OS 레벨에서 값싸게" 반응하는 coherence 메커니즘과는 반응 속도·개입 레이어가 다르다.

---

## Open questions / gaps

- [ ] Fault tolerance/preemption을 명시적으로 future work로 남김(§4.3 "Preemption and fault tolerance", p.200) — prefill instance 하나의 장애가 여러 decode instance에 전파될 수 있다는 위험을 인정만 하고 풀지 않음
- [ ] Resource-constrained(단일/소수 GPU) 시나리오에서 disaggregation이 오히려 불리하다는 것을 스스로 인정(§7) — 이 경계가 정확히 어디인지 정량적 기준은 없음
- [ ] Placement 알고리즘이 workload 분포(도착률·입출력 길이)의 정상성(stationarity)을 가정 — 짧은 시간 척도의 워크로드 변화엔 대응 못 함(§4.1 "unpredictable in the short-term")
- [ ] KV cache 전송이 "무시할 만하다"는 결론은 이 논문의 클러스터(NVLink 600GB/s, cross-node 25Gbps) 조건에 특정됨 — 대역폭이 훨씬 낮은 인터커넥트(예: 원거리 CXL fabric)에서도 같은 결론이 성립하는지는 열려있음

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [38] | Splitwise (Patel et al., 2023) | DistServe와 동시기 독립적으로 나온 유사한 prefill/decode phase-splitting 접근, 비교 가치 |
| ☐ | [27] | Inference without Interference (Hu et al., 2024) | Mixed downstream workload에서의 disaggregation, DistServe의 confirming 사례로 언급됨 |
| ☐ | [49] | DéjàVu (Strati et al., 2024) | KV-streaming 기반 fault-tolerant disaggregated serving — DistServe가 미룬 fault tolerance를 다룸 |
| ☐ | [25] | Mira (Guo et al., SOSP'23) | Program-behavior-guided far memory system — resource disaggregation 계보, CXL 방향과 더 가까운 각도 |
| ☐ | [43] | LegoOS (Shan et al., OSDI'18) | Disaggregated OS — §7에서 DistServe가 스스로 위치시키는 resource disaggregation 원류 |
| ☐ | [33] | AlpaServe (Zhuohan Li et al., OSDI'23) | Non-autoregressive generation에서 model parallelism으로 goodput 최적화 — DistServe가 "이 방향의 autoregressive 확장"이라 자평 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
