---
title: "Ethane: An Asymmetric File System for Disaggregated Persistent Memory"
aliases: [Ethane]
description: "자원 비대칭(CN 강한 연산 / MN 큰 메모리) DPM에서 파일시스템을 control-plane FS와 data-plane FS로 분리해 PM의 성능을 끌어내고 비용을 낮춘 비대칭 파일시스템."
venue: ATC
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/persistent-memory
  - topic/disaggregation
  - topic/filesystem
  - venue/atc
  - year/2024
---

# Ethane: An Asymmetric File System for Disaggregated Persistent Memory

> **ATC 2024** · `cluster/fs` · Source: [Ethane - An Asymmetric File System for Disaggregated Persistent Memory.pdf](Ethane%20-%20An%20Asymmetric%20File%20System%20for%20Disaggregated%20Persistent%20Memory.pdf)

저자: Miao Cai (Nanjing University of Aeronautics and Astronautics), Junru Shen (Hohai University), Baoliu Ye (State Key Laboratory for Novel Software Technology, Nanjing University)

## TL;DR
기존 distributed file system은 PM을 monolithic server마다 꽂아 쓰는 **symmetric PM** 구조라 (1) 비싼 cross-node interaction, (2) 약한 single-node 성능, (3) 비싼 scale-out이라는 세 가지 연쇄적 성능·비용 문제를 만든다. Ethane은 PM을 별도 memory node(MN) 풀로 **disaggregate(DPM)** 하고, DPM 고유의 **자원 비대칭**(CN은 연산 강함·DRAM 작음, MN은 PM 큼·연산 약함)에 맞춰 파일시스템을 **control-plane FS**(CN에서 동작, shared log 기반 메타·일관성 제어)와 **data-plane FS**(MN 위 shared KV store, 병렬·dependence-disentangled 데이터 경로)로 쪼갠다. 결과적으로 기존 DFS 대비 최대 68배 성능, 최대 1.71배 낮은 비용, data-intensive 애플리케이션 최대 17배 향상.

## 문제 & 동기
Killer microsecond 시대의 data center는 ~300ns latency, 큰 bandwidth의 PM을 distributed file system의 storage backend로 활발히 쓴다. 그러나 현재의 PM 사용은 모든 server에 CPU+PM을 함께 꽂는 **symmetric** 방식이고, 이 구조가 세 가지 연쇄 문제를 일으킨다.

- **비싼 cross-node interaction**: data가 여러 node에 흩어져 있어 path resolution 등에서 잦은 network round trip + 직렬화·역직렬화·여러 storage layer 복사가 발생. CephFS path resolution이 ~162μs, syscall 전체의 60.24%(6-component path에서는 interaction이 91.71%)를 차지.
- **약한 single-node 성능**: 제조 제약으로 한 머신은 few PM DIMM만 → 용량·bandwidth 상한. 256B IO에서 writer 4개면 bandwidth가 포화. skewed access시 hot file을 가진 node가 bottleneck이 되고 다른 node의 PM은 underutilized (throughput 최대 2배까지 하락).
- **비싼 scale-out**: 용량/성능을 늘리려면 PM 머신 자체를 사야 하므로 encapsulated processor·peripheral 비용까지 지불 → TCO 급증. Hadoop 같은 elastic resource scaling이 비효율적.

> [!quote]- 📄 원문 표현 (paper)
> "This paper examines and reveals a cascade of three performance and cost issues in the current PM provision scheme, namely expensive cross-node interaction, weak single-node capability, and costly scale-out performance, which not only underutilizes fast PM devices but also magnifies its limited storage capacity and high price deficiencies." (Abstract, p.1)
>
> "An RDMA-CAS/RDMA-READ and RDMA-WRITE latency is 4.8 μs and 3.2 μs." (§5.1, p.9)
>
> "a complete interaction includes costly data movements, encapsulation, de-/serialization and passes through several storage layers, which takes ~162 μs and occupies 60.24% of the component resolution time." (§2.1, p.3)

## 핵심 통찰

> [!note]- 통찰 1 — DPM의 "자원 비대칭"을 그대로 파일시스템 구조에 투영하라
> Disaggregated PM은 CN(강한 연산, 작은 DRAM)과 MN(큰 PM 풀, 약한 연산 ARM SoC/ASIC)으로 자원이 비대칭이다. 기존 DFS의 "separate object management"(meta/data object를 다른 node에 분산 저장·조작)는 symmetric PM 전제이고 DPM엔 안 맞는다 — CN도 MN도 object storage/manipulation에 충분한 PM·CPU가 없기 때문. 그래서 "FS object manipulation을 storage에서 분리"하는 원칙으로, 복잡한 제어 로직은 CN의 control-plane FS에, 저장은 MN의 data-plane FS에 둔다.

> [!note]- 통찰 2 — Linearizable syscall이 곧 shared log ordering 문제다
> MN이 모든 CN에 centralized view를 제공하므로, control-plane FS를 shared log abstraction 위에 올리면 concurrency control·crash consistency가 "log ordering 문제"로 환원된다. 단, linearizable syscall을 위해 mlog를 strict하게 append할 필요는 없다는 통찰이 핵심 — naive RDMA-CAS append는 RNIC contention으로 scalability bottleneck이 되므로 arena 기반 relaxed insertion으로 valid log order만 보장한다.

> [!note]- 통찰 3 — data path를 data access와 data processing으로 disentangle하면 DPM bandwidth를 포화시킬 수 있다
> 전통 FS의 path resolution은 dentry fetch(data access)와 permission check 등(data processing)이 직렬로 얽혀 있어 RDMA latency가 그대로 누적되고 aggregated bandwidth를 낭비한다. vectorized batched lookup으로 data access를 data processing에서 떼어내 pipeline하면, remote memory access와 연산을 overlap해 DPM의 parallel bandwidth를 활용할 수 있다.

> [!note]- 통찰 4 — extent tree 대신 data section으로 pointer chasing 제거
> 큰 file일수록 extent tree 깊이가 깊어 file mapping시 pointer chasing이 폭증한다(64GB read시 5.41회). 고정 크기(1GB/2MB/4KB) data section + concatenation key로 file mapping을 hash lookup으로 바꾸면 file 크기와 무관하게 항상 2회 pointer chasing.

## 설계 / 메커니즘

> [!abstract]- 전체 아키텍처: control plane / data plane 분리 (§3, p.4)
> - **Control-plane FS = cacheFS 집합 (CN에서 동작)**: 각 cacheFS는 작은 local DRAM에 volatile·partial state를 캐시(namespace cache = chain-based hash table, block cache = AVL tree). namespace query, crash consistency, concurrency control 같은 복잡한 object manipulation 담당. local state는 작고 휘발성이며 remote sharedFS에서 재구성 가능.
> - **Data-plane FS = sharedFS (MN 위 atop centralized)**: PB급 PM 모듈 풀을 RDMA/CXL로 묶은 shared 저장 계층. 모든 disjoint PM 모듈에 data를 shard·병렬화. KV 기반 unified storage paradigm.
> - object를 meta object와 data object로 분류하고, FS object manipulation을 storage에서 분리한다는 원칙으로 두 plane을 나눔.

> [!abstract]- Control-plane FS: shared log 3단 위임 (§4.1, p.5-7)
> **(1) Durability를 Log Persistence에 위임**: syscall마다 oplog = dlog(data log: opcode, path, credential, meta object addr, reuse field) + mlog(meta log: 8B에 12bit CID, 2B fingerprint, 26bit dlog offset, 9bit size, 1bit flag pack). cacheFS가 RDMA_WRITE로 dlog를 쓰고, 같은 QP에 RDMA_READ를 issue해 PCIe buffer를 flush → 두 RDMA를 동시 발행해 durability 보장.
> **(2) Linearizability를 Log Ordering에 위임**: mlog region을 **arena**(여러 slot)로 분할. arena insertion = (1) RDMA_CAS로 빈 slot에 mlog insert+persist, (2) 앞선 빈 slot 채우기. linearizable syscall이 linearizable mlog append를 요구하지 않는다는 통찰로, concurrent cacheFS의 mlog 순서 제약을 풀어 contention을 줄임. 빈 slot은 pseudo mlog(null op)로 채워 valid log order 유지(Figure 3b의 case I/II/III). 최적화: arena slot 수 < concurrent thread 수, step(2)를 log playback 후 수행.
> **(3) Coherence를 Log Playback에 위임**: file system interface는 non-nilext이므로 syscall이 effect를 즉시 외부화해야 함 → local state를 최신으로 forward 후 local 실행. 두 기법으로 playback 가속:
> - *file-lineage 기반 log dependence check*: file f의 lineage(f의 path prefix가 되는 file 집합)로 dependent oplog만 replay. direct/step(rename)/remote(symlink) lineage 구분(Figure 4). 각 file의 mlog skip table로 playback range 축소, path fingerprint 비교로 dependence 판정.
> - *collaborative log playback*: 거의 모든 metadata syscall은 file path walk(긴 부분) + 최종 modification으로 구성. 다른 cacheFS가 이미 replay한 path walk 결과를 reuse field로 재사용해 partial replay.

> [!abstract]- Data-plane FS: KV storage paradigm + data path disentanglement (§4.2, p.7-8)
> **Data Storage Paradigm**: 모든 FS object를 unified KV tuple로. meta object key = (meta object addr + parent dir meta object addr), value = 파일 meta(type, size, full path). hard link는 target meta object 포인터, symlink는 독립 meta object. data section(고정 1GB/2MB/4KB) key = (메모리 addr + section start + section size), value = backward extent 포인터. **cuckoo hash table**(constant slot probe → 병렬 lookup 설계 용이)로 KV 관리, 모든 MN에 stripe.
> **Access interface**: vectorized `vec_kv_get(key_t *k_vec, val_t *v_vec)` — batched key를 한 번에 lookup. approximate(hash collision 가능값 다 반환)라서 caller가 CN에서 filter. cuckoo의 two-probe·no-dependence 특성으로 computation과 one-sided RDMA_READ를 overlap한 **parallel, pipelined hash lookup**. optimistic concurrency control(version number, lock-free reader).
> **Data Path Disentanglement**: file path walk을 batch of dentry lookup으로 분해해 vec_key_get으로 병렬 fetch + sanity check 후 정확한 meta object만 비교(Figure 7). file data read는 Algorithm 1처럼 file mapping(batched data section lookup으로 extent 찾기)과 parallel data block read로 분리.
> **Log Ingestion**: sharedFS가 shared log를 두 phase로 ingest. (1) CN의 log ingestion worker N개가 fingerprint%N==i로 dependent log를 같은 worker에 모아 replay 후 vector put. (2) sharedFS가 KV tuple로 생성/삽입/갱신.

> [!abstract]- 구현 (§4.3, p.8)
> 밑바닥부터 prototype, 총 10910 LoC C. CN은 Linux OS로 POSIX 호환 인터페이스·자원 관리·데이터 보호 제공. cacheFS는 user-level library(4922 LoC). MN은 연산이 약해 full OS 못 돌리므로 thin sharedFS daemon(PM pool 관리 + log GC)만 실행. ZooKeeper로 namespace 관리·config 정보. 소스: github.com/miaogecm/Ethane.git

## 평가

> [!success]- 실험 환경 & 비교 대상 (§5, p.9)
> 4-blade rack, 각 blade는 2 NUMA, Intel Xeon Gold 5220 2.20GHz, 128GB DDR4, 512GB(4×128GB) Intel Optane DCPMM, 512GB Samsung PM981 NVMe SSD, 2× Mellanox ConnectX-6 100GbE NIC, Ubuntu 18.04, 100Gb 스위치. DPM은 2 CN + 2 MN, symmetric은 4 SN. 비교: CephFS(BlueStore OSD backend, 4 MDS+4 OSD), Octopus, Assise.

> [!success]- Control-plane FS 결과 (§5.1, p.9-10)
> - **Log persistence**: 8B mlog insert+persist 5.48μs, small/medium dlog 4.32μs, 4096B dlog 7.46μs (RDMA_CAS/READ 4.8μs + RDMA_WRITE 3.2μs를 병렬 발행).
> - **Log arena scalability**: Log+Arena가 RDMA_CAS 기반 Log+CAS보다 훨씬 우수, client 수에 따라 선형 증가(Figure 8a). RDMA_CAS 횟수가 적어 PCIe data transfer rate(PMWatch ddrt_write_ops)도 Log+Arena가 최대 3배 높음(Figure 9).
> - **Log replay**: baseline 5 Kops/s → +DepCheck 최대 1.7 Mops/s, +Reuse(collaborative playback)가 평균 42.21% 추가 향상(Figure 8b).

> [!success]- Data-plane FS 결과 (§5.2, p.10)
> - **Path walk latency**: Seq-Walk는 non-scalable(9-component 263μs로 1-component의 3.09배). Para-Walk가 receiver NIC IOPS 최대 2.43배 높음(Figure 8c).
> - **Data read throughput**: Disent-Read가 Ent-Read보다, 특히 큰 file에서 우수 — 64GB file read시 5.76배 throughput(Figure 8d). Ent-Read는 64GB read에서 pointer chasing 5.41회, file mapping이 전체 시간의 86.7% 차지. Disent-Read는 file 크기 무관 2회, file mapping 28.2~30.5%(Table 2).

> [!success]- Macrobenchmark & 비용 (§5.3, p.10-11)
> - **mkdir latency**: CephFS는 800μs 초과, Octopus ~60μs, Assise는 Ethane보다 평균 31.27% 높음(client-local NVM). Ethane은 K 작을 때 Assise와 유사, log playback latency가 효과적으로 줄어 Assise 대비 최대 33.54% 우수(Figure 10a).
> - **Metadata scalability (MDTest)**: creat/unlink에서 모든 DFS 압도. Octopus는 worker 하나라 48 client에서 throughput 정체(Figure 11).
> - **Fio throughput**: read는 Assise가 client-local NVM cache로 강하지만 large IO서 PM 한계. write peak — CephFS 0.63 GB/s, Octopus 4.4 GB/s, Ethane은 parallel data path로 선형 확장해 **peak 15.52 GB/s**(Figure 12).
> - **Cost efficiency (filebench, 1GB video)**: Ethane이 MB/s per dollar 최고 — PM이 부족할 때 다른 FS는 머신을 통째로 사야 하지만 Ethane은 PM DIMM만 추가. 2 DCPMM $838 vs SN 머신(2 DCPMM 포함) $3789(Figure 13).

> [!success]- Application 성능 (§5.4, p.11)
> - **Redis cluster**(16 shard, 1억 key): AOF throughput이 Octopus/CephFS/Assise 대비 각각 6.77×/17.98×/41.55× 높음. RDB(10GB) latency는 27.56×/8.21×/4.71× 낮음.
> - **Metis (MapReduce WordCount, 16GB)**: symmetric은 SN 2개(연산 over-provision), Ethane은 0.5 CN + 1.5 MN로 같은 비용. Ethane이 IO phase latency 짧아 동일 hardware 비용에서 우월.

## 섹션 노트
- §1 Introduction: 세 contribution — (a) 현 PM 사용의 세 문제 폭로 + 비대칭 FS 제안, (b) shared log 기반 control-plane FS로 functionality delegation, (c) KV store + dependence-disentangled data-plane FS. Ethane(C₂H₆)은 RDMA로 CN/MN 연결된 DPM과 구조식이 닮은 화합물 이름에서 따옴(각주, p.1).
- §2 Background: §2.1 symmetric PM의 세 문제 정량화(CephFS interaction 162μs/60.24%, Octopus load imbalance), §2.2 DPM의 resource asymmetry 소개.
- §3 Asymmetric FS Architecture: separate object management(symmetric용)가 DPM에 부적합 → FS object manipulation을 storage에서 분리. shared-log control plane + access-disentangled data plane.
- §4 Design: §4.1 control-plane(durability/linearizability/coherence 3위임), §4.2 data-plane(KV paradigm, disentanglement, log ingestion), §4.3 구현.
- §5 Evaluation: 세 질문 — DPM friendly한가, 기존 DFS보다 나은가, 실제 앱에서 어떤가.
- §6 Related Works: DFS(GFS/HDFS/CephFS/GIGA+/IndexFS/HopsFS/QFS), PM FS(NOVA/ctFS/SplitFS/NVFS/SingularFS), Octopus/Orion/Assise, DPM(LegoOS/Clio/CXL pooling). Ethane은 비대칭 아키텍처로 DPM 잠재력을 푸는 첫 file system이라 주장.
- §7 Conclusion: PM 사용 재검토 → 세 문제 → DPM 기반 비대칭 FS Ethane으로 data-intensive 앱에서 더 낮은 비용에 더 나은 성능.

## 핵심 용어
- **DPM (Disaggregated Persistent Memory)**: CN(compute node)과 MN(memory node)을 fast connectivity(RDMA/CXL)로 분리·집약한 구조. 두 자원을 독립적으로 scaling 가능.
- **Resource asymmetry**: CN은 강한 연산·작은 DRAM, MN은 큰 PM 풀·약한 연산(ARM SoC/ASIC)이라는 DPM 고유 특성. Ethane 설계의 출발점.
- **Symmetric PM architecture**: 모든 monolithic server에 CPU+PM을 함께 둔 egalitarian 구조. 세 가지 성능·비용 문제의 근원.
- **cacheFS**: control-plane FS의 단위. CN에서 동작하는 user-level FS로 작은 local DRAM에 volatile·partial state 캐시.
- **sharedFS**: data-plane FS. MN 위 centralized shared 저장 계층, KV 기반.
- **oplog (dlog + mlog)**: syscall의 operation log. dlog는 data log(opcode/path/credential/addr/reuse), mlog는 8B meta log(global order array에 기록).
- **arena**: mlog region을 나눈 slot 묶음. relaxed insertion으로 RDMA_CAS contention을 줄이면서 valid log order 보장.
- **pseudo mlog**: arena의 빈 slot을 채우는 null operation log. linearizability 위반 방지.
- **file lineage**: file f의 path prefix가 되는 file/dir 집합. direct/step(rename)/remote(symlink) lineage로 log dependence 판정.
- **collaborative log playback**: 다른 cacheFS의 path walk 결과(reuse field)를 재사용해 partial replay하는 가속 기법.
- **data section**: 고정 크기(1GB/2MB/4KB) logical linear space 단위. extent tree 대신 hash lookup으로 file mapping해 pointer chasing 제거.
- **vec_kv_get / Para-Walk**: vectorized batched cuckoo hash lookup 인터페이스. data access를 data processing에서 분리해 RDMA latency를 hide하고 aggregated bandwidth 활용.
- **non-nilext interface**: syscall이 effect를 즉시 외부화해야 하는 file system 인터페이스 속성. coherence를 위해 log playback이 syscall 경로에 나타나게 만듦.

## 강점 · 한계 · 열린 질문
- **강점**: DPM의 자원 비대칭을 control/data plane 분리라는 깔끔한 추상으로 매핑. shared log로 metadata 제어를, KV+disentangled path로 data를 각각 최적화한 일관된 설계. write/Redis/MapReduce 등 광범위한 평가와 cost-per-dollar까지 정량화. ATC 2024 artifact evaluated(available/functional/reproduced).
- **한계**: 평가가 emulation platform(4-blade rack, DCPMM, RDMA over ConnectX-6)이고 실제 CXL 기반 DPM은 아님 — CXL latency/coherence 특성에서의 검증은 future work. MN의 thin daemon만으로 log GC·PM 관리를 감당할 때의 연산 한계, 대규모 MN 풀 확장성은 제한적으로만 다룸. ZooKeeper 의존(namespace/config)이 단일 의존점·확장성 영향 미검토.
- **열린 질문**: arena slot 수 튜닝(< concurrent thread)이 워크로드 변동에 얼마나 robust한가? file lineage dependence check이 깊은/넓은 directory tree에서 fingerprint collision로 인한 over-replay를 얼마나 유발하나? CXL.mem coherent 접근이 가능해지면 one-sided RDMA 기반 설계 중 어떤 부분이 단순화될까?

## ❓ Q&A (자가 점검)

> [!question]- Q1. Symmetric PM 구조가 일으키는 세 가지 연쇄 문제는?
> > (1) expensive cross-node interaction — data가 흩어져 잦은 round trip·직렬화·복사(CephFS path resolution 162μs/60.24%), (2) weak single-node capability — few PM DIMM로 용량·bandwidth 상한, skew시 hot node bottleneck, (3) costly scale-out — PM 늘리려면 머신 통째 구매로 TCO 급증.

> [!question]- Q2. Ethane이 파일시스템을 control-plane과 data-plane으로 나눈 근거는?
> > DPM의 resource asymmetry — CN은 연산 강·DRAM 작고 MN은 PM 크고 연산 약하다. CN도 MN도 object storage/manipulation에 충분한 자원이 없어 기존의 separate object management가 안 맞으므로, 복잡한 제어(control-plane FS=cacheFS)를 강한 CN에, 저장(data-plane FS=sharedFS)을 큰 MN에 둔다.

> [!question]- Q3. naive RDMA-CAS로 mlog를 append하지 않고 arena를 쓰는 이유는?
> > strict order를 RDMA-CAS로 강제하면 RNIC이 내부 lock으로 concurrent CAS를 직렬화해 mlog list tail이 severe scalability bottleneck이 된다. linearizable syscall은 linearizable mlog append를 요구하지 않는다는 통찰로, arena의 빈 slot에 relaxed insert하고 pseudo mlog로 빈 자리를 채워 valid log order만 보장 → contention 대폭 감소.

> [!question]- Q4. log playback을 빠르게 하는 두 기법은?
> > (1) file-lineage 기반 log dependence check — target file의 lineage(path prefix file 집합)에 속한 oplog만 path fingerprint 비교로 골라 replay, mlog skip table로 range 축소. (2) collaborative log playback — 거의 모든 metadata syscall이 공유하는 긴 path walk 결과를 다른 cacheFS가 reuse field로 재사용해 partial replay. 평균 42.21% 추가 향상.

> [!question]- Q5. extent tree 대신 data section을 쓰면 무엇이 좋아지나?
> > extent tree는 file이 클수록 깊어져 file mapping시 pointer chasing이 폭증한다(64GB read 5.41회, file mapping이 전체의 86.7%). 고정 크기 data section + concatenation key의 hash lookup으로 바꾸면 file 크기와 무관하게 항상 2회 pointer chasing → 64GB read에서 Disent-Read가 5.76배 throughput.

> [!question]- Q6. data path disentanglement이 정확히 무엇을 분리하나?
> > 전통 path resolution에서 직렬로 얽힌 dentry fetch(data access)와 permission check 등(data processing)을 분리한다. vectorized batched cuckoo lookup(vec_kv_get)으로 여러 component를 병렬·dependence-free하게 fetch하고 sanity check 후 비교, RDMA latency를 computation과 overlap해 DPM의 aggregated bandwidth를 포화시킴.

> [!question]- Q7. data-plane FS가 vec_kv_get을 approximate(부정확)하게 둔 이유는?
> > hash collision 때문에 lookup이 key validation을 요구하는데, DPM bandwidth를 포화시키기 위해 validation을 미루고 가능한 후보값을 한 번에 다 반환한다. caller(CN)가 강한 연산으로 나중에 filter — 연산은 CN, 저장·접근은 MN이라는 비대칭 원칙과 일치.

> [!question]- Q8. Ethane이 비용 면에서 우월한 핵심 이유는?
> > PM이 부족해질 때 symmetric FS는 CPU·peripheral까지 포함된 머신을 통째로 사야 하지만(2 DCPMM 포함 SN $3789), Ethane은 MN에 PM DIMM만 추가하면 된다(2 DCPMM $838). disentangled data path로 PM utilization도 높아 MB/s per dollar가 최고.

## 🔗 Connections
[[File System]] · [[ATC]] · [[2024]]

## References worth following
- [49] Lu et al., **Octopus: an RDMA-enabled Distributed Persistent Memory File System** (ATC 2017) — shared PM pool 추상의 선행 연구, Ethane의 주요 비교 대상.
- [12] Anderson et al., **Assise: Performance and Availability via Client-local NVM in a Distributed File System** (OSDI 2020) — client-local PM 접근, Ethane의 latency 비교 baseline.
- [14][15] Balakrishnan et al., **CORFU / Tango: shared log & distributed data structures over a shared log** (NSDI 2012 / SOSP 2013) — control-plane FS의 shared log abstraction 토대.
- [69] Weil et al., **Ceph: A Scalable, High-Performance Distributed File System** (OSDI 2006) — symmetric PM 문제 정량화의 대상, namespace tree partitioning.
- [34] Guo et al., **Clio: a Hardware-software Co-designed Disaggregated Memory System** (ASPLOS 2022) / [61] Shan et al., **LegoOS** (OSDI 2018) — DPM/resource disaggregation 아키텍처 맥락.
- [53] Neal et al., **Rethinking File Mapping for Persistent Memory** (FAST 2021) — data section 기반 file mapping 설계의 배경.

## Personal annotations
<본인 메모 영역>
