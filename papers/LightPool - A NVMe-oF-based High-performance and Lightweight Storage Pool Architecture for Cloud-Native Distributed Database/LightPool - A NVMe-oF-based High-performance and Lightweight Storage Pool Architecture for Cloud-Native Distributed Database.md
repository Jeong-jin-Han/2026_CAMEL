---
title: "LightPool: A NVMe-oF-based High-performance and Lightweight Storage Pool Architecture for Cloud-Native Distributed Database"
aliases: [LightPool]
description: "클러스터 로컬 NVMe SSD를 NVMe-oF 스토리지 풀로 통합해 OceanBase 활용률을 40→65%로 끌어올린 Alibaba의 경량 스토리지 풀 아키텍처(HPCA 2024)"
venue: HPCA
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/infra
  - topic/nvme-of
  - topic/storage-pool
  - topic/disaggregation
  - venue/hpca
  - year/2024
---

# LightPool: A NVMe-oF-based High-performance and Lightweight Storage Pool Architecture for Cloud-Native Distributed Database

> **HPCA 2024** · `cluster/infra` · Source: [LightPool - A NVMe-oF-based High-performance and Lightweight Storage Pool Architecture for Cloud-Native Distributed Database.pdf](<LightPool - A NVMe-oF-based High-performance and Lightweight Storage Pool Architecture for Cloud-Native Distributed Database.pdf>)

저자: Jiexiong Xu, Yiquan Chen, Yijing Wang, Wenhui Shi, Guoju Fang, Yi Chen, Huasheng Liao, Yang Wang, Hai Lin, Zhen Jin, Qiang Liu, Wenzhi Chen — Zhejiang University, Alibaba Group, AntGroup

## TL;DR
Cloud-native 분산 DB(OceanBase 등)는 노드별 CPU/메모리/스토리지 비율 불균형 때문에 로컬 NVMe SSD를 평균 40%밖에 못 쓴다. LightPool은 클러스터 로컬 SSD들을 NVMe-oF(NVMe/TCP) 기반 **storage pool**로 묶고 Kubernetes 스케줄러와 통합해 노드 간 스토리지를 공유한다. 경량 user-level I/O stack과 로컬 접근용 **zero-copy** 메커니즘으로 native에 가까운 성능을 내며, 데이터 복제를 두지 않아(DB 레벨 복제에 위임) TCO를 낮춘다. Alibaba 8500+ 노드 production에 배포되어 스토리지 활용률을 40%→65%로 끌어올렸고, 추가 지연은 약 2.1µs에 불과, OpenEBS 대비 대역폭 최대 190.9% 향상.

## 문제 & 동기
Cloud-native 분산 DB 클러스터는 노드마다 고정된 하드웨어 구성(예: OceanBase production의 82/104 CPU core + 12×3.84TB P4510)을 갖는데, 컨테이너 워크로드의 CPU/메모리/스토리지 요구가 제각각이라 한 자원이 소진되면 다른 자원이 남는다. 그 결과 로컬 스토리지 활용률이 평균 40%에 머물러 대규모 자원 낭비와 TCO 증가가 발생한다. 기존 해법인 disaggregated storage(EBS)는 cross-cluster network 병목과 전용 스토리지 서버/네트워크 비용으로 부적합하고, Kubernetes 통합 솔루션 OpenEBS는 데이터 복제를 로컬에 최소 1개 강제(Feature 3와 중복)하고 software stack이 무거워 성능 저하가 크다.

> [!quote]- 📄 원문 표현 (paper)
> "our statistics show that local storage utilization of the OceanBase clusters with hundreds of PB local storage is only around 40%." (p.2)
>
> "Although disaggregated storage (EBS) can enhance storage utilization ... they suffer from performance bottlenecks and high costs." (p.1, Abstract)
>
> "OpenEBS must allocate at least one data replica in local node, which cannot solve the storage resources underutilization challenge." (p.2)

## 핵심 통찰

> [!note]- 핵심 통찰 1 — 스토리지를 풀로 묶어 노드 간 공유하면 활용률을 끌어올릴 수 있다
> 노드별 자원 불균형은 개별 노드 단위로는 해결 불가능하다. 로컬 SSD를 클러스터 단위 storage pool로 aggregate하고 NVMe-oF로 노드 간 공유하면, CPU가 바닥난 노드의 남는 스토리지를 다른 노드의 pod가 쓸 수 있다. 이것이 40%→65% 활용률 향상의 원천이다.

> [!note]- 핵심 통찰 2 — DB가 이미 복제하므로 스토리지 레벨 복제는 중복이다
> Cloud-native 분산 DB는 DB 레벨에서 multiple data replica(P0~P3)를 유지한다. 따라서 스토리지 시스템이 또 복제하면 비용·성능만 낭비된다. LightPool은 **no data replica** 설계로 스토리지 복제를 제거해 TCO와 성능 손실을 동시에 줄인다.

> [!note]- 핵심 통찰 3 — 원격 표준은 NVMe-oF/TCP, 로컬은 zero-copy로 분리 최적화
> 표준 NVMe-oF는 iSCSI 대비 성능 손실 최대 40%·CPU 30% 절감으로 원격 접근에 최적이다. RDMA는 하드웨어 의존으로 대규모 배포가 어려워 TCP를 택한다. 그러나 로컬 접근까지 tcp-loopback을 쓰면 중복 복사 2회가 생기므로, 로컬 경로는 shared memory + IOMMU DMA remapping 기반 zero-copy로 따로 최적화한다.

> [!note]- 핵심 통찰 4 — 가용성은 hot-upgrade(fork)·hot-migration(multipath)으로 무중단 확보
> User-level engine은 fork로 child를 띄워 무중단 업그레이드하고, 데이터 마이그레이션은 multipath 기반 동적 I/O path 전환으로 서비스 중단 없이 수행한다. 이로써 디스크가 user-level 컴포넌트라는 약점을 가용성 메커니즘으로 보완한다.

## 설계 / 메커니즘

> [!abstract]- 전체 아키텍처 & 컴포넌트 (Fig.3, Fig.4)
> LightPool 클러스터는 두 종류 노드로 구성된다. (1) **Kubernetes control plane node**: 핵심 컴포넌트인 **LightPool-Controller**가 클러스터 전체 스토리지의 global view를 유지하고 storage pool에서 volume을 할당. 내부적으로 **Storage Scheduler**(타깃 노드 선택)와 **Volume Manager**(free volume 생성·정보 저장)로 구성. (2) **Worker node**: 애플리케이션 컨테이너 + LightPool 컴포넌트 실행. 일부 worker만 NVMe SSD를 장착(node 1,3,N)하고 일부(node 2)는 pod만 실행하지만, LightPool-Controller를 통해 모든 노드가 클러스터 어디든 스토리지에 접근.
> Worker 노드 컴포넌트: **LightPool-Engine**(user-level, NVMe-oF Target + Bdev + NVMe Driver, 디바이스 직접 관리), **LightPool-Agent**(engine에서 정보 수집·Controller에 보고), **CSI Driver**(pod 스케줄 시 Initiator를 타깃 engine에 연결 통지), **LightPool-Initiator**(kernel-level, 할당 스토리지를 표준 NVMe 디바이스로 pod에 노출). 핵심 설계 목표: low cost / high storage utilization / high performance / high availability.

> [!abstract]- 스토리지 할당 알고리즘 (Algorithm 1, p.5)
> 2단계 전략. **Stage 1 pre-filtering**: Node_List를 순회해 storage 요구를 만족하는 노드만 Selected_List에 추가. **Stage 2 selecting**: 각 노드에 score 부여 — local/remote preference 부합 시 `Node.Score += 20`, 그리고 `Used_Score = 100 * (Total - Free) / Total`을 더해 잔여 용량이 적은(=더 채워진) 노드를 우선. 최대 score 노드를 타깃으로 선택. 이는 fragmentation을 줄여 향후 대용량 할당 요청을 한 노드에서 처리할 여지를 남긴다. 할당 후 Volume Manager가 타깃 노드에 지정 용량 volume 생성→정보(노드 IP/port/volume)를 control plane에 저장→CSI Driver가 Initiator를 engine에 연결.

> [!abstract]- I/O 경로: remote vs local zero-copy (Fig.5)
> **Remote I/O path (Fig.5a)**: ①app이 mount된 표준 NVMe 디바이스에 read/write(물리 위치 무관) → ②worker 노드 LightPool-Initiator가 I/O 수신 → ③NVMe-tcp Transport(NIC)로 타깃 노드 user-level engine에 전달 → ④engine이 NVMe command로 변환·물리 디바이스 통지 → ⑤디바이스가 engine의 data buffer로 read/write → ⑥TCP로 Initiator에 전송 → ⑦app buffer로 복사.
> **Local I/O path with zero-copy (Fig.5b)**: ①②는 동일. 원래는 tcp-loopback으로 Initiator↔Engine 전송 시 중복 복사 2회 발생. 이를 제거하기 위해 ③zero-copy transport가 Initiator와 Engine 간 **shared memory** 사용, Initiator가 I/O를 NVMe command로 변환하며 user buffer 주소를 **IOMMU page table에 등록된 IOVA**로 치환 → ④Engine이 submission queue에 넣고 디바이스 통지 → ⑤**DMA remapping**으로 로컬 디바이스가 app buffer에 직접 접근. 결과적으로 redundant copy 제거로 near-native 성능.

> [!abstract]- 고가용성: hot-upgrade & hot-migration (Fig.6)
> **Hot-upgrade**: LightPool-Engine이 hot-upgrade 상태로 진입해 `fork` system call로 child process(새 버전) 생성. parent(구 버전)는 I/O 수신 중단·진행 중 요청 완료 대기 후 subsystem 닫고 exit. child는 parent exit 감지 후 config 기반 subsystem 복원·TCP 재연결하고 I/O 처리 시작. 이 기간 app I/O는 block되지만 error는 발생 안 함. production에서 약 1s 내 완료.
> **Hot-migration (Fig.6)**: multipath 기반 동적 I/O path 전환. Ⓐ관리자가 새 노드에 동일 NQN/uuid로 target subsystem 생성, Initiator가 multipath로 연결(inactive 설정). Ⓑhot-migration module이 여러 round로 데이터 복사 — 1라운드 전체 복사 + bitmap으로 변경 LBA 기록, 이후 라운드에서 변경분만 복사(새 bitmap으로 현재 라운드 변경 기록). 남은 복사가 3~5s 내 끝날 때 Initiator에 요청 일시정지 통지 후 마지막 복사. Ⓒ마지막 후 Initiator가 I/O path를 새 노드로 전환(구 연결 drop). `/dev/nvme0n1`을 multipath가 유지하므로 block device 사라짐/remount 없이 무중단.

## 평가

> [!example]- FIO 마이크로벤치 IOPS/대역폭/지연 (Fig.7~9, p.6~7)
> 비교군: Native disk, LightPool, LP-NoOpt(zero-copy 없음), OpenEBS(Mayastor). 4노드 K8s 클러스터(Intel Xeon Platinum 8163, 768GB DDR4, CentOS 7.9, kernel 5.15.0, 2.0TB Intel P4510 NVMe).
> - **성능 비율**: LightPool은 native의 93.2%~103.4% 달성(오버헤드 미미). OpenEBS는 4k-rand-r-qd128-6numjobs에서 native의 34.0%, 4k-rand-w-qd16-4numjobs에서 46.3%에 그침.
> - **vs OpenEBS**: 최고 케이스(4k-rand-r-qd128-8numjobs)에서 LightPool이 OpenEBS보다 190.9% 높음. zero-copy는 LP-NoOpt 대비 대역폭 최대 35.9% 향상.
> - **지연(Fig.8)**: LightPool은 native 대비 약 2.1~3.5µs 추가 지연. zero-copy가 tcp-loopback 우회로 sequential write에서 평균 지연 최대 60.4% 감소.
> - **local vs remote(Fig.9)**: remote storage는 2×25Gbps 네트워크에서 상수 55.2~67.3µs 추가 지연만 유발(throughput은 네트워크 대역폭 영향 받음).

> [!example]- 확장성 (Fig.10, p.7)
> 1~12개 NVMe SSD에 대해 128k-seq-r-qd128-4numjobs 테스트. 대역폭이 SSD 수에 선형 증가, 12 SSD에서 **39.4 GB/s** 포화. LightPool이 노드의 12 디스크를 모두 saturate 가능 → 양호한 scalability.

> [!example]- 실제 애플리케이션: RocksDB(YCSB) & OceanBase (Fig.11~12, Table II, p.7)
> RocksDB-6.4.6 + YCSB-0.15.0(recordcount/operationcount 1000만, read/update proportion 0.5). 
> - **Throughput(Fig.11)**: LightPool은 native의 99.2% overall(read 99.1%, update 99.3%), OpenEBS 대비 106.9%.
> - **지연(Table II)**: Read Lat native 27.6 / LightPool 28.0 / OpenEBS 30.5µs; Update Lat 39.5 / 39.8 / 42.4µs.
> - **Tail latency(Fig.12)**: LightPool 99th latency가 OpenEBS Mayastor의 read 50.0%, update 53.2% 수준(절반).

> [!example]- 가용성 실측 (Fig.13~15, p.7~8)
> - **Hot-upgrade(Fig.13)**: 4K rand r/w, qd1, 1 job FIO 중 hot-upgrade 시 I/O pause는 **1s** 내 완료. Production OceanBase 3회 테스트(Fig.14)에서 SQL throughput 최대 31.0% 감소·latency 최대 1.49x 증가, 10s 미만 지속으로 수용 가능.
> - **Hot-migration(Fig.15)**: 400GB volume에 4KB write 중 마이그레이션. **7 round** 내 완료, 각 라운드 데이터 마이그레이션 대역폭 제시, FIO write 무중단.

> [!example]- Production 클러스터 (Fig.16, p.2/p.8)
> Alibaba 8500+ 노드에 배포, OceanBase 스토리지 활용률 **40%→65%**. local/remote 자원 모두 IOPS 영향 미미, await time은 remote가 local보다 약 0.3ms 높음. OceanBase SQL Response Time에 스토리지 위치 영향 적음 → 사용자가 latency 민감도에 따라 local/remote 선택 가능.

## 섹션 노트
- **I. Introduction / II. Background**: cloud-native DB의 3대 storage feature(고성능·고용량·DB레벨 복제) 정리, NVMe·NVMe-oF·Kubernetes/CSI 배경. 주요 기여 3가지(아키텍처/스토리지 스택/대규모 배포).
- **III. Motivation**: 로컬 스토리지 저활용·비유연성, disaggregated storage(대역폭 병목+고비용), cloud-native storage(OpenEBS의 로컬 복제 강제·성능 저하) 한계.
- **IV. Design & Implementation**: 설계 목표 → design choice(NVMe-oF over iSCSI, TCP over RDMA, no replica) → 아키텍처 → storage management(aggregation/maintenance/Algorithm 1) → I/O path(remote/local zero-copy) → 가용성(hot-upgrade/migration).
- **V. Evaluation**: 마이크로벤치/확장성/실앱/가용성/production 특성.
- **VI. Lessons and Experience**: NVMe/TCP production 튜닝(qdisc_run_end() 패치 kernel 5.19 머지), zero-copy 동기, user-level engine 장점, 노드당 공유 범위 5노드 제한(blast radius 관리), DPU/RDMA 등 emerging HW 기대.
- **VII. Related Work / VIII. Conclusion**: cloud-native storage(CNSBench, Kubestorage 등), remote storage protocol(iSCSI/NVMe-oF, HyQ). control path 컴포넌트 오픈소스(github.com/eosphoros-ai/liteio).

## 핵심 용어
- **NVMe-oF (NVMe over Fabrics)**: NVMe를 네트워크 fabric 너머 원격 디바이스로 확장하는 프로토콜. iSCSI 대비 성능·CPU 효율 우수, 로컬에 가까운 원격 스토리지 성능 제공.
- **Storage pool**: 클러스터 각 노드의 로컬 SSD를 논리적으로 통합한 자원 풀. LightPool-Controller가 여기서 volume을 동적 할당해 노드 간 공유.
- **Zero-copy transport**: 로컬 접근 시 Initiator-Engine 간 shared memory + IOMMU DMA remapping으로 중복 데이터 복사(tcp-loopback 2회)를 제거하는 LightPool의 로컬 I/O 최적화.
- **IOMMU / IOVA / DMA remapping**: IOMMU page table에 user buffer를 IOVA로 등록하고, 디바이스가 DMA remapping으로 app buffer에 직접 접근하게 하는 메커니즘.
- **LightPool-Engine / -Initiator / -Controller / -Agent**: 각각 user-level 디바이스 관리(NVMe-oF Target), kernel-level 표준 NVMe 노출, control plane의 스케줄·할당, worker의 정보 수집·보고를 담당하는 컴포넌트.
- **Hot-upgrade**: fork 기반 무중단 engine 업그레이드(약 1s I/O pause).
- **Hot-migration**: multipath 기반 동적 I/O path 전환으로 무중단 데이터 마이그레이션(bitmap 증분 복사).
- **No data replica**: 스토리지 레벨 복제를 두지 않고 DB 레벨 복제에 위임하는 설계로 비용·성능 손실 제거.
- **OceanBase**: Alibaba의 분산 SQL DB. LightPool의 주 배포 대상(8500+ 노드).
- **OpenEBS / Mayastor**: 비교 대상 cloud-native storage. 로컬 복제 강제·무거운 stack으로 성능 저하.

## 강점 · 한계 · 열린 질문
- **강점**
  - Production 8500+ 노드 대규모 배포 검증(40%→65% 활용률)으로 산업적 신뢰도 높음.
  - native 대비 2.1µs/93~103% 수준의 극저 오버헤드, OpenEBS 대비 명확한 우위(대역폭 190.9%, tail latency 절반).
  - local/remote 경로 분리 최적화(zero-copy)와 hot-upgrade/migration까지 실용성 완성도 높음. control path 오픈소스 공개.
- **한계**
  - **no data replica** 전제는 DB가 자체 복제하는 cloud-native 분산 DB에 특화 — 복제 안 하는 워크로드엔 안전성 보장 없음.
  - TCP 선택으로 RDMA 대비 잠재 성능 상한 존재(논문도 향후 RDMA/DPU 지원 언급).
  - 노드당 공유 범위를 5노드로 제한해 blast radius를 관리 — 활용률 향상 폭에 구조적 상한이 있을 수 있음.
  - zero-copy가 IOMMU/DMA remapping 등 하드웨어·커널 기능에 의존.
- **열린 질문**
  - RDMA/eRDMA/DPU(BlueField-3, Intel IPU) 적용 시 성능·host CPU 절감 폭은?
  - storage failure 시 한 노드 장애가 공유 5노드에 미치는 영향과 복구 절차의 정량적 SLA는?
  - DB 레벨 복제와 LightPool 배치(rack/노드 다양성)의 상호작용 — 복제본이 같은 물리 풀에 몰릴 위험 관리?

## ❓ Q&A (자가 점검)

> [!question]- Q1. LightPool이 풀려는 핵심 문제는 무엇이고 근본 원인은?
> 답: cloud-native 분산 DB 클러스터의 로컬 NVMe SSD 저활용(평균 40%). 근본 원인은 노드별 고정 하드웨어 구성과 컨테이너 워크로드의 CPU/메모리/스토리지 요구 불균형 — 한 자원 소진 시 다른 자원이 남아 낭비된다.

> [!question]- Q2. 왜 disaggregated storage(EBS)나 OpenEBS가 답이 아닌가?
> 답: EBS는 cross-cluster network 대역폭 병목(Feature 1 위배)과 전용 스토리지 서버·네트워크 비용으로 TCO가 높다. OpenEBS는 최소 1개 데이터 복제를 로컬에 강제해 저활용 문제를 못 풀고(Feature 3와 중복), 가용성·복제 중심의 무거운 software stack으로 성능 저하가 크다.

> [!question]- Q3. 왜 NVMe-oF인가, 그리고 왜 RDMA가 아니라 TCP인가?
> 답: NVMe-oF는 NVMe의 parallelism·multi-queue를 활용하며 iSCSI 대비 최대 40% 성능 손실·30% CPU 절감으로 우수하다. RDMA는 하드웨어 의존으로 모든 노드에 배포 불가하고 확장 제약이 있어, 하드웨어 불필요·안정적인 TCP를 택했다(RDMA는 향후 지원).

> [!question]- Q4. zero-copy 메커니즘은 무엇을 어떻게 제거하나?
> 답: 로컬 접근 시 원래 tcp-loopback으로 Initiator↔Engine 전송하며 중복 복사 2회가 발생. zero-copy는 둘 사이 shared memory를 쓰고, Initiator가 user buffer 주소를 IOMMU에 등록된 IOVA로 치환, DMA remapping으로 로컬 디바이스가 app buffer에 직접 접근하게 해 중복 복사를 제거한다(near-native).

> [!question]- Q5. 스토리지 할당 시 어떤 노드를 고르나?
> 답: Algorithm 1의 2단계. pre-filtering으로 요구 충족 노드를 추리고, selecting에서 local/remote preference 부합 시 +20, `Used_Score=100*(Total-Free)/Total`를 더해 더 채워진 노드를 우선해 fragmentation을 줄인다. 최대 score 노드 선택.

> [!question]- Q6. 무중단 업그레이드와 데이터 마이그레이션은 어떻게 하나?
> 답: Hot-upgrade는 fork로 새 버전 child를 띄우고 parent가 진행 요청 완료 후 exit, child가 subsystem 복원·재연결(약 1s I/O pause, error 없음). Hot-migration은 동일 NQN/uuid 새 subsystem에 multipath 연결 후 bitmap 증분 복사를 여러 round 수행, 마지막에 I/O path를 전환(`/dev/nvme0n1` 유지로 무중단).

> [!question]- Q7. 성능 수치로 본 LightPool의 우위는?
> 답: native의 93.2~103.4% 성능, 추가 지연 약 2.1~3.5µs. OpenEBS 대비 대역폭 최대 190.9% 높고 99th tail latency는 절반(read 50.0%, update 53.2%). RocksDB throughput native의 99.2%. 12 SSD에서 39.4GB/s 선형 확장.

> [!question]- Q8. no data replica가 안전한 이유와 production 효과는?
> 답: cloud-native 분산 DB가 DB 레벨에서 multiple replica를 이미 유지하므로 스토리지 레벨 복제는 중복·낭비. 제거 시 비용·성능 손실을 줄인다. Alibaba 8500+ 노드 배포에서 OceanBase 스토리지 활용률을 40%→65%로 향상.

## 🔗 Connections
[[Infra]] · [[HPCA]] · [[2024]]

## References worth following
- [17] Z. Guz et al., "NVMe-over-fabrics performance characterization and the path to low-overhead flash disaggregation," SYSTOR 2017 — NVMe-oF vs iSCSI 성능·CPU 정량 비교의 근거.
- [9] Z. Chen et al., "HyQ: Hybrid i/o queue architecture for hyper-scale hardware offloading," CCGrid 2023 — hybrid I/O queue로 NVMe-oF offloading 성능·다기능 병행(저자 그룹 후속/관련).
- [33] OpenEBS / [25][26] Mayastor — 주 비교 대상 cloud-native storage의 설계·한계.
- [21] A. Klimovic et al., "Flash storage disaggregation," EuroSys 2016 / [22] "Reflex: Remote flash = local flash," ASPLOS 2017 — disaggregation 성능·로컬 동등성 논의의 고전.
- [38] S. Xue et al., "Spool: Reliable virtualized nvme storage pool in public cloud infrastructure," ATC 2020 — public cloud NVMe storage pool 신뢰성 비교군.
- [39] Z. Yang et al., "OceanBase: a 707 million tpmc distributed relational database system," VLDB — 주 배포 대상 DB 이해용.

## Personal annotations
<본인 메모 영역>
