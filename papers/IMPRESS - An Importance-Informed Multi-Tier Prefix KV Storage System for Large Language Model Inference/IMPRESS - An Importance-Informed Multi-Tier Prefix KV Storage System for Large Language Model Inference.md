---
title: "IMPRESS: An Importance-Informed Multi-Tier Prefix KV Storage System for Large Language Model Inference"
aliases: [IMPRESS]
description: "중요 prefix KV만 골라 GPU/CPU/disk 3-tier로 적재해 LLM 추론 TTFT를 최대 2.8배 단축하는 importance-informed prefix KV 저장 시스템"
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/llm
  - topic/kv-cache
  - topic/llm
  - topic/prefix
  - venue/fast
  - year/2025
---

# IMPRESS: An Importance-Informed Multi-Tier Prefix KV Storage System for Large Language Model Inference

> **FAST 2025** · `cluster/llm` · Source: [IMPRESS - An Importance-Informed Multi-Tier Prefix KV Storage System for Large Language Model Inference.pdf](IMPRESS - An Importance-Informed Multi-Tier Prefix KV Storage System for Large Language Model Inference.pdf)
> 저자: Weijian Chen, Shuibing He (교신저자), Haoyang Qu, Ruidong Zhang, Siling Yang, Ping Chen, Yi Zheng, Baoxing Huai, Gang Chen — Zhejiang University, Huawei Cloud

## TL;DR
LLM 추론에서 공유 prefix의 KV cache를 disk에 두고 재사용할 때, SSD→GPU I/O latency가 TTFT의 51~98%를 차지하는 새로운 병목이 된다. IMPRESS는 (1) attention head 간 중요 토큰 index 집합이 매우 유사하다는 통찰을 이용해 일부 probe head의 key만 읽어 중요 토큰을 식별하는 similarity-guided ITF, (2) 중요 KV를 dense chunk로 재배치하는 KV reordering, (3) chunk의 중요도까지 반영하는 score-based cache 관리를 결합한다. 중요 KV만 선택적으로 GPU로 load함으로써 OPT-6.7B/13B/30B 3개 모델·4개 데이터셋에서 TTFT를 최대 2.8배 줄이면서 정확도 손실은 0.2% 미만으로 유지한다.

## 문제 & 동기
현대 LLM 응용은 사용자 질의 앞에 context-rich prefix(RAG 검색 문서, 대화 이력, few-shot 예시 등)를 붙여 출력 품질을 높인다. 이 prefix들은 요청 간에 부분/전체로 자주 반복되므로, 기존 시스템은 prefix의 K/V tensor(prefix KV)를 저장·재사용해 중복 계산과 TTFT(time to first token)를 줄인다. 그러나 GPU/CPU 메모리가 부족해 prefix KV를 disk에 두면, disk I/O latency가 너무 높아 재사용이 오히려 TTFT를 줄이지 못할 수 있다. 측정 결과 SSD→GPU I/O latency는 query 계산으로 숨기기 어렵고 전체 TTFT의 51~98%를 차지한다.

핵심 통찰은 "모든 토큰의 KV가 동등하게 중요하지는 않다"는 점이다. 일부 중요 토큰의 KV만 load해도 출력 품질을 유지할 수 있으므로, 느린 disk에서 읽는 데이터량을 줄여 I/O를 근본적으로 완화할 수 있다. 다만 두 가지 난제가 있다. (1) 공유 prefix 안에서도 어떤 토큰이 중요한지는 query마다 달라지므로 정적 사전 식별(SPI)은 정확도를 떨어뜨린다 — 모든 key를 GPU로 load해 attention score를 계산해야 하므로 I/O가 여전히 크다. (2) 기존 chunk 단위 저장·캐싱은 중요/비중요 KV를 섞어 read amplification과 cache 오염을 유발하고, chunk의 중요 KV 비율을 무시한 recency/frequency 기반 캐싱은 GPU hit ratio를 떨어뜨린다.

> [!quote]- 📄 원문 표현 (paper)
> "Our study shows that the I/O latency from SSD to GPU can be rarely hidden by query computation and accounts for 51%-98% of the total TTFT" (p.1, Abstract/§2.2 참조)
>
> "even with recall ratios above 80%, the omission of vital KVs results in accuracy and F1 score declines of up to 5% for RTE and 3.3% for SQuAD" (p.5, §3.1)
>
> "Figure 5(a) shows that only 46% of the KVs in the loaded chunks are important on average, leading to 2.2× read amplification." (p.6, §3.2)

## 핵심 통찰

> [!note]- 통찰 1 — 중요 토큰 index 집합이 head 간에 매우 유사하다 (Observation I)
> 같은 transformer layer 내의 서로 다른 attention head들은 같은 large K/V tensor에서 유도되므로, 한 head에서 중요한 토큰은 다른 head에서도 중요할 가능성이 높다. OPT-6.7B 중간 layer에서 head 간 중요 토큰 index 집합의 Jaccard 유사도 평균이 0.95를 넘는다 (p.6, Fig.7). 따라서 모든 head의 key를 읽지 않고 소수 "probe head"의 key만 읽어 중요 토큰을 추정할 수 있다 → I/O와 TTFT 동시 절감.

> [!note]- 통찰 2 — 유사성은 모델 규모·sampling 비율 전반에 존재 (Observation II)
> 더 많은 토큰(예: 40% vs 10%)을 중요하다고 선택할수록, 그리고 모델이 클수록 유사도가 높아진다. OPT-30B에서 40%/10% 선택 시 layer 평균 유사도가 각각 0.68/0.48로, 작은 모델·깊은 layer는 다소 낮지만 random 선택의 기대값보다는 유의하게 높다 (p.7, Fig.8). 유사도가 낮은 일부 layer에는 기법을 끄는 similarity threshold를 도입.

> [!note]- 통찰 3 — chunk의 hotness와 중요 KV 비율은 무관하다
> chunk 접근 빈도(hot/cold)와 그 chunk 안의 중요 KV 비율 사이에 상관관계가 없다 (p.5, Fig.5b). 따라서 recency/frequency만으로 캐싱하면 중요 KV가 많은 chunk가 느린 매체로 밀려나 GPU hit ratio가 떨어진다 → chunk의 중요도까지 점수에 반영해야 한다.

> [!note]- 통찰 4 — 정적 사전 식별(SPI)은 정확도를 해친다
> RAG처럼 같은 prefix를 공유해도 query별로 정답이 prefix의 다른 부분에 있을 수 있어, prefix별로 중요 토큰을 미리 고정하면 핵심 KV를 놓친다. recall 80% 이상에서도 정확도/F1이 최대 5% 하락 (p.5, Fig.4). → query마다 동적으로 식별하되 I/O는 최소화해야 한다.

## 설계 / 메커니즘

> [!abstract]- 전체 아키텍처 — 3-tier (GPU/CPU/disk) + ITF + PKM (§4.1, Fig.6)
> Data plane: 모든 prefix KV는 disk에 chunk 단위로 저장, 일부는 CPU memory cache 또는 GPU memory cache에 둔다(상호 배타적 — 공간 낭비 방지). CPU memory의 metadata는 radix tree로 조직되어 재사용 가능한 prefix KV를 빠르게 검색한다. 두 control 컴포넌트: ITF(Important Token Identification, §4.3)와 PKM(Prefix KV Management, §4.4). 데이터 흐름: 요청 S = [prefix 토큰들, query 토큰들]이 오면 radix tree로 GPU/CPU/disk에 존재하는 공유 prefix subsequence R을 찾고, ITF로 R 안의 중요 토큰 R_important를 식별한다. R_important만 GPU로 load하고(없으면 disk/CPU에서), 비중요 토큰의 KV는 재사용하지 않는다. R_important + (radix tree에 없는 새 prefix 토큰 NR) + query 토큰을 LLM에 보내 prefill을 완료한다. 중요도 metric은 H2O처럼 attention weight 행렬의 각 column 합을 토큰 중요도로 사용 (column 합이 클수록 중요).

> [!abstract]- Similarity-Guided Important Token Identification (ITF) (§4.3, Fig.9/10)
> 일부 head를 "probe head"로 골라 그 key만 disk에서 GPU로 load → attention weight 계산 → 각 probe head의 중요 토큰 index 집합 도출 → probe head 쌍 간 평균 Jaccard 유사도 계산. 유사도가 similarity threshold를 넘으면 모든 probe head가 가장 중요하다고 투표한 토큰 index {x}만 확정하고, 그 토큰들의 k/v를 모든 head에서 load해 나머지 prefill 계산. threshold 미달이면 해당 layer는 기법을 끄고 full로 fallback. probe head 수는 3개로 고정(1개는 bias, 2개는 voting 실패 위험; 더 늘리면 정확도 이득 미미·load 증가). 각 layer의 앞 3개 head를 probe로 사용(어떤 head를 고르든 정확도 영향 없음). 예: head 32, prefix 토큰 4개, 1개 중요 → 기존 full은 128 vector load 후 32 vector 추가 = 160; ITF는 probe 3 head의 key 12 vector(threshold 통과 시) + 중요 토큰 1개의 k/v 32 vector = 44 vector만 load.

> [!abstract]- KV Reordering — 중요 KV를 dense chunk로 재배치 (§4.4.1, Fig.12/13)
> 주기적으로(예: 10분마다, 평균 토큰 중요도 기준) 비동기로 chunk 내 토큰을 중요도 내림차순으로 재정렬·재패킹해 중요 KV를 적은 chunk에 모은다. 예: prefix [t0,t1,t2,t3]에서 t0,t3이 중요·chunk size 2이면, 재배치 전엔 두 chunk를 모두 읽어야 하지만 재배치 후 [t0,t3][t1,t2]로 묶여 한 chunk만 읽으면 됨. **Metadata 조정**: prefix KV 재사용은 첫 토큰부터 토큰 순서가 같아야 가능하므로, reordering은 radix tree 순서를 망가뜨린다. radix tree node마다 mapping list(예: m0=[0,2,3,1])를 추가해 원래 순서 s0를 torch index 연산 s0'[m0]으로 복원(전체 TTFT의 2% 미만). reordering 범위는 각 radix node 내부로 제한하고 cross-node reordering은 금지(공유되지 않는 토큰을 섞으면 radix tree 파괴·불필요 read 유발).

> [!abstract]- Score-Based Cache Management (§4.4.2, Fig.14)
> chunk마다 접근 빈도 × 보유 중요 KV 비율을 반영한 score 부여. score 높은 chunk를 GPU에 우선 캐싱, 낮으면 CPU로. importance ratio는 chunk 접근마다 moving average로 online 갱신. 예: chunk1(빈도 1.5, 중요 50%) score 0.75, chunk2(빈도 1.0, 중요 100%) score 1.0 → frequency만 보면 chunk1을 GPU에 두지만, score 기반은 chunk2를 GPU에 둬 CPU→GPU 전송을 20 key에서 15 key로 절감. **Dual-cache replacement**: CPU memory에 두 개의 min-heap으로 GPU/CPU cache의 최저 score chunk를 관리, eviction 시 disk에 chunk 복제본을 유지해 evict 시 I/O latency 제거. 새 요청 시 chunk가 GPU에 있으면 score 갱신 후 유지, CPU에 있으면 GPU 최저 score와 비교해 승격 여부 결정, disk에 있으면 CPU로 load 후 두 cache 최저와 비교.

> [!abstract]- 구현 (§5)
> FlexGen 위에 구현(white-box라 KV 식별 기법 개발 용이). prefix reuse를 위해 `mha` 함수를 수정하고 `attn_weight`의 used value로 KV 중요도 평가. KV reordering은 layer별 reordered KV·mapping을 저장하는 `PrefixKVLayer` 클래스, cache 관리는 score 정책의 `TokenCache` 클래스로 구현.

## 평가

> [!success]- 실험 환경 & 종합 성능 (§6.1~6.2)
> 환경: 2× AMD EPYC 7763(64코어), 128GB DRAM, NVIDIA A100 80GB HBM, 2TB Intel SSD(읽기 ~5GB/s), PCIe 4.0 ×16 (p.10). 모델: OPT-6.7B/13B/30B. 데이터셋: PIQA, RTE, COPA, OpenBookQA(LM-Evaluation-Harness). prefix는 4.8k~5.7k 토큰(OPT-30B 4K, 그 외 10K로 확장). cache: GPU 10GB·CPU 32GB 할당. baseline: ReComp(매번 재계산), AS-like(AttentionStore 재구현), AS+H2O+LRU, AS+H2O+LFU.
> - **정확도**: IMPRESS는 ReComp/AS+H2O+LRU 대비 정확도 하락 1% 미만, retention 50%(COPA)·25%(나머지) 설정에서 평균 0.2% 하락; 일부 경우엔 ReComp보다 약간 향상 (p.10~11, Fig.15).
> - **평균 TTFT**: 선두 대안 대비 1.2~2.8배 개선 (p.11, Fig.16). 중요 prefix KV를 GPU로 load하는 I/O time이 1.5~3.8배 감소 (Fig.17).
> - **tail latency(p99)**: RTE/OPT-30B에서 ReComp 3.9s, AS-like 9.3s, AS+H2O+LRU 6.6s, AS+H2O+LFU 5.9s, IMPRESS 2.95s (p.12, §6.2).

> [!success]- Ablation & sensitivity (§6.3~6.5)
> - **개별 기법 기여(+ITF/+RO/All)**: OPT-30B·RTE에서 세 기법 기여 60%/30%/10%, OPT-13B·COPA에서 36%/8%/56% (p.12, Fig.18). 데이터셋별로 다름.
> - **KV reordering**: load되는 chunk 수 평균 1.2배 감소 (p.13, Fig.20).
> - **score-based cache**: 평균 GPU hit ratio 68% → 80%로 향상 (p.13, Fig.21).
> - **alpha(threshold)**: PIQA top 25% 선택 시 threshold를 1→0이면 load key 4배 감소하나 정확도 79.1%→77.5%(1.6%↓); alpha=0.6에서 균형 (p.9 Fig.11, p.13 Fig.22).
> - **chunk size**: 16~256 전 범위에서 AS+H2O+LFU 대비 2.2~2.4배 TTFT 개선 (p.13, Fig.23).
> - **dataset size**: OpenBookQA 65GB~400GB에서 1.2~2.0배 speedup (p.13, Fig.24).
> - **Llama2-7B/13B**: PIQA·COPA에서 1.7~2.7배 speedup (p.14, Fig.25).
> - **overhead**: probe head load의 시간 overhead 평균 6%; reordering 비동기 실행으로 critical path 외, 전체 실행 1분 미만; 공간 overhead — mapping list + score + probe head key 중복 저장 합쳐 전체 prefix KV의 1.7%(0.5%+1.2%) 미만 (p.14, §6.5).

## 섹션 노트
- **§1 Introduction**: 3대 기여 — 첫 importance-informed 3-tier(GPU/CPU/disk) prefix KV 시스템; similarity-guided 중요 토큰 식별; KV reordering + score-based cache 관리; 최대 2.8배 TTFT 감소·정확도 0.2% 미만 하락.
- **§2 Background**: LLM 추론(prefill/decoding), transformer layer 계산(Q/K/V, head별 attention weight). 공유 prefix와 저장 시스템 — context-rich prefix가 TTFT를 크게 늘리고(Chameleon은 2600+ 토큰 추가, OPT-30B TTFT 9배), AttentionStore도 disk latency를 query 계산으로 완전히 숨기지 못함.
- **§3 Motivation/Challenges**: 모든 KV가 동등하지 않음 → 중요 KV만 load. 난제1(기존 중요 토큰 식별도 모든 key load 필요), 난제2(chunk 저장·캐싱이 중요도 무시). read amplification 2.2배, chunk hotness ≠ 중요 KV 비율.
- **§4 Design**: §4.1 개요, §4.2 통찰(Obs I/II), §4.3 ITF, §4.4 PKM(reordering + score cache).
- **§5 Implementation**: FlexGen 기반.
- **§6 Evaluation**: 종합 성능·ablation·sensitivity·overhead.
- **§7 Related Work**: KV cache reuse(decoding 내 vs prefill), KV pruning/quantization(H2O, InfiniGen 등 — IMPRESS와 직교, 병행 가능), 기타 효율 serving 시스템.
- **§8 Conclusion**: I/O latency가 있을 때 기존 prefix KV 재사용이 TTFT를 항상 줄이지 못함 → 중요 KV만 적재해 최대 2.8배 단축.

## 핵심 용어
- **prefix KV (cache)**: 요청 간 공유되는 prefix 토큰들의 K/V tensor. 재사용해 중복 prefill 계산과 TTFT를 줄인다. Q tensor는 후속 계산에 불필요해 저장하지 않는다.
- **TTFT (Time To First Token)**: 요청 도착부터 첫 출력 토큰 생성까지의 지연. prefill 단계 latency가 지배한다.
- **important token / KV**: attention weight 행렬 column 합이 큰, 출력 품질에 결정적인 토큰과 그 KV. H2O 방식의 중요도 metric 사용.
- **probe head**: 중요 토큰 식별을 위해 key를 실제로 load하는 소수(IMPRESS=3) attention head. head 간 유사성을 이용해 전체 head의 중요 토큰을 추정.
- **similarity-guided ITF**: probe head 중요 토큰 집합의 Jaccard 유사도가 threshold를 넘으면 그 합의된 중요 토큰만 load하는 식별 기법. I/O와 TTFT 절감.
- **KV reordering**: chunk 내 토큰을 중요도순 재정렬·재패킹해 중요 KV를 적은 chunk에 모으는 기법. read amplification 감소. radix node 내부로 범위 제한, mapping list로 순서 복원.
- **score-based cache management**: chunk 점수 = 접근 빈도 × 중요 KV 비율. 점수 높은 chunk를 GPU에 우선 캐싱해 GPU hit ratio↑, PCIe 전송↓.
- **read amplification**: 중요 KV를 읽으려고 같은 chunk의 비중요 KV까지 읽어 실제보다 많은 데이터를 읽는 현상(여기선 2.2배).
- **radix tree**: 재사용 가능한 공유 prefix subsequence를 빠르게 검색하기 위한 metadata 구조(SGLang/Chunkattention 계열).

## 강점 · 한계 · 열린 질문
- 강점: disk I/O가 새 병목이라는 측정 기반 동기(51~98%)가 명확하고, head 간 유사성이라는 비자명한 통찰을 I/O 절감으로 직접 연결. ITF/reordering/score-cache 세 기법이 직교적이며 ablation으로 각각의 기여를 분리. quantization/pruning과도 병행 가능. overhead(시공간) 분석이 충실(시간 6%, 공간 1.7% 미만).
- 한계: 통찰(head 유사성)은 수학적으로 증명되지 않고 OPT/Llama 대상 경험적 검증에 의존 — 다른 아키텍처(GQA/MQA, MoE)나 매우 깊은 layer에서의 일반성은 미지수. 작은 모델·깊은 layer는 유사도가 낮아 fallback 비율이 올라갈 수 있음. retention 비율·alpha 등 hyperparameter 튜닝 의존. FlexGen 단일 GPU 기반 구현이라 분산/멀티-GPU serving으로의 확장은 미평가. KV reordering의 10분 주기·평균 중요도 기반 트리거가 빠르게 변하는 workload에 적절한지 불명확.
- 열린 질문: GQA처럼 head가 K/V를 공유하는 구조에서 probe head 통찰이 유지되는가? decoding 단계 KV pruning(H2O)과 prefix KV importance를 통합하면 추가 이득이 있는가? cross-node reordering 금지로 인해 RAG처럼 부분 공유가 많은 workload에서 packing 효율이 얼마나 제한되는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 왜 prefix KV를 저장·재사용해도 TTFT가 줄지 않을 수 있는가?
> > GPU/CPU 메모리 부족으로 prefix KV를 disk에 두면, SSD→GPU I/O latency가 query 계산으로 거의 숨겨지지 않고 전체 TTFT의 51~98%를 차지한다. 이 I/O 병목 때문에 재사용이 재계산(ReComp)보다 느려질 수 있다 (p.1, §2.2).

> [!question]- Q2. IMPRESS가 의존하는 핵심 통찰 두 가지는?
> > (Obs I) 같은 layer 내 head들의 중요 토큰 index 집합이 매우 유사하다(OPT-6.7B 중간 layer Jaccard >0.95). (Obs II) 이 유사성은 모델 규모와 selection 비율 전반에 존재하며, 토큰을 많이 선택할수록·모델이 클수록 유사도가 높다 (p.6~7, Fig.7/8).

> [!question]- Q3. similarity-guided ITF는 어떻게 I/O를 줄이면서 정확도를 지키는가?
> > 모든 head 대신 3개 probe head의 key만 load해 중요 토큰을 추정하고, probe head 간 Jaccard 유사도가 similarity threshold를 넘을 때만 합의된 중요 토큰만 load한다. threshold 미달 layer는 full로 fallback하므로(평균 20% 미만 layer만 full) 정확도 저하를 막는다 (p.8, §4.3).

> [!question]- Q4. 정적 사전 식별(SPI)이 아니라 동적 식별을 쓰는 이유는?
> > 같은 prefix라도 query마다 중요 토큰이 달라(RAG에서 정답 위치가 다름), prefix별로 미리 고정하면 핵심 KV를 놓쳐 recall 80%에서도 정확도/F1이 최대 5% 하락하기 때문이다 (p.5, §3.1, Fig.4).

> [!question]- Q5. KV reordering이 radix tree 재사용을 깨지 않게 하는 방법은?
> > 토큰 순서가 바뀌면 첫 토큰부터 같은 순서를 요구하는 prefix 재사용이 깨진다. 각 radix node에 mapping list(예: m0=[0,2,3,1])를 추가해 torch index 연산으로 원래 순서를 복원하고(전체 TTFT의 2% 미만), reordering 범위를 node 내부로 제한하며 cross-node reordering을 금지한다 (p.8~9, §4.4.1).

> [!question]- Q6. score-based cache가 frequency/recency 기반보다 나은 이유는?
> > chunk의 hotness와 중요 KV 비율은 무관하므로(Fig.5b), 빈도만 보면 중요 KV가 많은 chunk가 느린 매체로 밀린다. score = 빈도 × 중요 KV 비율로 캐싱하면 GPU hit ratio가 68%→80%로 오르고 PCIe 전송이 준다 (p.9~10, 13, §4.4.2, Fig.14/21).

> [!question]- Q7. 핵심 성능 수치는?
> > TTFT 최대 2.8배(평균 1.2~2.8배) 단축, I/O time 1.5~3.8배 감소, read amplification 2.2배 해소, GPU hit ratio 68%→80%, p99 tail RTE/OPT-30B 2.95s, 정확도 하락 0.2%(평균)~1% 미만 (Abstract, §6).

> [!question]- Q8. IMPRESS의 overhead와 다른 KV 압축 기법과의 관계는?
> > probe head load의 시간 overhead 평균 6%, 공간 overhead 1.7% 미만(mapping+score+probe key 중복), reordering은 비동기로 critical path 밖. KV pruning/quantization(H2O, KVQuant 등)과 직교하여 결합 가능하다 (p.7 §7, p.14 §6.5).

## 🔗 Connections
[[LLM Systems]] · [[FAST]] · [[2025]]

## References worth following
- [5] B. Gao et al. **AttentionStore: Cost-Effective Attention Reuse across Multi-Turn Conversations** — IMPRESS의 직접 비교 대상(AS-like baseline), disk+CPU prefix KV 재사용.
- [44] Z. Zhang et al. **H2O: Heavy-Hitter Oracle for Efficient Generative Inference** (NeurIPS'23) — IMPRESS가 채택한 attention-column-합 중요도 metric의 출처.
- [17] W. Lee et al. **InfiniGen: Efficient Generative Inference with Dynamic KV Cache Management** (OSDI'24) — 동적 KV 관리, 중요 토큰 식별의 대안적 접근.
- [32] Y. Sheng et al. **FlexGen: High-Throughput Generative Inference with a Single GPU** (ICML'23) — IMPRESS 구현 기반.
- [45] L. Zheng et al. **SGLang: Efficient Execution of Structured Language Model Programs** — radix tree 기반 prefix 재사용.
- [21] Y. Liu et al. **CacheGen: KV Cache Compression and Streaming** (SIGCOMM'24) — prefix KV 압축·streaming, 직교적 결합 가능.

## Personal annotations
<본인 메모 영역>
