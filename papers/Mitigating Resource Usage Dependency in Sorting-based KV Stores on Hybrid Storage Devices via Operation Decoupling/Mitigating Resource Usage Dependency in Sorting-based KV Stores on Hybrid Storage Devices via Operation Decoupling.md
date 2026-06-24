---
title: "Mitigating Resource Usage Dependency in Sorting-based KV Stores on Hybrid Storage Devices via Operation Decoupling"
aliases: [Mitigating Resource Usage, DecouKV]
description: "정렬 기반 LSM KV의 flush/compaction을 CPU·I/O 작업으로 decoupling해 hybrid storage에서 자원 의존성과 write stall을 완화하는 DecouKV (ATC 2025)"
venue: ATC
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/kvlsm
  - topic/kv-store
  - topic/hybrid-storage
  - venue/atc
  - year/2025
---

# Mitigating Resource Usage Dependency in Sorting-based KV Stores on Hybrid Storage Devices via Operation Decoupling

> **ATC 2025** · cluster/kvlsm · Source: [Mitigating Resource Usage Dependency in Sorting-based KV Stores on Hybrid Storage Devices via Operation Decoupling.pdf](Mitigating%20Resource%20Usage%20Dependency%20in%20Sorting-based%20KV%20Stores%20on%20Hybrid%20Storage%20Devices%20via%20Operation%20Decoupling.pdf)

저자: Qingyang Zhang, Yongkun Li (corresponding), Yubiao Pan, Haoting Tang, Yinlong Xu — University of Science and Technology of China; Huaqiao University; Anhui Provincial Key Laboratory of High Performance Computing

## TL;DR
LSM-tree 기반 KV store의 flush·compaction(정렬 연산)은 CPU와 I/O 자원을 한 연산 안에서 뒤엉켜(intertwined) 소비하고, 연산들 사이에 의존성·경합을 만들어 **자원 사용 의존성(resource usage dependency)** 을 유발한다. 특히 빠른 장치(PM/NVMe)와 느린 장치(SATA SSD/HDD)를 섞은 hybrid storage에서는 장치별 병목(CPU vs I/O)이 번갈아 나타나 자원이 단편화되고 write stall이 심해진다. DecouKV는 인덱스를 데이터 파일에서 분리하여 정렬 연산을 **CPU 집약적 index merge**와 **I/O 집약적 data append/flush** 로 decoupling하고, 두 큐(IMQ/DFQ)와 auto-tuning으로 자원 사용을 스케줄링한다. RocksDB 대비 CPU 이용률 25.4~32.3% 향상, write-intensive에서 throughput 2.3~4.9배 향상, tail latency 74.3~91.4% 감소를 달성한다.

## 문제 & 동기
정렬 기반(LSM) KV store는 flush와 compaction이라는 무겁고 불가피한 정렬 연산으로 디스크의 KV 쌍을 관리한다. 이 연산들은 **operation coupling** 으로 인해 세 가지 문제를 일으킨다: (i) 한 연산 안에서 CPU(serialization·merge sorting)와 I/O(reading·writing)가 동시에 소비되는 **intertwined resource consumption**, (ii) 한 레벨의 정렬이 다음 레벨 임계치를 넘겨 후속 정렬을 유발하는 **interdependencies among operations**, (iii) 동시 실행되는 정렬 연산들이 자원을 두고 다투는 **resource contention**. hybrid storage에서는 빠른/느린 장치의 성능 격차 때문에 빠른 장치에서는 CPU 병목(이용률 94%, 디스크 48%), 느린 장치에서는 I/O 병목(디스크 88%, CPU 21%)이 번갈아 발생하여 평균 자원 이용률이 낮고 write stall이 악화된다. 기존 해법인 (i) 고정된 차등 데이터 관리(MatrixKV 등)와 (ii) 정렬 연산의 표면적 스케줄링(SILK, ADOC 등)은 intertwined 소비를 근본적으로 해결하지 못한다.

> [!quote]- 📄 원문 표현 (paper)
> "Operation coupling issues lead to severe resource usage dependency, making it difficult to isolate CPU and I/O resources and schedule them effectively. These issues are exacerbated on hybrid storage devices due to the performance disparity between fast and slow devices, which causes fluctuating resource consumption." (p.1, Abstract/Intro)
>
> "during sorting on fast devices, CPU utilization is high (94%) but disk utilization is low (48%), indicating a CPU bottleneck; during sorting on slow devices, disk utilization is high (88%) while CPU utilization is low (21%), indicating an I/O bottleneck." (p.5, §2.2.2)

## 핵심 통찰
> [!note]- 통찰 1 — 정렬 연산의 자원 소비는 분리 가능하다 (인덱스=CPU, 데이터=I/O)
> 정렬 연산에서 CPU 소비는 주로 **인덱스 정렬**에서, I/O 소비는 주로 **데이터 읽기/쓰기**에서 발생한다. 따라서 인덱스를 데이터 파일에서 분리하면 CPU 집약 작업과 I/O 집약 작업을 따로 떼어 독립적으로 스케줄링할 수 있다. 이것이 DecouKV의 핵심 가정이다. (p.4, §2/§3.1)

> [!note]- 통찰 2 — 빠른 장치의 높은 random access 성능이 unordered 인덱스/데이터를 허용한다
> 빠른 장치(FD)는 random access 성능이 sequential에 근접하므로, 정렬되지 않은 append-only file(AOF)과 skip list 기반 IndexTable을 써도 성능 손실이 작다. AOF가 기존 WAL을 대체하므로 별도 WAL이 불필요하고, FD에서는 Bloom filter를 제거해도 read 성능 영향이 작아 CPU/메모리 오버헤드를 줄인다. (p.7, §3.3)

> [!note]- 통찰 3 — 자원 병목 상태를 큐 길이로 측정해 능동적으로 조정한다
> IMQ 길이(CPU 압력)와 DFQ 길이(I/O 압력)를 점수화(Score_IM, Score_DF)하여 시스템이 CPU-bound인지 I/O-bound인지 판단하고, IMTN·DFTS·레벨 용량을 auto-tuning한다. 표면적 스케줄링과 달리 정렬 연산의 resource-bound 본질을 직접 다룬다. (p.8~9, §3.5~3.7)

> [!note]- 통찰 4 — 높은 레벨은 elastic capacity로 amplification factor를 완화한다
> 느린 장치(SD)의 high level(HL)은 메모리·random access 제약으로 decoupling 설계를 적용하기 어렵다. 대신 strict한 amplification factor를 풀어주는 **elastic capacity** 로 연산 간 의존성을 줄인다. (p.9, §3.6)

## 설계 / 메커니즘
> [!abstract]- DecouKV 전체 구조 및 핵심 컴포넌트
> **decoupling 원리.** 정렬 연산을 (a) CPU 집약적 **index merge**, (b) I/O 집약적 **data append**, (c) I/O 집약적 **data flush** 세 종류로 분리한다. (p.7, Fig.6)
>
> **IndexTable.** DRAM에 인덱스를 저장하는 mergeable **skip list** 구조. 각 entry는 key + data address(8-byte file number + 8-byte offset). RocksDB MemTable보다 같은 메모리에 더 많은 entry를 담는다. 초기 max_index_size는 작게(8MB, 평가에서는 16MB) 설정하고 **asynchronous insertion** 으로 I/O 완료를 기다리지 않고 삽입한다. 한도 도달 시 non-insertable로 전환하고 새 IndexTable 생성. 쿼리는 최신→오래된 순으로 IndexTable들을 순회. (p.7, §3.3)
>
> **Append-only file (AOF).** KV 쌍을 FD에 AOF로 저장하고 대응 인덱스는 DRAM IndexTable에. unordered지만 FD의 high bandwidth/concurrency로 sequential에 준하는 random 성능 확보. AOF가 WAL을 대체(crash recovery), Bloom filter 생성 제거로 CPU·메모리 절약. (p.7, §3.3)
>
> **Index merge (CPU-intensive).** unordered IndexTable이 query 성능을 저하시키므로 두 IndexTable을 하나로 병합. AOF 데이터는 건드리지 않고 DRAM에서만 수행. **multithreaded parallel merging**: skip list pointer 변경으로 query를 막지 않고 병합, 병합 중 노드는 **Merging Index Set**(CAS 기반 lock-free set)에 atomic pointer로 보관해 정확성 보장. (p.8, §3.4, Fig.7~8)
>
> **Data flush (I/O-intensive).** IndexTable이 data_flush_trigger_size(DFTS)에 도달하면 IndexTable + 대응 AOF를 SD의 SSTable(ordered)로 통합. IndexTable의 인덱스를 순차 스캔 → AOF에서 데이터 회수 → SSTable로 기록 → 낡은 IndexTable/AOF 삭제. (p.8, §3.4)
>
> **스케줄링: IMQ / DFQ + auto-tuning.** non-insertable IndexTable을 IMQ(index merge 대기) 또는 DFQ(data flush 대기)에 넣는다. Score_IM = L_IMQ / IMTN (>1.5면 CPU-bound), Score_DF = L_DFQ (>3이면 I/O-bound). CPU-bound면 IMTN을 키워 merge 빈도↓; I/O-bound면 DFTS를 키워 일부 IndexTable을 DFQ→IMQ로 되돌려 I/O 압력↓·CPU 수요↑. 두 큐 모두 일정 시간(60s) 혼잡 없으면 자원 idle로 보고 IMTN·DFTS·HL 용량을 동시 축소. (p.8~9, §3.5, Fig.9~10)
>
> **Elastic capacity high levels.** SD의 HL은 leveled compaction 유지하되 amplification factor를 완화. HL_0가 IO-bound면 HL_1 용량 임계치를 풀고 정렬을 미뤄 CPU index merge 우선. DFTS 조정으로 HL_0 파일 크기 제어, index merge가 duplicate key write amplification 일부 완화. (p.9, §3.6)

## 평가
> [!success]- 실험 환경 및 주요 수치 (p.10~16)
> **환경.** 2×20-core Intel Xeon Gold 5218R, 128GB DRAM, Ubuntu 20.04.6 / kernel 5.15. FD = 128GB Intel Optane DCPMM + Intel SSDPE2KE032T8 NVMe; SD = 960GB Intel S4520 SATA SSD. RocksDB v9.3.0과 MatrixKV, ADOC, PrismDB, SplitDB 비교. Memtable/SSTable 64MB, Bloom 10 bit/key, block cache 1GB, 4 CPU core, direct I/O. FD는 데이터셋의 10%로 제한. IndexTable 초기 크기 16MB, IMTN 기본 4, DFTS 기본 32MB(4×8MB). (p.10, §4)
>
> **Microbenchmark (100GB DB, KV 1KB=24B key+1000B value).** RocksDB 대비 CPU 평균 이용률 향상: insert +32.3%, update +25.4%, read +27.3% (Fig.11). Disk 이용률 향상: insert +17.6~31.6%, update +8.0~19.9% (Fig.12). DecouKV insert CPU Avg 74.2 / Rng 29.2, RocksDB Avg 41.9 / Rng 76.8 — 변동폭 대폭 감소. (p.10~11)
>
> **Throughput (Fig.13a).** RocksDB 대비 update 4.3×, insert 절대값(주석) 포함 write-intensive 우위, read 1.2×, scan 1.5×. MatrixKV·ADOC 대비 insert 1.6×·2.2×, update 2.2×·2.1×. (p.11~12)
>
> **Latency (Fig.13b, Table 1).** RocksDB 대비 평균 latency insert -76.0%, update -77.9%. MatrixKV·ADOC 대비 38.7~56.9% 감소. Tail latency: insert P90 -71.6~83.3%, P99 -74.3~91.4%; update P90 -69.8~80.4%. DecouKV insert P90 1.579ms / P99 3.227ms (vs RocksDB 9.454 / 37.557). (p.12)
>
> **YCSB (100M ops, 100GB DB).** write-intensive(Load, A, F): RocksDB 대비 2.3~4.9×, MatrixKV·ADOC 대비 1.4×·1.6×(Load) 및 1.5~3.4×(A/F), PrismDB·SplitDB 대비 1.5~3.6×. read/scan-intensive(B,C,D,E): RocksDB 대비 zipfian 1.2~1.9×, uniform 1.2~2.3×. 고편향 zipfian에서는 SplitDB(cascading skiplist index)가 우수. (p.12~14, Fig.14)
>
> **Breakdown (Fig.15).** DecouKV-D(decoupling만), DecouKV-DS(+scheduling), DecouKV(+elastic capacity)로 분해. decoupling이 Load·YCSB-A 모두 크게 기여(YCSB-A 더 큼), scheduling queue는 Load에 더 기여, elastic capacity HL은 Load에 더 큰 이득. IMTN 최대값 4, HL_0 파일 수 2 이하 유지. (p.14~15)
>
> **기타.** 500GB 데이터셋: RocksDB 대비 5.2×(load), 2.8×(YCSB-A), 2.4×(YCSB-F). Nutanix 실제 워크로드(57% update, 41% read, 2% scan) 1.3~2.0× 향상. value 1KB·4KB에서 최대 이득, NVMe-only에서도 1.4~2.4× 향상. core 수↑일수록 이득 커지나 16 core면 disk I/O 병목. Memory: IndexTable이 메모리의 23%(0.7GB, RocksDB MemTable 0.3GB보다 큼)지만 TableCache는 더 작음(no AOF Filter/Index Block). Recovery: 10GB DB에서 8.76s (RocksDB 8.12s와 유사). (p.15~17)

## 섹션 노트
- **§1 Introduction**: operation coupling을 자원 의존성·단편화의 근본 원인으로 지목. 기여 4가지(분석/decoupling 솔루션/elastic level capacity/task scheduling). 소스 공개(github.com/QingyangZ/DecouKV).
- **§2 Background and Motivation**: LSM 구조(MemTable→flush→L0→compaction), hybrid storage 배포(NoveLSM/MatrixKV/SpanDB/PrismDB). §2.2 operation coupling 3문제, §2.2.2 장치별 병목(Fig.4)·교차 병목(Fig.5), §2.3 기존 해법(차등관리 MatrixKV·표면적 스케줄링 ADOC)의 한계.
- **§3 DecouKV Design**: §3.1 설계 목표·도전, §3.2 개요, §3.3 IndexTable·AOF, §3.4 index merge·data append/flush, §3.5 IMQ/DFQ 스케줄링, §3.6 elastic capacity HL, §3.7 auto-tuning(Fig.10 control flow).
- **§4 Evaluation**: §4.1 microbenchmark, §4.2 YCSB, §4.3 breakdown, §4.4 other workloads(500GB/Nutanix), §4.5 understanding(KV size/core/device/memory/recovery).
- **§5 Related Work**: write stall 완화(SILK/ADOC), KV separation(WiscKey/HashKV), 새 매체 배포(NoveLSM/SLM-DB/MatrixKV/PrismDB/SplitDB), tuning(Monkey/Dostoevsky/Rafiki/TiKV).
- **§6 Conclusion / §7 Ack**: shepherd Jinkyu Jeong, NSFC·CAS 지원. Artifact Appendix(github.com/QingyangZ/DecouKV.git, test.sh ~50h).

## 핵심 용어
- **Operation coupling**: 정렬 연산(flush/compaction) 내부에서 CPU·I/O가 뒤엉켜 소비되고 연산 간 의존·경합이 생기는 현상. 자원 사용 의존성의 근본 원인.
- **Resource usage dependency**: 한 자원(CPU 또는 I/O)의 사용이 다른 자원/연산에 묶여 독립적으로 스케줄링할 수 없는 상태.
- **Operation decoupling**: 인덱스를 데이터 파일에서 분리하여 정렬 연산을 CPU 집약 index merge와 I/O 집약 data append/flush로 나누는 DecouKV의 핵심 기법.
- **IndexTable**: DRAM의 mergeable skip list 인덱스 구조. key + data address(file number+offset)를 보관, async insertion 지원.
- **AOF (Append-only file)**: FD에 unordered로 KV를 append하는 파일. WAL을 대체하고 Bloom filter 생성을 제거.
- **Index merge**: 두 IndexTable을 DRAM에서 병합하는 CPU 집약 작업. multithreaded parallel merging + Merging Index Set(CAS lock-free)으로 query 무중단.
- **Data flush**: IndexTable+AOF를 SD의 정렬된 SSTable로 통합하는 I/O 집약 작업. 트리거 임계치 = DFTS.
- **IMQ / DFQ**: Index Merge Queue / Data Flush Queue. 큐 길이로 CPU·I/O 자원 압력을 측정해 스케줄링.
- **IMTN / DFTS**: index_merge_trigger_num / data_flush_trigger_size. auto-tuning 대상 파라미터(Score_IM>1.5 → CPU-bound, Score_DF>3 → I/O-bound).
- **Elastic capacity high levels**: SD의 high level에서 amplification factor 제약을 완화해 연산 간 의존성을 줄이는 설계.
- **FD / SD**: Fast Devices(PM, NVMe SSD) / Slow Devices(SATA SSD, HDD). hybrid storage의 두 계층.

## 강점 · 한계 · 열린 질문
**강점**
- intertwined resource consumption을 인덱스/데이터 분리로 근본적으로 해결 — 표면적 스케줄링(ADOC)보다 한 단계 깊은 접근.
- auto-tuning(IMTN/DFTS/HL 용량)으로 워크로드·장치 변화에 능동 대응, 자원 변동폭(Rng) 대폭 축소.
- write-intensive에서 throughput 2.3~4.9×, tail latency 74.3~91.4% 감소로 효과가 크고, read/scan에서도 퇴보 없음(오히려 소폭 개선).
- 다양한 검증(microbench, YCSB, 500GB, Nutanix, NVMe-only, core/value scaling, recovery, artifact 공개).

**한계**
- IndexTable이 메모리의 23%(0.7GB)를 차지 — RocksDB MemTable(0.3GB)보다 메모리 사용↑. DRAM 인덱스 의존이 큼.
- 고편향 zipfian read에서 SplitDB(cascading skiplist + hot/cold promotion)에 밀림 — read 최적화는 본 설계의 초점이 아님.
- FD의 높은 random access 성능에 강하게 의존(AOF unordered, Bloom 제거). FD 성능이 낮으면 가정이 약화될 수 있음.
- 16 core 등 CPU가 충분해지면 disk I/O가 병목이 되어 이득이 포화.

**열린 질문**
- IndexTable이 dataset 대비 매우 커지는 초대형 워크로드에서 DRAM 압박과 index merge 비용의 trade-off는?
- elastic capacity HL의 write amplification 완화 효과를 정량적으로 어디까지 보장하는가(read overhead와의 균형)?
- CXL SSD 등 새로운 FD 계층에서 decoupling 가정(random≈sequential)이 어떻게 변하는가?

## ❓ Q&A (자가 점검)
> [!question]- Q1. "operation coupling"이 정확히 무엇이며 왜 hybrid storage에서 더 심각한가?
> 정렬 연산(flush/compaction)이 CPU와 I/O를 한 연산 안에서 뒤엉켜 쓰고(intertwined), 연산 간 임계치 의존(interdependency), 동시 연산 간 경합(contention)을 일으키는 것이다. hybrid storage에서는 FD/SD 성능 격차로 FD 정렬은 CPU 병목(94%), SD 정렬은 I/O 병목(88%)이 번갈아 발생해 평균 자원 이용률이 낮아지고 write stall이 악화된다.

> [!question]- Q2. DecouKV는 정렬 연산을 어떻게 decoupling하는가?
> 인덱스를 데이터 파일에서 분리한다. CPU 소비는 인덱스 정렬에서, I/O 소비는 데이터 읽기/쓰기에서 나온다는 통찰에 근거해, 정렬 연산을 CPU 집약 index merge(DRAM의 IndexTable 병합)와 I/O 집약 data append/flush(AOF→SSTable)로 나눈다.

> [!question]- Q3. unordered AOF와 IndexTable을 써도 성능 손실이 적은 이유는?
> FD(NVMe/PM)는 random access 성능이 sequential에 근접하기 때문이다. 따라서 정렬되지 않은 append-only file로도 충분한 성능을 내고, AOF가 WAL을 대체하며 Bloom filter를 제거해도 read 성능 영향이 작아 CPU·메모리 오버헤드를 줄인다.

> [!question]- Q4. index merge 중에도 query가 정확하고 막히지 않는 메커니즘은?
> multithreaded parallel merging으로 skip list pointer만 바꿔 query를 막지 않고, 병합 중인 노드의 atomic pointer를 CAS 기반 lock-free **Merging Index Set**에 보관한다. query는 먼저 이 set을 확인한 뒤 다른 IndexTable을 접근해 정확성을 보장하고, 삽입 완료 후 set에서 제거한다.

> [!question]- Q5. 자원이 CPU-bound인지 I/O-bound인지 어떻게 판단하고 대응하는가?
> IMQ 길이로 Score_IM = L_IMQ/IMTN, DFQ 길이로 Score_DF = L_DFQ를 계산한다. Score_IM>1.5면 CPU-bound로 보고 IMTN을 키워 merge 빈도를 낮추고, Score_DF>3이면 I/O-bound로 보고 DFTS를 키워 IndexTable을 DFQ→IMQ로 되돌려 I/O 압력을 낮추고 CPU 수요를 올린다. 두 큐 모두 60s 혼잡 없으면 idle로 보고 파라미터를 동시 축소한다.

> [!question]- Q6. elastic capacity high levels가 필요한 이유는?
> SD의 high level은 메모리 제약과 낮은 random access 성능 때문에 decoupling 설계(DRAM IndexTable+AOF)를 적용하기 어렵다. 대신 leveled compaction을 유지하되 strict한 amplification factor를 완화하여 연산 간 의존성과 단계적 정렬 유발(cascading)을 줄인다.

> [!question]- Q7. RocksDB·MatrixKV·ADOC 대비 핵심 성능 수치는?
> RocksDB 대비 CPU 이용률 25.4~32.3% 향상, write-intensive throughput 2.3~4.9×, tail latency 74.3~91.4% 감소, read-intensive 1.2~2.3× 향상. MatrixKV·ADOC 대비 insert throughput 1.6×·2.2×, 평균 latency 38.7~56.9% 감소.

> [!question]- Q8. DecouKV가 read-heavy 고편향 워크로드에서 약한 이유는?
> read 최적화가 설계 초점이 아니기 때문이다. 고편향 zipfian에서는 cascading skiplist index와 hot/cold promotion/demotion을 쓰는 SplitDB가 핫 데이터 lookup latency를 더 잘 줄여 우위를 보인다. DecouKV는 그래도 강한 전반 성능은 유지한다.

## 🔗 Connections
[[KV-LSM]] · [[ATC]] · [[2025]]

## References worth following
- [47] Yao et al., **MatrixKV** (ATC 2020) — L0-L1 compaction을 matrix container로 차등 관리, 본 논문의 주요 비교/베이스라인.
- [48] Yu et al., **ADOC** (FAST 2023) — dataflow를 자동 조율하는 표면적 스케줄링 SOTA, 본 논문이 한계로 지목하며 비교.
- [42] Raina et al., **PrismDB** (ASPLOS 2023) — slab 기반 in-place update + cold-aware로 hybrid storage read 최적화, zipfian 비교.
- [6] Cai et al., **SplitDB** (IEEE TC 2024) — cascading skiplist index + hot/cold promotion, 고편향 read에서 DecouKV보다 우수.
- [4] Balmau et al., **SILK** (ATC 2019) — background 우선순위·rate-limit로 write stall/tail latency 완화, 표면적 스케줄링 계열.
- [35] Lu et al., **WiscKey** (TOS 2017) — KV separation의 시초, 인덱스/데이터 분리 아이디어의 뿌리.

## Personal annotations
<본인 메모 영역>
