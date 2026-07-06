# Efficiently Scaling Transformer Inference

> **Source PDF**: [Efficiently Scaling Transformer Inference.pdf](Efficiently%20Scaling%20Transformer%20Inference.pdf)
> **NodeGraph**: [EfficientScaling.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/concepts/LLM/KV%20Cache/Efficiently%20Scaling%20Transformer%20Inference/EfficientScaling.html)
> **Authors**: Reiner Pope, Sholto Douglas, Aakanksha Chowdhery, Jacob Devlin, James Bradbury, Anselm Levskaya, Jonathan Heek, Kefan Xiao, Shivani Agrawal, Jeff Dean (Google)
> **Venue / Year**: arXiv preprint, 2022 (submitted 9 Nov 2022)
> **arXiv / DOI**: arXiv:2211.05102v1 [cs.LG]
> **Length**: 18 pages
> **Read status**: ☑ Full read (2026-07-06)
> **My reading purpose**: KV cache / memory-bandwidth 관점에서 LLM serving 병목을 이해하고, memory-system architecture(CXL, disaggregation) 연구 방향과의 접점을 찾기 위함

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

이 논문(Google, PaLM 팀)은 500B+ 규모 dense Transformer를 TPU v4 slice 위에서 서빙할 때 latency·throughput·cost를 어떻게 최적으로 trade-off할지에 대한 **분석적 partitioning framework**를 제시한다. Feedforward layer는 weight-stationary(1D/2D)와 weight-gathered 레이아웃 사이에서 batch size에 따라 통신량이 최소가 되도록 전환하고, attention layer는 **multiquery attention**(K/V head를 1개만 두고 batch 축으로 샤딩)을 이용해 decode 시 매 스텝 다시 읽어야 하는 KV cache의 메모리 트래픽을 chip 수만큼 줄인다. 그 결과 PaLM 540B에서 int8 weight 기준 29ms/token 저지연 decode, 76% MFU의 고처리량 prefill, 그리고 기존(single-head) 대비 최대 32배 긴 context length를 동일 메모리 예산 안에서 지원함을 보인다.

---

## Core thesis

> "We show that the best latencies are achieved by going far beyond the traditional paradigm of single-server inference, and scaling inference up to 64+ chips." (§7 Conclusions, p.14)

추가 설명: Transformer 추론(특히 decode)은 시퀀셜하고 병렬성이 낮아 학습보다 파티셔닝이 훨씬 까다로운데, 이 논문은 "어떤 조건에서 어떤 파티셔닝 레이아웃(weight-stationary vs weight-gathered, head-sharded vs batch-sharded attention)이 통신량을 최소화하는가"를 닫힌 형태의 analytic cost model로 유도해, exhaustive search 없이도 최적 구성을 즉시 골라낼 수 있게 한다. 이 프레임워크의 핵심 동기는 결국 "모델 파라미터"와 "KV cache"라는 두 가지 큰 텐서를 HBM에서 매 스텝 다시 읽어야 하는 **메모리 대역폭 병목**이다.

---

## Why this matters to me

내 연구 방향(메모리 시스템 아키텍처, CXL, disaggregation, tiered memory, GPU/가속기 메모리 확장)에서 이 논문은 "LLM serving이 왜 memory-bandwidth-bound인가"를 정량적으로 증명하는 근거 자료다. 특히 p.2-3에서 "chip이 매 토큰마다 KV cache를 off-chip HBM에서 다시 읽어야 하고 그동안 compute core는 사실상 idle 상태"라고 명시하는 부분, 그리고 batch 512·context 2048 조건에서 **KV cache 전체 크기가 3TB로 모델 파라미터의 3배**가 된다는 수치(p.3)는, "왜 accelerator당 HBM 용량·대역폭을 늘리거나(더 많은 chip으로 쪼개거나) 다른 메모리 계층(CXL-attached pooled/tiered memory)으로 KV cache를 옮기는 것이 serving 비용에 직접적 영향을 주는가"를 뒷받침하는 근거가 된다. 이 논문의 해법(multiquery attention + batch-axis sharding)은 순수 소프트웨어/알고리즘적으로 KV cache 크기를 줄이는 접근인데, 이는 내가 보려는 하드웨어 메모리 계층 확장(예: CXL 메모리로 KV cache를 오프로드하거나 여러 노드에 걸쳐 pool)과 상호보완적인 축이다 — 즉 "KV cache를 얼마나 작게 만들 것인가"(이 논문)와 "KV cache를 담을 메모리 용량·대역폭을 어떻게 물리적으로 확장할 것인가"(내 방향)는 같은 병목을 서로 다른 레이어에서 공격하는 것.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1-2 | 대규모 모델 추론의 memory-footprint/latency 문제 정의, 두 핵심 키(partitioning framework + memory optimization) 제시 |
| 2 | Inference Cost Tradeoffs | p.2-3 | latency/throughput/MFU 정의, memory cost vs compute cost 구분, KV cache 메모리 규모 수치 |
| 3 | Partitioning for Inference Efficiency | p.3-9 | feedforward/attention 레이어별 파티셔닝 전략과 통신 비용 유도, low-level 최적화, quantization |
| 4 | Case Study for PaLM Models | p.9-12 | PaLM 8B/62B/540B에 실제 적용한 실증 결과, Pareto frontier |
| 5 | FasterTransformer Benchmarks | p.12-13 | Megatron-Turing NLG 530B 대비 MFU 비교 |
| 6 | Related Work | p.14 | 병렬화·추론 효율 관련 선행연구 정리 |
| 7 | Conclusions | p.14 | 64+ chip 스케일 추론의 필요성, sparsity 등 향후 방향 |
| A | Appendix: Deriving Communication Costs | p.16-17 | 각 레이아웃의 통신시간 수식 유도 |
| B | Appendix: Minimum Prefill Latency | p.17 | prefill batch=1 Pareto frontier |
| C | Appendix: MFU vs Latency Tradeoff | p.17 | 모델 크기별 MFU-latency 관계 |
| D | Appendix: Full FasterTransformer Comparison | p.17-18 | 상세 벤치마크 테이블 |

---

## Section notes

### §1 Introduction (p.1-2)

생성형 추론이 학습보다 어려운 이유를 두 가지로 정리한다: (1) 토큰별 순차 생성이라 병렬성이 낮고, (2) weight + KV cache라는 큰 메모리 footprint 때문에 매 스텝 HBM에서 compute core로의 트래픽이 커서 memory bandwidth가 latency를 좌우한다. 저자들은 이를 해결하는 두 축을 "partitioning framework"(모델 크기·요구사항에 맞는 최적 파티셔닝을 해석적으로 도출)와 "memory optimization"(PaLM의 multiquery attention을 활용해 불필요한 tensor overhead 제거)으로 압축한다.

> "The large memory footprint gives rise to a large amount of memory traffic to load the parameters and KV cache from high-bandwidth memory (HBM) into the compute cores for each step, and hence a large total memory bandwidth required to meet a given latency target." (p.1)

이 문장이 논문 전체의 motivation을 가장 압축적으로 담고 있다.

### §2 Inference Cost Tradeoffs (p.2-3)

Latency = prefill(입력 토큰 병렬 처리) + decode(출력 토큰 순차 생성) 시간. Throughput은 초당 처리/생성 토큰 수, MFU는 관측 처리량 대비 이론적 peak FLOPS 처리량의 비율로 정의한다. Memory cost와 compute cost를 분리해서 본다: N-parameter 모델의 forward pass는 2N FLOPS/token이 필요(attention 자체 FLOPS는 상대적으로 작지만 메모리 용량·대역폭 비용은 크다는 점을 강조). 작은 batch·짧은 시퀀스에서는 weight loading이 memory time을 지배하지만, 2048+ 토큰의 큰 batch에서는 KV cache loading이 지배한다.

> "For a 500B+ model with multihead attention, the attention KV cache grows large: for batch size 512 and context length 2048, the KV cache totals 3TB, which is 3 times the size of the model's parameters." (p.3)

이 3TB / 3배 수치가 KV cache가 단순 부가비용이 아니라 **주된 메모리 비용 항목**임을 보여주는 핵심 근거다.

### §3.1 Partitioning notation & communication collectives (p.3-4)

TPU v4의 3D torus(X×Y×Z) 위에서 텐서 축을 표기하는 `BLE_xyz` 식 표기법을 도입하고, MPI 유래의 all-reduce, reduce-scatter, all-gather, all-to-all 네 가지 collective를 정의한다. 이 표기·collective 세트가 이후 모든 레이아웃 설명의 공통 언어가 된다.

### §3.2 Partitioning the feedforward layer (p.4-6)

1D weight-stationary(Megatron 스타일, d_ff 축만 분할)는 chip 수가 늘어도 통신량이 줄지 않아(O(1)) 병목이 되지만, 2D weight-stationary(d_model과 d_ff 두 축을 모두 분할)는 통신시간이 O(1/√n_chips)로 스케일한다 — `T_comm = 8BLE / (√n_chips × network bandwidth)`. Batch가 매우 커지면 오히려 weight를 chip 간 전송하는 weight-gathered 레이아웃(X/XY/XYZ 변형)이 더 유리해지며, 그 통신시간은 `T_comm = 4E√(BLF) / (√n_chips × network bandwidth)`로 batch에 대해 √BL 스케일이라 배치가 클수록 상대적으로 저렴해진다. Figure 3(p.6)이 이 교차점을 시각적으로 보여준다.

### §3.3 Partitioning the attention layer (p.6-8)

핵심 섹션. Multihead attention은 KV cache를 head 축으로 샤딩하면(Fig.4a) chip마다 replicate가 필요 없지만, **multiquery attention**(K/V head를 1개로 공유, Q만 n_heads 유지)을 그대로 head-샤딩하면 K/V head 하나를 모든 chip에 replicate해야 해서 오히려 메모리 절감 효과가 사라진다(Fig.4b). 저자들은 대신 Q/K/V를 **batch 축**으로 샤딩하는 전략을 제안(Fig.4c, Fig.5b) — 각 chip이 KV cache의 batch shard만 들고 있으면 되므로 KV cache 메모리 비용이 **n_chips배만큼 줄어든다.** 대가는 활성화를 다시 모으기 위한 all-to-all 통신 비용인데, prefill 시에는 Q 텐서(수천 토큰)가 KV 텐서보다 훨씬 크므로 head-샤딩을 쓰고, decode 시에는 KV cache가 Q/K/V 활성화보다 훨씬 크므로(과거 토큰 수만큼) batch-샤딩이 유리하다는 비대칭성을 명확히 짚는다.

> "During autoregressive generation, there is only one token per example of Q, K, and V tensors, whereas the KV cache has many (perhaps 2048) tokens. Since the KV cache is orders of magnitude larger than the Q, K, and V tensors, it is very profitable to spend the all-to-all communication time on the small tensors to save the memory time on the large tensors." (p.7)

### §3.4 Parallel attention/feedforward layers (p.8)

PaLM의 "parallel" Transformer block 구성(attention과 FFN을 layernorm 이후 병렬로 계산해 합산)을 표준 "serial" 구성과 비교. Parallel 구성은 layernorm 1회로 줄고, FFN 입력 행렬을 attention의 W_Q와, K/V 투영 행렬을 서로, FFN 출력 행렬을 attention의 W_O와 fuse할 수 있어 더 큰 matmul로 FLOPS 효율이 오르고 **layer당 all-reduce 통신을 절반으로** 줄인다.

### §3.5 Low-level optimizations (p.8)

Wang et al.(2023)의 "Looped CollectiveEinsum" 기법으로 통신을 계산과 오버랩시켜 reduce-scatter/all-gather의 지연을 상당 부분 숨긴다. Reduce-scatter는 batch/sequence 축이 아니라 hidden 축(E/F)으로 하도록 선택(Korthikanti et al. 2022와 반대 선택). 이런 저수준 최적화로 naive 컴파일러 스케줄 대비 **약 1.4배** 성능 향상을 얻었다(p.8).

### §3.6 Quantization (p.8-9)

AQT 라이브러리(Lew et al. 2022)로 16-bit weight를 int8로 변환, 품질 저하 없이 메모리 대역폭 비용을 줄인다. Activation quantization은 아직 미구현이나 잠재적으로 compute time·weight-gathered 레이아웃의 통신량까지 더 줄일 수 있다고 언급(p.9).

### §4 Case Study for PaLM Models (p.9-12)

PaLM 8B/62B/540B에 대해 실증. TPU v4 칩 사양: **bfloat16 275 TFLOPS, 32GiB HBM @ 1200GB/s, interconnect 270GB/s**(p.9). 540B 모델은 attention head 수를 48→64로 패딩해(2D 파티셔닝 효율을 위해, +18B 파라미터, 3% MFU 비용) 64+ chip에서 더 잘 분할되게 만들었다. Feedforward는 chip 수·batch에 따라 1D/2D weight-stationary와 weight-gathered 사이를 전환(Fig.6, 7 — 2D weight-stationary가 저-batch에서 유리, weight-gathered가 batch 1M 토큰 근처에서 76% MFU 달성).

Attention 파트(§4.2, Table 1, p.11): d_head를 맞춘 multihead vs baseline-multiquery(head-샤딩) vs optimized-multiquery(batch-샤딩)를 비교. Max context length(전체 메모리의 30%를 KV cache에 할당한다고 가정):

- Multihead: batch128 → 1,320 tokens, batch512 → 330 tokens
- Baseline multiquery(head-샤딩): 660 / 165
- Optimized multiquery(batch-샤딩): **43,000 / 10,700** → 최대 32~64배 긴 context

> "Table 1 shows that the optimized multiquery layout can fit up to 32-64 times longer context lengths than the multihead and baseline multiquery variant." (p.11)

§4.3(p.11)에서는 parallel vs serial 포맷 비교: serial 포맷은 decode 시 activation 통신 증가로 **14% 더 높은 latency**를 보인다.

§4.4 End-to-end results(p.11-12): cost = n_chips × time / (B×L) [chip-seconds/token]로 정의. Batch>512에서 최소 cost 달성(cost가 파라미터 수에 비례). Batch 64에서 **int8 28.5ms/token vs bfloat16 36.9ms/token**(약 1.3배 개선, 저-batch에서는 weight loading 지배라 quantization 효과가 큼, 고-batch에서는 compute가 지배라 효과가 작아짐). 모델 크기와 저-batch latency 사이에는 근사적으로 √(model size) 관계가 관찰된다(Fig.1 기반).

### §5 FasterTransformer Benchmarks (p.12-13)

Megatron-Turing NLG 530B(NVIDIA A100, 16-32 GPU) 대비 PaLM 540B(TPU v4, 최대 256 chip)를 MFU 기준으로 비교. FasterTransformer는 32-way tensor parallel에서 최대 33% MFU(16-way의 46%보다 낮음 → 통신 병목 시사)에 그치지만, 이 논문 구현은 **64-way까지 스케일하면서도 44% MFU**를 유지해 2D weight-stationary 파티셔닝의 상호연결 대역폭 활용 우수성을 보여준다. Megatron 대비 최대 10% MFU 우위를 달성했는데, 이는 주로 parallel attention/FFN 구조 덕분(짧은 context에서는 multiquery attention의 이점이 두드러지지 않음, p.13).

### §7 Conclusions (p.14)

500B+ 모델 접근성 "democratize"를 언급하며, single-server 패러다임을 넘어 64+ chip으로 확장하는 것이 최적 latency 달성의 핵심이라 결론짓는다. FLOP count와 통신량이 dense Transformer 추론 성능의 근본적 한계이며, sparsity(MoE)·adaptive computation 등이 향후 FLOPs/token을 줄일 수 있는 방향으로 제시된다.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "simple analytical model for inference efficiency"
- "engineering principles that enable serving large-scale Transformer-based models efficiently"
- "going far beyond the traditional paradigm of single-server inference"

**Technical concepts:**
- "KV cache" / "multiquery attention" (K/V head 공유로 KV cache 크기를 n_heads배 축소)
- "weight-stationary" vs "weight-gathered" partitioning layout
- "memory time" vs "compute time" (HBM→core 전송 시간 vs matmul 연산 시간)
- "model FLOPS utilization (MFU)"

**Value language:**
- "democratize access" (모델 규모 확대에 따른 접근성 문제)
- "Pareto frontier of efficiency versus latency"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "Looped CollectiveEinsum" (Google 사내 구현 디테일, 그대로 내 것처럼 쓰면 안 됨)
> - "XYZ-weight-gathered" 등 TPU torus 축 명명 자체 (구현 특정적)
> - "Efficiently Scaling Transformer Inference"라는 논문 제목 프레이즈 자체를 내 motivation 문장에 그대로 재사용하지 않기

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §1, p.2 | PaLM 540B, 64 TPU v4 칩에서 **29ms/token** decode latency(int8), **76% MFU** 대규모 prefill | LLM serving 효율의 SOTA 레퍼런스 수치로 인용 |
| §2, p.3 | batch 512·context 2048에서 **KV cache 총 3TB, 모델 파라미터의 3배** | KV cache가 memory 병목의 주범임을 뒷받침하는 핵심 수치 |
| §4.1, p.9 | TPU v4 칩: **bfloat16 275 TFLOPS, 32GiB HBM @ 1200GB/s, interconnect 270GB/s** | 가속기 compute:memory-bandwidth 비율 논의에 인용 |
| §4.2, p.11, Table 1 | optimized multiquery(batch-샤딩)가 multihead 대비 **최대 32-64배** 긴 context length 지원 | KV cache 절감 전략의 정량적 효과 인용 |
| §4.4, p.12 | batch 64에서 **int8 28.5ms/token vs bfloat16 36.9ms/token** | quantization의 memory-bound 상황에서의 효과 인용 |
| §5, p.13 | FasterTransformer 32-way TP 최대 **33% MFU**(16-way 46%보다 낮음) vs 본 논문 64-way **44% MFU** | 통신 대역폭·토폴로지가 파티셔닝 스케일링의 한계를 좌우한다는 근거 |

---

## 🎯 Strategic anchor

> "The on-chip memory needs to load the KV cache from off-chip memory once for every token generated during which the computational core of the chip is essentially idle." (§2, p.3, memory costs 문단)

→ **본인 활용**: 면담·자소서에서 "LLM decode가 memory-bandwidth-bound임을 이 논문이 정량적으로(3TB KV cache = 3× model size) 보여준다, 그런데 이 논문의 해법은 알고리즘(multiquery attention)과 chip 병렬화뿐이고 memory 계층 자체를 확장하는 방향(CXL 기반 pooled/tiered memory로 KV cache를 담는 것)은 다루지 않는다 — 그 공백이 내 연구 방향이 위치할 자리"라는 식으로 사용. p.3의 "compute core is essentially idle" 표현은 memory-bound 상황을 압축적으로 설명하는 데 매우 강력해서 그대로 인용하기 좋음.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | 단일 데이터센터 내 TPU v4 slice(최대 256 chip), on-chip HBM 용량·대역폭 안에서 KV cache/weight를 어떻게 나눌지 | Rack/노드 간 메모리 disaggregation, CXL 기반 memory pooling·tiering — HBM 바깥의 메모리 계층 자체를 다룸 |
| Mechanism | 소프트웨어/알고리즘: partitioning layout 전환 + multiquery attention으로 KV cache 크기 자체를 줄임 | 하드웨어/시스템: 물리적 메모리 계층(CXL attach memory, coherence protocol)으로 KV cache를 담을 용량·대역폭을 확장 |
| Workload | Dense Transformer(PaLM) 추론, 고정 chip 세트 내에서의 batch/context 스윕 | Multi-tenant serving, 여러 노드/가속기에 걸친 KV cache 공유·오프로드, 장기 실행 세션 |
| Open space | Chip 수를 늘리는 것으로 KV cache 총 용량 문제를 해결(더 많은 HBM을 사서 나눠 가짐) | 같은 총 HBM 용량 안에서 못 담는 경우, cold/warm KV cache를 CXL memory tier로 오프로드하거나 여러 가속기 간 coherent하게 공유하는 아키텍처 여지 |

이 논문은 "KV cache 크기를 줄이는 것"과 "KV cache를 담을 chip 수를 늘리는 것" 두 축으로만 문제를 풀지만, 두 축 모두 결국 **"HBM은 비싸고 유한하다"**는 전제 위에 있다. 내 연구 방향은 이 전제 자체를 바꾸는 것 — CXL 같은 memory expansion/disaggregation 기술로 저렴하고 큰 memory tier를 가속기에 붙여서, 이 논문이 all-to-all 통신·batch-sharding으로 애써 아끼려는 KV cache 용량 문제를 다른 차원(메모리 계층 구조)에서 완화할 수 있는지가 잠재적 확장점이다. 다만 CXL의 낮은 대역폭·높은 latency 특성상 "매 스텝 다시 읽어야 하는" hot KV cache에는 그대로 쓰기 어렵고, 이 논문이 정량화한 memory-time vs compute-time 트레이드오프 모델을 활용해 "CXL tier로 옮겨도 latency budget 안에 들어오는 조건"을 analytic하게 도출하는 것이 접점이 될 수 있다.

---

## Open questions / gaps

- [ ] 이 논문은 KV cache가 항상 accelerator-local HBM에 있다고 가정한다 — CXL-attached memory나 host memory로의 오프로드/계층화(memory tiering)는 다루지 않음
- [ ] Multi-tenant 환경에서 여러 요청이 KV cache를 부분적으로 공유(prefix caching)하는 경우의 파티셔닝은 다루지 않음
- [ ] 통신 토폴로지가 TPU v4 3D torus에 특화되어 있음 — CXL fabric이나 다른 상호연결 토폴로지(스위치 기반, PCIe 기반)에서 동일한 analytic cost model이 어떻게 바뀌는지는 미해결
- [ ] Activation quantization, MoE sparsity 등은 "hopeful future work"로만 언급되고 실증하지 않음(p.9, p.14)
- [ ] 노드 장애·coherence 유지 비용(예: 여러 chip에 분산된 KV cache shard의 신뢰성/일관성)은 논의되지 않음 — 순수 성능 최적화 관점

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | Aminabadi et al. | DeepSpeed Inference: Enabling Efficient Inference of Transformer Models at Unprecedented Scale (arXiv:2207.00032, 2022) | GPU HBM 밖으로(CPU/NVMe) KV cache/weight를 오프로드하는 기존 사례 — 내 CXL tiering 아이디어의 가장 가까운 선행연구 |
| ☐ | Shazeer | Fast Transformer Decoding: One Write-Head is All You Need (arXiv:1911.02150, 2019) | Multiquery attention의 원 논문 — KV cache 축소 알고리즘의 근원 |
| ☐ | Dao et al. | FlashAttention: Fast and Memory-Efficient Exact Attention with IO-Awareness (arXiv:2205.14135, 2022) | Memory-IO-aware attention 구현 — HBM 접근 최소화 설계 철학이 내 방향과 유사 |
| ☐ | Rajbhandari et al. | ZeRO: Memory Optimizations Toward Training Trillion Parameter Models (SC20, 2020) | Memory offload/partitioning 개념의 원류, 학습용이지만 memory-hierarchy 사고방식 참고 |
| ☐ | Zheng et al. | Alpa: Automating Inter- and Intra-Operator Parallelism for Distributed Deep Learning (arXiv:2201.12023, 2022) | 파티셔닝 탐색을 integer program으로 일반화 — 이 논문의 analytic 접근과 대조되는 자동 탐색 접근 |
| ☐ | Xu et al. | GSPMD: General and Scalable Parallelization for ML Computation Graphs (arXiv:2105.04663, 2021) | 파티셔닝 표기법(subscript notation)의 기반이 된 시스템 |
| ☐ | Wang et al. | Overlap Communication with Dependent Computation via Decomposition in Large Deep Learning Models (ASPLOS 2023) | Looped CollectiveEinsum 원 논문 — 통신/계산 오버랩 저수준 기법 |
| ☐ | Fedus, Dean, Zoph | A Review of Sparse Expert Models in Deep Learning (arXiv:2209.01667, 2022) | Conclusions(p.14)에서 언급된 MoE sparsity가 KV cache/FLOPs 문제를 어떻게 완화하는지 확인 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
