---
title: "Cheetah: Metadata Aggregation for Fast Object Storage without Distributed Ordering"
aliases: [Cheetah]
description: "객체의 모든 메타데이터를 MetaX로 집약(aggregate)해 distributed ordering을 제거하고, 메타/데이터를 병렬로 써서 small object의 put/get/delete I/O를 가속하는 directory-based object store"
venue: Eurosys
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/object-storage
  - topic/metadata
  - venue/eurosys
  - year/2025
---

# Cheetah: Metadata Aggregation for Fast Object Storage without Distributed Ordering

> **Eurosys 2025** · `cluster/fs` · Source: [Cheetah - Metadata Aggregation for Fast Object Storage without Distributed Ordering.pdf](Cheetah%20-%20Metadata%20Aggregation%20for%20Fast%20Object%20Storage%20without%20Distributed%20Ordering.pdf)

저자: Yiming Zhang (Shanghai Key Lab / NICE XLab, XMU), Li Wang (KylinSoft), Shengyun Liu (SJTU), Shun Gai · Haonan Wang (NICE XLab), Xin Yao · Meiling Wang (Huawei Theory Lab), Kai Chen (HKUST), Dongsheng Li (NUDT), Jiwu Shu (Tsinghua University) · (corresponding: Yiming Zhang)

## TL;DR
기존 directory-based object store는 객체→볼륨 매핑(volume metadata)을 중앙 디렉터리에, 볼륨 내 offset(offset metadata)을 데이터 서버에 분리 저장한다. 이 분리 때문에 한 번의 `put`에 대한 여러 메타/데이터 쓰기가 crash consistency를 위해 특정 **순서(distributed ordering)**로 직렬화되어 small object I/O 성능이 크게 떨어진다. Cheetah는 한 `put`의 **모든 메타데이터(volume + offset + checksum + meta-log)를 MetaX라는 단일 구조로 집약**하여, MetaX의 **local atomicity**만으로 일관성을 보장하고 distributed ordering을 제거한다. 그 결과 메타와 데이터를 병렬로 쓸 수 있어 Haystack 대비 put 지연 최대 2.37x 개선, get 최대 25% 개선을 달성한다.

## 문제 & 동기
- Object store는 immutable 객체에 대해 `put/get/delete` 인터페이스를 제공한다. SSD 가격 하락으로 latency-sensitive 앱들이 수 KB~수백 KB의 **small object**를 SSD에 저장하며 빠른 접근을 요구한다.
- 객체 배치는 (1) 중앙 directory service가 관리하는 **directory-based mapping**(Haystack, Tectonic)과 (2) 분산 계산으로 결정하는 **hash-based mapping**(CRUSH/Ceph)으로 나뉜다. 상용 시스템은 migration-free expansion 등 유연성 때문에 directory-based를 선호한다.
- 그러나 directory-based store는 **volume metadata(객체→볼륨, 디렉터리)**와 **offset metadata(볼륨 내 offset, 데이터 서버)**를 분리 저장한다. 한 `put`에서 둘이 따로 쓰이고, crash consistency를 위해 메타/데이터 쓰기가 특정 순서로 orchestrate되어야 한다. 이 **distributed ordering**은 small object의 임계 경로에 wait을 도입해 write 성능을 심하게 떨어뜨려 적용 범위를 좁힌다.

> [!quote]- 📄 원문 표현 (paper)
> "Unfortunately, the separation between volume/offset metadata complicates the processing of an object put: to ensure consistency, the multiple writes of the object's volume/offset metadata and object data have to be orchestrated in a particular order, which severely lowers object I/O performance." (p.1, Abstract)
>
> "Such distributed ordering introduces waits in the critical path of put/delete, and thus lowers I/O performance especially for small objects that have relatively-low overhead in writing objects' actual data." (p.3, §2.3)

## 핵심 통찰
> [!note]- 핵심 통찰 (토글)
> - **통찰 1 — 분리(disaggregation)가 아니라 집약(aggregation)이 답이다.** Tectonic/ADLS/HopsFS는 파일시스템 메타데이터를 naming/file/block 등으로 *disaggregate*해 확장성을 얻지만, Cheetah는 반대로 한 객체 `put`의 모든 메타(volume+offset+checksum+meta-log)를 **MetaX 하나로 aggregate**한다. 그러면 일관성은 단일 구조의 **local atomicity** 문제로 축소된다.
> - **통찰 2 — local atomicity가 distributed ordering을 대체한다.** 모든 메타가 한 구조에 모이면, 메타 서버의 atomic write로 메타 일관성을 보장할 수 있어 메타 서버 쓰기와 데이터 서버 쓰기 사이의 순서 제약이 사라진다. → 메타와 데이터를 **병렬**로 쓸 수 있다.
> - **통찰 3 — immutability를 적극 활용한다.** 객체가 immutable이라 한 객체에 대한 여러 `put`이 서로 덮어쓰지 않으므로, 동일 객체의 multiple write 순서를 신경 쓸 필요가 줄어든다. 이것이 병렬 처리의 정당성을 준다.
> - **통찰 4 — meta 집약의 비용(확장성·신뢰성)은 hybrid mapping으로 푼다.** 모든 메타를 모으면 메타 서버 부담이 커지므로, CRUSH를 확장한 hybrid mapping으로 PG→meta server 매핑과 VG(Vol Group)를 도입해 데이터 마이그레이션 없는 확장과 메타 확장성을 동시에 얻는다.

## 설계 / 메커니즘
> [!note]- 설계 / 메커니즘 (토글)
> **MetaX (§3.1).** 한 `put`의 모든 메타데이터를 집약한 write-optimal 구조: volume metadata $M_v$(객체→logical volume id), offset metadata $M_o$(볼륨 내 extents=in-volume blocks + checksum), meta-log $M_l$(object name, client proxy, checksum). MetaX = $M_l + M_v + M_o$.
>
> **Distributed ordering 제거 (Fig.1 → Fig.2, §3.2).**
> - 기존 Haystack(Fig.1): meta-log를 C에 먼저 쓰고($M_l$), M이 $M_v$ 영속화 후 반환, 그 다음 D가 데이터 + $M_o$ 영속화 — $M_l \to M_v \to (\text{data} \& M_o)$ 순서 강제.
> - Cheetah(Fig.2): 메타 서버 M이 $M_v$ 할당 + MetaX($M_v+M_o+M_l$) 영속화를 **병렬**로 하고, 동시에 C가 데이터+$M_v+M_o$를 데이터 서버 D에 보낸다. M, D 둘 다 정상 반환해야 `put` committed. distributed ordering 없음.
>
> **Cheetah 아키텍처 (§4.1).** manager cluster(홀수 개, Raft로 system manager 실행, topology map 유지) + meta server cluster(rich meta service, MetaX 유지) + data server cluster(raw block, object-agnostic) + client proxy(portal). n-way replication.
>
> **Hybrid Mapping (§4.2, Fig.3).** CRUSH로 객체를 PG(placement group)로, PG를 1개 primary + n−1 backup meta server로 매핑. PG가 자기 전용 logical volume 그룹 **VG(Vol Group)**를 가짐. 디스크는 physical volume → logical volume(n개 physical을 묶음)로 구성, CRUSH로 복제/조직. 확장 시 새 logical volume을 VG에 추가하거나 PG를 새 meta server로 remap — 객체→VG 멤버십이 불변이라 **데이터 마이그레이션 없음**.
>
> **Object Put (§4.3.1, Pseudocode 1, Fig.4).** C가 CRUSH로 primary meta server $M_1$ 계산, name/size/checksum/reqid 전송 → $M_1$이 VG에서 logical volume($M_v=lvid$) 선택, bitmap allocator로 블록 할당($M_o=extents$), MetaX 구성. 이후 **병렬**: ($M_1$이 C에 $M_v,M_o$ 반환 + backup($M_2,M_3$)에 MetaX 전송 + 로컬 영속화) ∥ (C가 데이터+$M_v+M_o$를 데이터 서버 $D_1,D_2,D_3$에 전송, raw block write). 모두 ack → committed. $M_1$이 객체를 visible(pending 해제)로 만든 뒤에야 get 응답. Step(2)/(3)/(5)에 distributed ordering 없음.
>
> **Object Get (§4.3.2).** C가 CRUSH로 $M_1$ 조회 → $M_v=lvid$, $M_o=extents$, checksum 획득. pending이면 데이터 서버 확인. C가 한 데이터 서버에 read 요청 → 블록 읽고 checksum 검증.
>
> **Object Delete (§4.3.3).** raw-block 기반이라 Tectonic/Haystack의 chunk-file compaction(공간 회수 시 객체 이동→I/O amplification) 불필요. $M_1$이 MetaX 레코드 삭제 + logical volume bitmap의 해당 bit($M_o=extents$) clear → **즉시 공간 재사용**. backup에도 전파.
>
> **MetaX in KV store (§5.2, Table 1).** RocksDB에 3개 KV로 저장하고 atomic batch write. (1) `OBMETA_name → {lvid, extents, checksum}` (2) `PGLOG_pgid_opseq → {name, pxlogkey}` (PG log, meta server crash 복구) (3) `PXLOG_pxid_reqid → {name, pglogkey}` (proxy log, client proxy crash 복구). 각 logical volume마다 in-memory bitmap 유지.
>
> **Recovery (§5).** view number + lease(stale topology 방지) 기반. meta server crash 시 영향 PG를 readonly로 표시 후 새 view로 CRUSH 재계산. data server crash 시 logical volume readonly 후 새 physical volume으로 교체·병렬 복구. client proxy crash 시 PXLOG로 미완료 put 검사(checksum 비교 후 완료 또는 revoke). orphan 방지(공간 할당이 meta server에 미기록). **Linearizability** 보장(Appendix A: Lemma 1/2 증명).

## 평가
> [!note]- 평가 (토글)
> **테스트베드 (§6).** 15대 머신(client 3 + Raft manager, data 9, meta 3), 3-way replication. data machine당 SSD 4개×100 physical volume → 총 3600 physical = 1200 logical volume, 200 VG. 비교 대상: Haystack(SeaweedFS 기반 구현), Tectonic, Ceph(v17.2, CRUSH/BlueStore). COSBench로 1천만 객체 put.
>
> - **Put latency (Fig.5a):** Cheetah가 Haystack 대비 mean put latency 최대 **2.37x** 개선(8KB-20). 병렬 메타/데이터 write + atomic MetaX(별도 메타 I/O 제거) + raw block I/O(파일시스템 오버헤드 회피) 덕분. Ceph보다도 낮음(Ceph의 복잡한 layered design이 concurrency 저하).
> - **Get latency (Fig.5b):** Haystack 대비 최대 **25%** 개선(8KB-500). Cheetah는 raw block만 읽고 Haystack은 in-volume FS 메타+데이터를 모두 읽어야 함.
> - **Delete latency (Fig.5c):** Cheetah는 메타 서버 1회 접근으로 삭제, Haystack은 다중 접근 → significantly outperforms.
> - **Latency 분해 (Fig.6):** small(8KB) put도 수백 µs 수준. Pre-MDS/MDS-1/MDS-2/Pre-DS/DS로 분해, ms-level small object I/O는 업계에서도 흔함.
> - **Throughput (Fig.7):** concurrency<600에서 Cheetah가 Haystack보다 훨씬 높음, peak에서 8KB-put concurrency=1000일 때 약 **6%** 향상. CPU 사용률 항상 <2200%(dual 16-core 비병목).
> - **Ordering 영향 (Fig.9):** Cheetah vs Cheetah-OW(ordered-writes). small object write에서 ordering 영향 최대 **40%**.
> - **Filesystem 영향 (Fig.10):** Cheetah vs Cheetah-FS(XFS). small(8KB-20)에서 약 **10%** 이점.
> - **RocksDB 설정 (Fig.11):** flush/merge rate 증가는 성능 영향 작음 → 메타 복잡도가 확장성 문제 아님.
> - **Meta service scalability (Fig.12):** machine 3→12에서 linear scale-out(약 60K→260K+ req/sec 수준). RAM disk로 상한 측정.
> - **Rich vs thin meta (Fig.13):** rich meta service가 simple directory service보다 약간 낮지만 offset 유지 부담을 데이터 서버에서 메타 서버로 옮긴 결과.
> - **In-expansion (Fig.14):** 확장 중 Cheetah가 Cheetah-NoVG/Ceph(migration)보다 훨씬 우수(VG 덕분 데이터 마이그레이션 없음).
> - **Recovery (Fig.15):** meta server crash 후 영향 PG 메타를 수 초 내 복구. disk failure 복구: 512KB 1천만 객체에서 Cheetah ~16.3초(aggregate 24.9 GB/sec), Ceph ~16.1초(CRUSH placement로 약간 빠름).
> - **Trace/Combined (§6.4, Fig.16~20):** 24대 3주 운영 trace(put≫get, delete 비율 높음, 8.9% delete 등). storage efficiency 항상 **>85%**(Fig.18). YCSB combined(delete 10%, put 10~80%, concurrency=20)에서 put 비율 증가 시 throughput 소폭 감소(Fig.20).

## 섹션 노트
- **§1 Introduction:** small object 저장 수요, directory-based vs hash-based mapping, distributed ordering 문제 제기. Haystack/Tectonic이 directory-based 대표.
- **§2 Background & Motivation:** §2.1 object placement(hash-based CRUSH의 마이그레이션 문제, MapX의 한계), §2.2 directory-based mapping의 volume/offset 분리 문제, §2.3 distributed write ordering(Fig.1, Haystack put 7단계 + 두 개의 → 순서 제약).
- **§3 Metadata Aggregation with MetaX:** §3.1 rich meta service vs thin directory service, §3.2 distributed ordering 없는 일관성(Fig.2 병렬 write).
- **§4 Cheetah Object Store:** §4.1 아키텍처(manager/data/meta server cluster, n-way replication), §4.2 hybrid mapping(PG/VG, CRUSH 확장, Fig.3), §4.3 object storage(put/get/delete, Pseudocode 1, Fig.4).
- **§5 Recovery:** §5.1 topology map/view number/lease, §5.2 RocksDB 3-KV(Table 1), §5.3 crash detection & recovery(meta/data/client proxy crash, concurrent crash, linearizability).
- **§6 Evaluation:** §6.1 micro benchmark, §6.2 design component 영향, §6.3 meta service scalability/recovery, §6.4 trace/combined workload.
- **§7 Discussion:** rich metadata 단점(복잡도/durability)과 대응, read optimization, availability, immutability(non-immutable 시 unique sub-name 또는 two-phase commit), directories vs hashing 딜레마.
- **§8 Conclusion + Appendix A:** local atomicity로 distributed ordering 대체. erasure coding/async replication을 future work로. Lemma 1/2로 consistency 증명.

## 핵심 용어
- **volume metadata ($M_v$):** 객체→data server 디스크 볼륨(logical volume id) 매핑. 전통적으로 중앙 directory에 저장.
- **offset metadata ($M_o$):** 객체 데이터의 볼륨 내 in-volume offset/blocks(extents). 전통적으로 데이터 서버에 데이터와 함께 저장.
- **MetaX:** 한 `put`의 모든 메타데이터($M_v + M_o + M_l$ + checksum)를 집약한 write-optimal 구조. Cheetah 일관성의 핵심.
- **meta-log ($M_l$):** object name, client proxy, checksum 등을 담은 write-ahead 로그. MetaX의 일부.
- **distributed ordering:** crash consistency를 위해 분리된 메타/데이터 쓰기를 특정 순서로 직렬화하는 제약. Cheetah가 제거 대상으로 삼는 것.
- **local atomicity:** 집약된 MetaX 구조를 메타 서버에서 원자적으로 쓰는 성질. distributed ordering을 대체.
- **PG (Placement Group):** CRUSH로 객체를 묶는 단위. 정확히 1개 primary + n−1 backup meta server에 매핑.
- **VG (Vol Group):** PG가 전용으로 관리하는 logical volume 그룹. 확장 시 데이터 마이그레이션을 막는 핵심 장치.
- **logical / physical volume:** physical volume(예: 1TB 디스크를 100GB×10)을 n개 묶어 logical volume 구성, 같은 raw data 복제.
- **hybrid mapping:** CRUSH(hash-based) 계산으로 객체→PG→meta server를 정하되 VG로 마이그레이션-free 확장을 얻는 방식.
- **rich meta service:** offset metadata까지 메타 서버가 보유하는 service(thin directory service 대비).
- **view number / lease:** topology map 변경 추적용 단조 증가 번호 / topology map 안정 기간 임차. stale 메타 접근 방지·get 최적화.

## 강점 · 한계 · 열린 질문
**강점**
- distributed ordering 제거라는 명확한 통찰로 directory-based store의 write 적용 범위를 read-dominant → write/hybrid까지 확장.
- raw block I/O + bitmap allocator로 즉시 공간 재사용, compaction/I/O amplification 회피.
- VG/hybrid mapping으로 hash-based의 확장 시 마이그레이션 문제와 directory-based의 small I/O 성능 문제를 동시 해결.
- linearizability를 증명(Appendix A)하고 다양한 crash 시나리오 복구를 체계적으로 다룸.

**한계**
- **immutability 의존.** mutable object(Ceph-RBD/FS류)에는 unique sub-name이나 two-phase commit이 필요하며 후자는 write 성능을 떨어뜨림(§7).
- 모든 메타를 메타 서버에 집약 → 메타 서버 복잡도/부담 증가(저자도 인정, hybrid mapping으로 완화하나 thin directory보다 throughput 약간 낮음, Fig.13).
- 데이터 서버가 self-contained하지 않아(offset이 메타 서버로 이동) durability 위험이 있어 정교한 복구 메커니즘에 의존.
- 현재 synchronous replication만으로 availability 측면에서 async/hybrid replication은 future work.
- erasure coding 미통합(future work) → 현재 n-way replication 저장 비용.

**열린 질문**
- mutable object 지원 시 two-phase commit이 distributed ordering 제거 이점을 얼마나 상쇄하는가?
- 메타 서버 클러스터가 데이터 클러스터 대비 작다는 가정이 EB급에서도 성립하는가?
- async/hybrid replication 도입 시 일관성 증명(Lemma)이 어떻게 변하는가?

## ❓ Q&A (자가 점검)
> [!question]- Q1. Cheetah가 제거하려는 "distributed ordering"은 정확히 무엇이며 왜 생기는가?
> 답: 한 `put`의 volume metadata(중앙 디렉터리)와 offset metadata(데이터 서버)가 **분리 저장**되기 때문에, crash 후 무엇이 영속화됐는지 추론하려면 메타/데이터 쓰기를 특정 순서로 강제해야 한다(Fig.1의 $M_l \to M_v \to \text{data}\&M_o$). 이 순서가 small object 임계 경로에 wait을 도입해 성능을 떨어뜨린다.

> [!question]- Q2. MetaX는 무엇을 집약하며, 그 집약이 어떻게 ordering을 없애는가?
> 답: MetaX는 $M_v$(volume), $M_o$(offset/extents), $M_l$(meta-log), checksum 등 한 `put`의 모든 메타데이터를 단일 구조로 모은다. 모든 메타가 한 곳에 모이므로 메타 서버에서 **local atomicity**(원자적 쓰기)만으로 메타 일관성을 보장할 수 있고, 따라서 메타 서버 쓰기와 데이터 서버 쓰기 사이의 순서 제약이 사라져 둘을 병렬로 쓸 수 있다.

> [!question]- Q3. immutability가 Cheetah 설계에서 왜 중요한가?
> 답: 객체가 immutable이라 한 객체에 대한 후속 `put`이 기존 객체를 덮어쓰지 않으므로, 동일 객체의 multiple write 순서를 고민할 필요가 줄고 병렬 처리가 정당화된다. mutable이면 한 클라이언트의 잘못된 `put` 메타가 기존 객체를 덮어쓰고 데이터는 유실되는 metadata-data inconsistency가 생길 수 있어, unique sub-name이나 two-phase commit이 필요하다(§7).

> [!question]- Q4. 메타 집약으로 생기는 확장성 문제를 hybrid mapping은 어떻게 푸는가?
> 답: CRUSH로 객체→PG→(1 primary + n−1 backup) meta server를 계산하되, 각 PG에 전용 logical volume 그룹 **VG**를 부여한다. 확장 시 새 logical volume을 VG에 추가하거나 PG를 새 meta server로 remap하는데, 객체→VG 멤버십과 VG의 volume 멤버십이 바뀌지 않으므로 **객체 데이터 마이그레이션이 발생하지 않는다**(Fig.14에서 Ceph migration 대비 우수).

> [!question]- Q5. Cheetah의 delete가 Haystack/Tectonic보다 빠른 이유는?
> 답: Haystack/Tectonic은 객체를 큰 파일에 append하거나 작은 파일로 할당하여, 삭제 후 공간 회수에 **compaction**(남은 객체를 새 파일로 이동, I/O amplification)이 필요하다. Cheetah는 raw block + bitmap allocator를 써서 MetaX 레코드 삭제 + 해당 bit clear만으로 **즉시 공간을 재사용**하므로 compaction이 불필요하고 메타 서버 1회 접근으로 끝난다.

> [!question]- Q6. RocksDB에 MetaX는 어떻게 저장되며 왜 3개의 KV가 필요한가?
> 답: 3개 KV를 atomic batch로 쓴다. (1) `OBMETA_name → {lvid, extents, checksum}`: get/delete용 객체 메타 조회. (2) `PGLOG_pgid_opseq`: **meta server crash** 시 PG의 진행 중 put 메타 복구. (3) `PXLOG_pxid_reqid`: **client proxy crash** 시 해당 proxy의 진행 중 put 추적. 세 KV의 원자적 동시 기록이 일관성을 보장하며 RocksDB/Badger가 이를 지원한다.

> [!question]- Q7. 평가에서 Cheetah의 성능 이점은 주로 어디서 오는가?
> 답: Fig.9/10 분해에 따르면 small object write에서 **distributed ordering 제거(병렬 write)** 영향이 최대 40%로 가장 크고, raw block I/O(파일시스템 회피) 영향이 약 10%다. 결과적으로 Haystack 대비 put 최대 2.37x, get 최대 25% 개선이며, 성능 이점은 주로 ordering 제거에서 온다.

> [!question]- Q8. Cheetah는 어떤 일관성 수준을 보장하며 어떻게 증명하는가?
> 답: **linearizability**(같은 객체에 대한 put/get/delete가 sequential execution과 동등)를 보장한다. Appendix A에서 Lemma 1(committed put이 notify되면 이후 get은 delete 전까지 데이터를 본다)과 Lemma 2(get이 객체를 보면 이후 get도 같은 데이터를 본다)를 view number/lease/immutability에 기반해 증명한다.

## 🔗 Connections
[[File System]] · [[EuroSys]] · [[2025]]

## References worth following
- **Haystack** (Beaver et al., OSDI'10, [21]) — directory-based object store의 대표이자 Cheetah의 주 비교 대상·구현 기반.
- **Tectonic** (Pan et al., FAST'21, [42]) — 파일시스템 메타데이터를 disaggregate하는 hyperscale store. Cheetah의 aggregation과 대비되는 철학.
- **CRUSH** (Weil et al., SC'06, [50]) — hash-based placement. Cheetah가 hybrid mapping으로 확장해 사용.
- **MapX** (Wang et al., FAST'20, [47]) — time-dimension mapping으로 hash-based 확장의 마이그레이션을 줄인 선행작(객체 block storage 한정).
- **Consistency without ordering** (Chidambaram et al., FAST'12, [24]) — distributed ordering 비효율 논의의 이론적 출발점.
- **RocksDB / LSM-Tree** ([3], O'Neil et al. [40]) — MetaX KV의 atomic batch write 기반.

## Personal annotations
<본인 메모 영역>
