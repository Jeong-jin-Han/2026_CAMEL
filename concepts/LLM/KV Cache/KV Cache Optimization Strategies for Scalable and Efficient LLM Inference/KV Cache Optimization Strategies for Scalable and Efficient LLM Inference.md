# KV Cache Optimization Strategies for Scalable and Efficient LLM Inference

> **Source PDF**: [KV Cache Optimization Strategies for Scalable and Efficient LLM Inference.pdf](KV%20Cache%20Optimization%20Strategies%20for%20Scalable%20and%20Efficient%20LLM%20Inference.pdf)
> **Authors**: Yichun Xu (Dell Technologies), Navjot K. Khaira (Dell Technologies), Tejinder Singh (Dell Technologies)
> **Venue / Year**: arXiv preprint, 2026-03-24 (survey, cs.LG)
> **arXiv / DOI**: arXiv:2603.20397v1
> **Length**: 24 pages
> **Read status**: ☑ Full read (2026-07-06)
> **My reading purpose**: LLM inference 시스템 최신 동향(KV cache 메모리 병목) 파악 + 메모리 시스템 아키텍처(CXL, tiered memory) 연구 방향과의 접점 탐색

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

Transformer 기반 LLM의 KV cache는 autoregressive 생성 시 과거 토큰의 key/value를 재계산하지 않도록 저장해두는 핵심 최적화이지만, 그 크기가 context length에 선형으로 증가하면서 GPU 메모리 용량·대역폭·throughput의 병목이 된다(§2, p.2-4). 이 논문은 개별 기법 하나만 파고들거나 반대로 얕게만 훑는 기존 서베이들과 달리, 최근 KV cache 최적화 기법들을 **cache eviction, cache compression, hybrid memory solutions, new attention mechanisms, combination methods**의 5개 축으로 체계적으로 taxonomy화하고(Fig.5, p.5), 각 기법의 메커니즘·trade-off·정량적 성능(메모리 절감, throughput, accuracy loss)을 비교 분석한 뒤(§4, Table 6, p.18-20), 7가지 실전 배포 시나리오(초장문맥 단일 요청, 최소 모델 수정, 고throughput 서빙, edge, multi-turn, prefill-heavy, accuracy-critical reasoning, 하드웨어 제약)에 기법을 매핑해 실무자에게 실행 가능한 가이드를 제공한다(§5, p.20-22).

---

## Core thesis

> "Our analysis reveals that no single technique dominates across all settings; instead, the optimal strategy depends on context length, hardware constraints, and workload characteristics, pointing toward adaptive, multi-stage optimization pipelines as a promising direction for future research." (Abstract, p.1)

추가 설명: 이 논문의 메시지는 "하나의 만능 KV cache 최적화는 없다"는 것. Eviction·compression은 초장문맥·edge에서 강하고, hybrid memory(offloading)는 고throughput 데이터센터 서빙에서 강하며, 새로운 attention(linear/log-linear)은 아직 accuracy-critical한 곳엔 부적합하다는 식으로 축별로 우위 영역이 갈린다(§6, p.22). 결론적으로 미래 방향은 이들을 상황에 맞게 조합하는 **adaptive, multi-stage pipeline**이라고 제안한다.

---

## Why this matters to me

이 논문의 §3.3 Hybrid Memory Solution(p.11-14)이 내 연구 방향(CXL 기반 memory disaggregation, tiered memory, coherence)과 가장 직접적으로 맞닿아 있다. 저자들은 "hybrid memory solutions address the limitations of GPU memory by leveraging multi-tier storage architectures"라고 명시적으로 문제를 **multi-tier storage architecture** 문제로 프레이밍하는데(p.11), 이는 내가 다루는 CXL 메모리 확장·계층화 문제와 동일한 형태다. 다만 이 논문이 다루는 모든 hybrid memory 기법(PagedAttention, InfiniGen, LayerKV, KVPR, Oneiros, CLO)은 offload 대상을 **CPU DRAM(PCIe 경유)** 으로만 한정하고, INF2만 유일하게 near-storage CSD(SSD+FPGA)로 확장한다(Table 4, p.13) — CXL이나 memory pooling, coherence protocol은 전혀 언급되지 않는다. 즉 이 서베이가 그리는 "GPU-CPU 2-tier, PCIe-bound" 세계관은, 내가 하려는 "CXL로 coherent하게 확장된 byte-addressable pooled memory" 세계관의 전 단계에 해당하며, 이 논문에 나온 offload/prefetch 시스템들(InfiniGen의 predictive prefetching, CLO의 zero-copy GDRCopy, Oneiros의 parameter remapping)이 왜 PCIe 대역폭(GH200 기준 450-900GB/s, p.21)에 근본적으로 발목 잡히는지, 그리고 그 한계를 CXL fabric이 어떻게 완화할 수 있는지를 구체적으로 논증할 근거 자료가 된다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1-2 | Context window 급증 → KV cache 관리가 first-order 과제. 개별 논문/얕은 서베이 대비 middle-ground 서베이임을 표방 |
| 2 | Background | p.2-5 | Context length, KV cache 수식(식 1,2), attention score(식 3-5), 5축 taxonomy(Fig.5, Table 1) |
| 3.1 | Cache Eviction | p.5-8 | H2O, SnapKV, NACL, InfiniPot, HASHEVICT, MorphKV, RocketKV, KVzip, Ada-KV (Table 2) |
| 3.2 | Cache Compression | p.8-11 | KIVI, MiniCache, PALU, KVQuant (Table 3) |
| 3.3 | Hybrid Memory Solution | p.11-14 | PagedAttention, InfiniGen, LayerKV, INF2, KVPR, Oneiros, CLO (Table 4) |
| 3.4 | New Attention Calculation | p.13-16 | Linear Attention, Log-Linear Attention, Local Linear Attention, KIMI Linear (Table 5) |
| 3.5 | Combination Methods | p.16-18 | FlexGen, Q-Hitter, ShadowKV, TailorKV |
| 4 | Comparative Analysis | p.18-20 | Table 6 — 전 기법 memory/speedup/accuracy loss/tradeoff 정량 비교 |
| 5 | Scenarios | p.20-22 | 7개 배포 시나리오별 기법 추천 |
| 6 | Summary | p.22 | Eviction/compression = ultra-long-context, hybrid memory = 고throughput 서빙, new attention = 미래 아키텍처 |

---

## Section notes

### §1-2 Introduction & Background (p.1-5)

KV cache는 각 토큰의 key/value projection을 저장해 재계산을 피하는 Transformer 고유 최적화지만(Fig.1-2, p.2-3), 크기가 `KV_per_token = 2 × H × D × B × L`로 고정되고 `KV_cache_size = KV_per_token × ContextLength`로 context length에 선형 비례한다(식 1-2, p.2). Fig.3(p.3)은 LLaMA-2 7B/13B/70B-GQA 모델의 KV cache 메모리를 context length 함수로 그려, 128K 토큰에서 7B 모델 KV cache가 약 64GB로 A100 VRAM 한계에 근접/초과함을 보여준다("At 128K tokens, a 7B model's KV cache (≈64GB) exceeds the capacity of an A100 GPU, illustrating the memory bottleneck that motivates KV cache optimization", p.3). Attention score의 non-uniform 분포(Fig.4, p.4 — "sweet"이 "apple"에 65% 집중)가 selective eviction의 근거가 된다. 5개 카테고리 taxonomy(Fig.5)와 optimization goal/tradeoff/representative method/적합 상황을 정리한 Table 1(p.5)이 이후 전체 구조의 지도 역할을 한다.

### §3.1 Cache Eviction (p.5-8)

Attention score 기반으로 덜 중요한 토큰을 버리는 방식. **H2O**(Heavy-Hitter Oracle)는 누적 attention score가 높은 "heavy-hitter" 토큰과 최근 토큰을 함께 유지하는 greedy 정책(p.6). **SnapKV**는 prefill 단계에서 observation window 내 voting으로 중요 토큰을 뽑고 1D pooling으로 주변 context까지 clustering해 유지한다(Fig.7, p.7) — "retain the features surrounding the selected attention features" (p.6). **NACL**은 proxy-token(예: 마지막 질문 부분) 기준으로 한 번에 single-shot eviction을 수행해 step-wise 방식보다 계산 부담을 줄인다(p.6). **InfiniPot**은 CaP(Catalyst Prompt)·NuC(Novelty under Compression) 두 지표로 "distillation" 비유를 써서 고정 메모리로 "무한" 문맥을 처리한다(p.6-7). **HASHEVICT**는 LSH(SimHash)로 attention 계산 전에 토큰 유사도를 근사해 pre-attention eviction을 수행하는 경량 기법(p.7). **MorphKV**는 최근 토큰의 attention 패턴(Sum/Max Fusion)으로 오래된 토큰의 관련성을 동적으로 재평가한다(p.7). **RocketKV**는 SnapKV식 coarse eviction(1단계)과 Hybrid Sparse Attention 기반 fine-grained 동적 선택(2단계)을 결합한다(p.7). **KVzip**은 query-agnostic하게 self-supervised reconstruction("Repeat the previous content")으로 토큰 중요도를 도출한다(p.8). **Ada-KV**는 헤드별로 균일하게 budget을 나누는 기존 방식의 한계를 지적하며, attention-sparse head에서 attention-dispersed head로 budget을 재분배하는 이론적 프레임워크를 제시한다(p.8).

### §3.2 Cache Compression (p.8-11)

주로 quantization을 통한 메모리 절감. **KIVI**는 key는 per-channel, value는 per-token 비대칭 양자화를 적용하고 residual은 full precision으로 유지(Fig.8, p.9) — key cache는 "there are a few fixed channels whose magnitudes are very large"라는 관찰에 기반한다(p.8-9). **MiniCache**는 층간(cross-layer) 중복을 이용, 인접 층의 KV 상태가 angular distance 기준으로 유사하면 SLERP로 병합한다(p.9). **PALU**는 projection weight matrix W를 SVD로 A×B 저차원 분해해 latent H만 caching하고 필요시 reconstruct하는 low-rank 압축(Fig.9, p.9-10). **KVQuant**은 per-channel key quantization + pre-RoPE quantization + sensitivity-weighted non-uniform quantization + per-vector dense-and-sparse(상위 1% outlier 별도 보존) + attention-sink-aware(첫 토큰 FP16 유지)를 결합해 최대 1천만 토큰 context를 지원한다(p.10-11).

### §3.3 Hybrid Memory Solution (p.11-14) — 나의 연구방향과 가장 직결

GPU 메모리 한계를 multi-tier storage architecture로 우회하는 접근. **PagedAttention**(vLLM)은 OS의 virtual memory paging에서 영감을 받아 KV cache를 고정 크기 block으로 나누고 block table로 관리, copy-on-write로 공유 시퀀스의 메모리 오버헤드를 줄인다(Fig.10, p.11). **InfiniGen**은 KV cache를 CPU 메모리에 두고 low-rank 근사 query(Partial Q, 식 6)로 다음 층에서 중요할 KV를 예측해 prefetch한다(Fig.11, p.12). **LayerKV**는 층 단위로 일부만 GPU에 남기고 나머지는 CPU로 offload, "offload time ≤ prefill time" 조건을 만족하도록 층 배치를 결정해 TTFT를 줄인다(p.12). **INF2**는 Computational Storage Devices(SSD+FPGA)를 이용, KV cache를 SSD에 직접 저장하고 attention 연산 자체를 storage 근처 accelerator에서 수행한다(p.12) — 이는 내가 이미 검토한 Smart-Infinity(near-storage processing)와 같은 계열의 아이디어. **KVPR**은 GPU의 부분 KV 재계산과 CPU→GPU 데이터 전송을 프로파일링 기반으로 동시에 진행해 GPU idle time을 줄인다(p.12). **Oneiros**는 비활성 모델의 파라미터를 GPU에서 잠시 치워("parameter remapping") 그 공간을 KV cache로 활용하는 multi-tenant 기법(p.12-13). **CLO**는 인접 디코딩 스텝의 query vector가 cosine 유사도가 높다는 "high temporal locality"에 착안해 KV cache 재사용 여부를 판단하고, GDRCopy 기반 zero-copy 전송과 GPU-centric 동기화로 PCIe 대역폭을 최대 활용한다(p.13).

### §3.4 New Attention Calculation (p.13-16)

Softmax attention의 O(T²) 복잡도를 근본적으로 바꾸려는 시도들. **Linear Attention**(Transformers-are-RNNs)은 kernel feature map φ로 softmax 유사도를 dot product로 근사, O(N) recurrence 형태로 재구성한다(식, p.14). **Log-Linear Attention**은 Fenwick-tree 구조로 과거 토큰을 log(t)개의 bucket으로 계층적 요약, O(N log N) 시간·O(log N) 메모리로 softmax와 linear attention 사이 middle ground를 제공한다(p.14). **Local Linear Attention(LLA)**은 attention을 국소 회귀(local regression)로 재해석, 각 query 주변에 작은 local linear model을 적합시킨다(p.14-15). **KIMI Linear**는 Kimi Delta Attention(KDA, forget gate α·update rate β로 memory state를 갱신, 식 p.15)을 도입하고 이를 full softmax attention과 3:1 비율로 hybrid하는 것이 "the optimal balance between efficiency and model accuracy"라고 밝힌다(p.15). Table 5(p.15)는 이들의 training/decoding time·space complexity를 정리한다.

### §3.5 Combination Methods (p.16-18)

단일 기법의 한계를 넘기 위해 복수 축을 결합. **FlexGen**은 GPU/CPU/disk 전체에 weight·activation·KV cache를 분산 배치하는 선형계획법 기반 cost model과 4-bit group-wise quantization, zig-zag block scheduling을 결합해 단일 GPU로 대형 LLM 서빙을 가능케 한다(p.16). **Q-Hitter**는 attention score와 quantization error를 함께 고려한 unified score로 sparse+quantized KV cache를 선택한다(p.17). **ShadowKV**는 pre-RoPE key에 SVD를 적용해 low-rank로 GPU에 유지하고 value는 CPU로 offload, landmark vector로 decoding 시 필요한 chunk만 선택적으로 fetch한다(Fig.13, p.17). **TailorKV**는 층별 attention 분포(top-k 집중도)를 기준으로 quantization-friendly 층(얕은 층, aggressive quantization)과 sparsity-friendly 층(깊은 층, CPU offload+동적 top-k fetch)으로 나눠 처리한다(Fig.14, p.17-18).

### §4 Comparative Analysis (p.18-20)

Table 6(3페이지에 걸침, p.18-20)이 이 논문의 핵심 자산 — 모든 기법의 memory reduction/speedup/accuracy loss/tradeoff를 한 표에 정리. 예: H2O 최대 5-10× 메모리 절감·29× throughput; KIVI 2.6× peak memory·2.35-3.47× throughput, <2% accuracy drop; LayerKV TTFT 최대 69× 개선; FlexGen 최대 10× 메모리·40-100× throughput(DeepSpeed/HF Accelerate 대비).

### §5 Scenarios (p.20-22)

7개 실전 시나리오별 추천: (5.1) 초장문맥 단일요청 → eviction/compression + KIMI Linear; (5.2) 최소 모델 수정 → Ada-KV/SnapKV/KIVI (tuning-free, plug-and-play); (5.3) 고throughput 서빙 → PagedAttention, Oneiros, ShadowKV, FlexGen/Q-Hitter; (5.4) edge/메모리 제한 → InfiniPot, TailorKV (Oneiros/PagedAttention은 부적합, 고대역폭 요구); (5.5) multi-turn 대화 → RocketKV-MT, KVzip, ShadowKV (H2O는 부적합 — 토큰 영구 삭제); (5.6) prefill-heavy → NACL, HASHEVICT, LayerKV, CLO; (5.7) accuracy-critical reasoning → PagedAttention 계열(hybrid memory, lossless)이 최선, eviction/compression·linear attention은 회피.

### §6 Summary (p.22)

Eviction/compression/combination이 100만 토큰급 ultra-long-context를 주도(RocketKV, KVzip, ShadowKV). Hybrid memory는 CPU offloading·block 기반 할당으로 물리적 GPU 한계를 넘는 데 필수적이며 고throughput/multi-tenant 데이터센터 서빙에서 강점. New attention(linear/log-linear/local linear/KIMI)은 asymptotic하게 우월하지만 재학습 필요·accuracy-critical reasoning에서 아직 열세. 결론: 미래는 context length·시스템 부하·하드웨어 제약에 동적으로 적응하는 **integrated, multi-stage KV optimization pipeline**.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "no single technique dominates across all settings"
- "adaptive, multi-stage optimization pipelines"
- "middle-ground perspective" (개별 논문 vs. 얕은 서베이의 중간)

**Technical concepts:**
- "multi-tier storage architecture" (KV cache offload를 memory hierarchy 문제로 재정의)
- "heavy-hitter tokens (H2)"
- "predictive prefetching" / "parameter remapping" / "zero-copy transfer engine"
- "Time to First Token (TTFT)" / "Time Per Output Token (TPOT)"
- "computational storage devices (CSDs)" / "attention-near storage"
- "per-channel / per-token quantization", "attention-sink-aware quantization"

**Value language:**
- "first-order challenge for scalable LLM deployment"
- "memory bottleneck that motivates KV cache optimization"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "Continual Context Distillation" (InfiniPot 고유 용어, 이 논문이 도입한 게 아니라 인용한 개념)
> - 논문 제목 자체의 "Scalable and Efficient LLM Inference" 프레이징을 그대로 자소서에 쓰면 echo로 보임

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §2, Fig.3, p.3 | "At 128K tokens, a 7B model's KV cache (≈64GB) exceeds the capacity of an A100 GPU" | Context 확장이 GPU 메모리 용량을 직접 위협한다는 motivation 인용 |
| §4, Table 6, p.18 | H2O: "Up to 5–10× memory reduction... Up to 29× throughput improvement" | Eviction 기법의 대표적 성능 수치 |
| §4, Table 6, p.19 | LayerKV: "Up to 69× TtFT improvement" (LosslessAccuracy) | Prefill/TTFT 최적화의 극단적 사례 |
| §4, Table 6, p.19 | KVQuant: "Towards 10 million context length" (원문 [20] 제목) | Ultra-long-context quantization의 스케일 인용 |
| §4, Table 6, p.20 | FlexGen: "40× to 100× higher maximum throughput compared to DeepSpeed Zero-Inference and HuggingFace Accelerate" | 단일 GPU 자원제약 환경의 offloading 효과 인용 |
| §3.3, p.21 | Oneiros/CLO는 NVIDIA GH200의 "high CPU–GPU bandwidth (450–900 GB/s)"에 의존 | PCIe/NVLink 대역폭이 hybrid memory offload의 근본 제약임을 보여주는 수치 — CXL 대역폭과 직접 비교 가능 |

---

## 🎯 Strategic anchor

> "Hybrid memory solutions address the limitations of GPU memory by leveraging multi-tier storage architectures to manage the KV cache efficiently. As context lengths grow, storing the entire KV cache on GPU becomes infeasible due to memory constraints and bandwidth bottlenecks. Hybrid approaches mitigate these challenges by offloading portions of the cache to slower but larger memory tiers, such as CPU memory, disk, or specialized accelerators." (§3.3, p.11)

→ **본인 활용**: 면담에서 "LLM 서빙 쪽에서도 이미 'multi-tier storage architecture'로 문제를 재정의하고 있는데, 이 논문에 나온 모든 hybrid memory 기법은 CPU DRAM을 PCIe로 접근하는 2-tier 구조에 갇혀 있다(p.11-13) — 제가 하려는 CXL 기반 memory disaggregation은 이 tier 자체를 coherent하고 byte-addressable하게 확장해서, LLM 추론 같은 워크로드가 이미 요구하고 있는 방향을 하드웨어 계층에서 근본적으로 풀어주는 것"이라는 논지로 사용 가능.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | LLM 추론 서빙에서의 KV cache 메모리 관리 (기존 GPU-CPU PCIe 계층 위의 알고리즘·시스템 co-design) | 메모리 시스템 아키텍처 자체 (CXL protocol/coherence, 여러 워크로드에 걸친 tiered memory fabric) |
| Mechanism | Application-level offloading/prefetching/compression, PCIe 위의 소프트웨어 스케줄링 (InfiniGen predictive prefetch, CLO zero-copy, Oneiros parameter remapping) | HW-level memory expansion/pooling, coherence protocol, address translation — CXL switch/fabric 설계 |
| Workload | LLM 추론(KV cache, attention)이 메모리 수요를 결정하는 유일한 축 | Workload-agnostic memory substrate — LLM 추론은 여러 소비자(HPC, DB, ML) 중 하나 |
| Open space | "CPU memory" offload는 전부 PCIe-attached, non-coherent, explicit-copy 가정 (§3.3 전체) — CXL·memory pooling·coherence는 전혀 언급 없음 | CXL-attached coherent pooled memory가 이 논문의 모든 hybrid memory 기법(InfiniGen, LayerKV, KVPR, Oneiros, CLO) 아래 공통 substrate로 들어가 PCIe bottleneck(p.21, GH200도 900GB/s 한계)을 완화할 여지 |

이 논문은 KV cache 메모리 문제를 "어떤 정보를 어디에 둘 것인가(알고리즘)"의 관점에서 풀지만, 나의 방향은 "그 '어디'가 어떤 물리적/coherence 특성을 가져야 하는가(아키텍처)"를 묻는다. 즉 이 서베이가 매핑한 7개 시나리오(§5) 중 특히 5.3(고throughput 서빙)·5.6(prefill-heavy)·5.8(하드웨어 제약)에서 요구하는 "낮은 offload overhead·높은 CPU-GPU 대역폭"은 CXL fabric이 정확히 겨냥하는 지점이며, INF2(CSD 기반 near-storage attention, p.12)는 Smart-Infinity류 near-storage processing과 CXL-SSD 교집합 연구로 직접 이어지는 연결고리다.

---

## Open questions / gaps

- [ ] CXL, memory pooling, coherence protocol이 이 서베이 전체(24페이지)에서 단 한 번도 언급되지 않음 — 모든 hybrid memory 기법이 PCIe-attached CPU DRAM만 가정
- [ ] Multi-GPU/multi-node에 걸친 KV cache 공유(disaggregated coherent memory 상에서)는 다루지 않음 — 전부 단일 노드 내 GPU-CPU 논의
- [ ] Oneiros/CLO가 요구하는 고대역폭(GH200 450-900GB/s, p.21)이 일반 하드웨어에선 병목이라는 점은 인정하지만, CXL 3.x switched fabric 같은 대안 토폴로지는 검토 안 됨
- [ ] KV cache를 pooled/disaggregated memory에 저장할 때의 reliability/fault-domain trade-off는 전혀 논의되지 않음
- [ ] INF2의 near-storage compute(CSD+FPGA)가 유일하게 "storage 근처 연산"을 다루지만, 이를 일반적인 disaggregated memory architecture 질문으로 확장하지 않음

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [22] | Kwon et al., "Efficient memory management for LLM serving with PagedAttention" (vLLM), 2023 | Hybrid memory 서베이 전체의 기반이 되는 foundational 시스템, OS paging 개념 직접 차용 |
| ☐ | [23] | Lee et al., "InfiniGen: Efficient generative inference of LLMs with dynamic KV cache management", 2024 | Predictive prefetching 메커니즘, CPU offload 대표 사례 |
| ☐ | [24] | Xiong et al., "LayerKV: Optimizing LLM serving with layer-wise KV cache management", 2024 | Layer-wise offload + SLO-aware scheduling, TTFT 69× 개선 |
| ☐ | [25] | Jang et al., "Inf²: High-throughput generative inference of LLMs using near-storage processing", 2025 | Computational Storage Device(CSD) 기반 near-storage attention — Smart-Infinity/CXL-SSD 교집합과 직접 비교할 대상 |
| ☐ | [27] | Li et al., "Oneiros: KV cache optimization through parameter remapping for multi-tenant LLM serving", 2025 | GH200 고대역폭 활용, parameter remapping이라는 새로운 memory reuse 패턴 |
| ☐ | [28] | Yi et al., "CLO: Efficient LLM inference system with CPU-light KVcache offloading via algorithm-system co-design", 2025 | 가장 최근(2025) 시스템, zero-copy(GDRCopy) + PCIe 대역폭 최대 활용 엔지니어링 |
| ☐ | [20] | Hooper et al., "KVQuant: Towards 10 million context length LLM inference with KV cache quantization", 2025 | Ultra-long-context quantization의 극단 사례, 10M 토큰 |
| ☐ | [15] | Kim et al., "KVzip: Query-agnostic KV cache compression with context reconstruction", 2025 | Query-agnostic reconstruction 기반 eviction, multi-turn에 특히 강점 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
