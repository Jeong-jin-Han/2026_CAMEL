---
title: "MaxEmbed: Maximizing SSD bandwidth utilization for huge embedding models serving"
aliases: [MaxEmbed]
description: "복제(replication)로 embedding 조합 가능성을 넓혀 SSD의 read amplification을 완화하고 effective bandwidth를 최대화하는 DLRM embedding 저장·서빙 시스템"
venue: ASPLOS
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/llm
  - topic/embedding
  - topic/recsys
  - topic/ssd-bandwidth
  - venue/asplos
  - year/2025
---

# MaxEmbed: Maximizing SSD bandwidth utilization for huge embedding models serving

> **ASPLOS 2025** · `cluster/llm` · Source: [MaxEmbed - Maximizing SSD bandwidth utilization for huge embedding models serving.pdf](MaxEmbed%20-%20Maximizing%20SSD%20bandwidth%20utilization%20for%20huge%20embedding%20models%20serving.pdf)

**저자**: Ruwen Fan, Minhui Xie\*, Haodi Jiang, Youyou Lu\* (Department of Computer Science and Technology, Tsinghua University). \*교신저자: Minhui Xie, Youyou Lu.

> [!note] 출판 정보
> 본문 PDF 헤더와 ACM Reference는 **ASPLOS '24, Volume 4 (April 27–May 1, 2024, La Jolla, CA)**, DOI 10.1145/3622781.3674172 로 표기됨. frontmatter의 `year: 2025`는 stub 작성 당시 메타데이터 기준이며, 실제 게재는 ASPLOS '24 spring round임.

## TL;DR
DLRM의 거대한 embedding table을 SSD에 올리면 embedding 크기(64~512B)와 SSD page granularity(4KB)의 불일치로 심각한 **read amplification**이 발생한다. 기존 SOTA(Bandana, hypergraph partition)는 자주 함께 읽히는 embedding을 같은 page에 co-locate하지만 각 embedding이 **단 하나의 cluster에만** 배치되어 조합 가능성이 제한된다(Criteo에서 effective bandwidth 8.58%에 불과). MaxEmbed는 hotspot embedding을 **선택적으로 복제(replication)**해 더 많은 co-appear 조합을 한 page에 담고, 온라인 서빙에서는 pipeline + index limit 최적화로 replica 선택(set cover, NP-hard) 오버헤드를 숨긴다. 결과적으로 effective bandwidth 최대 +19%, end-to-end throughput 최대 **18.7%** 향상.

## 문제 & 동기
- DLRM 파라미터는 연 10배로 증가하지만 DRAM 용량 성장은 느림 → 대용량·저비용인 SSD에 embedding 저장이 필수화.
- SSD는 page granularity(typically 4KB)로 읽지만 embedding은 64~512B로 작음 → 하나의 embedding을 위해 4KB page 전체를 읽어 effective bandwidth가 크게 떨어짐(**read amplification**).
- 선행 연구 Bandana는 hypergraph partition(SHP)으로 함께 등장하는 embedding을 같은 page에 co-locate하지만, partition은 각 vertex를 **하나의 cluster에만** 배치하므로 조합이 파괴됨. 실측 effective bandwidth는 Criteo에서 약 8.58%에 그침.
- 핵심 관찰: 자연적으로 함께 등장하는 embedding 수가 한 page 용량(8~32개)을 초과(CriteoTB는 top 5% hot embedding이 40개 이상과 co-appear)하므로 단순 재배치만으로는 부족하다.

> [!quote]- 📄 원문 표현 (paper)
> "SSDs have a fixed page granularity, typically 4KB, while embedding parameters are much smaller, ranging from 64 to 512 bytes [28, 30]. When reading an embedding, the whole 4KB page needs to be read from SSD, which causes read amplification in SSDs." (p.1)
>
> "Although it alleviates the phenomenon of reading amplification, the effective bandwidth of SSD is still very limited, only about 8.58% in our evaluation." (p.1)
>
> "the optimization goal of partition algorithms is to maximize the opportunity of co-appearance within one SSD page. Thus, any vertex in the hypergraph will be partitioned into only one cluster ... causing the combination ⟨1, 2, 3, 4⟩ to be destroyed." (p.3–4)

## 핵심 통찰

> [!abstract]- 토글: 핵심 통찰
> 1. **단일 배치(single placement)의 한계**: hypergraph partition은 vertex를 하나의 cluster에만 넣으므로, 한 embedding이 hyperedge 상에서 인접한 많은 vertex 중 일부와만 co-locate된다. 함께 쿼리되지만 co-locate되지 않은 embedding은 여러 번의 page read를 유발.
> 2. **복제로 조합 확장**: SSD는 용량이 매우 크므로(수 TB), 정해진 space amplification ratio `r` 하에서 hotspot embedding을 여러 page에 **복제**하면 더 많은 co-appear 조합을 한 page에 담을 수 있다. 이것이 effective bandwidth를 올리는 본질.
> 3. **무엇을·어디에 복제할지가 비자명**: Rep-MBEP(replication 있는 max-bandwidth embedding placement)은 NP-hard. 단순히 hot 기준으로 복제(RPP)하거나 cluster를 잘게 쪼개 채우면(FPR) 조합 관계를 무시하거나 파괴해 효과가 작거나 불안정.
> 4. **connectivity-priority가 답**: vanilla partition을 먼저 수행해 원래 조합을 보존한 뒤, hyperedge connectivity와 hotness를 함께 고려한 score로 복제 대상과 위치를 정한다.
> 5. **온라인 page selection도 NP-hard(set cover)**: 복제로 한 embedding이 여러 page에 존재 → 한 쿼리를 덮는 최소 page 집합 찾기는 set cover. greedy가 e2e latency의 56%+ 차지 → pipeline·index limit로 숨김.

## 설계 / 메커니즘

> [!abstract]- 토글: 설계 / 메커니즘
> **두 단계 구조 (Fig.4)**: Offline(partition & replication) + Online(query request processing).
>
> ### Offline Phase — Connectivity-priority Replication (§5)
> 목표: ratio `r` 하에서 일부 embedding을 선택 복제해 effective bandwidth 최대화 (Rep-MBEP, NP-hard).
> - **Strawman 1 (RPP, Replication Prior to Partition)**: hot vertex 상위 `r` 비율을 미리 복제 후 partition에 맡김 → replica가 인접 관계가 없을 수 있고 중복 조합으로 공간 낭비, 개선 미미.
> - **Strawman 2 (FPR, Finer-Partition + fill with Replication)**: `(1+r)N/d` 개의 더 작은 cluster로 partition 후 각 cluster를 most co-appear vertex로 채움 → finer partition이 원래 조합을 파괴, 데이터셋별로 불안정(때로 복제 없는 것보다 나쁨).
> - **MaxEmbed 해법 (§5.3)**:
>   1. vanilla SHP로 hypergraph partition (원래 조합 보존).
>   2. 각 vertex score 계산: `score(vᵢ) = Σ_{j ∈ related_edge(vᵢ)} (λ(j) − 1)`. `λ(j)`는 hyperedge `j`의 connectivity(쿼리가 걸친 cluster 수 ≈ read 연산 수). connectivity 기여 + hotness를 동시 반영.
>   3. 상위 `rN/d` 개 vertex 선택(`d`=한 cluster가 담는 embedding 수).
>   4. 각 선택 vertex에 대해, 연결된 hyperedge를 순회해 가장 빈번한 `d−1`개 이웃을 찾아(이미 같은 cluster에 있는 것 제외) replica cluster 생성.
> - 효과: partition 결과를 보존하므로 복제가 기존 조합을 파괴하지 않고, connectivity 높은 곳의 edge connectivity를 낮춰 page read 수를 줄임.
> - 출력: embedding key → SSD page 위치 mapping.
>
> ### Online Phase — Query Request Processing (§6)
> 복제로 한 embedding이 여러 page에 존재 → **page selection = set cover (NP-hard)**. greedy가 e2e latency의 56%+ 차지.
> - **Forward Index**: embedding → 그것을 담은 SSD page들. **Invert Index**: SSD page → 담긴 embedding들. 둘 다 DRAM-resident, O(1) 조회.
> - **One-pass selection (§6.1)**: ① replica 적은 embedding부터(replica count 오름차순 정렬) 순회 ② Forward Index로 후보 page 찾기 ③ Invert Index로 가장 많은 미충족 embedding을 덮는 page 선택 ④ SSD read 발행. 복잡도 O(|S|+|Q|)로 감소(naive O(|S|·|Q|)).
> - **Index shrinking (§6.1, Fig.7)**: 고복제 embedding의 index를 앞 `k`개(예: `k=3`) cluster로 제한. invert index 덕에 정확성 보존, ③ 검색 오버헤드를 O(k)로 제한.
> - **Pipelined replica selection + SSD access (§6.2, Fig.6)**: SSD read는 후속 page selection과 독립적이고 둘의 latency order가 비슷 → 매 iteration에서 선택된 page를 **async SSD read**로 발행하고 다음 selection 진행, 마지막에 일괄 poll. 소프트웨어 오버헤드를 SSD read 뒤에 숨김.

## 평가

> [!abstract]- 토글: 평가
> **셋업 (§8.1)**: Intel Xeon GOLD 6530, 128GB, Intel Optane P5800X SSD, Ubuntu 22.04, user-space driver SPDK, DRAM cache로 Meta의 CacheLib(LRU, 기본 cache ratio 10%). embedding dim 기본 64 (256B). 데이터셋 5개: Amazon M2(1.39M items/3.6M queries/0.26GB), Alibaba-iFashion(4.46M/999K/1.4GB), Avazu(9.45M/40.4M/5.3GB), Criteo(35M/45.8M/9.5GB), CriteoTB(882M/4.37B/1.1TB) (Table 3, p.9).
>
> **Effective bandwidth (Fig.8)**: r=10% 추가 공간으로 baseline(SHP) 대비 **+2~10%** 개선, e2e **1.7~8.8%** 향상. r=80%에서 effective bandwidth **+7~19%**, e2e throughput **+8.9~18.7%** (p.2, Abstract +18.7%, Conclusion 최대 1.19×).
>
> **End-to-end (Fig.10, Fig.11)**: throughput 5개 데이터셋에서 r=10% 시 1.7~8.88%, r=80% 시 8.9~18.7% 향상. latency는 r=10% 시 **2~7.4%**, r=80% 시 **10~14.8%** 감소(쿼리당 SSD page read 수 감소 덕). 쇼핑 데이터셋(co-appearance 강함)에서 이득이 더 큼.
>
> **Valid embeddings per read (Fig.9)**: Criteo r=10%에서 한 read당 평균 valid embedding이 **3.59 → 4.79**로 증가.
>
> **Replication 전략 비교 (Fig.14)**: MaxEmbed(connectivity-priority)가 모든 데이터셋에서 안정적이고 높은 개선. RPP는 안정적이나 개선 작음, FPR은 Amazon M2 외엔 불안정(때로 effective bandwidth 감소).
>
> **온라인 오버헤드 분석 (§8.4, Fig.15)**: 8 thread, Alibaba-iFashion r=40%. pipeline으로 request processing 오버헤드 **10.23%** 감소, index limit `k=5` 추가 시 pipeline 대비 **34.4%** 추가 감소. 최적화 후 selection이 전체 SSD operation의 CPU 25% 미만.
>
> **Index shrinking 영향 (Fig.16)**: r=80%에서 key당 10개 index만 저장해도 전체 저장 대비 effective bandwidth **98% 이상**, `k=5`로도 **96% 이상** 유지.
>
> **Sensitivity (§8.5, Fig.17)**: embedding dim 32/64/128에서 dim이 클수록 SHP의 한계가 커져 복제 효과 더 큼. SSD type(P5800X / P4510 / RAID0 2×P5800X)에 일관된 개선 → SSD 종류 영향 미미.
>
> **Overhead (§7)**: 메모리 O(N) (두 DRAM index, O((1+r)N), r은 작은 상수). partition time(Table 1): Criteo 16-in-part 5min, CriteoTB 3hour (r=10%). **TCO (Table 2)**: CriteoTB(225GB, r=0.8→~400GB). Perf/Cost: P5800X 1.04×, PM1735 1.12× (성능 1.16×).
>
> **Cache 민감도 (Fig.12, Fig.13)**: cache 1~40%에서 MaxEmbed 최대 1.2× throughput. **cacheless** 시 효과 더 두드러져 r=0.2로도 **1.08~1.31×**, pure DRAM 대비 9~26× (Fig.13).

## 섹션 노트
- **§1 Introduction**: SSD page(4KB) vs embedding(64~512B) 불일치 → read amplification. SHP도 Criteo 8.58%에 그침.
- **§2 Background**: DLRM 구조(Fig.1), embedding이 모델 파라미터의 99.9%까지 차지. Bandana/SHP의 hypergraph partition 소개(Fig.2).
- **§3 Motivation**: SHP가 vanilla 대비 effective bandwidth 1.1~2.2× 개선하나 여전히 SSD cap 대비 매우 낮음(Fig.3). 단일 cluster 배치가 조합 파괴 → **selective replication** 아이디어.
- **§4 Overview**: offline + online 두 단계(Fig.4), Forward/Invert Index.
- **§5 Offline**: Rep-MBEP(NP-hard), RPP/FPR strawman, connectivity-priority 해법(score 식, Fig.5).
- **§6 Online**: set cover, one-pass selection, index shrinking, pipeline(Fig.6, Fig.7).
- **§7 Overhead**: 메모리 O(N), partition time(Table 1), TCO(Table 2).
- **§8 Evaluation**: Fig.8~17, Table 3.
- **§9 Related Work**: Bandana, Merci(sub-query memoization), GRACE, RecSSD/FlashEmbedding(SSD 내부 변경 vs MaxEmbed는 general SSD).
- **§10 Conclusion**: replication 기반 hypergraph partition 저장·검색, effective bandwidth 최대 1.19×.
- **A Artifact**: code https://github.com/Ksitta/MaxEmbed (Artifacts Available/Evaluated/Reproduced 배지).

## 핵심 용어
- **DLRM (Deep Learning Recommendation Model)**: sparse feature를 embedding table로 표현하는 추천 모델. embedding이 파라미터의 대부분.
- **Read amplification**: SSD page granularity(4KB) > embedding 크기(64~512B)로 인해 작은 embedding 하나에 page 전체를 읽어 effective bandwidth가 떨어지는 현상.
- **Effective bandwidth**: 읽은 데이터 중 실제로 유효한(쿼리에 필요한) embedding이 차지하는 비율 기반 유효 대역폭.
- **Hypergraph partition / SHP (Social Hash Partitioner)**: 쿼리를 hyperedge로 보고 함께 등장하는 embedding(vertex)을 같은 cluster(SSD page)에 모으는 partition. Bandana가 사용.
- **Co-appearance / hyperedge connectivity λ(j)**: 한 쿼리(hyperedge)가 걸친 cluster 수 ≈ 그 쿼리의 SSD read 연산 수. 낮출수록 좋음.
- **Replication ratio `r` / space amplification**: 복제로 인한 추가 저장 공간 비율.
- **Rep-MBEP**: replication 있는 max-bandwidth embedding placement 문제. NP-hard.
- **Forward Index / Invert Index**: embedding→page / page→embedding 매핑. 온라인 page selection에 사용.
- **Set cover**: replica가 여러 page에 있을 때 한 쿼리를 덮는 최소 page 집합 선택 문제. NP-hard, greedy 근사 사용.
- **Index shrinking / index limit `k`**: 고복제 embedding의 index를 앞 `k`개 cluster로 제한해 selection 오버헤드 축소.

## 강점 · 한계 · 열린 질문
**강점**
- replication이라는 단순하지만 강력한 축으로 hypergraph partition의 single-placement 한계를 정면 돌파. SSD의 남는 용량을 활용한 합리적 trade-off.
- connectivity-priority score로 "무엇을·어디에 복제할지"를 원리적으로 정의(hotness + connectivity 동시 고려).
- 온라인 NP-hard(set cover)를 pipeline + index limit로 실용화. SSD 내부 변경 불요 → general off-the-shelf SSD에 적용 가능.
- artifact 공개·재현 배지, 5개 실데이터셋·여러 SSD type 검증.

**한계**
- 이득이 데이터셋 co-appearance 특성에 강하게 의존(advertising 데이터셋에서 개선 작음). 큰 이득(18.7%)은 r=80%라는 큰 추가 공간에서만.
- DRAM cache가 있을 땐 cache가 hot을 흡수해 개선폭이 줄어듦(최대 1.2×). 가장 큰 이득은 cacheless 시나리오.
- offline partition이 비쌈(CriteoTB 2.7~3 hour). 워크로드 drift 시 재partition/재replication 비용·주기 불명확.
- TCO 개선이 작음(Perf/Cost 1.04~1.12×) — 추가 SSD 공간 비용이 성능 이득을 상쇄.

**열린 질문**
- workload가 시간에 따라 변할 때 online 재복제/증분 업데이트는? log 기반 offline 재계산만 제시.
- write/update path(embedding 학습 갱신)에서 복제 일관성 유지 비용은?
- index limit `k`와 ratio `r`의 자동 튜닝(데이터셋별 최적값)은 수동 sweep 의존.

## ❓ Q&A (자가 점검)

> [!question]- Q1. SSD에 embedding을 저장할 때 핵심 문제는?
> 답: SSD는 page granularity(보통 4KB)로 읽는데 embedding은 64~512B로 작아, 하나를 위해 page 전체를 읽는 **read amplification**이 발생. effective bandwidth가 크게 떨어진다(SHP로도 Criteo 8.58%).

> [!question]- Q2. 기존 SOTA(Bandana, SHP hypergraph partition)의 한계는?
> 답: partition은 각 embedding(vertex)을 **단 하나의 cluster(page)에만** 배치한다. 실제로는 한 embedding이 page 용량(8~32)을 초과하는 많은 embedding과 co-appear하므로, 함께 쿼리되지만 같은 page에 없는 경우 여러 page read가 필요 → 조합 가능성이 구조적으로 제한된다.

> [!question]- Q3. MaxEmbed의 핵심 아이디어와 그것이 가능한 이유는?
> 답: hotspot embedding을 **선택적으로 복제(replication)**해 더 많은 co-appear 조합을 한 page에 담는다. SSD는 용량이 수 TB로 매우 크므로 정해진 ratio `r` 하에서 복제할 여유 공간이 충분하다.

> [!question]- Q4. 무엇을·어디에 복제할지 어떻게 정하나(connectivity-priority)?
> 답: ① vanilla SHP로 먼저 partition해 원래 조합 보존 → ② `score(vᵢ)=Σ(λ(j)−1)`로 connectivity+hotness 점수 계산 → ③ 상위 `rN/d` vertex 선택 → ④ 각 vertex의 hyperedge에서 가장 빈번한 `d−1` 이웃과 함께 replica cluster 생성. partition 후 복제라 기존 조합을 파괴하지 않는다.

> [!question]- Q5. RPP·FPR 같은 strawman은 왜 부족한가?
> 답: RPP(복제 후 partition)는 hot 기준 복제 replica가 인접 관계 없을 수 있고 중복 조합으로 공간 낭비 → 개선 미미. FPR(finer-partition 후 채우기)은 더 잘게 쪼개며 원래 조합을 파괴 → 데이터셋별 불안정(때로 복제 없는 것보다 나쁨).

> [!question]- Q6. 온라인 서빙의 page selection 문제는 무엇이고 어떻게 푸나?
> 답: 복제로 한 embedding이 여러 page에 존재 → 쿼리를 덮는 최소 page 집합 = **set cover(NP-hard)**, greedy가 e2e latency의 56%+ 차지. Forward/Invert Index 기반 one-pass selection(O(|S|+|Q|)), **index shrinking**(앞 k개 cluster로 제한), **pipeline**(async SSD read와 selection 중첩)으로 오버헤드를 숨긴다.

> [!question]- Q7. 정량적 성능 개선은?
> 답: r=10% 추가 공간으로 effective bandwidth +2~10%, e2e +1.7~8.8%. r=80%에서 effective bandwidth +7~19%, e2e throughput **+8.9~18.7%**, latency −10~14.8%. read당 valid embedding 3.59→4.79(Criteo). pipeline이 오버헤드 10.23%, index limit k=5가 추가 34.4% 감소.

> [!question]- Q8. DRAM cache 유무에 따라 효과는 어떻게 달라지나?
> 답: cache가 있으면 hot embedding을 cache가 흡수해 개선폭이 최대 1.2×로 제한된다. cacheless 시나리오에서는 cold embedding의 조합까지 살려 r=0.2로도 1.08~1.31×, pure DRAM 대비 9~26× throughput.

## 🔗 Connections
[[LLM Systems]] · [[ASPLOS]] · [[2025]]

## References worth following
- **Bandana** (Eisenman et al., MLSys 2019) [11] — SHP hypergraph partition으로 NVM에 DLRM embedding 저장. MaxEmbed의 직접 baseline.
- **SHP: Social Hash Partitioner** (Kabiljo et al., VLDB 2017) [20] — MaxEmbed가 사용하는 확장 가능 hypergraph partition 알고리즘.
- **Merci** (Yejin Lee et al., ASPLOS 2021) [23] — sub-query memoization으로 embedding reduction 오버헤드 완화. 메모리 기반 대안.
- **GRACE** (Haojie Ye et al., ASPLOS 2023) [43] — graph 기반 embedding item co-occurrence clustering으로 reduction 가속.
- **RecSSD** (Wilkening et al., ASPLOS 2021) [41] / **FlashEmbedding** (Wan et al., APSys 2021) [38] — SSD 내부 변경(near-data processing) 접근. MaxEmbed는 general SSD 지향이라 대비점.
- **Greedy Set Cover 근사** (Slavik, J. Algorithms 1997) [34] — 온라인 page selection의 이론적 근거.

## Personal annotations
<본인 메모 영역>
