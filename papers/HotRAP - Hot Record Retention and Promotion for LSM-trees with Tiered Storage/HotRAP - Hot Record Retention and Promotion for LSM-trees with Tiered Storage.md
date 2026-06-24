---
title: "HotRAP: Hot Record Retention and Promotion for LSM-trees with Tiered Storage"
aliases: [HotRAP]
description: "Tiered-storage LSM-tree KV store that tracks hotness on-disk (RALT) and retains/promotes hot records at the record level via compaction and flush."
venue: ATC
year: 2025
arxiv: "2402.02070"
tier: deep
status: done
tags: [paper, cluster/kvlsm, topic/lsm, topic/tiered-storage, topic/hot-data, venue/atc, year/2025]
---

# HotRAP: Hot Record Retention and Promotion for LSM-trees with Tiered Storage

> **ATC 2025** · cluster/kvlsm · Source: [HotRAP - Hot Record Retention and Promotion for LSM-trees with Tiered Storage.pdf](<HotRAP - Hot Record Retention and Promotion for LSM-trees with Tiered Storage.pdf>)

저자: Jiansheng Qiu, Fangzhou Yuan, Mingyu Gao, Huanchen Zhang (Tsinghua University)

## TL;DR

HotRAP은 RocksDB 기반의 tiered-storage key-value store로, **fast disk (FD, 빠른 SSD)** 와 **slow disk (SD, HDD/cloud storage)** 두 계층 위에서 동작한다. 핵심은 hot record를 **record 단위**로 정확히 추적·이동시키는 것이다. 기존 LSM-tree tiered storage는 (1) hotness를 메모리에서 추적해 메모리가 병목이 되고, (2) block/SSTable 단위로만 데이터를 옮겨 granularity가 거칠며, (3) hot record 승격을 compaction에 의존해 지연이 크다는 세 가지 한계가 있었다. HotRAP은 이를 (1) **RALT** (Recent Access Lookup Table, FD에 두는 특수 LSM-tree로 hotness를 추적), (2) record-level **retention/promotion**, (3) compaction을 기다리지 않는 **promotion by flush** 라는 세 경로로 해결한다. YCSB common skew에서 read-only 5.4×, read-write-balanced 3.8×, Twitter production trace에서 1.9×까지 2nd-best 대비 빠르다.

> [!quote]- 📄 원문 표현 (paper)
> - "We present HotRAP, a key-value store based on RocksDB that can timely promote hot records individually from slow to fast storage and keep them in fast storage while they are hot." (p.1)
> - "Our experiments show that HotRAP outperforms state-of-the-art LSM-trees on tiered storage by up to 5.4× compared to the second best under read-only and read-write-balanced YCSB workloads with common access skew patterns, and by up to 1.9× compared to the second best under Twitter production workloads." (p.1)

## 문제 & 동기

LSM-tree의 multi-level 구조는 tiered storage에 자연스럽게 맞는다: 상위 레벨(최근 insert/update된 record)은 빠른 저장소에, 하위 레벨(대부분의 record)은 느리지만 싼 저장소에 둔다. 하지만 자주 **읽히는** record와 자주 **쓰이는** record가 항상 겹치지는 않는다. "read-hot" set과 "write-hot" set이 다를 수 있어, 다수의 read-hot record가 느린 저장소에 가라앉아 read 성능을 해친다.

기존 해결책의 세 가지 한계:
- **Limitation 1 (메모리 병목)**: hotness를 메모리에서 추적하면 hot record가 메모리 용량보다 클 수 있다. Twitter trace 기준 1TB local SSD를 hot record 저장에 쓰면 hot key 추적에만 최소 166.7GB 메모리가 필요해 64GB 인스턴스 메모리를 초과한다.
- **Limitation 2 (거친 granularity)**: Mutant, LogStore, MirrorKV 등은 block/SSTable 단위로 배치를 조정하는데, hot block 안에 많은 cold record가 함께 묻혀(piggyback) 옮겨진다.
- **Limitation 3 (승격 지연)**: read-heavy workload에서는 compaction이 드물게 일어나 hot record가 SD에 쌓일 때까지 기다려야 하고, 이 지연이 record가 hot한 시간 창을 놓치게 한다.

> [!quote]- 📄 원문 표현 (paper)
> - "the 'read-hot' set and the 'write-hot' set may not always fully overlap. This leads to a majority of the read-hot records sitting in slow storage with higher latency, thus compromising the read performance of the LSM-tree." (p.1)
> - "If 1TB of its local SSD is used to store hot records, we need at least 1TB/(1+5) = 166.7GB memory to track those hot keys, which exceeds the physical memory of the instance." (p.3)
> - "these solutions can only promote hot data to fast storage through LSM-tree compactions. To deal with read-heavy workloads where compactions happen infrequently, several systems allow triggering compactions proactively, but they must wait for the hot records to accumulate in the slow storage before promoting them. Such a delay harms read performance because it could overstep the time window when a record is hot (limitation 3)." (p.1)

## 핵심 통찰

1. **Hotness를 on-disk LSM-tree(RALT)로 추적하면 메모리 병목을 푼다.** RALT는 값(value) 없이 key와 scoring metadata만 logging하므로 작고, LSM-tree의 낮은 write latency 덕에 lookup의 critical path에 access record를 삽입해도 부담이 적다.
2. **Record 단위로 hotness를 보면 cold record를 함께 끌고 가지 않는다.** RALT의 key 범위와 compaction iterator를 sort-merge로 함께 스캔해 효율적으로 hot record만 골라낸다.
3. **Compaction에 의존하지 않는 promotion by flush가 read-heavy에서 승격 지연을 없앤다.** SD에서 읽힌 record를 in-memory promotion cache에 캐싱했다가, 충분한 compaction이 없을 때 hot record를 L0으로 직접 flush한다.

> [!quote]- 📄 원문 표현 (paper)
> - "instead of tracking the record hotness in memory, HotRAP logs each record access in a small specially-made LSM-tree, called RALT (Recent Access Lookup Table), located in FD (addressing limitation 1)." (p.1)
> - "By utilizing RALT, HotRAP also supports hot record retention and promotion at the record level rather than at the block/SSTable level, thus preventing cold records from being piggybacked to FD (addressing limitation 2)." (p.2)
> - "HotRAP introduces an in-memory mutable promotion cache to cache records read from SD and timely promotes the hot ones (via checking RALT) by flushing them to the top of the data LSM-tree (addressing limitation 3)." (p.2)

## 설계 / 메커니즘

두 가지 핵심 컴포넌트: **RALT** (hotness 추적)와 **promotion cache** (mutable + immutable 리스트).

**세 가지 hot record 경로 (FD에 hot record를 두는 pathway):**

1. **Retention (FD→SD compaction 중)**: FD에서 SD로 compaction할 때, 각 record의 hotness를 RALT로 확인해 hot record는 SD로 내려보내지 않고 FD에 다시 쓴다. compaction iterator와 RALT iterator를 sort-merge 방식으로 함께 진행해 retain할 record를 결정한다. RALT iterator의 I/O는 작다 (RALT는 value를 저장하지 않음).

2. **Promotion by compaction (FD↔SD compaction 중)**: SD에서 읽혀 mutable promotion cache에 캐싱된 record 중, compaction key 범위에 들어오는 것을 추출(extract)해 함께 처리한다. RALT에 hotness를 물어 hot record(예: K1)는 FD의 마지막 레벨에 쓰고, cold record(K2)는 drop한다.

3. **Promotion by flush (read-heavy용)**: compaction이 부족하면 promotion cache가 커진다. mutable promotion cache가 64MiB(기본값)에 도달하면 immutable promotion cache로 전환하고 bulk-insert해 L0으로 flush한다. RALT를 조회해 hot record만 L0으로 넣고 cold는 drop한다.

**RALT (Recent Access Lookup Table)**: FD에 두는 경량 LSM-tree. 각 access record는 key, value의 길이(값 자체가 아님), scoring metadata(tick, score)로 구성. score가 threshold보다 크면 hot. 네 가지 연산: (1) access record 삽입, (2) key hotness 체크, (3) 범위 내 hot set size 계산, (4) 범위 내 hot key 스캔. 삽입은 in-memory **unsorted buffer**를 거친다(정렬 메모리 테이블은 도움이 적음). hotness 체크는 random disk read를 피하려 in-memory bloom filter(14-bit, false positive ≪1%)를 사용.

**Scoring**: exponential smoothing. key의 score = Σ aᵢαᵗ⁻ⁱ (N=time slice 수, t=현재 time slice, aᵢ=i번째 slice 접근 여부). γ=0.001, α=1−γ=0.999. tick은 현재 time slice sequence number이고, γ×(FD 크기)만큼 데이터가 접근될 때마다 증가.

**Eviction**: hot set size 또는 physical size가 limit 초과 시 β=10%의 access record를 evict하며 single sorted run으로 merge. score threshold를 직접 정하기 어려워, N개 점을 샘플링해 k번째 largest score를 근사 threshold로 삼는다.

**Concurrency control of promotion by flush**: hot record를 L0으로 flush하면 FD 상위 레벨의 **newer version**을 가릴 위험이 있다. 이를 막기 위해 SSTable에 "being compacted" 표시, snapshot에 *updated* field 부착, Checker background thread가 immutable MemTable과 FD 레벨에서 newer version을 찾아 제외한다. 이 체크의 abort rate는 1% 미만.

**Auto-tuning**: RALT의 두 파라미터(hot set size limit, physical size limit)는 workload 분포에 따라 dynamic하게 추정. counter c와 tag t로 stable/unstable record 구분, 접근량이 R에 도달하면 모든 counter를 1씩 감소(Algorithm 1). 파라미터: L_hs=0.05×FD size, R_hs=0.7×FD size, D_hs=0.1×FD size, R=FD size, Δ_c=2.6, c_max=5, p_t=1/(0.5×FD size).

**Write amplification of retention**: FD 마지막 레벨의 cold 비율이 p면, retention으로 인해 1/p× 더 많은 compaction이 필요. SD의 마지막 레벨 size ratio를 pT로 줄이고 그 뒤에 size ratio 1/p의 extra level을 추가하면 SD write amplification은 normal LSM-tree보다 1/(2p)만 커진다.

> [!quote]- 📄 원문 표현 (paper)
> - "During compactions from FD to SD, HotRAP checks the hotness of each record to be compacted, and retains the hot ones in FD. It does so by constructing a RALT iterator whose range is the key range of FD's input SSTables." (p.3)
> - "RALT is a lightweight LSM-tree on FD, logging the accesses to records in HotRAP." (p.4)
> - "We use exponential smoothing to calculate scores in order to utilize history information. The score for a key is defined as Σ aᵢαᵗ⁻ⁱ" (p.5)
> - "We use 14-bit bloom filters to achieve a low overall false positive rate (≪ 1%)." (p.5)
> - "Our experiments show that this check only aborts less than 1% of insertions into the promotion cache." (p.6)

## 평가

**Testbed**: AWS EC2 i4i.2xlarge (8 vCPU, 64GiB memory, 1875GB local Nitro SSD). FD=local SSD, SD=gp3. FD:SD size ratio 1:10, FD 기대 size 10GB, SD 100GB. 16 threads, 16KiB block, 10-bit bloom filter. Disk 성능: FD ≈83000 rand 16K read IOPS / ~1.4GiB/s read; SD 10000 IOPS / 1000MiB/s (Table 1).

**비교 대상**: Mutant, PrismDB, SAS-Cache, RocksDB-FD (전부 FD에, upper bound), RocksDB-tiered.

**YCSB (Figure 6, 7)**:
- hotspot-5% 에서 HotRAP은 RocksDB-FD(이상적 상한)에 근접 (~95% hit rate). read-only(RO)에서 2nd-best 대비 **5.4×**, read-write-balanced(RW)에서 **3.8×**.
- uniform에서는 RocksDB-tiered보다 0.4%만 느림 → promotion이 무의미할 때 overhead가 낮음.
- Zipfian(s=0.99)에서는 RocksDB-FD 대비 hit rate 79%로 차이 있으나 여전히 다른 설계보다 우수.
- Mutant/SAS-Cache는 granularity가 거칠어 RocksDB-tiered 대비 개선 미미, PrismDB도 비효율적 promotion으로 개선 작음.

**Tail latency (Figure 8)**: read-heavy에서 HotRAP은 FD hit rate가 높아 SD 접근이 줄어, 다른 시스템보다 낮은 tail latency.

**Twitter production traces (Figure 9, 10, 11)**: cluster 17에서 최대 **5.27×** speedup. sunk/hot record read 비율이 높을수록 HotRAP 우세. 거의 항상 best, 2nd-best 대비 최대 1.9×. PrismDB는 cluster 10에서 frequently-updated key를 균일하게 읽어 잦은 demotion lock contention으로 성능 심각 저하.

**Cost breakdown (Figure 12, 13)**: RALT는 total CPU time의 3.7~13.3%, total I/O의 5.5~12.7%만 차지.

**개별 기법 효과 (Table 3, 4)**:
- *no-retain* (retention 제거): RW hotspot-5%에서 hit rate 71.4% (HotRAP 94.5%), promoted 35.1GB vs 6.2GB → retention 없으면 hot record를 반복 승격해야 함.
- *no-hotness-check* (hotness 체크 없이 모두 승격): uniform RO에서 173.0× 더 많은 record 승격, 248.4× 더 많은 compaction I/O.

**Dynamic workload (Figure 15)**: hotspot이 expand/shrink/shift할 때 auto-tuning이 적응적으로 hot set size limit을 조정. hotspot 확장 시 새 hot key가 추가되며 hit rate 회복.

> [!quote]- 📄 원문 표현 (paper)
> - "Compared to other systems, HotRAP achieves 5.4× speedup over the second best for read-only (RO) workloads, and 3.8× for read-write-balanced workloads. On the other hand, HotRAP is only 0.4% slower than RocksDB-tiered under uniform workloads, showing the overhead of HotRAP is low when promotion has no benefits." (p.9)
> - "e.g., achieving up to 5.27× speedup under the trace of cluster 17." (p.10)
> - "The results show that RALT is only responsible for 3.7%–13.3% of total CPU time and 5.5%–12.7% of total I/O, hence the design is efficient." (p.11)
> - "Table 4 shows that under uniform workloads, no-hotness-check promotes 173.0× more records and thus incurs 248.4× more compaction I/O than HotRAP." (p.11)

## 섹션 노트

- **Leveling 정책 가정**: 본 논문은 RocksDB의 기본인 leveling compaction을 대상. partial compaction(L_{i-1}의 SSTable이 L_i의 최소 overlap SSTable과 merge)으로 write amplification ≈ nT/2.
- **MVCC와 supervision**: RocksDB의 snapshot(supervision)을 활용해 promotion 시 version 일관성 보장. promotion cache에 record 삽입 전 SD에 newer version이 있는지 SSTable "being compacted" 표시로 확인.
- **두 가지 size limit**: hot set size limit(hot record 총 크기 제한) vs physical size limit(RALT 자체의 disk 사용 제한, =r×hot set size, r은 평균 physical/HotRAP size 비율).
- **Memory usage**: bloom filter + index block만 메모리에 캐싱, 200B record/5% hot key 가정 시 bloom filter는 data size의 0.0438%, 전체 메모리는 data size의 0.056%.
- **Overestimation 처리**: 범위 hot set size 계산이 edge data block과 중복 key로 약 10% overestimate되나, 큰 size ratio에서는 무시 가능 (예: ratio 10일 때 약 1/10).

## 핵심 용어

- **RALT (Recent Access Lookup Table)**: FD에 위치한 경량 LSM-tree. value 없이 key·길이·scoring metadata만 저장해 record hotness를 추적하는 on-disk 구조.
- **FD (Fast Disk) / SD (Slow Disk)**: 빠른 저장소(local SSD)와 느린 저장소(HDD/cloud storage)의 약어. size ratio 1:10 가정.
- **Retention**: FD→SD compaction 시 hot record를 SD로 내려보내지 않고 FD에 다시 쓰는 것.
- **Promotion by compaction**: SD↔FD compaction 시 promotion cache의 hot record를 FD로 끌어올리는 경로.
- **Promotion by flush**: compaction이 부족한 read-heavy workload에서 promotion cache의 hot record를 L0으로 직접 flush해 승격하는 경로.
- **Promotion cache**: SD에서 읽힌 record를 캐싱하는 in-memory 구조. mutable 하나 + immutable 리스트로 구성.
- **Exponential smoothing**: 과거 접근 이력을 가중 합산해 key score를 계산하는 방식 (score = Σ aᵢαᵗ⁻ⁱ).
- **Sunk record**: 마지막 update 이후 DB size의 5% 초과 데이터가 쓰여, 최신 버전이 SD로 가라앉았을 가능성이 큰 record (Twitter trace 분석 정의).
- **Auto-tuning**: workload에 따라 hot set size limit과 physical size limit을 stable/unstable record counter로 동적 조정하는 메커니즘.

## 강점 · 한계 · 열린 질문

**강점**
- Record-level granularity로 cold record piggyback 문제를 근본적으로 제거.
- On-disk RALT로 메모리 병목 회피 (data size의 0.056% 메모리).
- Compaction에 의존하지 않는 promotion by flush로 read-heavy에서 즉각 승격.
- Promotion이 무의미할 때(uniform) overhead가 0.4%로 매우 낮아 robustness 우수.
- Auto-tuning으로 dynamic workload(hotspot 이동)에 적응.

**한계**
- Auto-tuning 알고리즘은 hot record가 random하게 접근된다고 가정. "sequential flooding" 같은 극단적 패턴에서는 D_hs를 키워야 동작 (real-world에선 드묾).
- RALT physical size limit 만큼의 추가 FD 공간과 access record 삽입 overhead 존재.
- Retention은 write amplification을 증가시켜 SD 레벨 구조 조정(extra level)이 필요.

**열린 질문**
- Promotion by compaction을 FD 상위 레벨에서도 트리거하려면 복잡한 version control이 필요 — 더 적극적 승격으로 얻을 이득은?
- Tiering compaction 정책(leveling이 아닌)에서 retention/promotion이 어떻게 일반화될까?
- Cloud storage(object store)를 SD로 쓸 때 network latency가 promotion 효과에 미치는 영향은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. HotRAP이 해결하는 기존 LSM-tree tiered storage의 세 가지 한계는?
> (1) hotness를 메모리에서 추적해 메모리가 병목(memory limitation), (2) block/SSTable 단위 데이터 이동으로 cold record가 piggyback되는 거친 granularity, (3) hot record 승격을 compaction에 의존해 발생하는 지연. 각각 RALT(on-disk 추적), record-level retention/promotion, promotion by flush로 해결한다.

> [!question]- Q2. RALT는 왜 LSM-tree로 구현되었고, value를 저장하지 않는 이유는?
> access record 삽입이 lookup의 critical path에 있어 낮은 write latency가 중요하기 때문에 LSM-tree를 선택했다. value를 저장하지 않고 key·길이·scoring metadata만 저장해 크기를 작게 유지하므로(메모리 0.056%, I/O의 5.5~12.7%만 차지) 효율적이다.

> [!question]- Q3. Retention과 promotion의 차이는?
> Retention은 이미 FD에 있는 hot record가 FD→SD compaction 시 SD로 내려가지 않도록 FD에 다시 쓰는 것(유지). Promotion은 SD에 있는 hot record를 FD로 끌어올리는 것(승격)이며, compaction(promotion by compaction)과 flush(promotion by flush) 두 경로가 있다.

> [!question]- Q4. read-heavy workload에서 promotion by flush가 왜 필요한가?
> read-heavy에서는 write가 적어 compaction이 드물게 일어난다. 따라서 promotion by compaction만으로는 promotion cache가 계속 커지고 hot record를 제때 승격하지 못한다. mutable promotion cache가 64MiB에 도달하면 immutable로 전환해 hot record를 L0으로 직접 flush함으로써 compaction을 기다리지 않고 승격한다.

> [!question]- Q5. promotion by flush에서 stale version이 newer version을 가리는 문제는 어떻게 방지하는가?
> SD에서 읽은 record를 L0으로 flush하면 FD 상위 레벨의 더 최신 version을 가릴 수 있다. SSTable에 "being compacted" 표시, immutable promotion cache에 *updated* field 부착, Checker background thread가 snapshot의 immutable MemTable과 FD 레벨에서 newer version을 찾아 제외하는 concurrency control로 정확성을 보장한다(abort rate <1%).

> [!question]- Q6. key score는 어떻게 계산되며 hotness는 어떻게 판정하는가?
> exponential smoothing으로 score = Σ aᵢαᵗ⁻ⁱ (α=0.999, γ=0.001). aᵢ는 i번째 time slice 접근 여부, t는 현재 time slice. time slice는 γ×(FD 크기)만큼 데이터가 접근될 때마다 증가. score가 threshold보다 크면 hot. hotness 체크는 random disk read를 피하려 14-bit in-memory bloom filter(false positive ≪1%)를 사용한다.

> [!question]- Q7. retention이 가져오는 write amplification 문제와 그 완화책은?
> FD 마지막 레벨의 cold 비율이 p면 retention 때문에 1/p× 더 많은 compaction이 필요해 write amplification이 늘어난다. SD 마지막 레벨의 size ratio를 pT로 줄이고 그 뒤에 size ratio 1/p의 extra level을 추가하면 SD write amplification 증가를 normal LSM-tree 대비 1/(2p)로 제한할 수 있다.

> [!question]- Q8. 대표 성능 수치는?
> YCSB common skew에서 2nd-best 대비 read-only 5.4×, read-write-balanced 3.8×. Twitter production trace에서 cluster 17 최대 5.27×, 전반적으로 2nd-best 대비 최대 1.9×. uniform에서는 RocksDB-tiered 대비 0.4%만 느려 overhead가 낮다. RALT는 total CPU의 3.7~13.3%, total I/O의 5.5~12.7%.

## 🔗 Connections

[[KV-LSM]] · [[ATC]] · [[2025]]

## References worth following

- [37] Yoon et al., **Mutant**: Balancing storage cost and latency in LSM-tree data stores. ACM SoCC 2018. (SSTable-level placement 비교 기준)
- [31] Raina et al., **PrismDB**: Efficient compactions between storage tiers with prismdb. ASPLOS 2023. (proactive compaction 기반 비교 대상)
- [32] Wang & Shao, **MirrorKV**: An efficient key-value store on hybrid cloud storage. SIGMOD 2023. (key/value LSM-tree 분리)
- [42] Cao et al., **SAS-Cache**: A semantic-aware secondary cache for LSM-based key-value stores. MSST 2024. (block-level secondary cache 비교)
- [41] Zhang et al., **SA-LSM**: optimize data layout for LSM-tree based storage using survival analysis. VLDB 2022. (survival analysis 기반 cold data 예측)
- [35] Yang et al., A large scale analysis of hundreds of in-memory cache clusters at Twitter. OSDI 2020. (Twitter production trace 출처)

## Personal annotations

<본인 메모 영역>
