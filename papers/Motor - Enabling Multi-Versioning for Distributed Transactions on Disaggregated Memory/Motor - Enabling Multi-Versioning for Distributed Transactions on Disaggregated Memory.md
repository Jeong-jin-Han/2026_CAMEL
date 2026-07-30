---
title: "Motor: Enabling Multi-Versioning for Distributed Transactions on Disaggregated Memory"
description: "분리(disaggregated) 메모리에서 CVT(consecutive version tuple) 구조와 순수 one-sided RDMA MVCC 프로토콜로 분산 트랜잭션 멀티버저닝을 구현한 시스템"
venue: OSDI
year: 2024
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/cxl
  - venue/osdi
  - year/2024
  - list/26s-v2
  - topic/disaggregated-memory
  - topic/rdma
  - topic/mvcc
  - topic/distributed-transactions
---

# Motor: Enabling Multi-Versioning for Distributed Transactions on Disaggregated Memory

> **OSDI 2024** · cluster/cxl · Source: [Motor - Enabling Multi-Versioning for Distributed Transactions on Disaggregated Memory.pdf](<Motor - Enabling Multi-Versioning for Distributed Transactions on Disaggregated Memory.pdf>)

저자: Ming Zhang, Yu Hua (corresponding author), Zhijun Yang — Wuhan National Laboratory for Optoelectronics, School of Computer, Huazhong University of Science and Technology

## TL;DR
Motor는 compute pool/memory pool로 분리된 disaggregated memory 위에서 분산 트랜잭션에 multi-versioning을 도입하는 시스템이다. 기존 linked-chain 버전 구조는 메모리 풀에 CPU가 없어 버전을 하나씩 원격으로 훑는 chain walking에 여러 RTT가 필요했는데, Motor는 여러 버전을 연속된 주소 공간에 저장하는 consecutive version tuple(CVT) 구조로 이를 단일 RDMA READ로 해결한다. 값(value)과 버전 메타데이터를 분리해 저장 공간과 read payload를 줄이고, anchor-assisted read로 one-sided RDMA에서도 버전-값 쌍의 원자적 읽기를 보장하며, in-flight transaction을 추적하지 않는 coordinator-active GC로 가비지 컬렉션 오버헤드를 없앤다. FORD(single-versioning)와 FaRMv2-DM(linked-chain multi-versioning을 disaggregated memory에 포팅) 대비 처리량을 최대 98.1% 개선하고 지연을 최대 55.8% 줄인다.

## 문제 & 동기
Memory disaggregation은 compute pool과 memory pool을 RDMA/CXL로 연결해 자원 활용률과 확장성을 높이지만, memory pool에는 강력한 CPU가 없어(약한 compute unit만 존재) 메모리 할당·인덱스 관리 정도만 수행하고 실제 트랜잭션 로직은 compute pool의 coordinator가 one-sided RDMA로 직접 처리해야 한다(p.802-803). 최신 시스템 FORD는 이 환경에서 분산 트랜잭션을 지원하지만 데이터당 하나의 버전만 유지하는 single-versioning을 채택해 두 가지 한계가 있다: (1) 커밋 중 쓰기가 데이터를 보이지 않게 만들어 읽기가 블록됨(낮은 동시성), (2) atomicity 보장을 위해 undo log를 모든 backup에 써야 해서 네트워크 대역폭을 소모하고 모든 ACK를 기다려야 함(높은 로깅 오버헤드)(p.801-803).

전통적인 monolithic 서버의 multi-versioning 기법(locking, validation, timestamp 계산)은 각 서버가 강한 CPU를 가진다고 가정하므로 memory pool의 약한 CPU에는 호환되지 않는다. 또한 기존 old-to-new/new-to-old linked chain 버전 구조는 monolithic 서버에서는 로컬 메모리를 포인터로 훑으면 되지만, disaggregated memory에서는 이 chain walking을 compute pool이 원격 라운드트립으로 하나씩 수행해야 해서 비용이 크다.

> [!quote]- 📄 원문 표현 (paper)
> - "FORD writes undo logs to back up the old data, which consumes the network bandwidth and decreases throughput." (p.801)
> - "Fig. 1c shows that when increasing the number of steps in the chain walking from 1 to 20, the RDMA read latency significantly increases by 24.8× in our testbed (§ 7.1)." (p.802)
> - "However, the linked chains become inefficient in disaggregated memory, since all the application data are stored in the remote memory pool, which does not contain powerful CPU to execute the chain walking." (p.802)

## 핵심 통찰 (Key Insight)

**1) Consecutive Version Tuple(CVT) — 버전을 연속 주소 공간에 배치.** 포인터로 흩어진 chain 대신 한 레코드의 여러 버전을 하나의 연속된 메모리 영역(Vcell 배열)에 모아 저장한다. 이렇게 하면 coordinator는 단 한 번의 RDMA READ로 CVT 전체(모든 버전의 메타데이터)를 가져와 로컬에서 target version을 탐색할 수 있어, 버전 수가 늘어나도 네트워크 라운드트립은 1회로 고정된다. Disaggregated memory에서 RTT가 지배적 비용이라는 점을 정확히 겨냥한 설계다.

> [!quote]- 📄 원문 표현 (paper)
> - "By using CVT, the coordinator is able to fetch multiple versions in a single RDMA READ, instead of performing chain walking to read remote versions one by one until the target version." (p.804)

**2) Value 영역과 버전(CVT) 영역의 분리 + attribute bar.** 값 자체를 CVT와 함께 저장하면 페이로드가 커져 read 지연과 대역폭이 늘어난다. Motor는 최신 전체 값(full-value area)만 저장하고, 오래된 버전은 그 시점에 실제로 수정된 속성(attribute)만 delta 영역의 attribute bar에 저장한다. Old version 값은 fetch한 old attribute를 최신 full value 위에 적용해 로컬에서 재구성한다. TPCC CUSTOMER 테이블 예시로 총 속성 크기(TotAttrSize)가 트랜잭션 종류별로 512B/12B/4B로 각각 10%/88%/2% 빈도로 발생한다는 관측에 기반해 attribute bar 크기(ABS)를 근사한다.

> [!quote]- 📄 원문 표현 (paper)
> - "Motor stores the variable-sized modified attributes, instead of full-sized values, to maintain different versions of values for any record, thus reducing memory overhead." (p.805)

**3) Anchor-assisted read — one-sided RDMA에서 버전-값 원자적 읽기.** 값과 버전이 분리 저장되어 있으므로, 동시에 GC나 갱신이 일어나면 coordinator가 손상되거나(다른 버전의 값과 결합된) 불일치 데이터를 읽을 위험이 있다(Fig.5). Motor는 Vcell·Vpkg 양쪽에 시작/끝 anchor(VcellSA/EA, VpkgSA/EA)를 두고, write 시 네 앵커를 모두 동일하게 증가시키며 write 순서를 Vpkg→attribute→Vcell로 고정한다. Read는 fetch 후 네 anchor가 모두 같은지 검사해 일관성을 판정하며, Silo처럼 두 번 읽을 필요 없이 한 번의 read로 검증한다.

> [!quote]- 📄 원문 표현 (paper)
> - "Unlike Silo [71] that reads the version twice to confirm consistency, our scheme only needs to read once and compares the four anchors to identify consistency." (p.807)

**4) Coordinator-active GC — in-flight transaction 추적 없는 가비지 컬렉션.** 기존 GC는 가장 오래된 실행 중 트랜잭션을 추적해 안전하게 지울 버전을 판단하는데, 이는 memory pool CPU가 하기엔 무리이고 compute pool이 하려면 동기화 RTT가 든다. Motor는 "RDMA가 트랜잭션을 매우 빠르게 만들기 때문에 CVT의 가장 오래된 버전이 사용될 확률이 가장 낮다"는 관찰에 기반해, 빈 Vcell이 없을 때 coordinator가 추적 없이 선제적으로(preemptively) 가장 오래된 버전을 덮어쓴다.

> [!quote]- 📄 원문 표현 (paper)
> - "We observe that the oldest version in CVT has the smallest probability to be used, given that RDMA significantly accelerates transactions... Hence, Motor enables coordinators to preemptively select the oldest version in CVT as the victim." (p.806)

## 설계 / 메커니즘 (Design)
**Memory store 구조 (§4, Fig.3, p.804).** CVT는 Header(TableID, Key, Lock, AttrBarPtr, VpkgPtr, auxiliary info)와 여러 Vcell(VcellSA, Valid, Version, Bitmap, StartOffset, VcellEA)로 구성된다. Value 영역은 full-value area(최신 값, Vpkg = VpkgSA+Data+VpkgEA)와 delta area(과거 버전의 수정 속성을 담는 attribute bar)로 분리된다. CVT는 hash table 또는 B+tree 인덱스로 접근하며, compute pool은 CVT address cache를 두어 반복 접근 시 hash bucket 재조회를 생략한다.

**버전 수(VNum) 설정 (§4.1, §7.2-7.3, Fig.8-10).** 메모리 풀 CPU가 약해 VNum을 동적으로 조정할 수 없으므로 워크로드별 고정값을 사용한다: read-only가 많고 contention이 낮은 TATP은 VNum=2, SmallBank=3, TPCC=4, KVS=4가 최적으로 나타났다(p.810-811). VNum이 너무 작으면 read-only 트랜잭션의 abort가 늘고(TPCC의 STOCK_LEVEL abort율이 VNum=2일 때 32.1%에서 VNum=4일 때 3.8%로 감소, p.810), 너무 크면 CVT 크기가 커져 RDMA read 지연이 늘어난다.

**Coordinator-active GC (§4.3, Fig.4, p.806).** 빈 Vcell이 없을 때, read queue를 이용한 baseline(진행 중 read를 skip하고 나머지 중 가장 오래된 버전 선택)이 아니라, 그냥 CVT 내 가장 오래된 버전을 선제적으로 덮어쓴다. attribute bar 공간이 부족하면 회수된 attribute에 해당하는 Vcell의 Valid를 0으로 설정해 무효화한다.

**Anchor-assisted read (§4.4, Fig.5, p.806-807).** Write는 Vpkg를 먼저 쓰고 이후 attribute, 마지막에 Vcell을 쓰는 순서(R1: Vpkg→attribute→Vcell, R2: 각 패키지 내부는 start anchor→content→end anchor)를 지킨다. 이 순서 보장을 위해 RDMA reliable connection 모드(메시지 무손실/무재정렬)와 RNIC의 write 순서 보장을 활용하며, Intel DDIO(Data Direct I/O)를 비활성화해 iMC가 first-come-first-serve 큐를 통해 메모리에 순서대로 직접 쓰도록 한다(DDIO를 켜면 L3 캐시 축출 순서가 예측 불가능해 R1/R2가 깨질 수 있음, p.807).

**트랜잭션 프로토콜 (§5.1, Fig.6, p.807-808).** 3 phase 구조.
- *Phase 1 실행*: $T_{start}$ 획득 → RO 데이터는 batched RDMA READ, RW 데이터는 doorbell-batched RDMA CAS+READ로 잠금과 읽기를 동시 수행 → CVT 내에서 $T_{start}$보다 작은 버전 중 가장 큰 버전 $V0$을 target으로 선택 → $V0$보다 큰 버전이 관측되면(다른 트랜잭션이 이미 커밋) serializability 위반 가능성으로 early abort.
- *Phase 2 검증*: RW 데이터를 포함한 트랜잭션만 수행. $T_{commit}$ 획득 후 RO 데이터의 CVT를 재조회해 잠겨있지 않고 largest version이 $V0$과 같은지 확인(다르면 abort). RO만 있는 read-only 트랜잭션은 anchor 일치로 스냅샷이 안정적이므로 이 validation이 필요 없음(single-versioning과 달리).
- *Phase 3 커밋*: doorbell-batched RDMA WRITE로 모든 replica에 갱신을 쓰고 primary를 unlock, 모든 ACK 수신 시 "committed" 보고. Update/Insert/Delete 세 케이스를 로컬에서 준비.

**Isolation levels (§5.2, p.808).** Serializability(SR)는 위 validation phase를 포함해 모든 read-write 트랜잭션이 $T_{commit}$ 순서로 직렬화됨을 보장. Snapshot Isolation(SI)은 read-write 트랜잭션의 read-only 데이터에 대한 재검증을 생략해 더 높은 처리량을 낸다(단, write-write 충돌은 여전히 lock으로 해결).

**Fault tolerance (§5.3, p.809).** (f+1)-way primary-backup replication. Replica 장애는 RDMA로 빠르게 감지, primary 장애 시 backup을 새 primary로 승격. Coordinator 장애는 lease 기반 탐지 + local operation log(UPS-backed DRAM에 저장, 트랜잭션당 최대 556B)로 새 coordinator가 in-flight commit/unlock을 재개.

## 평가 (Evaluation)
**환경**: Mellanox 100Gbps IB, 4대 서버(1대 compute pool 코디네이터, 3대 memory pool, 각 192GB DRAM), 벤치마크는 KVS(10M/20M KV pair, Zipfian skew 가변), TATP, SmallBank, TPCC. 비교 대상은 FORD(single-versioning, 원 논문 오픈소스)와 FaRMv2-DM(FaRMv2의 new-to-old chain multi-versioning 프로토콜을 one-sided RDMA로 disaggregated memory에 이식)(§7.1, p.810).

- **버전 구조 비교 (Fig.10, KVS, p.811)**: CVT가 O2N(old-to-new chain) 대비 처리량 1.7–2.4×, N2O(new-to-old chain) 대비 1.3–1.6× 개선. Skewness 0.99에서 O2N/N2O 대비 평균 50th/99th percentile 지연을 각각 59.8%/30.8% (O2N), 67.9%/47.7% (N2O) 감소.
- **End-to-end (Fig.11, p.811)**: FORD 대비 처리량 개선 — TATP 14.4%, TPCC 98.1%, SmallBank 65.4%; 50th percentile 지연 감소 — TPCC 55.8%, SmallBank 26.2%. FaRMv2-DM 대비 처리량 개선 — TATP/TPCC/SmallBank 각각 18.9%/44.3%/29.5%, 50th(99th) percentile 지연 감소 8.6%(39.1%)/52.1%(35.6%)/43.6%(34.5%).
- **메모리 오버헤드 (Fig.12, p.812)**: FORD가 단일 버전이라 가장 낮으나, Motor는 TPCC에서 4개 버전을 유지하면서도 FORD 대비 1.45×(naive라면 4×)만 소비. TATP 17.3%, SmallBank 32.7%, KVS 37.7% 더 사용(FORD 대비). FaRMv2-DM은 Motor보다 14.6–22.8% 더 많은 메모리 사용(포인터 및 full-size 버전 저장 때문).
- **메모리 풋프린트 가변 실험 (§7.6, Fig.13-16, p.812-813)**: VNum/ABS를 조정해 Motor의 메모리 사용량을 FORD 수준까지 낮춰도 여전히 FORD/FaRMv2-DM보다 높은 처리량 유지.
- **Isolation level 비교 (§7.7, Fig.17, p.813)**: Motor-SI가 TATP·TPCC 모두에서 Motor-SR보다 낮은 지연·높은 처리량(RW 트랜잭션의 validation phase 생략). TPCC에서 개선폭이 더 큼(read-only 데이터 접근 비중이 더 큼).
- **PM 적용 (§7.8, Fig.18, p.813)**: 6×128GB Intel Optane PM으로 memory pool 구성 시 TPCC 처리량이 DRAM 대비 13.1%만 감소 — DRAM/PM 양쪽에 이식 가능함을 시연.
- **Fault tolerance (§7.9, Fig.19, p.814)**: 84 coordinator 중 60개 동시 장애 시 새 coordinator 60개 생성·인계에 약 170ms 소요. Replica 복구는 CUSTOMER(대형 테이블) backup 추가 시 데이터 마이그레이션에 약 200ms, DISTRICT(소형 테이블)는 1.1ms.

> [!quote]- 📄 원문 표현 (paper)
> - "Compared with FORD, Motor respectively improves the transaction throughput by 14.4% on TATP, 98.1% on TPCC, and 65.4% on SmallBank." (p.811)
> - "Motor supports 4 versions of data in TPCC, but only consumes 1.45×, instead of 4×, of memory space over FORD." (p.812)
> - "Fig. 18 shows that the throughput only decreases by 13.1% on PM due to the limited PM bandwidth." (p.813)

## 섹션 노트
- **§1 Introduction**: FORD의 single-versioning 한계(낮은 동시성, 높은 로깅)와 기존 multi-versioning의 두 가지 비호환 이유(강한 CPU 가정, chain walking/GC 비용)를 제시하고 CVT + MVCC 프로토콜을 기여로 요약.
- **§2 Background and Motivation**: Memory disaggregation의 자원 풀링 이점, compute pool이 one-sided RDMA로 memory pool의 (f+1)-way replica를 다루는 시스템 모델, FORD의 한계, multi-versioning 도입 시의 두 가지 challenge를 정리.
- **§3 Motor Overview**: Motor memory store와 Motor transaction protocol 두 축, 그리고 client→coordinator→memory pool 워크플로(Fig.2).
- **§4 Motor Memory Store**: CVT 구조(4.1), 분리된 value 영역/attribute bar(4.2), coordinator-active GC(4.3), anchor-assisted read(4.4)를 각각 상세 설계.
- **§5 Motor Transaction Protocol**: 3-phase 프로토콜(5.1), SR/SI 두 isolation level 지원 방식(5.2), replica/coordinator/network 장애 대응(5.3).
- **§6 Implementations**: TxnBegin/GetTS/AddObject/FetchAll/Validate/TxnCommit 인터페이스, 코루틴 기반 실행 프레임워크(1개는 RDMA ACK polling, 나머지는 coordinator)로 RDMA 지연을 파이프라인으로 은닉.
- **§7 Performance Evaluation**: 버전 구조·VNum·isolation level·메모리 풋프린트·PM·fault tolerance에 걸친 다각도 평가(위 Evaluation 절 요약).
- **§8 Related Work**: RDMA 기반 fast distributed transaction 계열(FaRM, DrTM, Storm 등), memory disaggregation 인프라 연구(Clio, LegoOS, MIND 등), 전통 monolithic MVCC/GC 연구(vmvcc, Silo, Hekaton)와의 차별점(모두 강한 CPU 가정) 정리.
- **§9 Conclusion**: CVT + one-sided RDMA MVCC 프로토콜로 disaggregated memory에서 효율적 multi-versioning 분산 트랜잭션을 달성했다고 요약.

## 핵심 용어 (Key terms)
- **CVT (Consecutive Version Tuple)**: 한 레코드의 여러 버전 메타데이터(Vcell)를 연속된 주소 공간에 저장해 단일 RDMA READ로 전부 가져올 수 있게 하는 구조.
- **Vcell**: CVT 안에서 한 버전을 나타내는 슬롯(VcellSA/EA anchor, Valid, Version, Bitmap, StartOffset).
- **Vpkg (Value package)**: 실제 데이터 값을 감싸는 패키지(VpkgSA, data, VpkgEA anchor).
- **Attribute bar**: delta 영역에서 과거 버전들이 실제로 수정한 속성만 모아 저장하는 구조(전체 값 대신).
- **VNum**: CVT가 유지하는 최대 버전 수(고정값, 워크로드별 튜닝).
- **Anchor-assisted read**: 시작/끝 anchor 쌍의 일치 여부로 one-sided RDMA read의 버전-값 원자성을 검증하는 기법.
- **Coordinator-active GC**: in-flight 트랜잭션 추적 없이 coordinator가 가장 오래된 버전을 선제적으로 덮어쓰는 GC.
- **FaRMv2-DM**: FaRMv2의 new-to-old chain 기반 multi-version 프로토콜을 one-sided RDMA로 disaggregated memory에 이식한 비교 대상.
- **FORD**: 선행 연구로, disaggregated persistent memory에서 single-versioning 기반 one-sided RDMA 분산 트랜잭션을 지원하는 시스템(비교 baseline).
- **DDIO (Data Direct I/O)**: RNIC이 L3 캐시에 직접 데이터를 쓰는 Intel 기능. Motor는 write 순서 보장을 위해 이를 비활성화.
- **Doorbell-batched RDMA CAS+READ**: 여러 RDMA verb를 하나의 doorbell로 배치 전송해 락 획득과 읽기를 동시에 수행하는 기법.

## 강점 · 한계 · 열린 질문
**강점**: memory pool CPU를 전혀 사용하지 않는 순수 one-sided RDMA 설계, 버전 수와 무관하게 1 RTT로 버전 fetch, attribute bar를 통한 낮은 메모리 오버헤드, SR/SI 두 isolation level의 유연한 지원, 실제 RDMA 테스트베드 + PM에서의 검증, ms 단위 fault recovery.

**한계**: VNum이 테이블/워크로드별 고정값이라 사전 프로파일링·튜닝이 필요하며 런타임 접근 패턴 변화에 동적으로 적응하지 못한다(§7.2에서 VNum 최적값이 워크로드마다 다름을 명시). Coordinator-active GC는 in-flight 트랜잭션 추적을 포기한 대가로, 아직 읽고 있는 버전이 조기에 회수되면 long-running 트랜잭션이 abort될 수 있음(§4.3에서 트레이드오프로 언급). Anchor 메커니즘의 정확성은 RDMA reliable-connection 순서 보장과 DDIO 비활성화라는 특정 하드웨어/설정 가정에 의존한다(§4.4). 평가에서 compute pool은 1대 서버로만 구성되어(§7.1), 다수 compute 노드 간 delta 공간 경쟁이나 coordinator 수가 훨씬 많을 때의 확장성은 별도로 다루지 않는다. 네트워크 파티션 시 major partition만 서비스한다는 CAP 트레이드오프로 가용성을 희생한다(§5.3).

**열린 질문**: RDMA 대신 CXL 같은 memory-semantic fabric 위에서 CVT/anchor 설계가 그대로 성립하는가(캐시 일관성이 있는 환경에서 anchor의 필요성 자체가 달라질 수 있음)? VNum을 워크로드 변화에 맞춰 온라인으로 재구성하는 방법은? Compute pool이 다수 노드로 확장될 때 attribute bar 공간 경쟁(§4.2에서 언급된 delta space contention)이 병목이 되는 지점은 어디인가?

## ❓ Q&A (자가 점검)
> [!question]- CVT가 linked chain보다 빠른 근본 이유는?
> 여러 버전을 연속된 주소 공간에 저장해 단일 RDMA READ로 전부 가져올 수 있기 때문에, chain walking이 요구하는 반복적 RTT(버전마다 포인터 추적)를 제거한다 (p.804).

> [!question]- Anchor-assisted read가 막는 구체적 오류 두 가지는?
> Fig.5 기준 (1) GC에 의해 부분적으로 덮어써진 손상된 full value를 읽는 오류, (2) 오래된 attribute와 최신 version 번호가 잘못 결합되어 잘못된 값을 재구성하는 오류다 (p.806).

> [!question]- Coordinator-active GC가 in-flight 트랜잭션 추적을 피하는 이유는?
> Memory pool CPU가 너무 약해 tracking을 감당할 수 없고, compute pool이 추적하면 동기화 RTT가 추가되며 컴퓨트 파워를 낭비한다. RDMA가 트랜잭션을 빠르게 만들어 CVT의 가장 오래된 버전이 쓰일 확률이 가장 낮다는 관찰에 근거해 추적 없이 선제적으로 덮어쓴다 (p.806).

> [!question]- Motor가 undo log를 쓰지 않는 이유는?
> Multi-versioning 자체가 old version을 "undo log"처럼 유지하므로, FORD처럼 별도로 backup에 undo log를 써서 모든 ACK를 기다리는 오버헤드가 필요 없다 (p.801, p.809).

> [!question]- SR과 SI의 성능 차이가 나는 이유는?
> SI는 read-write 트랜잭션이 가진 read-only 데이터에 대한 validation phase를 생략하기 때문에 지연이 낮고 처리량이 높다. Read-intensive한 TATP과 write-intensive한 TPCC 모두에서 SI가 더 나은 성능을 보인다 (p.813, §7.7).

> [!question]- VNum은 어떻게 정해지나?
> 워크로드의 contention과 트랜잭션 길이에 따라 실험적으로 정한다: TPCC=4, TATP=2, SmallBank=3, KVS=4가 최적 처리량을 낸다 (p.810-811, §7.2-7.3).

> [!question]- DDIO를 왜 비활성화하는가?
> DDIO는 RNIC의 write를 L3 캐시로 보내는데, 캐시 축출 순서가 예측 불가능해 anchor-assisted read가 요구하는 write 순서 보장(R1, R2)을 깨뜨릴 수 있다. 이를 막기 위해 iMC가 직접 메모리에 순서대로 쓰도록 DDIO를 끈다 (p.807).

> [!question]- FORD 대비 TPCC에서 개선폭(98.1%)이 가장 큰 이유는?
> TPCC는 write-intensive해서 undo log 회피의 이득이 크고, STOCK_LEVEL 같은 long-running read-only 트랜잭션의 abort을 multi-versioning으로 크게 줄이기 때문이다 (p.811).

## 🔗 Connections
[[CXL]] · [[OSDI]] · [[2024]]
관련: [[Ethane - An Asymmetric File System for Disaggregated Persistent Memory]] (동일 disaggregated memory/PM 아키텍처를 파일시스템 관점에서 다룸)

## References worth following
- Zhang et al., "FORD: Fast One-sided RDMA-based Distributed Transactions for Disaggregated Persistent Memory," FAST 2022 [84] — Motor의 직접 비교 대상인 single-versioning 시스템.
- Shamis et al., "Fast general distributed transactions with opacity," SIGMOD 2019 [64] — Motor가 disaggregated memory용으로 이식·비교한 FaRMv2 multi-versioning 프로토콜의 원 논문.
- Tu et al., "Speedy transactions in multicore in-memory databases (Silo)," SOSP 2013 [71] — anchor-assisted read가 대조하는 "두 번 읽어 확인" 검증 방식의 원조.
- Zuo et al., "One-sided RDMA-conscious extendible hashing for disaggregated memory," ATC 2021 [86] — Motor가 CVT 인덱싱에 사용하는 해시 테이블 구조.
- Böttcher et al., "Scalable garbage collection for in-memory MVCC systems," VLDB 2019 [16] — Motor가 극복 대상으로 삼는 tracking 기반 legacy GC 스킴.

## Personal annotations
<!-- 본인 메모 영역 -->
