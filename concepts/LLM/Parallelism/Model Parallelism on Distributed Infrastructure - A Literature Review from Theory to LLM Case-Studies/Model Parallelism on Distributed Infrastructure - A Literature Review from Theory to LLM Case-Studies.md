# Model Parallelism on Distributed Infrastructure: A Literature Review from Theory to LLM Case-Studies

> **Source PDF**: [Model Parallelism on Distributed Infrastructure - A Literature Review from Theory to LLM Case-Studies.pdf](Model%20Parallelism%20on%20Distributed%20Infrastructure%20-%20A%20Literature%20Review%20from%20Theory%20to%20LLM%20Case-Studies.pdf)
> **NodeGraph**: [ModelParallelismReview.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/concepts/LLM/Parallelism/Model%20Parallelism%20on%20Distributed%20Infrastructure%20-%20A%20Literature%20Review%20from%20Theory%20to%20LLM%20Case-Studies/ModelParallelismReview.html)
> **Authors**: Felix Brakel (Univ. of Amsterdam / VU Amsterdam), Uraz Odyurt (Radboud Univ. / Nikhef), Ana-Lucia Varbanescu (Univ. of Twente / Univ. of Amsterdam)
> **Venue / Year**: arXiv preprint, 2024 (cs.DC)
> **arXiv / DOI**: arXiv:2403.03699v1
> **Length**: 10 pages
> **Read status**: ☑ Full read (2026-07-06)
> **My reading purpose**: LLM 시스템 배경지식 확보 — model parallelism 분류·용어를 익혀서, GPU 메모리·interconnect 제약이 왜 memory-system/CXL 연구와 맞닿는지 이해하기 위한 background reading

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

이 논문은 신경망, 특히 거대 Transformer(LLM)를 여러 device에 나눠 학습·추론시키는 **model parallelism**을 정리한 literature review다. 두 단계 snowballing 방법론으로 (1) DNN auto-parallelisation 논문들을 걸러 이론적 taxonomy(operator graph, intra-/inter-operator parallelism)를 세우고, (2) Megatron·Gopher·PaLM·GPT 계열의 실제 대규모 LLM 학습 사례를 수집해 어떤 parallelism 조합이 실전에서 쓰였는지 정리한다. 결론적으로 intra-operator(=tensor) parallelism은 통신량이 매우 커서 고속 interconnect(NVLink 등)가 있는 단일 노드 안에서만 쓰이고, inter-operator(=pipeline) parallelism은 노드 간 확장에 쓰이며, 대부분의 실전 시스템은 여기에 data parallelism까지 더한 hybrid 전략을 채택한다는 것을 실제 utilisation 수치와 함께 보여준다.

---

## Core thesis

> "Model parallelism then has the potential to meet the ever-growing demands computational demands of neural networks." (§1, p.1)

추가 설명: 논문은 model parallelism을 "operator graph를 partition해서 여러 device에 분산시키는 것"으로 정의하고, 이를 어떤 차원(dimension)을 활용하느냐(intra-operator vs inter-operator)와 어떻게 그 전략을 찾느냐(ad-hoc vs auto-parallelisation)라는 두 축으로 분류하는 taxonomy(Figure 3, p.4)를 제시한다. 이 taxonomy를 이론적 뼈대로 삼아 실제 multi-billion parameter LLM들이 어떤 조합을 택했는지(Table 2, p.6) 비교한다.

---

## Why this matters to me

내 연구 방향은 memory-system architecture(CXL, memory disaggregation, cache coherence, tiered memory, GPU memory scaling)이지 LLM 학습 자체는 아니다. 다만 이 논문은 왜 "device 간 interconnect/메모리 fabric"이 LLM 시스템에서 근본적 병목인지를 정량적으로 보여준다 — 예컨대 "even when only considering a single server ... the bandwidth is already a factor two below that of the A100's DRAM"(§1, p.1)이라는 서술은 NVLink조차 온-디바이스 DRAM 대역폭에 못 미친다는 점을 명시한다. 즉 model parallelism 연구가 왜 hybrid 전략(intra-operator는 고속 interconnect 있는 노드 내부에만, inter-operator는 노드 간 확장에)으로 수렴했는지를 이해하면, CXL 기반 memory/interconnect fabric이 개선되었을 때 이 partitioning 선택지 자체가 어떻게 바뀔 수 있는지(예: intra-operator parallelism을 노드 경계 너머로 확장 가능해질지)에 대한 motivation 배경을 얻을 수 있다. 이 논문 자체는 memory fabric/coherence를 다루지 않으며, 어디까지나 "compute/model이 어떻게 쪼개지는가"에 대한 background reading으로 활용한다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1 | 모델 규모 증가 → 계산/메모리 요구 증가 → model parallelism으로 대응, interconnect 대역폭이 DRAM보다 열등함을 지적 |
| 2 | Study Design | p.1-2 | 2단계 snowballing 방법론: (1) DNN auto-parallelisation 논문 필터링(22→8→7→6편), (2) Megatron/Gopher/PaLM/GPT 사례 수집 |
| 3 | Model Parallelism (배경/이론) | p.2-4 | operator graph 정의, intra-/inter-operator parallelism 구분, taxonomy(Figure 3) 제시 |
| 4 | Challenges | p.4-5 | inter-operator의 pipeline bubble, intra-operator의 높은 통신량, search-space/evaluation/search-method 문제 |
| 5 | Use-Cases | p.5-7 | Megatron/Gopher/PaLM/GPT의 실제 parallelism 조합과 hardware utilisation 수치 |
| 6 | Discussion | p.8-9 | ad-hoc vs auto 프레임워크 비교(Alpa, FlexFlow, RaNNC, FTPipe, TensorOpt 등), MFU vs HFU 지표 논의 |
| 7 | Conclusion / Future work | p.9-10 | 연구질문 재확인, DNN auto-parallelisation 표준화(NAS-Bench 유사) 필요성 제안 |

---

## Section notes

### §1 Introduction (p.1)

논문은 신경망이 커질수록 계산량과 메모리 footprint가 함께 증가한다는 기본 전제에서 출발한다. Model parallelism을 "모델을 partition해서 여러 device에 워크로드를 분산"시키는 해법으로 제시하되, 곧바로 그 한계를 짚는다: parameter와 activation data가 크기 때문에 device 간 통신이 병목이 된다는 것이다. 특히 인상적인 수치는 NVLink 대역폭이 A100의 DRAM 대역폭보다도 두 배 가까이 낮다는 지적으로, 이는 "왜 model parallelism이 memory-bound problem인가"를 단적으로 보여준다.

### §2 Study Design (p.1-2)

두 단계로 나뉜 방법론. Phase 1은 2023년 survey [16]을 seed로 한 snowballing으로 DNN auto-parallelisation 논문 22편을 code 공개 여부, 유지보수 여부, 완전 자동화 여부, 기존 파일 포맷 호환성 순으로 필터링해 최종 6편으로 좁힌다. Phase 2는 Figure 1(모델 크기 추이, source: [29])과 Table 1([8]에서 인용한 GPT-3/Gopher/Megatron-Turing/PaLM 비교표)을 seed로 삼아 실전 LLM parallelism 사례를 수집, 결국 구현 세부사항이 공개된 Megatron 계열 4편, Gopher 1편, PaLM 1편만 남긴다.

### §3 Model Parallelism (배경/이론) (p.2-4)

신경망을 **operator graph** O = (V, E)로 표현한다 — 노드는 operator(연산)나 tensor, 엣지는 데이터 흐름을 나타낸다. Parameter tensor(정적 입력)와 activation tensor(연산 결과)를 구분하고, forward pass(X→Y 계산)와 backward pass(back-propagation, forward의 activation에 의존)를 정의한다. 여기서 핵심 분류: **inter-operator parallelism**(그래프를 sub-graph로 나눠 device별 배정, 통신은 sub-graph 경계에서만 발생)과 **intra-operator parallelism**(단일 operator 내부의 연산을 device들에 분산, 통신량이 훨씬 큼). 산업계에서는 이를 각각 pipeline parallelism / tensor parallelism이라 부른다(Google TPU 진영 용어). 두 방식을 결합한 것이 hybrid parallelism이며, 전략을 찾는 방법에 따라 ad-hoc(모델·하드웨어를 미리 알고 설계) vs auto-parallelisation(범용 탐색)으로도 나뉜다. Checkpointing, algebraic transformation, quantization, pruning, distillation 등 다른 최적화 기법들과 병렬화는 상호보완적이라는 점도 짚는다(단, algebraic optimisation이 parallelism 기회를 해칠 수도 있다는 주의도 [31] 인용으로 언급, p.3).

> "model parallelism encompasses the strategies that utilise parallelisable dimensions within O, while data parallelism are those strategies that utilise parallelisable dimensions in the data" (§3.1.3, p.3)

### §4 Challenges (p.4-5)

Inter-operator parallelism의 근본 문제는 **pipeline bubble**이다 — 각 partition의 입력이 이전 partition의 출력이라 순차 의존성이 생기고, backward pass는 forward pass 완료 후에만 시작 가능해 device utilisation이 떨어진다(Figure 5c, p.5). Intra-operator parallelism의 문제는 반대로 극심한 통신량 — 매 batch마다 input tensor를 scatter, output tensor를 gather해야 한다. 두 방식 결합(hybrid) 시엔 기존 프레임워크가 "simple and suboptimal"([13] 인용)하다는 문제가 생긴다. Auto-parallelisation은 결국 탐색 문제로 환원되며, 그 하위 난제로 (1) search-space 정의(합법적/최적 전략만 포함), (2) strategy evaluation(통신 시간 예측이 특히 어려움 — compute/memory는 예측 쉬우나 network latency/bandwidth 모델링이 open challenge), (3) search method(탐색 알고리즘 자체의 다양성과 비교 곤란함)를 든다.

### §5 Use-Cases (p.5-7)

MLP(fully-connected → GeLU → fully-connected)를 기본 building block으로 삼아 Megatron 계열의 intra-operator 전략을 수식으로 설명한다: weight matrix A를 column-split하면 GeLU 비선형성 때문에 통신 없이 처리 가능하고, 이어서 B를 row-split하면 최종 reduction만 하면 되므로 이 조합이 최적이라는 것(Shoeybi et al. [28], p.6). Table 2(p.6)는 실전 수치를 집대성한다:
- **Megatron 계열**: [28]은 8.3B 모델을 32×16 V100(intra 8-way, inter 1-way)으로 학습, hardware utilisation 30% 미만. [21]은 1T 모델을 8×384 A100(intra 8, inter 64)으로 52% hardware utilisation. [29](NVIDIA+Microsoft, Megatron+DeepSpeed 결합)는 530B 모델을 8×420 A100(intra 8, inter 35, data 12)으로 36.2%. [14]는 activation memory 절감 기법(sequence parallelism 등)을 더해 1T 모델을 8×64 A100(intra 8, inter 64)에서 56.3% model utilisation 달성 — data parallelism 없이.
- **Gopher/PaLM**: Google은 TPU + custom JAX/Pathways 소프트웨어 스택 사용. Gopher([23])는 4-way inter-layer parallelism으로 280B 모델을 4×1024 TPUv3에서 학습. PaLM([8])은 inter-layer parallelism을 아예 쓰지 않고 12-way intra-layer + 256-way data parallelism(pod 내부) + 2-way data parallelism(pod 간)으로 540B 모델을 2×3072 TPUv4에서 46.2% model utilisation 달성 — pipeline bubble 문제를 회피.
- **GPT**: OpenAI는 GPT-3/GPT-4의 구현 세부사항을 공개하지 않아 "완전성을 위해서만" 언급(p.7).

### §6 Discussion (p.8-9)

Intra-operator parallelism은 통신 비용이 너무 커서 고속 interconnect(NVLink) 없이는 사실상 불가능하다는 점을 재확인 — Megatron 계열은 단일 노드(NVLink) 내로 제한되는 반면, Google의 TPU pod는 훨씬 빠른 전용 interconnect 덕에 최대 12-way intra-operator parallelism까지 확장 가능했다([8]). Model FLOPs Utilisation(MFU) 지표가 Hardware FLOPs Utilisation(HFU)보다 나은 비교 기준으로 제안된다([8]) — rematerialisation 같은 기법이 memory를 아끼는 대신 추가 FLOPs를 쓰기 때문에 HFU만으로는 실제 throughput 개선을 왜곡할 수 있다는 것. Auto-parallelisation 프레임워크 비교에서는 Alpa([35])가 hierarchical search-space(inter-layer는 ILP, intra-layer는 dynamic programming)로 Megatron-LM을 따라잡고 DeepSpeed를 능가함을 보였으나, FlexFlow([13])나 Tofu([33])와는 공정 비교가 이뤄지지 않았다는 한계도 지적한다. FlexFlow의 SOAP search-space(Sample/Operator/Attribute/Parameter 4차원)와 Markov Chain Monte Carlo 탐색 방식도 상세히 설명된다(p.8-9).

> "intra-layer and inter-layer parallelism take place at different granularities of the DL computation and have distinct communication requirements, which happen to match the structure of today's compute clusters" (§6, p.8, Zheng et al. [35] 인용)

### §7 Conclusion / Future work (p.9-10)

세 연구질문에 대한 결론을 재확인하고, future work로 DNN auto-parallelisation 분야의 **표준화 부재**를 지적한다 — 서로 다른 논문의 search-space·평가 방식이 제각각이라 SOTA 기여를 특정 요소로 귀속시키기 어렵다는 것. Neural Architecture Search(NAS) 분야의 (HW-)NAS-Bench 같은 "완전 탐색된 search-space 데이터셋"을 벤치마크로 제안한다(p.10) — 값비싼 하드웨어 접근 없이도 방법론을 비교 가능하게 만드는 아이디어.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "operator graph"
- "intra-operator parallelism" / "inter-operator parallelism" (= tensor parallelism / pipeline parallelism)
- "hybrid parallelism"

**Technical concepts:**
- "activation tensor" vs "parameter tensor"
- "pipeline bubble"
- "sequence parallelism" (Korthikanti et al. 도입)
- "Model FLOPs Utilisation (MFU)" vs "Hardware FLOPs Utilisation (HFU)"
- "SOAP search-space" (Sample/Operator/Attribute/Parameter, FlexFlow)

**Value language:**
- "the ever-growing demands computational demands of neural networks"
- "pushing the limits of hardware"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "answering three research questions" 식의 survey 프레이밍 문구
> - "SOAP" (FlexFlow 고유 용어이므로 그대로 쓰면 출처가 바로 드러남)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §1, p.1 | "the bandwidth is already a factor two below that of the A100's DRAM" (NVLink vs DRAM 대역폭) | interconnect가 memory fabric 대비 열세라는 motivation 인용에 사용 가능 |
| §5.1 / Table 2, p.6 | Megatron-LM [28]: 8.3B 모델, 32×16 V100, hardware utilisation <30% | 초기 model parallelism의 낮은 효율 사례 |
| §5.1 / Table 2, p.6 | Korthikanti et al. [14]: 1T 모델, 8×64 A100, 56.3% model utilisation (data parallelism 없이) | activation memory 최적화의 효과를 보여주는 최고 효율 수치 |
| §5.3, p.7 | PaLM [8]: 540B 모델, 2×3072 TPUv4, 46.2% model utilisation, inter-layer parallelism 미사용 | 고속 전용 interconnect(TPU pod)가 intra-operator parallelism 확장을 가능케 함을 보여주는 수치 |
| §2 / Table 1, p.2 | GPT-3 175B(21.3%), Gopher 280B(32.5%), Megatron-Turing 530B(30.2%), PaLM 540B(46.2%) hardware utilisation 비교 | 모델 규모 대비 실제 활용률이 낮다는 배경 설명에 사용 가능 |

---

## 🎯 Strategic anchor

> "Compared to fetching of data from memory, these interconnects pose serious bandwidth limitations. Even when only considering a single server, where devices can send data over NVLink, the bandwidth is already a factor two below that of the A100's DRAM [7]." (§1, p.1)

→ **본인 활용**: 면담·자소서에서 "LLM model parallelism 연구조차 결국 GPU 간 interconnect가 온-디바이스 DRAM 대역폭을 못 따라간다는 사실에서 출발한다"는 점을 인용해, 왜 memory/interconnect fabric(CXL 등) 자체의 개선이 상위 계층(모델 병렬화 전략)의 설계 공간을 바꾸는 근본 변수인지 motivation으로 연결. 단, 이 논문이 CXL/coherence를 다루지 않으므로 "이 논문이 그 해법을 제시한다"고 과장하지 않고, "이 문제의 존재를 정량적으로 확인해주는 background reading"으로만 사용할 것.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | 신경망(주로 Transformer/LLM)의 model parallelism 전략 분류·사례 조사 | Memory-system architecture 전반(CXL, disaggregation, coherence, tiered memory) — LLM은 하나의 워크로드 예시일 뿐 |
| Mechanism | Operator graph를 device에 매핑하는 partitioning 알고리즘(auto-parallelisation search) | Device 간 데이터 이동 자체를 담당하는 memory fabric/interconnect의 구조·coherence protocol |
| Workload | Forward/backward pass, activation/parameter tensor 이동 | 범용 memory access pattern (GPU 학습 workload도 포함 가능하지만 특정하지 않음) |
| Open space | "통신 시간 모델링이 어렵다"(§4, p.4)는 문제를 알고리즘/스케줄링으로 우회하려 함 | 그 통신 자체을 더 빠르고 coherent하게 만드는 하드웨어/fabric 레이어에서 접근 — 이 논문의 gap을 다른 layer에서 보완하는 관계 |

이 논문은 "쪼개는 방법"(partitioning strategy)에 집중하고, 쪼갠 조각들 사이의 통신이 이뤄지는 물리적 fabric(NVLink, Ethernet, 혹은 향후 CXL)은 주어진 상수로 취급한다. 내 연구 방향은 정반대로 그 fabric 자체를 대상으로 삼는다 — 즉 이 논문이 "interconnect 대역폭이 DRAM보다 나쁘다"고 진단만 하고 넘어가는 지점이, CXL 기반 memory disaggregation/coherence 연구가 실제로 개입하려는 지점이다. 두 방향은 계층이 다르지만(상위 소프트웨어 전략 vs 하위 하드웨어 fabric), 이 논문이 보여주는 "왜 intra-operator parallelism이 노드 경계를 넘지 못하는가"라는 제약이 완화된다면 model parallelism의 설계 공간 자체가 바뀔 수 있다는 점에서 motivation 차원의 연결고리를 가진다.

---

## Open questions / gaps

- [ ] 통신 시간(특히 network latency/bandwidth) 모델링이 "major open challenge"라고 명시(§4, p.4) — 이 부분에 memory-fabric 관점(CXL coherence traffic 모델링 등)이 기여할 여지가 있는지는 이 논문에서 다루지 않음
- [ ] Interconnect 기술 자체(NVLink 이후의 차세대 fabric, CXL 등)의 발전이 parallelism 전략에 미칠 영향은 전혀 논의되지 않음 — 이 논문은 하드웨어를 고정 상수로 취급
- [ ] Auto-parallelisation 프레임워크 간 표준화된 비교 벤치마크 부재(§6-7)를 저자들도 future work로 인정 — memory-system 벤치마크 설계 경험이 있다면 참고할 만한 논의
- [ ] GPT-3/GPT-4의 실제 구현 세부사항은 비공개라 이 survey도 다루지 못함(§5.4, p.7) — 산업계 최신 시스템의 memory/fabric 구성은 여전히 블랙박스

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [28] | Shoeybi et al., "Megatron-LM: Training Multi-Billion Parameter Language Models Using Model Parallelism" (2020) | Intra-operator parallelism의 원형 설계, 이 survey의 핵심 사례 |
| ☐ | [21] | Narayanan et al., "Efficient Large-Scale Language Model Training on GPU Clusters Using Megatron-LM" (SC 2021) | Hybrid parallelism(tensor+pipeline+data) 조합 이론, 1T 모델 스케일링 분석 |
| ☐ | [14] | Korthikanti et al., "Reducing Activation Recomputation in Large Transformer Models" (2023) | Activation memory footprint 수식화 — memory-bound 분석 방법론 참고용 |
| ☐ | [35] | Zheng et al., "Alpa: Automating Inter- and Intra-Operator Parallelism for Distributed Deep Learning" (OSDI 2022) | Hierarchical search-space 설계가 compute cluster의 mesh 구조와 어떻게 매칭되는지 — device topology-aware 설계 참고 |
| ☐ | [13] | Jia et al., "Beyond Data and Model Parallelism for Deep Neural Networks" (MLSys 2019, FlexFlow) | SOAP search-space와 execution simulator 설계 — cost model 방법론 |
| ☐ | [8] | Chowdhery et al., "PaLM: Scaling Language Modeling with Pathways" (2023) | TPU pod의 전용 고속 interconnect가 intra-operator parallelism 확장을 가능케 한 실제 사례 — fabric 성능과 parallelism 선택의 직접적 상관관계 |
| ☐ | [7] | Choquette et al., "NVIDIA A100 Tensor Core GPU: Performance and Innovation" (2021) | NVLink vs DRAM 대역폭 비교 수치의 원출처 — Strategic anchor 인용의 근거 확인용 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
