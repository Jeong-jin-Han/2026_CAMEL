---
title: "Holistic and Automated Task Scheduling for Distributed LSM-tree-based Storage"
aliases: [Holistic and Automated, HATS]
description: "분산 LSM-tree KV store에서 foreground read와 background compaction을 통합 스케줄링해 latency fluctuation을 완화하는 HATS 프레임워크 (FAST 2026)"
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/kvlsm
  - topic/lsm
  - topic/scheduling
  - topic/distributed
  - venue/fast
  - year/2026
---

# Holistic and Automated Task Scheduling for Distributed LSM-tree-based Storage

> **FAST 2026** · `cluster/kvlsm` · Source: [Holistic and Automated Task Scheduling for Distributed LSM-tree-based Storage.pdf](Holistic%20and%20Automated%20Task%20Scheduling%20for%20Distributed%20LSM-tree-based%20Storage.pdf)

**저자**: Yuanming Ren, Siyuan Sheng, Zhang Cao (The Chinese University of Hong Kong), Yongkun Li (University of Science and Technology of China), Patrick P. C. Lee (The Chinese University of Hong Kong). 교신저자: Patrick P. C. Lee. 코드: https://github.com/adslabcuhk/hats

## TL;DR
분산 LSM-tree 기반 KV store에서 latency fluctuation은 distribution layer의 load imbalance뿐 아니라 storage layer에서 foreground read와 background compaction이 자원을 두고 경쟁하는 tight coupling에서 비롯된다. **HATS**는 read task 분배와 compaction rate를 closed-loop으로 통합 co-scheduling하는 프레임워크로, (i) coarse-grained read task assignment(epoch 단위 글로벌 균형), (ii) fine-grained read task coordination(unified score 기반 instantaneous 재분배), (iii) compaction task scheduling(read load에 비례한 compaction rate)을 수행한다. Cassandra v5.0 위에 구현했고 YCSB read-dominant 워크로드에서 C3/DEPART 대비 P99을 58.6%/59.9% 줄이고 throughput을 2.41x/2.90x 향상시켰다.

## 문제 & 동기
분산 KV store는 client I/O에 low-latency를 보장해야 하지만, latency는 **distribution layer**(client와 store 간 라우팅/큐잉)와 **storage layer**(노드 내 LSM-tree 처리)에 모두 누적된다. 기존 replication 기반 load balancing 연구들은 distribution layer의 foreground task 균형에만 집중하고, storage layer에서 foreground read와 background compaction의 상호작용에는 거의 주목하지 않았다. 논문은 (1) access frequency를 고르게 분산해도 read-latency가 균형되지 않으며(노드 간 최대 4.24x 차이), (2) latency fluctuation이 작은 timescale(1초 window)에서 심하고, (3) compaction이 read와 자원 경쟁을 일으켜 read throughput을 급락시키며(26.3 → 7.3 KOPS), (4) 그럼에도 compaction은 장기 read 성능에 필수임(disable 시 평균 read throughput 29.8 → 40.7 KOPS 증가, 즉 enable 시 SSTable 병합으로 read 성능 개선)을 측정으로 보인다. 따라서 foreground/background task의 신중한 co-scheduling이 latency fluctuation 완화의 핵심이라고 주장한다.

> [!quote]- 📄 원문 표현 (paper)
> "Mitigating latency fluctuations for distributed key-value (KV) stores is critical, yet it is often hindered by the tight coupling of foreground and background tasks related to data distribution and storage management." (Abstract, p.1)
>
> "A study of YouTube's data centers shows that even when CPU loads are perfectly balanced across nodes, query latencies fluctuate over time and latency spikes still occur, as current resource usage poorly implies future request latencies." (§1, p.1)
>
> "Current replication-based load-balancing designs mainly focus on balancing foreground tasks in the distribution layer, but pay limited attention to the interplay between foreground and background tasks in the storage layer." (§1, p.1)

## 핵심 통찰

> [!note]- 통찰 1 — Access frequency 균형 ≠ read-latency 균형 (Observation 1, §3.1)
> Replication으로 access frequency를 고르게 분산(노드 간 최대 차이 18.9%로 감소)해도 read latency는 노드 간 최대 4.24x 차이가 남는다(Figure 2b). 자원 사용량이나 access pattern만으로는 미래 request latency를 예측할 수 없으므로, request-level / 작은 timescale 수준의 균형이 필요하다.

> [!note]- 통찰 2 — Latency fluctuation은 작은 timescale에서 발생 (Observation 2, §3.1)
> 1분 window에서는 latency가 평균의 0.5x~2.0x 안에 안정적이지만(Figure 3a), 1초 window에서는 90.8%의 window가 그 범위를 벗어난다(Figure 3b). 이 fine-grained fluctuation은 foreground I/O와 transient background task(compaction)가 CPU/DRAM/I/O를 두고 경쟁하는 동적 상호작용에서 비롯된다.

> [!note]- 통찰 3 — Compaction은 양날의 검 (Observations 3 & 4, §3.2)
> Compaction이 켜지면 read throughput이 2분 만에 26.3 → 7.3 KOPS로 급락(자원 경쟁). 그러나 compaction이 SSTable을 병합해 탐색할 SSTable 수를 줄이므로, 장기적으로는 enable 시 평균 read throughput이 29.8 → 40.7 KOPS로 개선. 즉 단순 rate-limiting이나 deferring이 아니라 **read load에 맞춘 co-scheduling**이 정답이다.

> [!note]- 통찰 4 — Coarse + fine-grained 2단계가 모두 필요 (§6.2)
> Coarse-grained read assignment만으로는 throughput은 오르지만(최대 10.9%) P99은 거의 개선 안 됨(4.1%). Fine-grained coordination을 더하면 P99이 추가로 8.3%~40.4% 감소. 두 granularity의 holistic한 결합이 일관된 low latency의 열쇠다.

## 설계 / 메커니즘

> [!abstract]- HATS 전체 구조 — closed-loop 3단계 (§4.1, Figure 5)
> M개 노드가 hash ring에 배치, replication factor R(기본 3). HATS는 closed-loop으로 매 epoch(길이 L, 기본 60초)마다 다음 3가지를 반복한다.
> 1. **Coarse-grained read task assignment** (§4.2): scheduler node가 글로벌 current state를 모아 balanced expected state를 계산해 epoch 단위로 모든 노드의 read 분배 균형.
> 2. **Fine-grained read task coordination** (§4.3): coordinator가 instantaneous load를 보고 작은 timescale에서 read를 fastest가 아닌 적절한 replica로 재라우팅.
> 3. **Compaction task scheduling** (§4.4): read load가 높은 key range의 compaction을 우선시하되 allowed compaction rate로 상한.
>
> HATS는 write를 **절대 무시하지 않으며**(read latency가 compaction 등 write에 영향받으므로) Cassandra의 정합성/장애복구 메커니즘을 그대로 유지한다.

> [!abstract]- Coarse-grained read task assignment (§4.2)
> - **Extended Gossip protocol** (§4.2.1): 각 노드가 R개 key range의 평균 read latency와 request 수를 측정해 version number와 함께 Gossip으로 비동기 공유. scheduler node가 current state(M×R matrix C)를 구성.
> - **Raft 합의** (§4.2.1): scheduler node를 seed node 중 Raft leader로 선출해 single-point failure/bottleneck 방지. Cassandra 코어 정합성은 건드리지 않음.
> - **Adjustment (Algorithm 1, §4.2.2)**: scheduler가 노드를 high-load(\(\sum C_{i-j,j} > L/T_i\))/low-load로 분류하고, greedy하게 high-load → low-load로 read request를 \(\delta = \min(|\Delta_{i+h}|, |\Delta_{i+\ell}|, E_{i,h})\)만큼 이동해 expected state E를 만든다. 시간복잡도 O(MR²/4)로 epoch당 1회라 무시 가능.
> - **Read task assignment (§4.2.3)**: client는 key range의 replica \(N_{i+j}\)를 확률 \(E_{i,j}/\sum E_{i,j}\)로 coordinator로 선택.
> - **네트워크 overhead**: Gossip 메시지에 \((8+4R) \times M\)(monitored) + \((8+4MR)\)(expected) 바이트 추가. R=3, M=100일 때 15.2% overhead.

> [!abstract]- Fine-grained read task coordination — unified score (§4.3)
> - Fastest replica만 고르면 load oscillation과 tail latency 증가 → **load-aware replica selection** 채택.
> - Coordinator가 j번째 replica의 instantaneous read latency \(t_{i,j}\)(네트워크+storage 처리시간)를 EWMA(weight 0.5)로 추적.
> - 각 replica에 **unified score** \(\frac{L}{t_{i,j}} - Q_{i+j}\) 부여: \(L/t_{i,j}\)는 epoch 동안 처리 가능한 instantaneous request 수, \(Q_{i+j}\)는 expected state 기준 처리 예정 수. score가 높을수록 여유 용량 큼 → 가장 높은 score replica로 재라우팅.
> - vanilla Cassandra의 dynamic snitching과 유사한 정보를 쓰므로 overhead 미미. 큰 timescale에선 expected state로 수렴, 작은 timescale에선 잘 compaction된(낮은 storage latency) replica가 선택되도록 §4.4와 정렬.

> [!abstract]- Compaction task scheduling (§4.4)
> - 노드별 **allowed compaction rate** 상한(노드당 64 MiB/s, Cassandra 기본 compaction_throughput) 설정. 단, 두 번째로 낮은 level부터 위쪽 compaction에만 적용; lowest-level compaction은 새로 flush된 SSTable의 read amplification 완화에 필수이므로 unbounded.
> - **Compaction rate provisioning**: read load 비율 \(E_{i,j}/Q_i\)에 비례해 compaction rate 배분 → read load 높은 key range를 우선 compaction.
> - **Replica decoupling (DEPART [55] 차용)**: 노드 \(N_i\)가 자신 origin replica \(K_{i,0}\)와 R-1개의 \(K_{i-j,0}\)를 **별도 LSM-tree**로 분리해, 개별 replica의 compaction rate-limiting을 가능케 함. 구현은 약 150 LOC, write time overhead 0.4%.
> - **Starvation avoidance**: write-heavy + 낮은 read load key range가 compaction starvation 겪지 않도록 하한 \(\frac{compaction\_throughput}{R}\) 보장. 초과 시 per-key-range compaction, 미만 시 Cassandra 기본 FCFS로 복귀.

> [!abstract]- 구현 (§5)
> Cassandra v5.0 + java driver v3.0.0 기반, 6K LOC 수정(전체 1.3M LOC). state monitoring/sharing, current state adjustment, score 계산, compaction rate provisioning, client-side read assignment를 구현. Raft은 production-grade 라이브러리(SOFAJRaft [48]) 통합. epoch length L=60초로 Cassandra 기본 compaction 주기(60초)에 정렬.

## 평가

> [!success]- 평가 요약 (수치 + p.X)
> **Testbed (§6.1, p.9)**: 22머신(20 server + 2 client), 10Gbps, Ubuntu 22.04. 기본 10노드 homogeneous(i5-3570, 16GiB, 128GiB SATA SSD). R=3, read consistency=1. YCSB + Facebook production trace.
>
> **Exp#1 microbenchmark (각 기법 기여, §6.2, p.10)**: mLSM 대비 CoarseSchedule throughput +3.3%/+67.2%/+10.9% (A/B/C)이나 P99은 4.1%만 감소. FineSchedule 추가 시 P99 -8.3%/-24.4%/-40.4%. HATS는 FineSchedule 대비 throughput +32.4%/+5.7%(A/B), P99 -49.0%/-9.8%(A/B).
>
> **Exp#2 YCSB macrobenchmark (§6.3, p.10–11)**: HATS throughput gain 최대 1.53x/2.47x/2.67x/2.90x/2.04x (A/B/C/D/F). P50/P99/P999 latency를 최대 53.6%/62.2%/88.7% 감소(B/C/D/F). Workload E(scan)는 mLSM/DEPART와 유사(DEPART보다 5.4%/6.0% 낮음).
>
> **Exp#3 Facebook production (§6.3, p.11)**: HATS throughput 48.8 KOPS, mLSM/C3/DEPART(17.1/20.2/21.5) 대비 2.85x/2.42x/2.27x. 평균 latency Get -68.0%/Put -40.7%/Seek -41.9%, P99 Get 최대 -83.2%.
>
> **Exp#4 load balance (CoV, §6.4, Table 1, p.12)**: HATS가 read latency CoV 최저(A/B/C에서 0.12/0.12/0.11), mLSM 대비 최대 -29.4%/-52.0%/-72.5%.
>
> **Exp#5 highest-latency node 분포 (Figure 9, p.12)**: HATS가 모든 워크로드에서 read latency tail이 가장 짧음.
>
> **Exp#6 performance breakdown (Table 2, p.12)**: write path(Workload A)에서 WAL/compaction step을 최대 -61.9%/-81.8%. read path(Workload C)에서 replica selection latency를 mLSM/C3/DEPART 대비 -66.5%/-93.5%/-80.7%. HATS는 remote replica로 0.04% request만 redirect(mLSM/C3/DEPART는 6.2%/84.9%/19.3%).
>
> **Exp#7 resource usage (Figure 10, p.13)**: HATS가 CPU/disk I/O/network I/O 최저(최대 -47.5%/-81.7%/-64.6%), memory는 유사.
>
> **Exp#8 scalability (20노드 heterogeneous, Figure 11, p.13)**: HATS throughput gain 최대 2.08x/1.87x/2.11x (A/B/C), P99 최대 -64.3%/-48.3%(A/B).
>
> **Exp#9–12 sensitivity (p.13–14)**: read consistency 1~3에서 DEPART 대비 throughput +42.9%/+29.3%/+24.5%; value size 512~2048B에서 +20.5~50.6%; key distribution(uniform/0.9/0.99)에서 +59.7%/+51.2%/+42.9%; client 100/150/200에서 +42.9%/+48.2%/+36.1%. epoch length L=60s에서 최대 throughput.

## 섹션 노트
- **§1 Introduction**: distribution + storage 2-layer latency 모델. 기존 replication LB는 storage layer interplay 미고려. HATS 3-step closed-loop 개요. 핵심 수치(P99 -58.6%/-59.9%, throughput 2.41x/2.90x vs C3/DEPART).
- **§2 Background**: Cassandra 구조 — consistent hashing, hash ring, replication group, coordinator + dynamic snitching, Gossip membership. Storage layer는 LSM-tree(MemTable→WAL→SSTable flush→leveled compaction).
- **§3 Motivation**: 4개 observation을 측정으로 정립(latency fluctuation 출처, read/compaction coupling).
- **§4 HATS Design**: closed-loop 3-step 상세. Applicability — replica decoupling + replica selection 두 기존 기술에 의존. B+-tree 기반 store에도 원리 적용 가능.
- **§5 Implementation**: Cassandra v5.0, 6K LOC.
- **§6 Evaluation**: Exp#1~12, microbench/macrobench/system analysis/sensitivity/discussion.
- **§7 Related Work**: local LSM-tree 최적화, replication 기반 LB(Kinesis, C3, Rein, SPORE, Slicer, Prequal), replication overhead 감소(Hailstorm, Nova-LSM, Tebis, RubbleDB), replica 관리(DEPART, ELECT).
- **§8 Conclusion** + Appendix A(artifact, v1.0 tag, ansible/YCSB).

## 핵심 용어
- **HATS**: Holistic and Automated Task Scheduling. read와 compaction task를 통합 co-scheduling하는 프레임워크.
- **Distribution layer / Storage layer**: 각각 client↔store 라우팅·큐잉 지연 / 노드 내 LSM-tree 처리 지연을 담당하는 두 계층.
- **Coarse-grained read task assignment**: epoch 단위로 글로벌 current state를 모아 balanced expected state를 계산·전파하는 큰 timescale 균형.
- **Fine-grained read task coordination**: coordinator가 instantaneous load 기반 unified score로 read를 작은 timescale에서 재라우팅.
- **Unified score**: \(\frac{L}{t_{i,j}} - Q_{i+j}\). replica가 처리할 수 있는 추가 read request 수를 나타내며 높을수록 여유.
- **Current state / Expected state**: 각각 현재 측정된 read 분포(matrix C) / 균형을 위해 목표로 하는 read 분포(matrix E).
- **Epoch (length L)**: coarse-grained 균형을 수행하는 주기. 기본 60초로 Cassandra compaction 주기에 정렬.
- **Allowed compaction rate**: 노드별 최대 compaction 처리량 상한(기본 64 MiB/s), lowest-level compaction은 제외(unbounded).
- **Replica decoupling**: R개 replica를 노드 내 별도 LSM-tree로 분리(DEPART 차용)해 개별 compaction rate-limiting을 가능케 하는 기술.
- **Load-aware replica selection**: fastest가 아니라 load를 고려해 replica를 고름. load oscillation과 tail latency 방지.
- **Dynamic snitching**: Cassandra가 replica의 최근 latency를 모니터링해 read 라우팅에 쓰는 기존 모듈. HATS의 fine-grained coordination이 이를 확장.
- **mLSM / C3 / DEPART**: 비교 baseline. multi-LSM(replica decoupling만) / adaptive replica selection + client-side rate control / replica decoupling + two-layer log.

## 강점 · 한계 · 열린 질문
**강점**
- Distribution layer와 storage layer를 동시에 고려하는 최초의 holistic 접근. coarse + fine 2단계 + compaction scheduling이 서로 정렬되어 일관된 low latency 달성.
- 측정 기반 motivation(4 observation)이 탄탄하고, write를 무시하지 않아 compaction이 read latency에 미치는 실제 효과를 반영.
- 광범위한 평가(12 실험, synthetic + production trace, scalability, sensitivity)와 artifact 공개(reproduced 배지). resource(CPU/I/O/network)까지 최저.

**한계**
- Cassandra/replica decoupling/replica selection에 의존 — 이 두 기술을 지원하지 않는 시스템엔 직접 적용 어려움(논문도 "research prototype" 한계 명시).
- Scan-heavy(Workload E)에서는 이득 거의 없고 DEPART보다 약간 낮음. write-heavy에서 P50 latency가 mLSM보다 높을 수 있음(scheduling overhead).
- Gossip overhead가 M, R에 따라 증가(M=100, R=3에서 15.2%) — 더 큰 클러스터에서의 scalability는 미검증(최대 20노드).
- Scheduler node(Raft leader)에 글로벌 상태 집중 — 매우 큰 클러스터에서 병목 가능성.

**열린 질문**
- B+-tree 등 non-LSM storage에 실제 적용 시 compaction 대신 index maintenance를 어떻게 모델링할까?
- Heterogeneous/multi-tenant 환경에서 epoch length L의 자동 적응이 가능한가(현재 60초 고정)?
- Disaggregated storage(NVMe-oF, RDMA 기반 RubbleDB/Tebis)와 결합 시 co-scheduling 이점이 유지되는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. HATS가 해결하려는 핵심 문제는 무엇인가?
> > 답: 분산 LSM-tree KV store에서 foreground read와 background compaction이 storage layer 자원을 두고 경쟁(tight coupling)하면서 발생하는 latency fluctuation. 기존 replication LB는 distribution layer의 foreground 균형만 다뤄 이 interplay를 놓쳤다.

> [!question]- Q2. 왜 access frequency를 고르게 분산하는 것만으로 부족한가?
> > 답: Observation 1에서 access frequency 차이를 18.9%로 줄여도 노드 간 read latency가 최대 4.24x 차이 남(Figure 2b). 자원 사용량/access pattern이 미래 request latency를 잘 예측하지 못하기 때문(YouTube 데이터센터 연구 인용).

> [!question]- Q3. Compaction을 그냥 끄거나 무한정 미루면 안 되는 이유는?
> > 답: compaction은 SSTable을 병합해 read 시 탐색할 SSTable 수를 줄인다. 끄면 단기 read throughput은 오르지만(7.3→) 장기적으로 read 성능이 나빠짐 — disable 대비 enable 시 평균 read throughput 29.8→40.7 KOPS. 그래서 deferring이 아니라 read load에 맞춘 co-scheduling이 필요.

> [!question]- Q4. Coarse-grained와 fine-grained는 각각 무엇을 담당하나?
> > 답: coarse-grained는 epoch(60초) 단위로 글로벌 current→expected state를 계산해 큰 timescale 균형(워크로드 skew, straggler 대응). fine-grained는 coordinator가 instantaneous load와 unified score로 작은 timescale에서 read를 재라우팅(순간 spike 대응). Exp#1에서 coarse만으로는 P99이 거의 안 줄고 fine 추가 시 크게 감소.

> [!question]- Q5. Unified score는 어떻게 정의되며 무엇을 의미하나?
> > 답: \(\frac{L}{t_{i,j}} - Q_{i+j}\). \(L/t_{i,j}\)는 instantaneous latency 기준 epoch당 처리 가능한 request 수, \(Q_{i+j}\)는 expected state 기준 처리 예정 수. 둘의 차이가 추가로 처리 가능한 여유 용량 → 가장 높은 replica로 read를 보낸다.

> [!question]- Q6. Replica decoupling이 compaction scheduling에 왜 필요한가?
> > 답: Cassandra 기본은 한 노드의 모든 key range를 단일 LSM-tree에서 관리해 replica별 compaction rate를 따로 제어할 수 없다. R개 replica를 별도 LSM-tree로 분리(DEPART 차용)하면 read load 높은 replica의 compaction을 우선/제어할 수 있다(구현 150 LOC, write overhead 0.4%).

> [!question]- Q7. HATS는 baseline 대비 얼마나 좋아졌나?
> > 답: YCSB read-dominant에서 C3/DEPART 대비 P99 -58.6%/-59.9%, throughput 2.41x/2.90x. Facebook production에서 P99 Get -78.9%/-68.3%, throughput 2.42x/2.27x. macrobench throughput gain 최대 2.90x.

> [!question]- Q8. HATS의 주요 overhead와 한계는?
> > 답: Gossip 메시지에 monitoring/expected state 정보 추가로 M=100·R=3에서 15.2% 네트워크 overhead. Algorithm 1은 O(MR²/4)이나 epoch당 1회라 무시 가능. 한계: Cassandra/replica decoupling 의존, scan-heavy 워크로드 이득 미미, write-heavy에서 P50 약간 상승, 최대 20노드까지만 검증.

## 🔗 Connections
[[KV-LSM]] · [[FAST]] · [[2026]]

## References worth following
- [55] Q. Zhang et al. **DEPART: Replica decoupling for distributed key-value storage.** USENIX FAST 2022. — HATS가 replica decoupling을 차용한 직접 기반.
- [49] L. Suresh et al. **C3: Cutting tail latency in cloud data stores via adaptive replica selection.** USENIX NSDI 2015. — fine-grained replica selection의 대표 비교 대상.
- [52] B. Wydrowski et al. **Prequal: Load is not what you should balance.** USENIX NSDI 2024. — 자원 균형이 latency를 보장하지 않는다는 motivation의 핵심 근거.
- [33] B. Lepers et al. **KVell: the design and implementation of a fast persistent key-value store.** ACM SOSP 2019. — compaction의 자원 경쟁 특성 관련.
- [34] H. Li et al. **RubbleDB: CPU-efficient replication with NVMe-oF.** USENIX ATC 2023. — disaggregated/replica compaction 오프로딩 대안.
- [54] J. Yu et al. **ADOC: Automatically harmonizing dataflow between components in LSM-tree KV stores.** USENIX FAST 2023. — 단일 노드 내 foreground/background 조화의 인접 연구.

## Personal annotations
<본인 메모 영역>
