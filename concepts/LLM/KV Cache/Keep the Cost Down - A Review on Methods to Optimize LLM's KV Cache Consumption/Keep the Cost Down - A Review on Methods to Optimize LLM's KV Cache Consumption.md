# Keep the Cost Down - A Review on Methods to Optimize LLM's KV Cache Consumption

> **Source PDF**: [Keep the Cost Down - A Review on Methods to Optimize LLM's KV Cache Consumption.pdf](Keep the Cost Down - A Review on Methods to Optimize LLM's KV Cache Consumption.pdf)
> **NodeGraph**: [KeepCostDown.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/concepts/LLM/KV%20Cache/Keep%20the%20Cost%20Down%20-%20A%20Review%20on%20Methods%20to%20Optimize%20LLM%27s%20KV%20Cache%20Consumption/KeepCostDown.html)
> **Authors**: Shi Luohe, Zhang Hongyi (Wuhan University, National Engineering Research Center for Multimedia Software), Yao Yao, Zhao Hai (Shanghai Jiao Tong University, Dept. of CS&E), Li Zuchao* (Wuhan University, corresponding author)
> **Venue / Year**: COLM 2024 (Conference on Language Modeling)
> **arXiv / DOI**: arXiv:2407.18003v4 [cs.CL] (20 Nov 2024)
> **Length**: 19 pages
> **Read status**: ☑ Full read (2026-07-06)
> **My reading purpose**: KV Cache 최적화 landscape 전체를 조망하고, 그중 memory-system architecture(CXL, disaggregation, tiering)와 교집합 있는 부분을 식별하기 위해

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

LLM 추론에서 KV Cache는 attention 재계산을 막아 토큰 생성 시간복잡도를 quadratic에서 linear로 바꿔주지만, 그 대가로 시퀀스 길이에 비례해 GPU 메모리를 선형으로 잡아먹는다. 이 논문은 이 KV Cache 메모리 문제를 해결하는 기법들을 **training stage(아키텍처 변경: MQA/GQA/cross-layer reuse/MLA), deployment stage(서빙 프레임워크: PagedAttention, 분산/오프로딩), post-training stage(inference-time 최적화: eviction/merging/quantization)** 3단계로 나누어 체계적으로 정리한 survey다. 마지막으로 long-context 평가 지표(throughput/latency, per-token GPU memory, perplexity)까지 정리하고, "KV Cache를 아예 외부 storage medium에 저장하는" 미래 방향을 제안하며 끝난다.

---

## Core thesis

> "KV Cache will increase linearly with the length of the sequence, and the memory required will become larger and larger, especially for giant models like GPT-3." (p.1-2)

추가 설명: KV Cache는 추론 효율을 위한 핵심 메커니즘이지만 그 자체가 새로운 메모리 병목이 된다. 이 논문은 이 병목을 "모델을 언제(when) 건드리느냐"라는 축으로 분류한다 — pre-training 시점(가장 강력하지만 재훈련 필요), 배포 프레임워크 시점(모델은 안 건드리고 시스템만 최적화), 추론 도중 시점(가장 유연, on-the-fly). 세 단계 모두 결국 "K/V 중 무엇을 얼마나 압축·삭제·이동시킬 것인가"의 변주라는 것이 핵심 통찰이다.

---

## Why this matters to me

이 논문은 순수 알고리즘 압축 리뷰가 아니라, **§4 Deploy-Stage Optimization**에서 명시적으로 memory-system 관점의 기법들을 다룬다: PagedAttention(vLLM)의 가상메모리식 paging, DistAttention/DistKV-LLM의 cross-server 분산 배치, ChunkAttention/CachedAttention의 계층적 storage device 활용, 그리고 Jin et al./Lee et al.의 KV Cache CPU 오프로딩(speculative reload)이 그것이다(p.5). 특히 References의 Qin et al. (2024) "Mooncake"는 **"KV-cache-centric disaggregated architecture for LLM serving"**이라는 표현을 논문 제목에 그대로 쓰는데, 이는 내 CXL memory disaggregation 연구 방향과 거의 1:1로 겹치는 문제의식이다. 즉 KV Cache는 "GPU capacity가 부족할 때 어디로, 어떤 latency/bandwidth trade-off로 밀어낼 것인가"라는 tiered-memory 문제의 새로운 워크로드(workload)로 볼 수 있다 — 내 방향이 다루는 CXL expander/disaggregated pool이 정확히 이 KV Cache offload target이 될 수 있다는 뜻이다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Abstract | — | p.1 | KV Cache 압축법을 training/deployment/inference 단계로 나눠 리뷰 |
| 1 | Introduction | p.1-2 | Quadratic→linear attention의 대가가 선형 증가하는 메모리; 저 memory bandwidth GPU에서 병목 심화 |
| 2 | Preliminary and Notations | p.2-4 | LLM/Attention/MHA 수식 정의, Figure 1(전체 개요), Figure 2(MHA/MQA/GQA 비교) |
| 3 | Training Stage Optimization | p.4-5 | MQA, GQA, cross-layer reuse(CLA/YOCO/GoldFinch), MLA(저랭크), CEPE(encoder 압축) |
| 4 | Deploy-Stage Optimization | p.5 | PagedAttention/vLLM, DistAttention/DistKV-LLM, ChunkAttention, CachedAttention(계층 storage), CPU 오프로딩(InfiniGen 등), FastDecode |
| 5 | Post-Training Optimizations | p.6-9 | 5.1 Eviction(static: window/attention sink; dynamic: TOVA/H2O/PyramidInfer/Keyformer/FastGen/SparQ) & Merging(DMC/KVMerger/Anchor-LLM); 5.2 Quantization(KVQuant/LESS/MiKV/QAQ/GEAR/FlexGen/WKVQuant) |
| 6 | Evaluation | p.9-10 | Long-context 벤치마크(LongBench 등), key retrieval(Needle-in-Haystack, RULER), few-shot; 지표(per-token GPU memory, throughput/latency, PPL) |
| 7 | Key Takeaways | p.10 | Deletion vs compression trade-off; **KV Cache를 다른 storage medium에 저장하는 극단적 방향 제안** |
| 8 | Conclusion | p.10 | 리뷰 요약 |
| A | Popular Models with GQA | p.17-18 | Table 2 — 실제 오픈소스 LLM들의 GQA/MoE/파라미터/KV Cache 비율 R 정리 |
| B | Dataset Examples | p.18-19 | Passkey retrieval / Needle-in-Haystack 예시 |
| C | Other Methods | p.19 | Linear-Transformer(KV Cache 완전 제거), Prompt/Embedding 압축(LLMLingua 등) |

---

## Section notes

### §1 Introduction (p.1-2)

KV Cache는 auto-regressive decoding에서 매 토큰마다 반복되는 attention 계산을 캐싱해 시간복잡도를 quadratic에서 linear로 낮추지만, 시퀀스 길이에 비례해 메모리를 소비한다. 저자들은 GPU가 컴퓨팅 속도 대비 memory bandwidth가 낮다는 점(Yu et al., 2022 인용)을 KV Cache 문제의 근본 원인으로 지목한다. 또한 대화(dialogue)마다 개별 KV Cache를 가지므로 재사용이 어렵다는 점도 지적한다.

> "different dialogues can hardly reuse them, which will become a bottleneck in the generation speed on modern inference hardware like GPU as they usually suffers from the low memory bandwidth comparing to their computing speed" (p.2)

### §2 Preliminary and Notations (p.2-4)

LLM을 $\mathcal{LLM}(X) = p_1p_2\ldots p_n$ 형태로 정의하고, Self-Attention/MHA의 수식(Formula 1, 2)을 통해 KV Cache가 정확히 어떤 텐서(K, V)를 저장하는지 formalize한다. Figure 1(p.3)은 전체 리뷰의 지도 역할을 하며 training/post-training/deployment 세 축을 시각화한다. Figure 2(p.4)는 MHA→MQA→GQA로 갈수록 key/value head 수가 줄어드는 구조를 비교한다.

### §3 Training Stage Optimization (p.4-5)

Pre-training 단계에서 아키텍처 자체를 바꾸는 것이 가장 강력하지만 재훈련 비용이 크다. MQA(Shazeer, 2019)는 모든 query head가 key/value head 1개를 공유해 KV Cache를 $1/n_h$로 줄인다. GQA(Ainslie et al., 2023)는 그 중간 지점으로, $n_g$개 그룹으로 나눠 $n_g/n_h$ 비율로 절감하며 efficiency-performance 균형을 조절 가능하게 한다($\eta = 0.5 + 0.5 \cdot n_g/n_h$). Cross-layer reuse(CLA, YOCO, GoldFinch)는 레이어 간 KV Cache를 공유하지만, 저자들은 "이런 방법들은 memory bandwidth 병목을 최적화하지 않으며, cross-layer reuse는 scattered access를 유발해 세심한 CUDA 커널 없이는 실제 이득이 없을 수 있다"(p.4)고 경고한다. MLA(DeepSeek-AI, 2024)는 저랭크 압축으로 KV를 압축·복원하지만 여전히 확장된 대형 벡터를 메모리 대역폭으로 옮겨야 하는 문제는 남는다.

### §4 Deploy-Stage Optimization (p.5)

이 섹션이 memory-system 관점에서 가장 중요하다. 저자들은 KV Cache의 시스템적 문제를 두 가지로 요약한다: (1) 반복적인 메모리 할당/해제로 인한 fragmentation, (2) batch 처리 불가능성으로 인한 memory bandwidth 병목.

> "From the perspective of inference systems, KV Cache presents significant challenges in two key areas. First, ... memory is repeatedly allocated and released, resulting in substantial fragmentation. Second, the inability to batch-process KV Cache exacerbates severe memory bandwidth bottlenecks" (p.5)

PagedAttention(Kwon et al., 2023, vLLM)은 OS의 page 메모리 기법을 GPU에 적용해 fragmentation을 거의 없앤다. DistAttention/DistKV-LLM(Lin et al., 2024)은 이를 발전시켜 **multi-server 분산 배포**를 가능케 한다. ChunkAttention(Ye et al., 2024)은 dictionary tree로 대화 간 KV Cache의 longest common prefix를 찾아 재사용한다. Gao et al.(2024, CachedAttention)은 **계층적 다중 storage device**를 활용하는 시스템으로 이를 확장한다. Jin et al./Lee et al.(2024, InfiniGen 계열)은 KV Cache 대부분을 CPU로 오프로드하고 critical한 일부만 GPU에 speculative하게 재로드한다. He & Zhai(2024, FastDecode)는 CPU와 GPU가 attention 계산을 협력 수행하는 방식을 제안한다. Qin et al.(2024, Mooncake)은 기업 관점의 종합 기술 보고서다.

### §5 Post-Training Optimizations (p.6-9)

**5.1 Eviction and Merging (p.6-7)**: Eviction은 static policy(recent tokens만 유지하는 sliding window; Xiao et al./Han et al.의 attention sink — 초기 토큰이 항상 높은 attention을 받는다는 관찰)와 dynamic policy(attention 가중치 기반: TOVA, H2O, PyramidInfer, Keyformer, FastGen, SparQ Attention)로 나뉜다. Liu et al.(2023)이 발견한 **Repetitive Attention Pattern**("이전 스텝에서 중요했던 토큰은 이후에도 중요할 것"이라는 가설의 근거)이 dynamic eviction 전체의 이론적 기반이다. Merging(DMC, KVMerger, Anchor-LLM)은 hard eviction 대신 soft한 방식으로 여러 토큰의 K/V를 합친다.

**5.2 Quantization (p.7-9)**: Full quantization(모델 가중치+KV Cache 모두, 예: FlexGen)과 KV-only quantization(KVQuant, LESS, MiKV, QAQ, GEAR, WKVQuant)으로 나뉜다. KVQuant(Hooper et al., 2024)는 key 행렬의 outlier channel 문제를 다루기 위해 per-channel(Key)/per-token(Value) quantization + RoPE 왜곡 보정을 결합한다. Table 1(p.8)은 LLaMA2-7B/13B에서 GEAR/WKVQuant/QAQ가 FP16 baseline 대비 GSM8k/MMLU/BBH 등 성능을 거의 유지하면서 quantization한 결과를 보여준다(2-bit 극한까지).

### §6 Evaluation (p.9-10)

Long-context 벤치마크(LongBench, ZeroSCROLLS, L-Eval, BAMBOO, XL²bench, InfiniteBench, LooGLE)는 4,000~20,000 토큰, 최대 200,000 토큰까지 다루지만 "모델별 지식 요구, 고정된 텍스트 길이, 주관적 채점"이라는 한계가 있다(p.9). Key retrieval(Needle in a Haystack, Passkey Retrieval, RULER)은 더 단순하고 통제 가능하지만 "너무 simplistic하다"는 비판이 있다. 평가지표로 **per-token GPU-memory usage**(LLaMA2-7B는 토큰당 이론상 0.5MB, 단 실제 fragment까지 고려해 측정해야 함, p.9), throughput/latency, perplexity(Formula 3, ANLL 기반)를 제시한다.

### §7 Key Takeaways (p.10)

저자들이 직접 제시하는 미래 방향이 이 리뷰의 백미다. Deletion vs compression의 trade-off(삭제는 즉각적 메모리 확보지만 성능 손실 위험, 압축은 정보 보존 지향)를 정리한 뒤, 더 급진적인 방향을 제안한다.

> "A more radical approach could involve storing the KV Cache externally, possibly on a different storage medium. This method would transform KV Cache management into a retrieval challenge, where the relevant KV pairs are fetched and reintegrated into the model as needed." (§7, p.10)

### §8 Conclusion (p.10)

리뷰 전체를 요약하며 KV Cache 최적화가 "환경적으로도 책임 있는(environmentally responsible)" LLM을 위한 핵심 영역이라고 마무리한다.

### Appendix A (p.17-18)

Table 2에서 Grok1, DBRX, Gemma, DeciLM, Phi-2, Deepseek, Qwen1.5, Yi, Mixtral, Mistral, GLM2/3, LLaMA2 등 실사용 오픈소스 모델의 GQA 채택 여부와 $\mathcal{R}$(토큰당 KV Cache 크기 / embedding 벡터 크기 비율)을 비교한다. GQA 채택 모델은 $\mathcal{R}$이 3.5~20 수준으로, 미채택 모델(64~80)보다 훨씬 낮다.

### Appendix C (p.19)

KV Cache를 아예 없애는 접근(Linear-Transformer: RetNet, RWKV, Mamba 등 softmax 제거로 RNN급 선형 복잡도 달성)과, prompt/embedding 자체를 압축해 KV Cache 필요 시퀀스 길이를 줄이는 접근(LLMLingua, gist token)을 briefly 다룬다.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "KV Cache will increase linearly with the length of the sequence, and the memory required will become larger and larger"
- "converting the time complexity of token generation from quadratic to linear, albeit with increased GPU memory overhead"
- "transform KV Cache management into a retrieval challenge"

**Technical concepts:**
- "Multi-Query Attention (MQA)" / "Grouped Query Attention (GQA)" / "Multi-Head Latent Attention (MLA)"
- "PagedAttention" / "DistAttention" / "cross-server inference"
- "KV-cache-centric disaggregated architecture" (Mooncake, ref)
- "hierarchical system that utilizes multiple storage devices"
- "speculatively reloading" (CPU→GPU KV Cache)
- "per-token GPU-memory usage"

**Value language:**
- "critical focus for enhancing LLMs' performance with longer contexts"
- "storage and retrieval technologies might become as crucial as the computational models themselves"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "Keep the Cost Down" (이 논문 제목 자체의 캐치프레이즈)
> - "Awesome-KV-Cache" (저자들의 GitHub repo 이름)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §6.2, p.9 | LLaMA2-7B는 "theoretically occupies 0.5MB of memory for each KV Cache entry" | KV Cache가 얼마나 빠르게 GPU 메모리를 소진하는지 정량적으로 보여주는 motivation 문구 |
| §6.1, p.9 | Long-context 벤치마크는 "4,000 to 20,000 tokens", 일부는 "200,000 tokens"까지 | LLM 컨텍스트 길이 스케일 인용 시 |
| Appendix A, Table 2, p.18 | GQA 채택 모델의 $\mathcal{R}$(KV Cache/embedding 비율) 3.5~20 vs 미채택 모델 64~80 | GQA의 메모리 절감 효과를 수치로 인용 |
| Table 1, p.8 | GEAR/WKVQuant/QAQ가 2-bit 근처까지 quantization해도 GSM8k/MMLU 등 정확도 거의 유지 | KV Cache quantization의 성숙도를 인용할 때 |
| Ref (Hooper et al., 2024) 제목 | "KVQuant: Towards 10 million context length LLM inference with KV cache quantization" | 초장문 컨텍스트 목표 수치로 인용 가능 |

---

## 🎯 Strategic anchor

> "A more radical approach could involve storing the KV Cache externally, possibly on a different storage medium. This method would transform KV Cache management into a retrieval challenge, where the relevant KV pairs are fetched and reintegrated into the model as needed. While this could reduce memory usage on primary devices, it would introduce complexities in retrieval and integration processes. ... Future Directions in Storage and Retrieval Technologies: These discussions point towards an evolving future where storage and retrieval technologies might become as crucial as the computational models themselves." (§7 Key Takeaways, p.10)

→ **본인 활용**: 면담·자소서에서 "이 review 논문이 §7에서 '급진적으로는 KV Cache를 별도 storage medium에 저장하고 retrieval 문제로 전환할 수 있다'고 명시적으로 미래 방향을 제시했는데, 바로 그 지점 — GPU 1차 메모리 바깥의 storage/retrieval tier를 어떻게 설계할 것인가 — 이 CXL 기반 memory disaggregation·tiering 연구가 답할 수 있는 문제"라는 식으로, 저자들이 스스로 열어둔 공백을 내 연구 방향의 진입점으로 사용할 수 있다.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | LLM inference 전체(training/deploy/inference 3단계)의 KV Cache 최적화 기법을 폭넓게 survey | Memory-system architecture(CXL, disaggregation, coherence, tiered memory) 자체의 설계·구현 |
| Mechanism | 대부분 알고리즘적(MQA/GQA/MLA, eviction/merging/quantization) — 압축·삭제로 "메모리 요구량 자체"를 줄임 | 하드웨어/시스템 계층에서 "메모리 요구량이 있을 때 어디로/어떤 latency-bandwidth로 옮길지"를 설계 — CachedAttention·InfiniGen·Mooncake류가 겹치는 지점 |
| Workload | LLM 추론의 KV Cache — 순차적 append, prefix 재사용 가능, read-heavy | 범용 memory workload — 다만 KV Cache는 "capacity-bound, append-only, prefix-shareable"이라는 뚜렷한 access pattern을 가진 특수 워크로드로서 CXL tier 설계의 흥미로운 실험 대상이 됨 |
| Open space | 저자들 스스로 "외부 storage medium + retrieval"을 미해결 미래 방향으로 명시(§7, p.10); Deploy-stage 섹션의 CPU offload/분산 서빙은 ad-hoc 시스템 수준 해법에 머묾 | CXL expander/pool을 KV Cache의 두 번째 tier로 정식화하고, coherence·bandwidth 보장 하에서 speculative reload(InfiniGen 등)를 하드웨어 수준에서 지원하는 설계는 미개척 |

이 논문의 스코프는 "KV Cache 자체를 작게 만드는 법"에 집중되어 있고, 내 연구 방향은 그보다 한 단계 아래, "작아지지 않는 나머지를 어디에 어떻게 배치할 것인가"라는 메모리 시스템 문제에 있다. §4(Deploy-Stage)의 PagedAttention→DistAttention→CachedAttention→CPU offload로 이어지는 흐름은 정확히 "capacity가 부족할 때 tier를 늘려나가는" 궤적이며, 이 tier 확장의 다음 논리적 단계가 CXL 기반 memory pool/disaggregation이라는 점에서 내 연구는 이 survey가 그린 지도의 자연스러운 확장선상에 위치한다.

---

## Open questions / gaps

- [ ] Deploy-stage의 CPU offload/분산 서빙 기법들(InfiniGen, DistKV-LLM, CachedAttention, Mooncake)이 CXL 같은 coherent memory fabric 위에서 구현됐을 때의 latency/bandwidth 이득이 정량적으로 다뤄지지 않음 — 순수 network-attached(NVMe/Ethernet) 오프로딩만 논의됨
- [ ] "KV Cache를 외부 storage medium에 저장"(§7)이라는 제안이 구체적 하드웨어·인터커넥트 없이 추상적으로만 제시됨 — CXL이 그 storage medium의 구체적 후보가 될 수 있는지에 대한 논의 부재
- [ ] Cross-layer reuse(CLA/YOCO)가 "scattered access를 유발해 세심한 CUDA 커널 없이는 이득이 없을 수 있다"(p.4)고 지적하는데, 이 scattered access 패턴이 CXL 같은 memory tier에서는 오히려 access pattern 최적화(coalescing, prefetching)의 여지가 있는지 미탐구
- [ ] Multi-tenant/multi-server 환경(DistKV-LLM)에서 KV Cache의 cache coherence 문제(내 방향의 핵심 주제)는 이 리뷰에서 전혀 언급되지 않음 — 여러 서버가 KV Cache를 공유/재사용할 때 일관성 보장 메커니즘은 열린 문제로 보임

---

## References worth following up

| 상태 | Ref | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | Qin et al. (2024) | Mooncake: A KVCache-centric disaggregated architecture for LLM serving | 제목부터 내 연구 방향과 직결. Disaggregation의 실제 프로덕션 사례 |
| ☐ | Lin et al. (2024) | Infinite-LLM: DistAttention / DistKV-LLM — distributed KV cache across servers | Cross-server KV Cache 배치의 구체적 메커니즘 |
| ☐ | Gao et al. (2024, USENIX ATC) | CachedAttention — hierarchical multi-storage-device KV cache | Tiered storage를 실제로 다루는 시스템 논문 |
| ☐ | Lee et al. (2024, OSDI) | InfiniGen — dynamic KV cache management with CPU offload + speculative reload | CPU-GPU tiering + speculation의 구체적 설계 |
| ☐ | He & Zhai (2024) | FastDecode — CPU-GPU collaborative attention computation | Heterogeneous memory/compute 협력 패턴 |
| ☐ | Kwon et al. (2023, SOSP) | PagedAttention / vLLM | Fragmentation 해결의 원조 — OS paging 유추가 CXL 논의에도 참고될 수 있음 |
| ☐ | DeepSeek-AI et al. (2024) | DeepSeek-V2 / Multi-Head Latent Attention (MLA) | 최신 산업 모델의 저랭크 KV 압축 실제 채택 사례 |
| ☐ | Sun et al. (2024) | YOCO — decoder-decoder, cross-layer KV reuse | Cross-layer reuse의 memory access pattern 이해에 참고 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
