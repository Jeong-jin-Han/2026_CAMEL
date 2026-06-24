---
title: "AnyKey: A Key-Value SSD for All Workload Types"
aliases: [AnyKey]
description: "value-to-key 비율이 낮은(low-v/k) 워크로드에서 메타데이터 폭증을 막아 모든 KV 워크로드에 대응하는 LSM-tree 기반 KV-SSD 설계"
venue: ASPLOS
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/kvlsm
  - topic/kv-ssd
  - topic/key-value
  - venue/asplos
  - year/2025
---

# AnyKey: A Key-Value SSD for All Workload Types

> **ASPLOS 2025** · `cluster/kvlsm` · Source: [AnyKey - A Key-Value SSD for All Workload Types.pdf](AnyKey%20-%20A%20Key-Value%20SSD%20for%20All%20Workload%20Types.pdf)

**저자:** Chanyoung Park, Jungho Lee, Kyungtae Kang (Hanyang University) · Chun-Yi Liu (Micron Technology Inc.) · Mahmut Taylan Kandemir (Pennsylvania State University) · Wonil Choi (Hanyang University, corresponding author)

## TL;DR
기존 LSM-tree 기반 KV-SSD(예: PinK)는 값(value)이 키(key)보다 훨씬 큰 "high-v/k" 워크로드에만 최적화되어 있다. 그러나 실제 데이터센터/캐시/블록체인 워크로드 중에는 키가 상대적으로 큰 "low-v/k" 워크로드가 존재하며, 이 경우 KV 쌍을 찾기 위한 메타데이터(키 + PPA)가 폭증해 device-internal DRAM에 다 담기지 못하고 flash로 흘러넘쳐 추가 flash 접근이 발생한다. **AnyKey**는 여러 KV 쌍을 정렬된 "data segment group" 단위로 묶고 각 group당 키 하나만 메타데이터로 유지하며, 32-bit hash value로 group 내 KV를 식별해 메타데이터 크기를 DRAM에 맞게 줄인다. PinK 대비 low-v/k 11개 워크로드에서 95th percentile tail latency 56% 개선, IOPS 299% 향상. 강화판 **AnyKey+**는 LSM-tree 관리(compaction chain) 오버헤드까지 줄여 low-v/k에서 315%, high-v/k에서 15% IOPS 향상으로 모든 워크로드 유형에서 우위를 보인다.

## 문제 & 동기
KV-SSD는 host SW 스택을 우회해 KV 요청을 device로 직접 보내 low-latency, scalability, write amplification 절감 등의 이점을 준다. 그러나 PinK 등 SOTA LSM-tree 기반 KV-SSD는 모두 value가 key보다 큰 "typical(high-v/k)" 워크로드로만 검증되었다. 실제로는 value/key 크기 비율이 낮은(low-v/k) 워크로드가 데이터베이스, 캐시, 소셜 네트워크, 블록체인 등에서 흔하다. value-to-key 비율이 낮아질수록(상대적으로 key가 커질수록), KV 쌍 위치를 가리키는 metadata 크기가 커져 device-internal DRAM에 담기지 못하고, 결국 target KV 쌍을 읽기 전 추가 flash 접근이 발생한다. PinK를 read-only 워크로드로 재평가하면 value 크기가 1,280B→20B로 줄 때(key 40B 고정) 95th percentile read latency가 891μs→8,904μs로 증가하고 IOPS는 94% 감소한다.

> [!quote]- 📄 원문 표현 (paper)
> "Unfortunately, the existing KV-SSD designs are tuned for a specific type of workload, namely, those in which the size of the values are much larger than the size of the keys." (p.1)
>
> "assuming that an SSD device is full of KV pairs, as the value-to-key ratio decreases (i.e., the relative size of keys increases), the size of the metadata (which indicates the locations of KV pairs in the flash) increases. This creates situations where the metadata are not fully accommodated in the device-internal DRAM, and consequently, each request may incur additional flash accesses before it actually reads the target KV pair." (p.2)
>
> "the read latency (for the 95th percentile) of PinK increases from 891μs to 8,904μs, and ... the IOPS of PinK decreases by 94%, as the value-to-key ratio decreases from 1280/40 to 20/40." (p.2)

## 핵심 통찰

> [!note]- 통찰 1: 메타데이터 폭증이 low-v/k 성능 저하의 근본 원인
> PinK는 KV 쌍마다 key + PPA를 meta segment에, 각 meta segment의 첫 key + 위치를 level list에 저장한다. high-v/k에서는 key가 작아 level list + meta segment가 DRAM에 들어가 flash 접근이 최대 3회로 제한된다. 그러나 low-v/k에서는 누적 메타데이터가 DRAM(예: 64MB) 용량을 넘어 flash로 흘러넘쳐, 한 key를 찾기 위해 level list조차 flash에서 여러 번 읽어야 한다 (p.4, Figure 5).

> [!note]- 통찰 2: KV를 group으로 묶어 정렬하면 group당 메타데이터 하나로 충분
> 기존 설계는 각 KV마다 직접 위치를 가리키는 메타데이터가 필요하다. AnyKey는 여러 KV 쌍을 "data segment group"으로 묶고 group 내부를 정렬해, group 메타데이터만으로 target KV를 찾을 수 있게 한다. 이로써 메타데이터를 KV 단위가 아니라 group 단위로 유지해 크기를 급감시킨다 (p.5).

> [!note]- 통찰 3: hash value로 큰 key를 대체해 메타데이터를 더 줄임
> low-v/k에서는 key 자체가 커서 group 내 정렬 키로 쓰면 메타데이터가 다시 커진다. AnyKey는 큰 key 대신 32-bit hash value를 사용해 group 내 KV를 정렬·식별한다. 분석상 32-bit hash가 수천 개 KV를 group 내에서 고유 식별 가능하며, hash collision은 group 단위 0.075%로 무시 가능하다 (p.6).

> [!note]- 통찰 4: 남는 DRAM을 hash list로 활용해 헛된 flash 접근 차단
> level list는 키 "범위"만 제공해 인접 레벨 간 범위가 겹치므로, target KV가 실제로 없는 data segment group을 읽는 헛수고가 생긴다. AnyKey는 남는 DRAM에 "hash list"(해당 group에 실존하는 키들의 hash value)를 두어, flash 페이지를 읽기 전에 KV 존재 여부를 확인한다 (p.6-7).

## 설계 / 메커니즘

> [!abstract]- 4대 자료구조 (Figure 6, p.5-7)
> AnyKey는 두 종류 메타데이터를 DRAM에, 두 종류 데이터를 flash에 둔다.
> - **Data segment group (flash):** 여러 flash page(현 구현 32개 8KB page)를 묶어 100~5,000개 KV 쌍을 hash value 기준 정렬 저장. 각 KV entry = {(i) key, (ii) key의 32-bit hash value, (iii) value 또는 value log를 가리키는 PPA}.
> - **Value log (flash):** WiscKey에서 영감. value를 key와 분리해 별도 영역에 저장. 모든 write의 value는 먼저 value log에 기록되고 일부가 compaction 시 data segment group으로 병합. 남는 SSD 용량의 절반만 value log로 예약(없으면 over-provisioned 용량의 절반 사용).
> - **Level list (DRAM):** 각 data segment group에 대응. {(i) 가장 작은 key, (ii) 첫 page의 PPA, (iii) 각 page 첫 key의 hash value 리스트(현 구현 Hash[0:15] 상위 16bit만)}. 레벨별 정렬.
> - **Hash list (DRAM):** 각 level list entry가 가진, 해당 group에 실제 존재하는 모든 key의 hash value. 남는 DRAM 공간에 상위 레벨부터 채우며, 공간 부족 시 하위 레벨은 생략.

> [!abstract]- Read 동작 (Figure 6, p.7-8)
> 주어진 key("PP", hash "4791")에 대해 level list를 L1→Ln 순회하며 key 범위에 들어가는 entry를 찾는다 → 해당 hash list에서 4791 확인(없으면 다음 레벨로) → group 식별 후, page 첫 key들의 hash value 비교로 정확한 flash page 선택해 읽음 → KV entry가 value를 직접 가지면 종료, PPA만 있으면 value log page를 추가로 읽음. AnyKey는 대부분 flash 접근을 최대 2회로 제한(value가 value log에 있으면 entry용 1회 + value용 1회).

> [!abstract]- Write / Compaction / GC / Range query (p.8-9)
> - **Write:** write buffer(L0)에 버퍼링, 가득 차면 L0→L1 compaction으로 새 data segment group과 level list 생성, value는 value log에 기록.
> - **Compaction:** (1) **Tree-triggered** — Ln-1 크기가 임계 초과 시 conventional Ln-1→Ln 병합. (2) **Log-triggered** — value log가 가득 차면 target Ln-1 선택해 value log의 value들을 새 Ln의 data segment group으로 옮겨 log 공간 확보. compaction 시 KV는 hash value 기준 재정렬, value는 현 위치(group 또는 log)에서 읽음.
> - **GC:** 한 레벨의 data segment group은 같은 block에 모이는 경향이 있어 함께 재배치 → 대부분 victim block에 valid data가 없어 곧바로 erase, GC 오버헤드 낮음. value log의 free block 회수는 log-triggered compaction이 담당.
> - **Range query:** group 내 KV는 hash 기준 정렬이라 key 기준 재정렬 비용이 큼 → 각 KV의 {target page, page offset}를 key 기준 정렬해 group 첫 page에 추가 저장(전체 group 크기의 1% 미만)해 빠른 scan 지원.

> [!abstract]- AnyKey+ : compaction chain 제거 (Figure 9, p.9-10)
> log-triggered compaction 중 다량 value가 destination level로 병합되면 그 레벨이 임계 초과 → 연속적 tree-triggered compaction("compaction chain") 유발해 high-v/k 및 일부 low-v/k에서 IOPS 저하. **Modified log-triggered compaction:** 병합 중 destination level 크기를 모니터링해 threshold×α(0≤α≤1) 도달 시 나머지 value는 value log로 되돌려 씀. 또한 target level을 "value log 내 invalid value가 가장 많은" 레벨로 선택해 chain 회피와 log 공간 확보를 동시에 달성.

## 평가

> [!example]- 실험 설정 및 주요 수치 (p.10-13)
> - **Testbed:** FEMU flash emulator + uNVMe driver, FIO plugin으로 host SW 스택 우회. PinK 소스를 포팅해 AnyKey/AnyKey+ 구현. 코드 공개: https://github.com/chanyoung/kvemu (p.10).
> - **Device:** 64GB SSD, 8 channel × 8 chip, page 8KB, DRAM 64MB(SSD 용량의 0.1%). flash read/write/erase = 56.5/77.5/106 μs, (0.8, 2.2, 5.7) ms. 1.2GHz 16-core. 별도로 4TB SSD + 4GB DRAM도 평가 (p.10).
> - **Workloads:** 14개 KV 워크로드(Table 2). high-v/k(KVSSD 16/4096, YCSB 20/1000, W-PinK 32/1024)와 low-v/k(예: RTDATA 24/10, ZippyDB 48/43, Crypto1 76/50 등). write 비율 기본 20%, Zipfian θ=0.99 (p.11).
> - **Tail latency / IOPS (p.2, p.11-12):** AnyKey는 PinK 대비 low-v/k 11개에서 95th percentile tail latency 56%, IOPS 299% 개선. AnyKey+는 PinK 대비 low-v/k 315%, high-v/k 15% IOPS 향상.
> - **Metadata (Table 1, p.9):** 64GB SSD full 기준 PinK는 v/k 1.0(80B/80B)에서 metadata 합 703MB(level list 200MB + meta segment 503MB)인데 AnyKey는 64MB(level list 38MB + hash list 26MB)로 DRAM에 적합. v/k 2.0에서 PinK 531MB vs AnyKey 64MB.
> - **Flash 접근 (Figure 11b, p.12):** AnyKey는 대부분 read당 flash 접근 최대 2회.
> - **Compaction/GC (Table 3, p.12):** Crypto1(low-v/k) compaction read/write — PinK 85M/88M vs AnyKey 49M/26M vs AnyKey+ 49M/26M. GC는 AnyKey/AnyKey+ 거의 0.
> - **Device lifetime (Figure 13, p.13):** AnyKey+는 PinK 대비 page write 평균 50% 감소.
> - **Computation overhead (p.10):** 40B key의 32-bit xxHash 생성 79ns, 8,192 KV entry 두 group 병합 정렬 118μs (1.2GHz ARM Cortex-A53), flash latency 대비 무시 가능.
> - **Sensitivity:** DRAM 32→96MB, flash page 4→16KB(클수록 tail 단축), value log 3.2→9.6GB, key 분포 θ 변화 등 분석. 4TB SSD에서는 AnyKey metadata 3.65GB vs PinK 25.2GB (p.13-14).
> - **Multi-workload:** 2-워크로드(W-Pink + ZippyDB) 파티션 시 PinK 대비 99.9th percentile tail latency 각 14%, 216% 개선 (p.14).

## 섹션 노트
- **§1 Introduction:** KV-SSD 이점/현황, high-v/k 편향 문제 제기, 기여 요약(low-v/k 워크로드 발굴, AnyKey, AnyKey+).
- **§2 Preliminaries & Related Work:** KV store 실행 모델 3종(Host+Block / Host+SSD support / KV-SSD), KV-SSD 하드웨어 구조, hash-based vs LSM-tree-based KV-SSD 비교.
- **§3 Re-evaluation of Current Design:** PinK 구조(data/meta segment, level list)와 동작, low-v/k에서 metadata 폭증 분석(Figure 5).
- **§4 Proposed Design:** 메타데이터 최소화(data segment group + hash), flash 접근 추가 차단(hash list), value log, 상세 동작(read/write/compaction/GC/range), metadata/overhead 분석, AnyKey+ 강화.
- **§5 Evaluation:** 방법론, tail latency/IOPS, compaction/GC, lifetime/storage utilization, sensitivity, key distribution, range query, value log 크기, scalability, multi-workload.
- **§6 Conclusions:** low-v/k 워크로드 존재와 메타데이터 최소화 원리 요약.
- **Appendix A:** Artifact(FEMU 기반 재현 절차, 스크립트, 워크로드 로딩 단계).

## 핵심 용어
- **KV-SSD (Key-Value SSD):** KV store를 SSD device 내부에 통합해 KV 쌍을 device에 직접 저장/조회하는 SSD. host SW 스택 우회로 latency/scalability 이점.
- **value-to-key ratio (v/k):** value 크기 / key 크기 비율. high-v/k는 value가 큰 typical 워크로드, low-v/k는 key가 상대적으로 큰 미탐구 워크로드.
- **PinK:** SOTA LSM-tree 기반 KV-SSD. bounded tail latency를 위해 LSM-tree 상위 레벨 메타데이터를 DRAM에 유지. 본 논문의 비교 대상.
- **metadata (KV-SSD 맥락):** flash 내 KV 쌍 위치(PPA)를 찾기 위한 자료구조. PinK에서는 meta segment + level list.
- **data segment group:** AnyKey의 핵심. 여러 flash page를 묶어 100~5,000개 KV를 hash value 기준 정렬해 저장하는 단위.
- **level list:** DRAM 상의 group 메타데이터. 가장 작은 key, 첫 page PPA, page별 첫 key의 hash value 리스트로 구성.
- **hash list:** 각 level list entry가 가진, group에 실존하는 key들의 hash value 집합. 헛된 flash 접근 차단용 (Bloom/Cuckoo filter 대신 정수 배열 + binary search).
- **value log:** value를 key와 분리 저장하는 flash 영역(WiscKey 영감). compaction 효율화·write-intensive 대응.
- **compaction chain:** log-triggered compaction이 destination level을 임계 초과시켜 연속적 tree-triggered compaction을 유발하는 현상. AnyKey+가 제거.
- **AnyKey+:** modified log-triggered compaction으로 compaction chain을 막아 high-v/k까지 개선한 강화판.

## 강점 · 한계 · 열린 질문
- **강점:** (1) 단순한 핵심 아이디어(group화 + hash 식별)로 metadata를 DRAM에 맞게 급감. (2) 기존 high-v/k 성능을 해치지 않고 low-v/k까지 커버, AnyKey+는 모든 유형에서 우위. (3) 실제 워크로드 14종 발굴 및 광범위 sensitivity 분석. (4) GC/lifetime/storage utilization 등 부수 이점까지 정량화. (5) 코드 공개로 재현성 확보.
- **한계:** (1) FEMU 에뮬레이터 기반 평가(실제 ASIC/FW 미검증), 64GB 소형 SSD 가정(4TB는 부분 외삽). (2) hash value 사용으로 range query는 별도 정렬 정보 저장 필요(group 1% 오버헤드). (3) value log의 값이 갱신 없이 머물면 read error 가능(write-intensive 가정에 의존). (4) 비교 대상이 사실상 PinK 단일 라인.
- **열린 질문:** read-mostly low-v/k(value 갱신 드문 경우) value log 안정성은? hash collision bit 처리(0.075%)가 worst-case에서 tail에 미치는 영향은? multi-workload 파티셔닝의 동적 자원 분배는?

## ❓ Q&A (자가 점검)

> [!question]- Q1. low-v/k 워크로드에서 PinK 성능이 무너지는 근본 원인은?
> > KV마다 필요한 metadata(key+PPA)가 key가 상대적으로 커지면서 폭증해 device-internal DRAM에 다 담기지 못하고 flash로 흘러넘친다. 결과적으로 target KV를 읽기 전 level list/meta segment를 flash에서 여러 번 읽어야 해 read latency·IOPS가 급격히 악화된다 (v/k 1280/40→20/40에서 95th latency 891μs→8,904μs, IOPS −94%).

> [!question]- Q2. AnyKey가 metadata 크기를 줄이는 두 가지 핵심 기법은?
> > (1) 여러 KV를 정렬된 "data segment group"으로 묶어 KV 단위가 아닌 group 단위로 메타데이터를 유지. (2) group 내 정렬·식별에 큰 key 대신 32-bit hash value를 사용해 메타데이터를 더 축소.

> [!question]- Q3. hash list는 왜 필요하며 어떻게 동작하나?
> > level list는 키 "범위"만 제공하고 인접 레벨 범위가 겹쳐, target KV가 실제로 없는 group을 읽는 헛수고가 생긴다. hash list는 남는 DRAM에 group에 실존하는 key들의 hash value를 저장해, flash page를 읽기 전 존재 여부를 확인함으로써 불필요한 flash 접근을 차단한다.

> [!question]- Q4. AnyKey가 read당 flash 접근을 최대 2회로 제한할 수 있는 이유는?
> > level list(키 범위) + hash list(존재 확인) + level list 내 page별 첫 key hash로 정확한 flash page를 한 번에 특정한다. KV entry가 value를 직접 가지면 1회, value가 value log에 있으면 entry용 1회 + value용 1회로 끝난다.

> [!question]- Q5. value log를 도입한 목적과 그것이 야기하는 문제는?
> > 목적은 value를 key와 분리해 compaction 시 value 이동을 막고 write-intensive 워크로드의 compaction 효율을 높이는 것이다. 문제는 log-triggered compaction이 destination level을 임계 초과시켜 연속적 tree-triggered compaction("compaction chain")을 유발해 IOPS를 떨어뜨릴 수 있다는 점이다.

> [!question]- Q6. AnyKey+는 compaction chain을 어떻게 제거하나?
> > modified log-triggered compaction에서 destination level 크기를 모니터링해 threshold×α 도달 시 나머지 value는 value log로 되돌려 쓰고, target level을 "value log 내 invalid value가 가장 많은" 레벨로 선택한다. 이로써 chain 형성을 막으면서도 log 공간을 확보한다.

> [!question]- Q7. AnyKey/AnyKey+의 정량 성능 이득은?
> > AnyKey는 PinK 대비 low-v/k 11개에서 95th percentile tail latency 56%·IOPS 299% 개선. AnyKey+는 PinK 대비 low-v/k 315%·high-v/k 15% IOPS 향상, page write는 평균 50% 감소.

> [!question]- Q8. range query는 hash 정렬과 어떻게 양립하나?
> > group 내 KV가 hash value로 정렬돼 key 기준 scan이 어렵다. 이를 위해 각 KV의 {target page, page offset}를 key 기준으로 정렬해 group 첫 page에 추가 저장(group 크기의 1% 미만)함으로써, 정렬 없이도 range query 대상 KV를 빠르게 찾는다.

## 🔗 Connections
[[KV-LSM]] · [[ASPLOS]] · [[2025]]

## References worth following
- **PinK: High-speed In-storage Key-value Store with Bounded Tails** (Jin et al., USENIX ATC 2020 / TOS 2021) — 본 논문의 직접 비교 대상이자 baseline LSM-tree KV-SSD (ref [30,31]).
- **WiscKey: Separating keys from values in ssd-conscious storage** (Lu et al., FAST 2016 / TOS 2017) — value log(key-value 분리) 아이디어의 출처 (ref [49]).
- **iLSM-SSD: An intelligent LSM-tree based key-value SSD for data analytics** (Kang et al., MASCOTS 2019) — 동일 계열 LSM-tree KV-SSD 설계 (ref [43]).
- **Lightstore: Software-defined network-attached key-value drives** (Chung et al., ASPLOS 2019) — KV-SSD 설계 비교군 (ref [13]).
- **Towards building a high-performance, scale-in key-value storage system (KV-SSD)** (Kang et al., SYSTOR 2019) — Samsung KV-SSD, hash 기반 설계 (ref [36]).
- **The log-structured merge-tree (LSM-tree)** (O'Neil et al., Acta Informatica 1996) — LSM-tree 원전 (ref [52]).

## Personal annotations
<본인 메모 영역>
