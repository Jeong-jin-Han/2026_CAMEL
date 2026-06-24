---
title: "SolidAttention: Low-Latency SSD-based Serving on Memory-Constrained PCs"
aliases: [SolidAttention]
description: "메모리 제약 PC에서 sparse attention과 SSD 스토리지 관리를 공동 설계해 KV cache를 SSD로 오프로딩하면서도 저지연 LLM 추론을 달성하는 시스템"
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/llm
  - topic/llm-serving
  - topic/ssd-offload
  - topic/attention
  - venue/fast
  - year/2026
---

# SolidAttention: Low-Latency SSD-based Serving on Memory-Constrained PCs

> **FAST 2026** · `cluster/llm` · Source: [SolidAttention - Low-Latency SSD-based Serving on Memory-Constrained PCs.pdf](<SolidAttention - Low-Latency SSD-based Serving on Memory-Constrained PCs.pdf>)

저자: Xinrui Zheng, Dongliang Wei, Jianxiang Gao, Yixin Song, Zeyu Mi, Haibo Chen — Institute of Parallel and Distributed Systems (IPADS), Shanghai Jiao Tong University

## TL;DR
AIPC(AI Personal Computer)처럼 8~16GB 메모리뿐인 PC에서 long-context LLM을 돌리면 KV cache가 모델 가중치의 4배가 넘는 메모리를 잡아먹는다. SolidAttention은 전체 KV cache를 SSD에 두고 **dynamic attention sparsity 알고리즘과 SSD 스토리지 관리를 tight co-design**하여, (1) KV pair를 coarse-grained block으로 묶어 SSD-friendly 순차 접근으로 바꾸는 KV Consolidator, (2) temporal locality를 이용한 Speculative Prefetcher, (3) attention을 microtask로 쪼개 GPU 연산과 SSD I/O를 fine-grained overlap하는 SSD-aware Scheduler를 도입한다. 128k context에서 추론 속도를 최대 **3.1×** 높이고 KV cache 메모리를 최대 **98%** 줄이면서 정확도 손실이 없다.

## 문제 & 동기
- **KV cache 메모리 폭증**: 8B 모델도 128k context에서 KV cache가 16GB 이상으로, 모델 가중치(INT4)의 4배 이상이며 8/16GB PC 메모리를 초과한다 (Figure 1a).
- **양자화의 한계**: KV cache를 INT4 이하로 공격적으로 양자화하면 정확도가 크게 떨어진다.
- **기존 SSD 오프로딩의 한계**: FlexGen류는 throughput 지향으로 여러 request를 batching해 SSD 지연을 숨기지만, batch=1인 로컬 single-user 환경에서는 computation이 부족해 SSD 접근 지연을 가릴 수 없다 (Figure 2).
- **근본 원인**: sparse attention의 fine-grained random I/O 패턴과, 큰 순차 전송에서만 최적 성능을 내는 SSD 특성 사이의 **fundamental conflict**. 기존 연구는 sparsity와 storage를 별개 문제로 다뤄 이 상호작용을 놓친다.

> [!quote]- 📄 원문 표현 (paper)
> "we argue that the root cause of the latency issues when integrating dynamic attention sparsity and SSD offloading is **the fundamental conflict between sparse attention computation and SSD characteristics**. Specifically, SSDs achieve optimal performance only when processing coarse-grained sequential operations. In contrast, the dynamic and irregular data access patterns inherent in the sparse attention mechanism introduce numerous fine-grained random I/O requests." (p.2)
>
> "loading a 1k-token KV cache (128 MB) from SSD takes about 40ms, which can account for nearly half of the time in a decode step." (p.4)

## 핵심 통찰

> [!note]- 토글: 3가지 핵심 통찰
> 1. **데이터 접근 granularity 정렬**: KV pair를 block으로 consolidate하면 sparse attention의 불규칙 접근이 coarse-grained 순차 접근으로 바뀌어 SSD bandwidth를 정확도 손실 없이 최대화한다. block size 32가 16보다 약 14% 높은 throughput을 준다 (Figure 18).
> 2. **selection의 temporal locality**: 연속된 decode iteration 간 KV block 선택 결과가 약 81% 유사하다 (Figure 9). 이 시간적 지역성을 이용하면 다음 layer가 어떤 block을 쓸지 미리 예측·prefetch할 수 있다.
> 3. **relaxed ordering**: self-attention은 K/V pair가 token마다 정렬되기만 하면 KV cache 내 token의 global ordering이 임의여도 무방하다. 따라서 잘못 prefetch한 block을 비싼 reordering 없이 직접 overwrite로 교정할 수 있다.

## 설계 / 메커니즘

> [!abstract]- 토글: SolidAttention 3대 컴포넌트
> 전체 KV cache는 SSD에 상주하고 추론 중 GPU 메모리로 동적 로딩된다. block-wise attention sparsity를 채택해 KV cache를 Init Blocks(초기 토큰 고정 윈도우), Local Blocks(최근 토큰 슬라이딩 윈도우), Selected Blocks(동적 선택)로 분류한다. block size는 32, context budget은 기본 1k(4k 미만 입력은 입력 길이의 25%). budget의 절반은 init/local, 절반은 selected에 할당한다 (Figure 3, 4).
>
> **(1) KV Consolidator** — K와 V cache가 같은 shape(N×H)임을 이용해 token granularity로 **interleaving**하여 하나의 transfer unit으로 묶는다. 이로써 전송 단위를 2배로 키우고 I/O 횟수를 절반으로 줄인다. runtime reordering을 피하려고 K/V projection 가중치 텐서를 모델 초기화 단계에서 미리 concatenate하여, 단일 행렬 곱으로 interleaved K/V를 직접 생성한다 (Figure 7, 8). strided access의 latency overhead는 contiguous 대비 ≤2%.
>
> **(2) Speculative Prefetcher** — init/local block은 결정론적으로 미리 로드하고, selected block은 직전 iteration의 selection history(81% 유사성)에 기반해 투기적으로 prefetch한다. SSD bandwidth가 idle한 구간을 활용한다 (Figure 10a).
>
> **(3) SSD-aware Scheduler** — attention 모듈을 microtask(q proj., kv proj., prefetch, select, load, store, attention)로 분해하고 데이터 의존성을 DAG로 모델링(Algorithm 1). critical path를 식별해 최고 우선순위로 실행하고, non-critical I/O는 가능한 한 일찍 발행해 GPU 연산과 overlap한다. **synchronization point reuse**로 non-critical task를 critical task와 묶어 device handshake 횟수를 줄인다. unified memory(iGPU)에서는 explicit sync를 대부분 제거할 수 있다.
>
> **Out-of-Order Overwrite** — 잘못 prefetch한 block(예: block 6 대신 block 3 필요 시)을 reordering 없이 SSD에서 로드해 직접 덮어쓴다 (Figure 10b, §5.2).

## 평가

> [!success]- 토글: 주요 수치 결과
> **환경**: CUDA backend(Intel Ultra 9 185H + RTX 4070 Laptop 8GB GDDR6), SYCL backend(Intel Ultra 7 255H + Arc 140T iGPU). 둘 다 64GB DDR5 + 1TB Samsung 990 PRO PCIe 4.0 SSD. 모델: Llama-3.1-8B, Llama-3.2-3B, Qwen-2.5-7B (INT4 가중치, FP16 KV cache). batch=1, max output 512. DRAM 사용 16GB로 제한. baseline: Offload, Offload+Sparse(InfLLM), FlexGen (p.10).
>
> **End-to-end throughput (CUDA)**: Offload+Sparse 대비 Llama-3.2-3B/3.1-8B/Qwen-2.5-7B 각각 **2.8× / 3.1× / 2.4×** 가속. FlexGen 대비 16k token에서 최대 **58.9×** (Figure 12, p.10). FlexGen은 16k 초과 context에서 CUDA OOM 발생.
>
> **End-to-end throughput (SYCL)**: Offload+Sparse 대비 최대 **2.1× / 2.5× / 1.9×** (Figure 13, p.10).
>
> **메모리**: KV cache 메모리를 Llama-3.2-3B/3.1-8B/Qwen-2.5-7B에서 각각 **61.9× / 62.0× / 61.9×** 감소(즉 최대 98% 절감), 1k context 분량 한 layer 버퍼만 할당 (Figure 14, p.10).
>
> **In-memory 대비**: SSD 오프로딩에도 in-memory 대비 throughput 저하 ≤**11%** (Figure 15, p.11).
>
> **정확도**: OpenCompass(Winogrande, Arc-Challenge, MMLU, GSM8K, LongBench)에서 원본과 거의 동일, 단순 INT4 KV 양자화(Quant)는 특히 Qwen-2.5-7B에서 큰 저하 (Table 1, p.11).
>
> **Ablation**: speculative prefetching이 blocking latency를 SYCL 최대 **3.1×**, CUDA **3.9×** 감소 (Figure 16). interleaved KV cache로 attention latency 최대 **22%** 감소 (Figure 17a). fine-grained overlap이 최대 **25%** 성능 향상, sync reuse 추가 **22%** latency 감소 (Figure 17b).
>
> **에너지**: GovReport(평균 10k token)에서 llama.cpp 5.37 J/token 대비 SolidAttention **3.68 J/token** (46% 개선), peak power는 32.98W→36.27W (Table 2, p.13).
>
> **민감도/간섭**: context budget 4k에서 I/O를 연산으로 못 가려 attention latency 급증(Figure 19). 동시 I/O 간섭 시 throughput 58%(4GB/s 배경 트래픽)·54%(800k IOPS) 하락, p99 latency 2.9× 증가하나 multi-token 추론에서는 amortize됨 (Figure 20, 21, p.12).

## 섹션 노트
- **§1 Introduction**: AIPC 하드웨어 제약(8~16GB DRAM, iGPU 또는 6~8GB VRAM 엔트리 dGPU), KV cache 메모리 disconnect, 3대 challenge(Accuracy Loss / Prefetching Indeterminacy / Data Inconsistency), core insight(sparsity와 storage co-design) 제시.
- **§2 Background and Motivation**: Transformer/autoregressive decode, KV cache 메모리 overhead(Figure 1a), static(SnapKV, StreamingLLM) vs dynamic(Quest, InfLLM) sparsity, SSD 오프로딩(FlexGen)의 throughput 지향 한계.
- **§3 Overview**: 시스템 아키텍처(Figure 3), block-wise sparsity 3종, single-layer dataflow timeline(Figure 5), block size 클수록 recall 저하(Figure 6).
- **§4 KV Consolidator**: token-level interleaving(Figure 7), 가중치 pre-concatenation(Figure 8), strided access ≤2% overhead.
- **§5 Speculative Prefetcher**: 81% selection 유사성, 결정론적 prefetch(init/local) vs 투기적 prefetch(selected), Out-of-Order Overwrite.
- **§6 SSD-aware Scheduler**: DAG 기반 fine-grained overlap(Algorithm 1, critical path, LST 기반 우선순위), synchronization point reuse.
- **§7 Implementation**: llama.cpp 위 약 25k LOC(GPU kernel 12k, llama.cpp adapter 1k), I/O 전용 코어 1개 + SSD-GPU coordination 코어 1개, write amplification 완화 위해 layer당 32KB write buffer.
- **§8 Evaluation**: setup, end-to-end, accuracy, ablation, sensitivity, SSD interference, energy.
- **§9 Related Works**: KV Cache Compression(H2O, InfiniGen, RetrievalAttention, ClusterKV), Model Offloading(PowerInfer, FlexInfer, eMoE, ProMoE), KV Cache Offloading(CachedAttention, IMPRESS), Emerging Storage(PCIe 5.0, ZNS SSD).
- **§10 Conclusion**: sparse attention 알고리즘과 storage 관리 co-design으로 최대 3.1× 가속, 98% 메모리 절감, 정확도 손실 미미.

## 핵심 용어
- **AIPC (AI Personal Computer)**: 로컬에서 LLM 추론을 수행하는 개인용 PC. 보통 8~16GB DRAM과 iGPU 또는 6~8GB VRAM 엔트리 dGPU를 가진 메모리 제약 환경.
- **KV cache**: autoregressive decode에서 이전 토큰의 K/V 텐서를 저장·재사용해 재계산을 피하는 캐시. context 길이에 비례해 선형 증가.
- **Dynamic attention sparsity**: 전체 KV cache를 유지하되 decode 단계에서 query와 유사도가 높은 일부 KV block만 동적으로 선택해 attention에 참여시키는 기법(Quest, InfLLM).
- **Block-wise sparsity**: KV cache를 context 차원으로 block 분할하고 각 block의 representative vector로 선택하는 방식. Init/Local/Selected block으로 구분.
- **KV Consolidator**: K와 V를 token granularity로 interleaving하여 하나의 coarse-grained 전송 단위로 묶는 컴포넌트. 전송 단위 2배, I/O 횟수 절반.
- **Speculative Prefetcher**: 직전 iteration의 selection history(81% 유사성)로 다음 layer의 selected block을 투기적으로 prefetch하는 컴포넌트.
- **SSD-aware Scheduler**: attention을 microtask로 분해해 DAG로 의존성을 모델링하고, critical path 우선 실행 + non-critical I/O overlap + synchronization point reuse로 blocking latency를 줄이는 스케줄러.
- **Out-of-Order Overwrite**: self-attention의 token ordering 무관성을 이용해 잘못 prefetch한 block을 reordering 없이 직접 덮어써 교정하는 기법.
- **Context budget**: 한 번에 attention에 유지하는 최대 토큰 수(기본 1k). 절반은 init/local, 절반은 selected에 할당.
- **Synchronization point reuse**: 서로 다른 microtask의 device 동기화 지점을 통합·재사용해 handshake 빈도를 줄이는 최적화.

## 강점 · 한계 · 열린 질문
**강점**
- sparsity 알고리즘과 SSD storage를 분리하지 않고 co-design한 점이 핵심 차별점. fine-grained random I/O → coarse-grained 순차 I/O 변환이 SSD 특성에 정확히 맞춤.
- batch=1 single-user 로컬 시나리오를 정조준 — FlexGen류 throughput 지향 시스템이 못 다루는 영역.
- CUDA(dGPU)와 SYCL(iGPU) 양 backend 검증, 정확도 무손실 + 98% 메모리 절감을 동시 달성.

**한계**
- context budget이 4k에 도달하면 I/O를 연산으로 가리지 못해 attention latency 급증 (Figure 19). 단, §8.3에서 1k budget로도 long-context 정확도 저하가 미미하다고 주장.
- 동시 I/O 간섭(다른 디스크 집약 앱) 시 throughput 최대 58% 하락, p99 latency 2.9× 증가.
- write amplification(KV cache 쓰기)을 32KB write buffer로 완화하나 SSD 수명에 대한 장기 영향은 미평가.

**열린 질문**
- model offloading(PowerInfer류)과 결합 시 KV cache 로딩과 weight 로딩 간 I/O 경합 — 논문도 future work로 명시.
- PCIe 5.0 / ZNS SSD에서 더 큰 KV 차원이나 명시적 zone 배치를 활용한 추가 이득은?
- speculative prefetch의 81% 유사성이 다른 모델/태스크에서 얼마나 견고한가(Qwen처럼 KV cache가 작은 모델은 이득 축소).

## ❓ Q&A (자가 점검)

> [!question]- Q1. SolidAttention이 해결하는 근본 충돌(fundamental conflict)은 무엇인가?
> > sparse attention의 dynamic·irregular한 fine-grained random I/O 패턴과, coarse-grained 순차 접근에서만 최적 성능을 내는 SSD 특성 사이의 충돌이다. 기존 연구는 sparsity와 storage를 별개로 다뤄 이 상호작용을 놓쳤다 (p.2).

> [!question]- Q2. 왜 FlexGen 같은 기존 SSD 오프로딩이 로컬 PC에서 실패하는가?
> > FlexGen은 여러 request를 batching해 SSD 지연을 다른 request의 연산과 overlap한다. 그러나 로컬 single-user 환경은 batch=1로 request 동시성이 낮아, small-batch 입력이 만드는 연산량이 I/O 지연을 가리기에 턱없이 부족하다 (Figure 2, p.1-2).

> [!question]- Q3. KV Consolidator의 interleaving은 어떻게 정확도 손실 없이 전송 효율을 높이나?
> > K와 V가 동일 shape이라는 관찰을 이용해 token granularity로 K/V를 번갈아 배치한다. 이는 block 내 token 수를 늘리지 않으므로(representative vector로 인한 정보 손실 없음) 정확도 저하가 없고, 전송 단위를 2배로 키우고 I/O 횟수를 절반으로 줄인다 (§4, Figure 7).

> [!question]- Q4. Speculative Prefetcher가 다음에 필요한 block을 어떻게 예측하나?
> > dynamic sparsity의 block 선택이 다음 layer 연산 전에는 결정될 수 없는 indeterminacy 문제가 있다. 그런데 연속 iteration 간 선택 결과가 약 81% 유사하다(temporal locality). 이를 이용해 직전 iteration의 selection history로 selected block을 투기적으로 prefetch한다 (Figure 9, 10a).

> [!question]- Q5. 잘못 prefetch한 block은 어떻게 교정하며 왜 reordering이 불필요한가?
> > self-attention은 각 token의 K/V pair가 같은 위치에 정렬되기만 하면 KV cache 내 token의 global ordering이 임의여도 무방하다. 따라서 잘못 prefetch한 block을 invalidate하고 SSD에서 올바른 block을 로드해 직접 overwrite하면 되며, 비싼 reordering 단계가 필요 없다 (§5.2, Figure 10b).

> [!question]- Q6. SSD-aware Scheduler의 두 핵심 원리는?
> > (1) microtask decomposition & fine-grained overlap: attention을 microtask로 쪼개 DAG로 의존성을 모델링하고 critical path를 우선 실행, non-critical I/O는 일찍 발행해 GPU 연산과 overlap. (2) synchronization point reuse: 여러 microtask의 동기화 지점을 통합·재사용해 device handshake 빈도를 줄인다 (§6).

> [!question]- Q7. 정량적 핵심 성과는?
> > 128k context에서 추론 속도 최대 3.1× 향상, KV cache 메모리 최대 98% 절감(약 62×)을 정확도 손실 없이 달성. Offload+Sparse 대비 2.4~3.1× 가속, in-memory 대비 throughput 저하 ≤11%, 에너지 3.68 J/token으로 llama.cpp 대비 46% 개선 (Abstract, Table 2).

> [!question]- Q8. SolidAttention의 주요 실패/한계 조건은?
> > context budget이 4k에 도달하면 I/O 지연을 연산으로 더 이상 가리지 못해 attention latency가 급증한다. 또 동시 디스크 집약 워크로드 간섭 시 throughput이 최대 58% 하락하고 p99 latency가 2.9× 증가한다 (Figure 19, 20, 21).

## 🔗 Connections
[[LLM Systems]] · [[FAST]] · [[2026]]

## References worth following
- **FlexGen** (Sheng et al., ICML'23, [40]): SSD 오프로딩 + attention sparsity로 단일 GPU 고처리량 추론. SolidAttention의 주요 baseline이자 throughput 지향 접근의 대표.
- **InfLLM** (Xiao et al., NeurIPS'24, [51]): block representative vector 추출 방식의 기반. SolidAttention의 block-wise sparsity가 이를 따름.
- **Quest** (Tang et al., ICML'24, [45]): query-aware dynamic sparsity, max-min representative vector. block-wise selection의 또 다른 축.
- **InfiniGen** (Lee et al., OSDI'24, [28]): dynamic KV cache management로 generative inference 가속. token-granularity selection의 read/write amplification 문제 대비.
- **CachedAttention** (Gao et al., ATC'24, [19]): multi-turn 대화용 multi-tier KV cache로 TTFT 절감. KV cache offloading 관련 연구.
- **IMPRESS** (Chen et al., FAST'25, [12]): importance-informed multi-tier prefix KV storage. 같은 venue의 직접 비교 대상.

## Personal annotations
<본인 메모 영역>
