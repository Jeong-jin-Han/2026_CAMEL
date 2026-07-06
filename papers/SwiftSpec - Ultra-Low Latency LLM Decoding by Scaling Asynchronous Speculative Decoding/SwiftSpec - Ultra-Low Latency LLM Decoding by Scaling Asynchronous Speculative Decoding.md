# SwiftSpec: Ultra-Low Latency LLM Decoding by Scaling Asynchronous Speculative Decoding

> **Source PDF**: [SwiftSpec - Ultra-Low Latency LLM Decoding by Scaling Asynchronous Speculative Decoding.pdf](SwiftSpec%20-%20Ultra-Low%20Latency%20LLM%20Decoding%20by%20Scaling%20Asynchronous%20Speculative%20Decoding.pdf)
> **NodeGraph**: [SwiftSpec.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/SwiftSpec%20-%20Ultra-Low%20Latency%20LLM%20Decoding%20by%20Scaling%20Asynchronous%20Speculative%20Decoding/SwiftSpec.html)
> **Authors**: Ziyi Zhang, Ziheng Jiang, Chengquan Jiang, Menghan Yu, Size Zheng, Haibin Lin, Henry Hoffmann, Xin Liu (ByteDance Seed, University of Chicago)
> **Venue / Year**: arXiv preprint, 2025
> **arXiv / DOI**: arXiv:2506.11309v1 [cs.DC]
> **Length**: 19 pages
> **Read status**: ☑ Full read (2026-07-06)
> **My reading purpose**: 메모리 시스템 아키텍처(CXL/coherence) 방향 탐색 중, GPU 클러스터에서 KV-cache/상태를 어떻게 "분산시키고도 일관성 있게" 관리하는지 참고 사례로 정독

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

SwiftSpec은 단일 요청(single-request) LLM 서빙에서 디코딩 지연시간을 최소화하기 위해, speculative decoding의 draft 단계와 target(verification) 단계를 **서로 다른 GPU 그룹에 배치해 비동기·분리(disaggregated) 실행**시키는 시스템이다. 기존에는 draft 모델과 target 모델이 같은 GPU 세트를 공유해 순차 실행되었기 때문에 draft 단계가 critical path에 끼어들었고, tensor parallelism을 단순 적용해도 통신 오버헤드와 GPU 유휴 시간 때문에 지연시간이 잘 줄지 않았다. SwiftSpec은 (1) draft tree를 target의 검증과 병렬로 계속 자라게 하는 **parallel tree generation**, (2) 검증마다 draft tree를 재-rooting하면서도 KV cache를 최대한 재사용하는 **consistent KV-cache management**, (3) all-reduce와 GEMM을 융합하고 NCCL Low-Latency(LL) 프로토콜을 활용하는 **latency-optimized fused kernel**, 이 세 기법을 결합한다. 5개 모델 패밀리·6개 데이터셋에서 평균 1.75× (경쟁 baseline 대비), SGLang 대비 평균 2.23× 속도 향상을 보였고, Llama3-70B를 8×H800 GPU에서 평균 348 tokens/s로 서빙해 "현재까지 가장 빠른 Hopper급 저지연 LLM 서빙"이라고 주장한다 (Abstract, p.1; §7, p.14-15).

---

## Core thesis

> "we redesign the speculative decoding process in an asynchronous, disaggregated manner... We partition GPUs into two groups: verification and draft... The verification and draft phases proceed in parallel." (§1, p.2)

추가 설명: 기존 speculative decoding은 draft→verify를 **엄격한 순차 의존성**으로 취급해 draft 단계가 critical path 위 오버헤드가 된다. SwiftSpec은 draft 모델과 target 모델을 물리적으로 다른 GPU 그룹에 배치하고, 검증 그룹이 iteration n-1을 검증하는 동안 draft 그룹이 iteration n의 후보를 동시에 생성하게 만들어 이 의존성을 깬다. 대신 두 그룹 사이의 KV-cache 일관성을 유지하는 새로운 문제가 생기고, 이를 재-rooting 기반 관리 스킴으로 해결한다.

---

## Why this matters to me

이 논문은 스토리지가 아니라 **GPU 클러스터 안의 상태(KV-cache) 분산·동기화 문제**를 다룬다는 점에서 내 CXL/memory-architecture 방향과 표면적으로는 거리가 있다. 하지만 핵심 challenge가 "두 개의 독립적으로 스케일링되는 실행 그룹(draft GPU 그룹 vs target GPU 그룹) 사이에서, 강한 동기화 없이 상태(KV cache)를 얼마나 일관되게 유지할 수 있는가"라는 점에서, 내가 CXL multi-node coherence에서 보고 있는 "범용 강한 coherence는 비싸다 → 워크로드에 특화해서 필요한 동기화만 남긴다"는 가설(H1, [[memory-architecture-research-direction]])과 같은 모양의 문제다. SwiftSpec은 **정합성을 매 iteration 재-rooting 시점에만 명시적으로 동기화**하고 나머지는 비동기로 흘려보내는데, 이는 내가 찾고 있는 "release-consistency 스타일 동기화 지점" 아이디어의 실제 사례로 읽을 수 있다. 다만 이 논문은 GPU 그룹 간 통신을 NVLink/NCCL 기반으로 처리하고 memory subsystem 자체를 재설계하지 않으므로, "누가 coherence를 명시하는가"라는 내 질문에 대한 답은 아니고 어디까지나 하나의 **응용 사례(workload-specific consistency scheme)**로 참고할 대상이다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1-3 | TP만으로는 저지연 한계, draft/target 비동기 분리 제안 |
| 2 | Background and System-level Challenges | p.3-6 | TP의 draft/target 비대칭성, tree-based spec decoding, 3대 한계(scalability/KV consistency/kernel utilization) 정리 |
| 3 | SwiftSpec System Design and Architecture | p.6-10 | parallel tree generation(§3.1), KV cache consistency(§3.2), latency-optimized fused kernel(§3.3) |
| 4 | Implementation | p.9-10 | CUDA/C++ ~3000줄 + Python/C++ ~4000줄, CUTLASS 기반 |
| 5 | Evaluation | p.10-14 | end-to-end 결과, ablation, kernel microbenchmark, design choice 분석 |
| 6 | Discussion and Limitations | p.14 | EAGLE과의 비호환, 고throughput 미적합, disaggregated prefill과 직교 |
| 7 | Conclusion | p.14-15 | 348 tokens/s Llama3-70B, 기법의 일반성 주장 |

---

## Section notes

### §1 Introduction (p.1-3)

저지연 디코딩이 chatbot·코드 어시스턴트·로봇 제어·CoT 추론에서 중요한데, throughput 최적화 위주 서빙 프레임워크는 single-request 저지연에 최적이 아니라는 문제의식에서 출발한다. Table 1(p.2)이 핵심 관찰을 보여준다: Llama3 family에서 GPU 수를 늘려도(TP 확장) 지연시간 감소는 금방 diminishing return에 도달하고, 오히려 작은 draft 모델(3B)은 TP를 2→4로 늘리면 통신 오버헤드 때문에 추론 시간이 **증가**한다(2.61ms→2.80ms). Draft 모델과 target 모델은 근본적으로 다른 compute 요구를 가지므로, 같은 GPU 세트에 동일한 TP를 적용하는 기존 방식이 비효율적이라는 것이 이 논문의 출발점이다.

### §2 Background (p.3-6)

Table 2(p.4)에서 SpecInfer/EAGLE, PEARL, PipeInfer, AMUSD, SGLang/vLLM과 SwiftSpec을 비교하는데, SwiftSpec만 "Tree + Spec Parallel + fine-grained reorg KV + fused latency-optimized ops"를 모두 갖췄다고 주장한다. Figure 2(p.4)의 compression ratio 실험이 흥미로운 설계 근거를 제공한다: target/draft 배치 크기를 키워도 **16을 넘으면 정답률(compression ratio) 개선이 미미**해서, 저지연 목적에는 큰 배치가 불필요하다는 것을 실증적으로 보인다. §2.4는 기존 연구의 3대 한계 — (1) draft/target의 독립적 스케일링 불가, (2) speculative execution 하 KV cache consistency 부재, (3) 저배치에서 GPU 커널 활용률 저조(Table 3, p.5: all-reduce 대역폭 활용률 <10%) — 를 짚는다.

### §3.1 Parallel Tree Generation (p.6-8)

> "the draft worker keeps generating new leaves in the tree, appending the tree cache after the existing entries" (§3.2, p.8)

Algorithm 1(p.7)이 핵심 로직이다. Draft worker는 계속 tree를 확장(가장 확률 높은 leaf log-softmax 값 기준 우선순위 큐, O(k log s))하고, target worker는 draft tree의 subgraph를 배치로 받아 검증한다. Figure 3(p.6)의 예시가 이해에 결정적인데, iteration마다 verified token 경로로 tree가 재-rooting되며 grow를 이어간다. GPU 배분은 k개 GPU 중 x개를 target에, k-x를 draft에 주는 식으로 하는데, **더 강력한 draft 모델일수록 최적 x가 작아진다**는 실증 결과(§3.1 말미, p.8)가 있다.

### §3.2 KV Cache Consistency Management (p.7-9)

> "the KV states of the verified tokens are stored continuously in the prefix of the KV cache (which we call prefix cache), and the KV states of the tree are stored right after the prefix (which we call tree cache)" (p.8)

Figure 4(non-square tree mask)와 Figure 5(재-rooting 시 prefix/tree cache 재배치)가 핵심. 검증된 토큰이 확정되면 draft worker가 tree를 그 토큰까지 walk-down해 재-rooting하고, 여전히 유효한 subtree는 버리지 않고 재사용한다. 이게 "compression ratio는 직렬 방식보다 9% 낮지만, draft 추론 시간은 79% 절감"(Table 6, p.12)이라는 트레이드오프로 이어진다.

### §3.3 Latency-Optimized Kernels (p.9-10)

NCCL Low-Latency(LL) 프로토콜(Algorithm 2, p.9)을 활용해 **명시적 동기화 없이** GPU 간 send/receive를 구현하고, 이를 GEMM+all-reduce 융합 커널(Figure 6, p.9)과 SwiGLU 융합 연산에 적용한다. 핵심 아이디어는 store할 때 flag를 함께 실어 보내고(64-bit → 2×32-bit atomic store/load), 수신측이 flag 일치를 폴링하는 것만으로 barrier 없는 동기화를 구현하는 것 — 이는 lock-free 프로토콜의 memory-ordering 트릭에 가깝다.

### §5 Evaluation (p.10-14)

5개 모델 패밀리(Llama3, DeepSeek-Coder, Qwen2, DeepSeek-R1-Distill-Qwen/Llama, Table 4 p.10) × 6개 데이터셋(MT-bench, HumanEval, GSM8K, Alpaca, CNN/DailyMail, Natural Questions)에서 vLLM, SGLang, TensorRT-LLM 대비 벤치마크. Figure 7(p.11)에서 SwiftSpec-full이 전 모델에서 최고 성능. Ablation(Figure 8, p.12)에서 parallel tree generation만으로 평균 43% 개선, latency-optimized kernel만으로 추가 16-21% 개선임을 분리해서 보여준다 — 두 기법의 기여가 실제로 독립적으로 검증됨.

### §6 Discussion and Limitations (p.14)

저자들이 스스로 한계를 명확히 인정한다: (1) EAGLE 방식(target 출력을 draft 모델 입력으로 쓰는 tighter dependency)에는 적용이 어려움 — "how to integrate our method with EAGLE is left as future work" (p.14). (2) int4 양자화 기반이라 batch≥64의 고throughput 서빙에는 부적합. (3) prefill 단계 최적화(DistServe 등)와는 **직교(orthogonal)** 하다고 명시 — 서로 다른 단계를 다루므로 결합 가능.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "asynchronous, disaggregated" execution of two coupled model components
- "independent scalability" across heterogeneous compute components (draft vs target)
- "remove draft overhead from the critical path"

**Technical concepts:**
- parallel tree generation / re-rooting the draft tree
- prefix cache vs tree cache (KV cache 재구성 불변식)
- NCCL Low-Latency (LL) protocol — barrier-free flag-based synchronization
- fused GEMM + all-reduce kernel
- compression ratio (평균 검증 성공 토큰 수 / target inference)

**Value language:**
- "ultra-low latency" single-request serving (throughput-oriented SLO 서빙과 대비)
- "ambient" 자원 재배분: draft/target 간 GPU x를 유연하게(x, k-x) 나누는 설계

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "SwiftSpec" 자체, "348 tokens/s" 수치는 이 논문 고유의 headline claim — 내 글에서 그대로 반복하면 표절처럼 보임
> - "currently the fastest system"류의 단정적 최상급 표현

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.1 | 평균 1.75× speedup, Llama3-70B 348 tokens/s (8×H800) | LLM 저지연 서빙의 최신 SOTA 수치 언급 시 |
| §1, p.2, Table 1 | Llama3-3B draft 모델, TP 2→4시 추론시간 2.61ms→2.80ms(증가) | "GPU 늘려도 소형 모델은 통신 오버헤드로 오히려 느려진다"는 motivation 인용 시 |
| §2.3.2, p.4, Fig.2 | target batch 16 초과 시 compression ratio 개선 미미 | 저지연 목적엔 큰 배치가 불필요하다는 근거 |
| §5.2, p.11 | SwiftSpec-base 대비 baseline 1.75×, SGLang 대비 평균 2.23× | 비교 baseline 대비 개선폭 인용 |
| §5.3, p.12, Table 6 | 병렬 tree generation 도입시 draft 추론시간 3.72ms→3.25ms(79% GPU 절감), throughput 200→275 tokens/s(+37%) | KV consistency 스킴의 구체적 효과 수치 |
| §5.4, p.13, Table 3 | TP=4, batch=8 하에서 all-reduce 대역폭 활용률 <10% | 저배치 GPU 커널 활용도 문제의 근거 |

---

## 🎯 Strategic anchor

> "It is important (yet challenging) to keep a consistent view of the KV cache of accepted tokens and the draft tokens that might be useful in the future." (§1, p.3)

→ **본인 활용**: 면담/자소서에서 "GPU 클러스터에서도 상태(KV cache) 일관성 유지가 여전히 시스템 설계의 핵심 난제로 남아있다"는 예시로 인용 가능. 내가 관심 있는 multi-node CXL coherence 문제와 "그레인이 다를 뿐 구조가 같은 문제"라는 연결고리로 사용 — "SwiftSpec은 애플리케이션 레이어(스케줄러)에서 재-rooting 시점마다 명시적으로 동기화해 문제를 해결했는데, 내가 보는 문제는 이걸 메모리 시스템 레이어에서, 하드웨어/OS가 얼마나 자동화할 수 있는가"라는 대비로 쓸 수 있음.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | 단일 노드(8-GPU) 내 draft/target 그룹 간 KV-cache 동기화 | Multi-node CXL coherence, 노드 간 memory 공유 |
| Mechanism | 스케줄러/커널 레벨에서 수동 설계한 재-rooting + barrier-free NCCL LL 프로토콜 | HW/OS 레벨 coherence protocol 특화 설계 (H1) |
| Workload | LLM 디코딩(speculative decoding) 특정 워크로드에 강하게 특화된 KV cache invariant | "범용 coherence는 비싸다 → 워크로드 특화"라는 동일한 방법론적 태도, 다만 대상은 memory subsystem 자체 |
| Open space | EAGLE류의 tighter dependency 구조로 확장 못 함(§6에서 스스로 인정); 이 스킴을 다른 workload/그레인에 일반화하는 문제는 다루지 않음 | "누가 동기화 지점을 명시하는가"(API/컴파일러/HW)를 시스템 계층에서 일반화하는 것이 내 문제의식(H2, PGAS) |

SwiftSpec은 소프트웨어 스케줄러 레벨에서 "동기화 지점을 재-rooting 시점으로 한정"하는 특화된 consistency 스킴을 성공적으로 구현한 사례이고, 이는 내가 memory architecture 레벨에서 하려는 것(coherence를 워크로드에 맞게 특화해 범용성과 비용을 트레이드오프)의 응용 계층 유사물이다. 다만 이 논문은 memory subsystem이나 HW coherence protocol을 재설계하지 않고 순수 소프트웨어(스케줄러+커널 fusion)로 문제를 풀었다는 점에서, "이 문제를 한 단계 아래 계층(HW/OS)으로 밀어넣으면 무엇이 달라지는가"가 내 방향과의 차이점이자 잠재적 확장 지점이다.

---

## Open questions / gaps

- [ ] EAGLE류(target 출력에 강하게 의존하는 draft)처럼 더 강한 의존성을 가진 speculative decoding 변형에 이 비동기 분리 기법을 일반화하는 문제 (저자들이 §6에서 future work로 명시)
- [ ] 이 재-rooting 기반 KV consistency 스킴을 GPU 노드 간(multi-node, NVLink 범위 밖)으로 확장했을 때 통신 지연·coherence 비용이 어떻게 변하는지는 다루지 않음
- [ ] NCCL LL의 barrier-free flag polling 트릭이 CXL 같은 memory-semantic 인터커넥트에서도 동일하게 적용 가능한지(레이턴시·polling overhead 특성이 다를 수 있음)는 미탐구
- [ ] 고throughput(batch≥64) 시나리오에서 이 기법이 유효한지는 "미래에는 유용할 수 있다"는 추측만 있고 실증 없음(§6, p.14)

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [2] | PipeInfer (Butler et al., 2024) | tree-based 비동기 speculative decoding 선행연구, SwiftSpec의 직접 비교 대상 |
| ☐ | [19] | EAGLE-2 (Li et al., 2024) | SwiftSpec이 통합 못한다고 밝힌 tighter-dependency 방식, gap 이해에 필요 |
| ☐ | [21] | PEARL (Tang et al., 2024) | sequence 기반 parallel speculative decoding, KV consistency 처리 방식 비교 |
| ☐ | [29] | SplitWise (Patel et al., 2024) | prefill/decoding disaggregation — SwiftSpec이 직교하다고 언급한 지점 |
| ☐ | [45] | DistServe (Zhong et al., 2024) | 마찬가지로 prefill/decode disaggregation, CXL/memory-architecture 논의에서 disaggregation 패턴 비교 참고용 |
| ☐ | [17] | vLLM / PagedAttention (Kwon et al., 2023) | KV cache 관리의 baseline 패러다임, SwiftSpec의 prefix/tree cache 설계와 대비 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
