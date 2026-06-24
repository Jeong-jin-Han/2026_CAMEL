---
title: "ELECT: Enabling Erasure Coding Tiering for LSM-tree-based Storage"
aliases: [ELECT]
description: "Replication↔Erasure Coding 하이브리드 이중화로 LSM-tree 기반 분산 KV 스토어의 hot tier 저장 비용을 56.1% 절감하면서 성능·가용성을 유지하는 ELECT 제안"
venue: FAST
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/kvlsm
  - topic/erasure-coding
  - topic/lsm
  - topic/tiering
  - venue/fast
  - year/2024
---

# ELECT: Enabling Erasure Coding Tiering for LSM-tree-based Storage

> **FAST 2024** · `cluster/kvlsm` · Source: [ELECT - Enabling Erasure Coding Tiering for LSM-tree-based Storage.pdf](<ELECT - Enabling Erasure Coding Tiering for LSM-tree-based Storage.pdf>)

**저자:** Yanjing Ren, Yuanming Ren (The Chinese University of Hong Kong); Xiaolu Li, Yuchong Hu (Huazhong University of Science and Technology); Jingwei Li (University of Electronic Science and Technology of China, 교신저자); Patrick P. C. Lee (The Chinese University of Hong Kong)

## TL;DR
분산 KV 스토어(Cassandra)에서 hot tier는 빠른 접근을 위해 비싼 replication을 쓰지만 저장 오버헤드가 크다. ELECT는 LSM-tree 레이아웃에 맞춰 replication과 erasure coding(EC)을 결합한 **hybrid redundancy** 기법으로, hot SSTable은 replication으로 두고 cold SSTable은 cross-SSTable EC로 변환(redundancy transitioning)하며, 사용자가 지정한 단일 파라미터 α(storage saving target)로 저장 절감과 접근 성능의 trade-off를 조절한다. Alibaba Cloud 실험에서 edge(hot tier) 저장 56.1% 절감, 전체 39.1% 절감을 달성하면서 정상 모드 read/write 성능은 Cassandra와 유사하게 유지한다.

## 문제 & 동기
실용 KV 워크로드는 skew가 심해 소수의 hot KV만 자주 접근되고 대부분은 cold하다. 따라서 hot tier(빠른 접근)와 cold tier(저비용 영속)로 나누는 storage tiering이 자연스럽다. 그런데 노드 장애가 흔한 분산 hot tier에서는 가용성 보장이 필요하고, 기존 분산 KV 스토어는 정확한 복사본을 여러 노드에 두는 **replication**(예: 3-way → 3× 저장)을 쓴다. 이는 자원이 제한된 edge node 같은 hot tier에 과도한 저장 비용을 유발한다. Erasure coding은 저비용 이중화 대안이지만(예: (14,10) RS는 1.4×로 4-노드 장애 허용 vs. 3-way replication은 3×로 2-노드 장애), reconstruction 시 더 많은 데이터를 읽어야 하는 접근 성능 패널티가 근본적으로 존재한다. ELECT는 이 trade-off를 LSM-tree 구조와 결합해 풀고자 한다.

> [!quote]- 📄 원문 표현 (paper)
> "Given the skewed nature of practical key-value (KV) storage workloads, distributed KV stores can adopt a tiered approach to support fast data access in a hot tier and persistent storage in a cold tier. ... existing distributed KV stores often rely on replication and incur prohibitively high redundancy overhead. Erasure coding provides a low-cost redundancy alternative, but incurs high access performance overhead. We present ELECT ... by adopting a hybrid redundancy approach that carefully combines replication and erasure coding with respect to the LSM-tree layout." (p.1, Abstract)
>
> "in contrast, erasure coding significantly reduces storage overhead, but it does not keep redundant copies for load balancing and is known to incur higher bandwidth and I/Os in reconstructing lost data when failures happen [19,26]." (p.2)

## 핵심 통찰

> [!note]- 통찰 1: LSM-tree의 마지막 레벨에만 EC를 적용하면 저장 절감은 크고 degraded read 오버헤드는 작다
> Cassandra 측정(p.4, Figure 2)에서 최하위(가장 큰) 레벨 L4는 전체 SSTable의 56.2%를 차지하지만 접근의 10.2%만 받는다. 또한 L4 내부에서도 18.2%의 SSTable만 접근된다. 즉 마지막 레벨의 SSTable은 "대량이지만 거의 안 읽히는" cold 데이터라서, 여기에만 EC를 적용하면 저장 절감 효과는 극대화하면서 degraded read penalty는 거의 무시할 수 있다.

> [!note]- 통찰 2: replication↔EC 변환을 LSM-tree 관리와 분리(decoupled replication management)해야 cross-node 인코딩이 가능하다
> 원래 Cassandra는 한 노드의 모든 replica(primary + secondary)를 단일 LSM-tree로 관리한다. ELECT는 이를 primary LSM-tree 1개 + secondary LSM-tree (R-1)개로 분리한다. 이 분리는 (i) cross-node로 primary replica들만 모아 cross-SSTable 인코딩하고, (ii) 인코딩 후 secondary replica를 안전히 제거(secondary replica removal)하게 해주며, 부수적으로 개별 LSM-tree 크기를 줄여 compaction의 I/O amplification까지 감소시킨다(DEPART/Tebis 아이디어 차용, p.6).

> [!note]- 통찰 3: 단일 파라미터 α로 redundancy transitioning과 data offloading을 단계적으로 조절
> 저장 절감 목표 α(0~1)를 사용자가 정하면, 각 노드는 중앙 조정 없이 독립적으로 (i) EC로 변환할 데이터 SSTable 수 → (ii) cold tier로 offload할 parity SSTable 수 → (iii) cold tier로 offload할 data SSTable 수를 Case 1/2/3 순서로 결정한다(p.9, Eq. 4-6). α가 클수록 더 많이 EC하고 더 많이 cold tier로 내려보낸다.

## 설계 / 메커니즘

> [!abstract]- 설계 전체 구조 (펼치기)
> ELECT는 Cassandra v4.1.0 위에 약 27K LOC로 구현(전체 1.25M LOC)되며, EC 연산은 Intel ISA-L을 JNI로 연동한다(p.9). 핵심은 세 가지 — (A) LSM-tree-based redundancy transitioning, (B) hotness awareness, (C) storage-performance trade-off 조절. RS code (n,k)=(6,4) 기본, storage saving target α=0.6 기본.
>
> **(A) Redundancy Transitioning (§4.1)** — 4단계로 분해:
> 1. **LSM-tree management (§4.1.1):** decoupled replication management. R-factor에 대해 primary LSM-tree 1개 + secondary LSM-tree (R-1)개로 분리. 또한 마지막 레벨 L_ℓ가 너무 작아지지 않도록 LSM-tree level generation을 수정 — L_{ℓ-1} 크기가 다음 레벨 한계의 10×(T)를 넘어도 SSTable을 계속 추가하다 10T 초과 시에만 새 L_ℓ 생성하고 최소 9T 분량만 L_ℓ로 이동 → 마지막 레벨이 항상 충분히 큰 SSTable 수(논문 사례 전체의 약 90%)를 갖게 함.
> 2. **Parity node selection (§4.1.2):** hash ring 상 연속한 n개 노드로 coding group 구성(앞 k개 data node, 뒤 n-k개 parity node). 각 노드는 단조 증가 시퀀스 Q를 유지하고, primary LSM-tree의 SSTable마다 leader parity node N_p를 p=(i+(Q mod k)+1) mod M (Eq. 1)로 결정론적·분산적으로 선택(중앙 조정 불필요, load balancing).
> 3. **Cross-SSTable encoding (§4.1.3):** k개 data node가 각자 SSTable 1개를 leader parity node로 전송 → leader가 n-k개 parity SSTable로 인코딩, 하나는 로컬 저장, 나머지는 다른 parity node로 전송. 재구성용 메타데이터 **ECMeta**(SSTable당 SHA-256 해시 32B, 크기 4B, 노드 식별자 4B, coding group 내 위치 4B의 4-tuple ×n)를 생성해 replication group의 R개 노드에 분배. SSTable 해시를 deduplication처럼 고유 식별자로 사용. **Compaction-triggered parity updates:** primary LSM-tree compaction 시 RAID식 delta-based parity update로 parity SSTable 갱신.
> 4. **Secondary replica removal (§4.1.4):** 인코딩 후 secondary LSM-tree의 중복 replica 제거로 공간 회수. 노드별 compaction이 비동기라 버전이 다를 수 있어, primary가 key list(키 + 타임스탬프)를 만들어 secondary들에 보내 안전하게 제거(현재/구버전만 제거, 신버전은 보존).
>
> **(B) Hotness Awareness (§4.2):** SSTable의 access frequency와 lifetime 두 지표로 hotness 추적. 마지막 레벨에서 frequency가 낮고(동률이면 lifetime이 긴) SSTable을 우선 인코딩. **Cold-data offloading:** hot tier 오버헤드를 더 줄이려 cold SSTable을 cold tier로 동적 offload — 먼저 lifetime 긴 parity SSTable을, 그 다음 우선순위 높은 data SSTable을 offload. data SSTable offload 시 data component만 내려보내고 metadata component는 hot tier에 남겨 read/compaction 지원.
>
> **(C) Storage-Performance Trade-off (§4.3):** 저장 오버헤드를 primary LSM-tree의 SSTable 수로 근사(p.9, Eq. 2). 사용자 지정 α에 대해 Eq. 3을 만족하도록 C_rt(EC 변환 수), C_pm(offload parity 수), C_dm(offload data 수)를 각 노드가 독립 계산. Case 1(EC만, Eq. 4) → Case 2(parity offload, Eq. 5) → Case 3(data offload, Eq. 6) 순으로 진행. EC된 SSTable만 cold tier로 offload하므로 α가 너무 크면 달성 불가할 수 있음.
>
> **장애 처리(§5):** Full-node recovery는 LSM-tree 단위 — secondary LSM-tree는 다른 노드에서 replica 직접 복사, 마지막 레벨 EC된 SSTable은 같은 coding group의 k개 data/parity SSTable + ECMeta로 디코딩. Degraded read는 살아있는 노드로 요청 relay 후 디코딩. Degraded write는 Cassandra hinted handoff 사용. (한계: 개별 SSTable 단위 incremental recovery 미지원, replication+EC를 함께 다루는 Merkle tree 필요.)

## 평가

> [!example]- 평가 요약 (펼치기, 수치 + p.X)
> **환경(§6.1, p.10):** edge-cloud 셋업. edge(hot tier) M=10 노드(ecs.i3g.2xlarge, 8 vCPU/32GiB/447GiB SSD), Alibaba Object Storage가 cloud(cold tier), 두 region 간 latency ≥45ms, edge 내부 ≤1ms. 기본 (n,k)=(6,4), α=0.6, SSTable 4MiB, R=3, compression 비활성(정확한 저장 측정). YCSB 벤치마크, Zipf 0.99.
>
> **Exp#1 YCSB core workloads (Figure 5, p.11):** ELECT는 edge 저장 **56.1% 절감**, 전체(edge+cloud) **39.1% 절감**. 처리량은 대부분 워크로드에서 Cassandra와 유사(≤3% 차이), 단 scan 위주 Workload E에서는 **2.84× throughput 향상**(decoupled replication management로 LSM-tree 크기·I/O amplification 감소). LSM-tree metadata component가 edge 저장의 6.9% 차지.
>
> **Exp#2 KV operation benchmarking (Figure 6, p.11):** 정상 모드 — read/write/update에서 최대 2.7% 높은 latency, scan latency는 21.5% 감소. Degraded 모드 — write/update 최대 3.3% 높음, scan 21.1% 감소. 단 degraded read는 cloud에서 복구하느라 **5.32× latency 증가**(마지막 레벨 KV의 degraded read가 cloud 검색 유발). 단 99%ile read는 양쪽 모두 약 1.7ms로 유사.
>
> **Exp#3 Performance breakdown (Table 1, p.12):** compaction latency가 Cassandra 대비 **17.9% 감소**(205.87→169.03 ms/MiB). redundancy transitioning(239.05 ms)이 가장 큰 단계지만 백그라운드라 영향 제한적. degraded read의 recovery 단계가 병목(1957.64 ms/MiB).
>
> **Exp#4 Full-node recovery (Figure 7, Table 2, p.12):** 복구 시간 데이터 크기에 거의 선형. Cassandra 대비 약 50% 더 느림(다른 노드/cloud에서 data·parity SSTable 검색해 디코딩). 30GiB 복구 분해: copy 13.54s, retrieve 373.98s(93.3% 차지, network-bound), decode 13.34s.
>
> **Exp#5 Resource usage (Figure 8, p.12-13):** 95%ile CPU는 Cassandra 대비 load에서 23.4%, degraded에서 23.3% 적음. memory는 load에서 7.0% 증가하지만 정상/degraded에서 각 4.9%/5.1% 감소. disk I/O는 load에서 23.6% 감소. network는 정상 모드 유사, load/degraded에서 각 22.1%/6.3% 증가.
>
> **Exp#6 key/value size (Figure 9, p.13):** key 8→128B 증가 시 edge 절감 55.5%→56.0%, 전체 34.9%→39.3%. value 32B→8KiB 시 edge 48.2%→58.5%, 전체 33.9%→41.1%(큰 KV일수록 SSTable 메타데이터 비중↓ → 절감↑).
>
> **Exp#7 α 영향 (Figure 10, p.13):** α 0.1→0.9 변화 시 edge 절감 9.2%→86.0%. α≥0.5에서 cloud offload 시작하나 전체 절감은 40.8%로 고정(EC는 마지막 레벨 SSTable에만 적용 가능). 정상 read latency는 α≤0.6까지 안정, 0.6→0.9에서 0.53→1.89ms 증가. degraded read는 α=0.9에서 4.62ms.
>
> **Exp#8 coding parameters (Figure 11, p.14):** k 4→8 변화 시 edge 저장은 ≤1.7% 차이(α가 실제 저장 크기 결정). k 클수록 parity 적게 생성·offload돼 degraded read는 약간 개선.
>
> **Exp#9 read consistency level (Figure 12, p.14):** consistency 1→3 시 read throughput Cassandra 23.2%/ELECT 25.8% 감소, 99%ile은 약 50% 증가 — Cassandra와 일관성 보존.
>
> **Exp#10 number of clients (Figure 13, p.14):** simulated client 256까지 throughput 선형 증가. Cassandra 대비 read 4%, write 5.7% 낮음(redundancy transitioning 오버헤드).

## 섹션 노트
- **§1 Introduction:** edge-cloud storage가 주요 use case. IoT 데이터 79.4ZB(2025 예측). edge(hot tier) replication 비용 문제 제기.
- **§2 Background:** §2.1 분산 KV(consistent hashing, replication group, primary/secondary replica). §2.2 LSM-tree(SSTable, level L_0~L_ℓ, MemTable/WAL/compaction). §2.3 Erasure coding(RS code (n,k), MDS, coding group, reconstruction penalty).
- **§3 Design Considerations:** 5개 질문 — Q1 인코딩 granularity(cross-encoding 선택), Q2 on/off write path(offline 선택), Q3 skewed access(cold SSTable만 EC), Q4 cold tier 접근 오버헤드(거의 안 읽히는 것만 offload), Q5 trade-off(tunable α). Figure 2가 마지막 레벨 EC의 동기를 측정으로 뒷받침.
- **§4 ELECT Design:** redundancy transitioning, hotness awareness, trade-off balancing. (위 설계 토글 참조)
- **§5 Implementation:** Cassandra v4.1.0 + 27K LOC, ISA-L, consistent reads/writes, full-node recovery, degraded read/write, hinted handoff. 한계: incremental recovery 미지원, dynamic topology 미지원.
- **§6 Evaluation:** Exp#1~#10 (위 평가 토글 참조).
- **§7 Related Work:** DEPART/Tebis(LSM-tree management 분리 but EC 없음), EC-Cache/C2DN(caching/CDN tiering but centralized), Convertible codes(transitioning I/O 최소화), CassandrEAS(EC Cassandra but transitioning 없고 small value에서 1.6× 오버헤드).
- **§8 Conclusions:** hybrid redundancy + selective offload + 단일 α로 storage-efficient·high-perf·fault-tolerant KV 달성. 오픈소스: github.com/adslabcuhk/elect.

## 핵심 용어
- **Redundancy transitioning:** replication으로 저장된 데이터를 erasure coding으로 변환하는 과정. ELECT의 핵심 메커니즘으로 4단계(LSM-tree management, parity node selection, cross-SSTable encoding, secondary replica removal)로 구성.
- **Hybrid redundancy:** hot 데이터는 replication, cold 데이터는 erasure coding으로 두어 접근 성능과 저장 효율을 함께 잡는 이중화 전략.
- **Cross-SSTable encoding (cross-encoding):** 여러 노드의 SSTable(각각을 하나의 chunk로 취급)을 묶어 RS 인코딩. self-encoding 대비 메타데이터 오버헤드가 작아 small KV 위주 워크로드에 유리. LSM-tree의 SSTable 단위와 정렬됨.
- **Decoupled replication management:** 한 노드의 primary/secondary replica를 단일 LSM-tree가 아니라 primary LSM-tree 1개 + secondary LSM-tree (R-1)개로 분리 관리. cross-node 인코딩과 I/O amplification 감소를 가능케 함(DEPART/Tebis 차용).
- **Storage saving target (α):** 사용자가 지정하는 0~1 사이 파라미터. 얼마나 많은 SSTable을 EC하고 cold tier로 offload할지 결정하여 저장 절감과 접근 성능 trade-off를 단일 노브로 조절.
- **ECMeta:** 재구성을 위한 메타데이터. coding group 내 각 SSTable의 SHA-256 해시(32B)·크기(4B)·저장 노드 식별자(4B)·coding group 내 위치(4B) 4-tuple ×n개.
- **Leader parity node:** coding group에서 k개 data SSTable을 받아 n-k개 parity SSTable을 인코딩·분배하고 ECMeta를 생성하는 노드. Eq. 1로 결정론적·분산적으로 선택.
- **Data offloading:** hot tier의 cold SSTable(parity 먼저, 다음 data)을 cold tier로 내려보내 hot tier 저장 오버헤드를 추가로 줄이는 것. data SSTable은 data component만 내려보내고 metadata는 hot tier에 잔류.
- **Degraded read:** 일부 chunk가 unavailable한 상황에서 같은 coding group의 k개 살아있는 chunk를 모아 디코딩해 요청 KV를 복원하는 읽기.

## 강점 · 한계 · 열린 질문
**강점**
- LSM-tree 구조(마지막 레벨 = cold 대량 데이터)를 정확히 활용해 저장 절감(edge 56.1%)과 성능 유지를 동시 달성.
- 단일 파라미터 α로 trade-off를 직관적으로 조절 — 운영 단순성.
- decoupled replication management 덕에 compaction I/O amplification까지 감소(scan 2.84× 향상, compaction 17.9% 감소)하는 부수 이득.
- Alibaba Cloud 실제 edge-cloud 환경에서 폭넓은 실험(Exp#1~#10), 오픈소스 + artifact evaluated(Available/Functional/Reproduced).

**한계**
- Degraded read가 마지막 레벨 KV에 걸리면 cloud 복구로 5.32× latency 증가 — cold tier 접근 시 성능 급락.
- Full-node recovery가 Cassandra 대비 약 50% 느림(network-bound, retrieve가 93.3%).
- Incremental(개별 SSTable) recovery 미지원, dynamic topology(노드 join/leave) 미지원.
- α≥0.5에서 전체 저장 절감이 40.8%로 포화 — EC가 마지막 레벨에만 적용되어 하위 레벨 replicated SSTable은 더 못 줄임.
- Hotness를 access pattern 직접 측정이 아니라 frequency/lifetime 근사로 판단 — read/write 분포가 다르면 실제 성능이 크게 달라질 수 있음(§6.5).

**열린 질문**
- skew가 덜한 워크로드에서 마지막 레벨까지 read가 몰리면 degraded 모드 성능 저하 정도는? (§6.5에서 future work로 언급)
- replication+EC를 함께 다루는 Merkle tree 설계로 incremental recovery를 어떻게 효율화할 것인가?
- 99%ile latency에 대한 α sensitivity 분석(논문은 average만, future work).
- compression 활성 시 padding-zero 방식의 실제 저장 절감 효과 정량화.

## ❓ Q&A (자가 점검)

> [!question]- Q1. ELECT는 왜 LSM-tree의 "마지막 레벨"에만 erasure coding을 적용하는가?
> 답: Cassandra 측정(Figure 2)에서 최하위 레벨 L4는 전체 SSTable의 56.2%를 차지하지만 접근의 10.2%만 받고, L4 내부에서도 18.2%만 접근된다. 즉 "대량이지만 거의 안 읽히는" cold 데이터라서 여기에만 EC를 적용하면 저장 절감은 극대화하고 degraded read penalty는 거의 무시할 수 있기 때문이다.

> [!question]- Q2. Decoupled replication management가 왜 필요하고 어떤 부수 효과가 있는가?
> 답: Cassandra는 한 노드의 모든 replica를 단일 LSM-tree로 관리하는데, cross-node로 primary replica들만 모아 인코딩하고 인코딩 후 secondary replica를 안전히 제거하려면 primary LSM-tree 1개 + secondary LSM-tree (R-1)개로 분리해야 한다. 부수 효과로 개별 LSM-tree 크기가 줄어 compaction의 I/O amplification이 감소하고, scan 성능이 최대 2.84× 향상되며 compaction latency가 17.9% 감소한다.

> [!question]- Q3. 단일 파라미터 α는 구체적으로 무엇을 어떤 순서로 조절하는가?
> 답: 저장 절감 목표 α(0~1)에 대해 각 노드가 독립적으로 Eq. 3을 만족하도록 Case 1→2→3 순으로 결정한다. Case 1은 EC로 변환할 data SSTable 수(C_rt)를 최대화(Eq. 4), 그래도 부족하면 Case 2에서 parity SSTable을 cold tier로 offload(C_pm, Eq. 5), 더 부족하면 Case 3에서 data SSTable을 offload(C_dm, Eq. 6). α가 클수록 더 많이 EC하고 더 많이 offload한다.

> [!question]- Q4. cross-SSTable encoding이 self-encoding보다 유리한 이유는?
> 답: self-encoding은 KV pair를 고정 크기 chunk로 나눠 인코딩해 병렬성은 좋지만, 각 KV의 모든 chunk를 인덱싱하는 메타데이터 오버헤드가 커서 small KV 위주 서비스에서 비효율적이다. cross-encoding은 여러 KV를 묶어 인코딩해 메타데이터를 줄이고, LSM-tree의 SSTable 단위(각 SSTable을 하나의 chunk로)와 자연스럽게 정렬된다. cold KV만 인코딩하면 degraded read 오버헤드도 제한된다.

> [!question]- Q5. degraded read의 성능 패널티가 가장 큰 경우와 그 이유는?
> 답: 마지막 LSM-tree 레벨의 EC된 KV에 degraded read가 걸리고 그 data SSTable이 cloud(cold tier)로 offload된 경우다. 이때 cloud에서 k개 chunk를 retrieve해 디코딩해야 하므로 ≥45ms의 edge-cloud latency가 더해져 read latency가 최대 5.32× 증가한다(Exp#2). 단 99%ile은 약 1.7ms로 Cassandra와 유사하며, scan은 일부 unavailable SSTable이 초기 단계에서 복구돼 영향이 완화된다.

> [!question]- Q6. 인코딩 후 secondary replica를 잘못 제거하지 않으려면 어떻게 하는가?
> 답: 노드별 compaction이 비동기라 secondary LSM-tree들이 서로 다른 SSTable 버전을 가질 수 있다. primary LSM-tree가 키와 written timestamp를 담은 key list를 만들어 secondary LSM-tree들에 보내고, 각 secondary는 마지막 레벨에서 그 키들을 커버하는 SSTable 중 현재 또는 구버전 KV만 제거한다(신버전은 보존). 아직 마지막 레벨로 안 내려온 현재 버전은 compaction 시점에 처리한다(Figure 4).

> [!question]- Q7. ELECT의 저장 절감이 α를 아무리 키워도 전체 기준 40.8%에서 포화되는 이유는?
> 답: redundancy transitioning(EC 변환)이 LSM-tree의 마지막 레벨 SSTable에만 적용되기 때문이다. 하위 레벨의 replicated SSTable은 EC로 변환하거나 더 줄일 수 없어, α≥0.5에서 edge→cloud offload로 edge 절감은 86%까지 늘어도 전체(edge+cloud) 저장 절감은 40.8%로 고정된다(Exp#7).

> [!question]- Q8. ELECT는 Cassandra의 일관성(consistency) 보장을 유지하는가?
> 답: 유지한다. write는 항상 replication으로 이뤄져 Cassandra와 동일한 consistent write를 따른다. read는 replicated KV면 동일한 consistent read 경로를, EC된 KV면 primary LSM-tree에서 반환하거나(없으면) degraded read를 발행한다. Exp#9에서 read consistency level 1→3 변화에 따른 throughput/99%ile 변화가 Cassandra와 유사함을 확인했다(단 EC된 KV의 read는 verify 미지원).

## 🔗 Connections
[[KV-LSM]] · [[FAST]] · [[2024]]

## References worth following
- DEPART (Zhou et al., TOS '23) / Tebis [57,68] — LSM-tree replica management 분리로 I/O amplification 감소. ELECT의 decoupled replication management 기반.
- EC-Cache (Rashmi et al., OSDI '16) [49] — 클러스터 캐싱에 online erasure coding 적용한 storage tiering. ELECT와 문제 정의·기법 비교 대상.
- Convertible codes (Maturana & Rashmi, IEEE TIT '22) [39] — transitioning I/O를 최소화하는 코드 변환. redundancy transitioning 심화.
- CassandrEAS (Cadambe et al., NCA '20) [8] — EC를 적용한 Cassandra. transitioning 없고 small value 오버헤드 큼 — ELECT 차별점.
- f4 (Muralidhar et al., OSDI '14) [43] — Facebook warm BLOB store, (14,10) RS 실사용 — production EC 동기.
- PebblesDB (Raju et al., SOSP '17) [48] — fragmented LSM-tree. LSM-tree 레이아웃 이해 배경.

## Personal annotations
<본인 메모 영역>
