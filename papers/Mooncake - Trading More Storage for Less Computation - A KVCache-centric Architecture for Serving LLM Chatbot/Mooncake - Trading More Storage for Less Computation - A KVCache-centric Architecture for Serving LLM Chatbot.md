---
title: "Mooncake: Trading More Storage for Less Computation — A KVCache-centric Architecture for Serving LLM Chatbot"
aliases: [Mooncake]
description: "Prefill/Decoding 분리 + 클러스터 유휴 CPU/DRAM/SSD를 활용한 분산 KVCache 풀과 KVCache 중심 스케줄링으로 LLM 서빙 처리량을 극대화하는 아키텍처"
venue: FAST
year: 2025
arxiv: "2407.00079"
tier: deep
status: done
tags:
  - paper
  - cluster/llm
  - topic/kv-cache
  - topic/llm-serving
  - topic/disaggregation
  - venue/fast
  - year/2025
---

# Mooncake: Trading More Storage for Less Computation — A KVCache-centric Architecture for Serving LLM Chatbot

> **FAST 2025** · cluster/llm · Source: [Mooncake - Trading More Storage for Less Computation - A KVCache-centric Architecture for Serving LLM Chatbot.pdf](<Mooncake - Trading More Storage for Less Computation - A KVCache-centric Architecture for Serving LLM Chatbot.pdf>)

**저자:** Ruoyu Qin (Moonshot AI / Tsinghua), Zheming Li, Weiran He (Moonshot AI), Mingxing Zhang (Tsinghua, 교신), Yongwei Wu, Weimin Zheng (Tsinghua), Xinran Xu (Moonshot AI, 교신)
(Moonshot AI ♠, Tsinghua University ♡. Qin과 Li가 공동 1저자, Qin은 Moonshot AI 인턴 기간 수행.)

---

## TL;DR

Mooncake은 Moonshot AI의 LLM 서비스 **Kimi**를 떠받치는 서빙 플랫폼으로, (1) **prefill instance와 decoding instance를 물리적으로 분리(disaggregation)** 하고, (2) GPU 클러스터에서 놀고 있는 **CPU·DRAM·SSD·RDMA 자원을 묶어 분산 KVCache 풀**을 구성한 뒤, (3) **KVCache를 스케줄링의 중심**에 둔 cache-aware 스케줄러로 latency SLO(TTFT·TBT)를 지키면서 전체 effective throughput을 극대화한다. 핵심 트레이드오프는 제목 그대로 "**저장(storage)을 더 쓰고 계산(computation)을 덜 쓰는 것**" — KVCache를 최대한 재사용해 중복 prefill 연산을 줄인다. overload 상황을 정면으로 다뤄 미래 부하를 예측하는 **early rejection** 정책까지 제안한다.

---

## 문제 & 동기

LLM 서빙의 두 단계(prefill / decoding)는 연산 특성이 완전히 다르다. Prefill은 입력 전체를 병렬 처리해 compute-bound(특히 attention이 입력 길이에 대해 superlinear)이고, decoding은 토큰 하나씩 생성하는 memory-bound 단계다. MaaS 제공자(Kimi)의 목표는 TTFT·TBT 같은 latency SLO를 지키면서 effective throughput(=goodput)을 최대화하는 것인데, KVCache 재사용(연산 절감)과 큰 배치(MFU 향상) 두 최적화가 서로 SLO를 위반시키는 방향으로 충돌한다. 게다가 GPU 공급이 한정된 현실에서 **overload는 상수**이며, 기존 연구처럼 "모든 요청을 처리한다"는 가정이 깨진다.

> [!quote]- 인용 (p.1, p.4)
> "The optimization goal is to maximize overall effective throughput, which directly impacts revenue, while the constraints reflect varying levels of SLOs. These SLOs typically involve meeting latency-related requirements, mainly the time to first token (TTFT) and the time between tokens (TBT)." (p.1)
>
> "Our approach differs in that only requests that fully complete their execution are counted in the measure of goodput. Otherwise, all previously consumed/generated tokens are not counted, and the corresponding resources are wasted. In other words, a request should be rejected as early as possible if it cannot finish its full execution under the SLO." (p.4)

---

## 핵심 통찰

1. **KVCache가 스케줄링의 1급 시민이다.** prefill 단계의 핵심 목표는 KVCache를 최대한 재사용해 중복 연산을 피하는 것이므로, 스케줄링은 요청 수(load)뿐 아니라 prefix cache 길이와 재사용 가능한 KVCache 블록의 분포까지 고려해야 한다.
2. **"더 많은 저장 ↔ 더 적은 계산" 트레이드오프.** 클러스터의 유휴 CPU/DRAM/SSD/RDMA를 모아 분산 KVCache 풀(near-GPU prefix cache)을 추가 비용 없이 구성하면, GPU 재연산을 storage 재사용으로 대체할 수 있다.
3. **Disaggregation은 단점이 아니라 기회.** prefill을 decoding 배치에 inline하는 chunked-prefill 대신 분리를 유지하면 (a) prefill에 다른 cross-node 병렬 설정(CPP)을 줄 수 있고 (b) prefill VRAM을 KVCache 점유와 무관하게 다룰 수 있어 스케줄링이 단순해진다.
4. **Overload는 회피 대상이 아니라 설계 대상.** 부하가 임계치를 넘으면 일부 요청을 거부해야 하며, prefill을 낭비하지 않으려면 **가능한 한 일찍(early)** 거부해야 한다. 단순 early rejection은 prefill↔decoding 부하의 anti-phase 진동을 유발하므로 부하 예측이 필요하다.

> [!quote]- 인용 (p.2)
> "Building on this idea, we found that the scheduling of KVCache is central to LLM serving scheduling." (p.2)

---

## 설계 / 메커니즘

전역 스케줄러 **Conductor**가 prefill pool / KVCache pool / decoding pool을 조율한다. 요청 한 건의 워크플로(Figure 4)는 4단계다.

> [!note]- 상세 메커니즘
> **(s1) KVCache Reuse:** Conductor가 prefill 노드 한 쌍과 decoding 노드를 고른다. 선택된 prefill 노드는 재사용 가능한 prefix cache를 원격 CPU 메모리에서 로컬 GPU 메모리로 로드(prefix가 없으면 생략). 세 목표 균형 — 최대 재사용, prefill 노드 간 부하 분산, TTFT SLO 보장.
>
> **(s2) Incremental Prefill:** prefix cache를 써서 prefill을 완료하고 새로 생성된 incremental KVCache를 CPU 메모리에 저장. 캐시 안 된 입력 토큰 수가 `prefill_chunk`(보통 1000 토큰 이상) 임계치를 넘으면 chunk로 쪼개 pipeline 방식으로 처리.
>
> **(s3) KVCache Transfer:** 각 노드의 **Messenger**(GPUDirect RDMA 기반 독립 프로세스)가 layer 단위로 생성되는 KVCache를 비동기로 decoding 노드 CPU 메모리에 스트리밍 — incremental prefill과 overlap.
>
> **(s4) Decoding:** decoding 노드 CPU DRAM에 KVCache가 다 모이면 continuous batching에 합류. Conductor가 미리 골라뒀지만 local scheduler가 TBT SLO를 **double-check** 하여 부하 변화 시 거부할 수 있음(이 경우 prefill 비용은 낭비).
>
> **KVCache 풀(Figure 3):** CPU 메모리에 paged block으로 저장. LRU/LFU 등 eviction 가능. 각 블록은 자기 hash + prefix hash로 dedup용 hash 부여. CPU↔GPU 전송은 Messenger 담당. 외부에 context caching API도 제공.
>
> **§5.1 Multi-node Prefill (CPP):** 긴 컨텍스트(8k→128k→1M)에서 input이 output의 10~100배. TP를 단일 8x GPU 노드 넘어 확장하면 layer당 RDMA all-reduce 2회로 MFU가 급감. SP(Ring/Striped Attention)도 잦은 cross-node 통신으로 KVCache 전송과 경쟁. 그래서 **Chunked Pipeline Parallelism(CPP)** — X개 노드를 pipeline prefill 그룹으로 묶고 요청을 chunk로 분할해 노드 간 병렬 처리. 통신이 pipeline stage 경계에서만 발생해 overlap 용이, 짧은/긴 컨텍스트 모두에 자연스럽게 맞음. (저자들이 아는 한 inference에 pipeline 가속을 처음 적용.)
>
> **§5.2 Layer-wise Prefill:** prefill은 layer 단위 compute-bound이므로 KVCache load/store를 비동기 launch/wait로 연산과 overlap. 각 layer attention 전 해당 layer KVCache 로딩 완료를 대기하고 다음 layer 로딩을 트리거, attention 후 저장을 비동기 launch. 덕분에 **prefill 스케줄링에서 VRAM 크기를 무시**할 수 있음(단일 요청만 담으면 됨).
>
> **§6 KVCache-centric Scheduling (Algorithm 1):** 입력 토큰을 블록(block size B=512)으로 나눠 prefix hash 계산 → 각 prefill 노드 cache key와 비교해 prefix match length 산출 → 요청 길이와 prefix_len으로 prefill 실행시간 추정 + 대기시간(queue) 합산해 instance별 TTFT 추정 → **가장 짧은 TTFT** instance에 배정. SLO 불가 시 **HTTP 429** 반환.
>
> **§6.2 Cache Load Balancing (hot-spot migration):** prefill 노드마다 로컬 prefix cache 보유, 인기도 편차 큼(시스템 프롬프트 vs 개인 문서). 미래 사용 정확 예측이 불가하므로 **heuristic 기반 자동 hot-spot migration** — 최적 원격 prefix가 (현재 로컬 재사용 prefix × threshold)보다 길지 않으면 차라리 로컬에서 재연산, hot block을 여러 노드로 자동 복제해 transfer 혼잡 방지(Algorithm 1 line 28-29).
>
> **§7 Overload-oriented Scheduling / Early Rejection:** disaggregation 덕에 prefill·decoding 부하를 독립 측정(예측 최대 TTFT/TBT vs l_ttft, l_tbt). **Early Rejection** = decoding instance 부하 평가를 prefill 시작 전으로 앞당김. 단순 early rejection은 예측-실행 time lag 때문에 prefill↔decoding의 **anti-phase 부하 진동(Figure 9, 10a)** 유발. 해결책은 **Early Rejection Based on Prediction** — prefill 후 decoding 부하를 예측. 현재는 **system-level prediction**(요청마다 균일 decoding 시간 t_d 가정, 시점 t에 완료될 요청 가감 후 평균 TBT ratio로 부하 예측), request-level은 future work.

---

## 평가

8x NVIDIA A800-SXM4-80GB(80GB HBM, NVLINK, 노드 간 최대 800 Gbps RDMA), dummy **LLaMA2-70B** 모델(독점 정보 보호용, 동일 아키텍처). baseline은 **vLLM**(continuous batching + PagedAttention). SLO 기준: TTFT_P90 = 10×, TBT_P90 = 5× (관측 최저 RPS 대비). 모든 값은 SLO 상한 1.0으로 정규화.

> [!note]- 수치 결과
> - **Abstract 헤드라인:** 특정 시뮬레이션 시나리오에서 baseline 대비 **최대 525% throughput 증가**(SLO 준수). 실제 워크로드에서 Kimi가 **75% 더 많은 요청** 처리. (p.1)
> - **공개 데이터셋(§8.1.1, Figure 11):** Mooncake-[3P+1D]가 ArXiv Summarization / L-Eval에서 vLLM-[4M] 대비 throughput 각각 **+20% / +40%**, SLO 준수. L-Eval은 prefix caching으로 prefill 단축되어 추가 향상. (p.15)
> - **시뮬레이션 데이터(§8.1.2, Figure 12, 16k/32k/64k/128k prompt):** 2단계 disaggregation이 prefill의 decoding 간섭을 최소화해 TBT SLO를 절대 깨지 않음. throughput 향상 **50%~525%**. (p.16)
> - **실제 워크로드(§8.1.3, Figure 13):** Mooncake-[10P+10D] vs vLLM-[20M], TTFT 상한 30s, TBT 0.1s/token. TTFT는 둘 다 거의 100% SLO 충족하나, TBT SLO는 Mooncake ~100% vs **vLLM 57%**. 결과적으로 Mooncake가 **약 75% 더 많은 요청** 처리. (p.17)
> - **Overload(§8.2, Table 3):** 8P+8D, 실제 trace 23,000 요청을 2배속 재생. 거부 요청 수 — Baseline **4,183** / Early Rejection **3,771** / Early Rejection based on Prediction **3,589**. early & prediction 기반이 불필요한 prefill 연산과 부하 진동을 줄임. (p.17)
> - **Trace 통계(§4):** 23,608 entries, 평균 input 7,590 tokens, 평균 output 182 tokens(input/output ≈720). cache hit ratio는 capacity 1,000→50,000 block에서 30%→50% 상승, 그 이상은 미미. LRUCache가 최고. block 50% 이상은 미사용, 일부 block은 수만 회 접근(hot block 복제 필요, Figure 6). (p.7-8)

---

## 섹션 노트

- **§1 Introduction:** 동기(MaaS 최적화 문제), KVCache 중심 스케줄링 제안, disaggregated 설계 개요. trace는 dummy LLaMA2-70B 기반·실제 텍스트 미포함, github.com/kvcache-ai/Mooncake 공개.
- **§2 Preliminary & Problem Definition:** prefill(compute-bound, superlinear) vs decoding(memory-bound) 특성, TTFT/TBT SLO, goodput 정의. Figure 2가 길이/배치별 latency·throughput 곡선.
- **§3 Architecture Overview:** Figure 1·3·4. prefill/decoding 분리 + 분산 KVCache 풀 + Conductor + Messenger. 4단계 워크플로.
- **§4 Sampled Real-world Request Trace:** 1시간 trace 공개(timestamp/input_length/output_length/hash_ids, block size 512). 최초의 reuse 분석용 오픈 데이터셋 주장.
- **§5 Implementation of the Prefill Pool:** disaggregation 유지 이유, CPP(§5.1), layer-wise prefill(§5.2). Figure 7이 layer-wise가 long context 저장 latency를 크게 줄임을 보임.
- **§6 KVCache-centric Scheduling:** Algorithm 1, prefill global scheduling(§6.1), cache load balancing & hot-spot migration(§6.2). Figure 8: KVCache-centric > cache-aware > load-balancing > random.
- **§7 Overload-oriented Scheduling:** overload 정의, early rejection(§7.2), 부하 진동 문제(§7.3), 예측 기반 early rejection(§7.4, system-level).
- **§8 Evaluation:** 공개/시뮬/실제 데이터셋 + overload 실험.
- **§9 Related Work / §10 Future Work / §11 Conclusion:** DistServe·TetriInfer·Splitwise(동시기 disaggregation), Prompt Cache·SGLang(RadixAttention)·AttentionStore(동시기 계층 KVCache)와 비교. future: heterogeneous accelerator, PIM/hybrid bonding, MLA, KVCache 압축, priority 기반 스케줄링.

---

## 핵심 용어

- **KVCache:** prefill에서 계산된 key/value 중간 결과. decoding 단계에서 재사용되어 autoregressive 생성을 가속하는 캐시.
- **Disaggregation:** prefill 노드 풀과 decoding 노드 풀을 물리적으로 분리하고, CPU/DRAM/SSD/RDMA까지 별도 자원 풀로 재구성하는 것.
- **TTFT / TBT:** Time To First Token(prefill 지연) / Time Between Tokens(decoding 토큰 간 지연). 두 latency SLO.
- **Goodput / Effective Throughput:** SLO 안에서 **완전히 끝낸** 요청만 카운트하는 처리량(중도 거부 요청의 소비 토큰은 0으로 계산).
- **Conductor:** 전역 스케줄러. KVCache 분포·부하 기반 요청 분배 + 블록 복제/swap.
- **Messenger:** 각 노드의 GPUDirect RDMA 기반 KVCache 전송 컴포넌트.
- **CPP (Chunked Pipeline Parallelism):** 긴 컨텍스트 prefill을 여러 노드의 pipeline으로 chunk 단위 병렬 처리하는 기법.
- **Layer-wise Prefill:** layer 단위 비동기 KVCache load/store를 연산과 overlap해 VRAM 점유를 무시 가능하게 함.
- **Early Rejection / Prediction-based:** overload 시 prefill 전 단계에서 decoding 부하를 (예측해) 평가해 일찍 거부, prefill 낭비와 부하 진동 완화.
- **prefill_chunk:** prefill을 chunk로 분할하는 임계 토큰 수(보통 1000 이상).

---

## 강점 · 한계 · 열린 질문

**강점**
- 실제 상용 서비스(Kimi)에서 검증된 production 아키텍처. dummy 모델 기반이지만 실제 trace 공개로 재현성 확보.
- KVCache를 스케줄링 중심에 둔 통합 관점 — reuse·load balance·SLO를 한 알고리즘에 묶음.
- overload를 1급 문제로 다룬 드문 연구(early rejection + 부하 진동 분석 + 예측).
- CPP/layer-wise prefill로 long-context prefill의 MFU·VRAM 문제를 실용적으로 해결.

**한계**
- 실제 워크로드 KVCache 재사용률이 공개 벤치마크보다 훨씬 낮음 — 이론상 최대 50%만 재사용 가능(서비스에 따라 90%까지 가능하나 chat-to-paper 같은 특수 케이스). (p.18)
- prefill/decoding 노드 비율을 미리 고정([3P+1D] 등) — 동적 변환은 future work. Mooncake-[2P+2D]는 부하 불균형으로 TTFT가 나쁨.
- 부하 예측이 현재 **system-level**(균일 t_d 가정)만 구현, 정밀한 request-level output 길이 예측은 미해결.
- hot-spot migration의 threshold가 **수동 조정**(future: 적응적 알고리즘).

**열린 질문**
- request-level output 길이 예측을 저비용·고정밀로 어떻게 달성할까?
- heterogeneous accelerator(bandwidth-oriented vs compute-oriented) 분리로 decoding memory-bound 연산을 얼마나 더 싸게?
- KVCache 압축(MLA, token pruning 등)과 Mooncake의 reuse 스케줄링이 어떻게 상호작용하나?

---

## ❓ Q&A

> [!question]- Q1. Mooncake의 핵심 트레이드오프는?
> A. "더 많은 저장(storage), 더 적은 계산(computation)." 유휴 CPU/DRAM/SSD를 KVCache 저장에 동원해 중복 prefill 연산을 재사용으로 대체한다.

> [!question]- Q2. prefill과 decoding을 왜 분리하나?
> A. 둘의 연산 특성(compute-bound vs memory-bound)과 SLO(TTFT vs TBT)가 다르고, 분리하면 각 풀을 독립 최적화(prefill에 CPP, layer-wise VRAM 무시)하고 부하를 독립 측정할 수 있어 SLO 위반·간섭이 줄기 때문이다.

> [!question]- Q3. cache-aware 스케줄링은 기존 load-balancing과 무엇이 다른가?
> A. 요청 수(load)뿐 아니라 각 instance의 **prefix cache match length**와 KVCache 분포를 함께 보고, 요청별 추정 TTFT(prefill+queue)가 가장 짧은 노드에 배정한다(Algorithm 1).

> [!question]- Q4. CPP가 SP(Ring/Striped Attention)보다 나은 이유는?
> A. SP는 layer마다 cross-node 통신이 발생해 MFU를 낮추고 KVCache 전송 대역폭과 경쟁한다. CPP는 통신이 pipeline stage 경계에서만 발생해 연산과 overlap하기 쉽고 짧은/긴 컨텍스트 모두에 부담이 없다.

> [!question]- Q5. layer-wise prefill의 효과는?
> A. layer 단위 KVCache load/store를 연산과 비동기 overlap해 전송 오버헤드를 prefill 시간 안에 숨긴다. 덕분에 prefill 스케줄링에서 VRAM 크기를 사실상 무시하고(단일 요청만 담으면 됨) KVCache 분포·DRAM만 고려하면 된다(Figure 7).

> [!question]- Q6. 단순 early rejection의 문제와 해결책은?
> A. 예측-실행 time lag 때문에 prefill↔decoding 부하가 anti-phase로 크게 진동(수락 폭주→거부 폭주 반복)해 자원 활용이 나빠진다. 해결책은 prefill 후 decoding 부하를 **예측**해 수락 여부를 정하는 Early Rejection Based on Prediction(현재 system-level)이다(Figure 9, 10).

> [!question]- Q7. 실제 워크로드에서 vLLM 대비 어떤 metric에서 갈렸나?
> A. TTFT는 양쪽 다 거의 100% SLO 충족했지만 **TBT SLO**가 갈렸다 — Mooncake ~100% vs vLLM 57%. long context가 vLLM decoding을 방해하는 반면 Mooncake는 disaggregation으로 TBT를 지켜 약 75% 더 많은 요청을 처리했다.

> [!question]- Q8. 525%라는 수치는 어디서 나왔나?
> A. 시뮬레이션 데이터 실험(§8.1.2, Figure 12)에서 특정 long-context 시나리오의 throughput 향상 범위(50%~525%)의 상단. 실제 워크로드 수치는 +75%다.

---

## 🔗 Connections

[[LLM Systems]] · [[FAST]] · [[2025]]

관련: [[InstAttention]] · [[Sparse Checkpointing for Fast and Reliable MoE Training]]

- **Disaggregation 동시기 연구:** DistServe [8], TetriInfer [9], Splitwise [7] — prefill/decoding 분리로 goodput 최적화. Mooncake은 여기에 KVCache 중심 전역 스케줄링과 분산 캐시 풀을 더함.
- **KVCache 저장/재사용:** AttentionStore [35](동시기 계층 KVCache), Prompt Cache [33], SGLang RadixAttention [34]. Mooncake은 standalone 캐시가 아니라 memory-efficient 저장 + cache-aware 스케줄링을 통합.
- **Baseline:** vLLM [13](PagedAttention, continuous batching).
- **병렬화:** Ring Attention [18], Striped Attention [19], TeraPipe [24](training용 token-level pipeline) — CPP의 inference 적용 영감.

---

## References worth following

- **[8] DistServe** (arXiv:2401.09670) — prefill/decoding 분리로 goodput 최적화, Mooncake과 직접 비교군.
- **[9] TetriInfer** (arXiv:2401.11181) — chunked prefill + 2-stage disaggregation + 예측 기반 스케줄링.
- **[7] Splitwise** (arXiv:2311.18677) — phase splitting, Mooncake 개발 초기 동기.
- **[35] AttentionStore** (arXiv:2403.19708) — 계층적 KVCache 재사용, Mooncake과 설계 공유.
- **[13] vLLM / PagedAttention** (SOSP 2023) — baseline, KVCache paging의 기준.
- **[34] SGLang** (arXiv:2312.07104) — RadixAttention(LRU radix tree)로 자동 prefix 공유.
- **[15] Sarathi-Serve** (arXiv:2403.02310) — chunked prefill, disaggregation 대안 논쟁의 핵심.
- **[14] LoongServe** — elastic sequence parallelism, 동적 SP 그룹.
- **[47] DeepSeek-v2 MLA** — attention arithmetic intensity 증가, KVCache 축소 방향.

---

## Personal annotations

<본인 메모 영역>
