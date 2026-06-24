---
title: "Reviving In-Storage Hardware Compression on ZNS SSDs through Host-SSD Collaboration"
aliases: [CCZNS, Reviving ZNS Compression]
description: "ZNS SSD에서 압축 실행은 SSD에 두고 인덱싱은 호스트로 분리하는 host-SSD 협력형 압축 인터페이스(CCZNS) 설계"
venue: HPCA
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/zns
  - topic/zns
  - topic/compression
  - topic/in-storage
  - venue/hpca
  - year/2025
---

# Reviving In-Storage Hardware Compression on ZNS SSDs through Host-SSD Collaboration

> **HPCA 2025** · `cluster/zns` · Source: [Reviving In-Storage Hardware Compression on ZNS SSDs through Host-SSD Collaboration.pdf](<Reviving In-Storage Hardware Compression on ZNS SSDs through Host-SSD Collaboration.pdf>)

저자: Yingjia Wang, Tao Lu, Yuhong Liang, Xiang Chen, Ming-Chang Yang (The Chinese University of Hong Kong; DapuStor Corporation)

## TL;DR
ZNS SSD에 ASIC급 in-storage hardware compression을 살리되, 기존 host-transparent(TCSSD) 방식이 가진 (1) 압축 chunk 위치 추적용 fine-grained on-board DRAM 비용과 (2) zone 경계 때문에 공간 절감을 못 거두는 두 가지 본질적 문제를 해결한다. 핵심 통찰은 ZNS의 logical/physical address가 zone 내에서 자연스럽게 결합(coupled)되어 있으므로, 호스트 소프트웨어가 이미 가진 fine-grained index를 그대로 써서 SSD 내부 압축 데이터를 직접 관리하면 SSD 내부 인덱스가 불필요해진다는 것이다. 이를 위해 CCZNS는 압축 **실행은 SSD에 offload**하되 압축 후 정보(post-compression size 등)는 **호스트로 돌려주는** collaborative ZNS 인터페이스(CW/DR 명령, byte-addressable storage management)를 제안한다. RocksDB+ZenFS 사례연구에서 YCSB Load 기준 write volume을 NO/Snappy/Balloon 대비 평균 44.5%/25.2%/12.6% 감소시키고, throughput을 최대 3.6배(vs NO), host CPU 사용량을 ZSTDPC 대비 평균 73.1%/최대 75.4% 절감한다.

## 문제 & 동기
ZNS SSD는 block interface tax(성능·비용)를 없애 cloud SSD 배포에 유망하지만, 단가를 더 낮추려면 데이터 압축이 필요하다. Host CPU 압축은 CPU를 과소비하고, external/on-chip 하드웨어 가속기는 작은 chunk((de)compression 시 PCIe round-trip)에서 성능이 나쁘다. 산업계가 채택한 in-storage hardware compression(ScaleFlux CSD, Samsung SmartSSD, DapuStor Roealsen6)은 ASIC 기반으로 압축/해제 최대 11GB/s·14GB/s를 5% 추가 area로 달성하지만, 기존 솔루션은 **host-transparent(TCSSD)** 방식이라 ZNS 위에서 sub-optimal하다. 두 가지 본질적 challenge:
- **Challenge #1 (chunk indexing)**: 압축 chunk 위치 추적용 fine-grained index가 ZNS의 저비용 on-board DRAM 목표와 충돌하고, coarse-grained 대안은 공간 낭비/double read로 write amplification과 read 성능을 희생.
- **Challenge #2 (space harvesting)**: ZNS의 명시적 zone 경계 때문에 압축으로 생긴 공간 절감을 거두기 어렵다(zone 확대 시 낭비, 물리 flash 축소 시 per-zone bandwidth 제한).

> [!quote]- 📄 원문 표현 (paper)
> "existing solutions based on the host-transparent methodology are sub-optimal on ZNS SSDs due to two intrinsic challenges: (1) locating compressed chunks and (2) harvesting space savings from compression" (p.1, Abstract)
>
> "our key insight is that, given the coupled logical and physical addresses within each zone, host software can leverage existing fine-grained indexes to directly manage compressed data inside the SSD, which can well resolve the aforementioned challenges" (p.1, Introduction)
>
> "Challenge #1: The fine-grained indexes to locate compressed chunks conflict with the cost objective of ZNS and further worsen the on-board DRAM shortage issue; the compromised coarse-grained approach sacrifices both write amplification and read performance." (p.4)
>
> "Challenge #2: The explicit zone boundaries of ZNS hinder the host from harvesting space savings effectively; due to the mismatching between zone and flash superblock, either enlarging logical zone size or shrinking physical flash space allocation is not practical." (p.4)

## 핵심 통찰

> [!note]- 토글: 핵심 통찰 (Key Insight)
> - **ZNS에서 logical/physical address는 zone 내에서 자연스럽게 coupled** 되어 있다. ZNS는 sequential write 제약으로 zone당 coarse-grained mapping만 필요(전통 SSD 대비 on-board DRAM 10-20배 작음). 이 결합 덕분에 호스트가 이미 관리하는 fine-grained index(RocksDB/F2FS/BtrFS가 chunk를 byte granularity로 관리)를 SSD 내부 압축 데이터를 직접 가리키는 데 재사용할 수 있다.
> - 이로써 (1) SSD 내부 중복 인덱스를 제거 → DRAM-less/저비용 ZNS의 장점 유지, on-board DRAM shortage 문제 회피. (2) logical zone과 physical flash superblock이 항상 일치하므로 공간 절감을 정확·효과적으로 harvest.
> - 결정적 설계 분기: **압축 실행(execution)은 SSD에 두고, 인덱싱(indexing)은 호스트로 분리(decouple)**. 단, 두 쪽이 반대편으로 분리되면 두 가지 장애물 발생 — (a) host/SSD 간 uniform data view 유지(압축이 address·size를 바꾸므로 자동 동기 불가), (b) host-side 압축처럼 byte granularity로 공간 절감하려면 I/O stack에 박힌 logical block granularity(예: 4KB)를 우회해야 함.
> - 두 장애물 해법이 각각 CW/DR 명령(NVMe metadata field 양방향 전달로 post-compression 정보 응답)과 byte-addressable storage management(zone write pointer를 byte 단위로 노출).

## 설계 / 메커니즘

> [!note]- 토글: CCZNS 설계 (CW/DR, byte-addressable, harmonize)
> **Overview (§III-A, p.5, Fig.3)**: CCZNS는 압축 실행만 SSD로 offload하고 post-compression 정보는 호스트로 응답. 두 가지 전용 read/write 명령 도입 — **compressed-write (CW)**, **decompressed-read (DR)**. NVMe **metadata field**를 활용해 (1) (de)compression·read/write 보조 정보 전달, (2) 인덱스 유지용 정보를 호스트로 전달. Linux NVMe driver를 수정해 **bi-directional NVMe metadata transfer**를 지원(수 줄 변경)하여 SSD가 돌려준 정보로 호스트가 인덱스 갱신.
> - CW 흐름: chunk+index를 DMA로 SSD 전송(❶) → compress(❷) → zone에 write(❸) → metadata field에 post-compression size를 덮어써 호스트로 전송(❹) → 호스트가 chunk index 갱신(❺).
> - DR 흐름: 호스트가 index 정보를 metadata field에 첨부(①) → SSD가 zone에서 chunk 위치 파악(②) → compression engine으로 보내 decompress(③) → 호스트로 전송(④).
>
> **CC-specific Read/Write Commands (§III-B, p.6, Fig.4)**: CW/DR은 전통 NVMe read/write에서 reserved bit(최대 2개)로 식별. LBA·NLB 외 추가 정보 — CW는 **intra-block offset**(byte-granular write 위치, write pointer 정합성 검사), DR은 **post-compression size**(byte-granular read 범위). 단일 chunk만 담는 **single-chunk CW/DR**은 큰 파일에 적합하나 작은 chunk(RocksDB 4KB, F2FS 16KB, BtrFS 4KB 기본)에서 I/O 수 폭증.
>   - **Multi-chunk CW/DR**: reserved bit 하나 더로 single/multi 모드 구분. Multi-chunk CW는 write buffer와 협력해 chunk batch flush(metadata에 intra-block offset, NCK(# chunks), 모든 chunk의 pre-compression size 포함; 압축 후 post-compression size로 overwrite). Multi-chunk DR은 prefetching에 사용(intra-block offset, CNLB(압축 후 logical block 수), PDSZ(마지막 block의 padding size) 포함; post-compression size는 OOB로 SSD가 직접 위치 파악 → command·SW 단순화). DR은 [47]의 **prefix-chunk-based prediction**으로 NLB 설정(첫 chunk의 compression ratio로 전체 추정).
>
> **Byte-Addressable Storage Management (§III-D, p.7)**: 압축 chunk 수용 위해 zone의 storage space·write pointer를 **byte 단위**로 노출. flash write granularity 미만 데이터는 capacitor-backed buffer에 머물다 후속 데이터와 merge. 각 chunk의 lightweight metadata(page 내 offset, post-compression size, **feedback size**)를 flash page **OOB**(128-256B/4KB)에 유지 → DR 정합성 검사, chunk 경계·독립 해제, resilience. 추가 공간 오버헤드는 무시 가능(4KB flash·CR 4 가정 시 chunk당 (2+2+4)*4=40B).
>
> **Harmonize CW with Traditional Writes (§III-E, p.7)**: 같은 zone에 CW와 전통 write 혼재 시 granularity 불일치 문제 → 새 zone 추상화 **padding zone** 제안(압축 후 마지막 chunk를 logical block 경계에 자동 align). zone type은 explicit zone open command의 reserved bit로 지정. CR 4·1MB raw·256KB I/O 가정 시 padding으로 인한 평균 공간 amplification은 0.8%(4/2=2KB vs 256KB).
>
> **Discussions (§III-F, p.7)**: 비압축 데이터는 host가 전통 read/write로 처리(불필요한 연산·에너지 회피); FTL도 sampled 압축/entropy 추정으로 unrewarded compression skip. Zone Append 지원은 CW가 actual write address를 byte granularity로 돌려주도록 포팅 가능(현재 호환 호스트SW 부재로 future work).
>
> **RocksDB/ZenFS 적용 (§IV, p.8-9, Fig.5/6)**: data block은 CW/DR로 (de)compression offload, 그 외(index block·filesystem metadata)는 전통 read/write. SST 저장 zone은 padding mode로 open. **index entry 확장** — post-compression size(chunk 위치용)에 더해 pre-compression size(R) 필드 추가(Fig.6). data block의 index entry 구성은 모든 data block persist 후로 지연(CW 완료 후에만 post-compression size 보고 가능, Zone Append와 유사). Multi-chunk CW=flush/compaction write buffer, multi-chunk DR=compaction/iterator prefetch. Crash consistency 영향 없음(index는 chunk보다 늦게 persist 원칙 준수). 엔지니어링 비용: RocksDB ~800 LoC, ZenFS ~300 LoC.

## 평가

> [!note]- 토글: 평가 결과 (수치)
> **Setup (§V-A, p.9)**: VM(32GB, 16 cores, Linux 5.10.9) 위 **FEMU** 에뮬레이터. flash page 16KB, block 32MB, 64 dies, zone=1GB, 총 128GB(WD ZN540 유사). TLC flash read/write/erase = 90µs/700µs/5ms. 각 4KB block에 64B metadata. 하드웨어 압축은 ASIC 통계 참조해 extra latency로 emulate; FEMU에 8(I/O proc)+8+16(compression)=32 core 할당(고품질 emulation용이며 실제 제품엔 불필요). 비교군: NO, Snappy, QAT(deflate), ZSTD(level1), ZSTDPC(6-thread), Balloon-ZNS(ZSTD), CCZNS(ZSTD level1).
>
> **Raw SSD access (§V-B, p.10, Fig.7)**: sequential write에서 CC가 NO 대비 CR=2일 때 37.6%~99.9%, CR=4일 때 49.1%~2.1배 향상. 하드웨어 압축 엔진으로 같은 flash 병렬구조에 더 많은 데이터 program → user-perceived write throughput이 이론 SSD bandwidth를 CR=2/4에서 최대 2.0배/3.1배 초과. sequential read는 NO 대비 최대 20.4%/42.0% 향상. random read는 NO와 유사(±5% 이내); 128KB I/O·CR=2/4에서 평균 latency 38.8%/36.0%, 99.99p 55.9%/60.2% 감소(read 시 더 적은 flash page 접근). metadata DMA transfer는 SSD 총 처리시간의 <1%(영향 미미).
>
> **RocksDB (§V-C, p.10-11, Fig.8-11, Table IV/V)**: RocksDB 8.11.0 + YCSB, block cache 4GB(direct I/O), datasets = Amazon/Reddit(실데이터)+random(1KB, 상대적 비압축).
> - **Write volume (YCSB Load, Fig.8)**: CC가 NO/Snappy/Balloon 대비 평균 **44.5%/25.2%/12.6%** 감소. QAT/ZSTD/ZSTDPC 대비는 6.5%/8.8%/8.6% 더 많음(padding으로 인한 minor extra write amplification, 허용 가능).
> - **Throughput (Fig.9)**: 전 YCSB workload에서 CC가 최고. NO/Snappy/QAT/ZSTD/ZSTDPC/Balloon 대비 평균 **3.6×/43.7%/3.58×/3.21×/75.1%/65.3%** 향상(Load). read-intensive에서도 10.2%/11.7%/32.0%/24.1%/20.2%/10.4% 향상.
> - **Latency (Fig.10)**: YCSB Load에서 ZSTDPC 대비 avg/99.9p/99.99p latency 49.5%/49.5%/56.8% 감소; Balloon 대비 50.0%/47.9%/53.7% 감소. YCSB C(read-only)는 decompression 오버헤드가 compression보다 작아 개선폭 작음.
> - **CPU/Memory (Table IV/V)**: host CPU(YCSB Load, Amazon) avg/max를 ZSTDPC 대비 73.1%/75.4% 절감(NO/Balloon급). system memory는 확장된 index block 때문에 증가하나 ZSTDPC 대비 +2.9%(Load)/+2.8%(C)로 허용 가능.
>
> **Extended study (§V-D, p.12, Fig.11-12)**: block size 4→32KB에서 CC가 YCSB Load avg 3.6×/3.8×/1.6×(vs QAT/ZSTD/ZSTDPC), YCSB C 26.2%/44.7%/29.6% 향상. multi-chunk 없이 single-chunk DR만 쓰면 throughput 55.2%로 급락, single-chunk CW는 20M KV 적재 시 공간 할당 실패. extra write volume 비율(CC vs ZSTD)은 적재량 증가 시 감소(30M KV 시 4.0%/5.9%/2.8% — LSM 자체 WA가 커지며 영향 희석). 극단적 비압축 데이터는 host가 전통 read/write로 전환해 NO급 성능 확보.
>
> **요약**: write volume·cost를 최대화(minor loss)하면서 throughput·latency·CPU 모두 best급 — 다른 어떤 scheme도 이 모든 장점을 동시 제공 못함.

## 섹션 노트
- **§I Introduction (p.1-2)**: ZNS+압축의 cost 동기, host CPU/external/on-chip 압축 한계, in-storage HW 압축의 유망성과 두 challenge, CCZNS·CW/DR·byte-addressable 개요, RocksDB/ZenFS 사례연구 예고.
- **§II Background & Motivation (p.2-5)**: ZNS 개념(coupled address, coarse mapping, device-level GC 제거), 4가지 압축 아키텍처 비교(Table I), TCSSD 특성과 Table II(on-chip/external QAT throughput), 두 challenge 정식화(Fig.2), Balloon-ZNS 한계(zone을 superblock에 묶어 per-zone bandwidth 제한, 평균 write throughput이 CCZNS의 60.5%), Key Insight.
- **§III Design of CCZNS (p.5-8)**: Overview(Fig.3) → CW/DR 명령 포맷(Fig.4, single/multi-chunk) → multi-chunk aggregation → byte-addressable storage(OOB metadata, feedback size) → padding zone으로 전통 write 조화 → 비압축 데이터·Zone Append·FDP discussion.
- **§IV Unleashing the Potential (p.8-9)**: 일반화 원칙 2개(fine-grained host-side index 필요, index는 chunk보다 늦게 persist), RocksDB/ZenFS 배경(LSM, SST, compaction), CW/DR 적용, index entry 확장(Fig.6), FEMU 하드웨어 압축 emulation 방식.
- **§V Evaluation (p.9-12)**: setup, raw SSD access(Fig.7), RocksDB(Fig.8-11, Table IV/V), extended study(Fig.12), 요약.
- **§VI Related Work (p.12-13)**: ZNS interface/design, in-storage computing(FusionFS CISCOps 대비 CCZNS의 압축 특화), in-storage HW 압축(Balloon-ZNS와 차별점=host-transparent 문제 근본 해결).
- **§VII Conclusion (p.13)**: 새 compression system architecture로 in-storage HW 압축을 ZNS에서 부활, host가 fine-grained index로 SSD 내부 압축 데이터 직접 관리.

## 핵심 용어
- **ZNS (Zoned Namespace)**: 저장공간을 zone으로 나눠 zone 내 sequential write만 허용, logical/physical address가 결합되어 coarse-grained zone-level mapping만 필요한 SSD 인터페이스. block interface tax 제거.
- **In-storage hardware compression**: FPGA/ASIC 압축 엔진을 SSD 내부에 통합, (de)compression을 device-side I/O path에서 inline 수행(host CPU·PCIe round-trip 제거). ASIC 기준 11/14GB/s, 5% extra area.
- **TCSSD (Transparent Compression SSD)**: 압축을 host-transparent하게 수행하는 기존 in-storage 압축 SSD(ScaleFlux/SmartSSD/Roealsen6). block interface 준수, fine-grained mapping과 enlarged logical volume 노출.
- **CCZNS / CC (Collaborative Compression)**: 압축 실행은 SSD, 인덱싱은 host로 분리하는 본 논문의 advanced ZNS 인터페이스.
- **CW (Compressed-Write) / DR (Decompressed-Read)**: CC 전용 read/write 명령. NVMe reserved bit로 식별, metadata field로 압축 정보 양방향 전달.
- **Multi-chunk CW/DR**: 한 I/O에 여러 chunk를 담는 모드. write buffer flush·prefetching과 협력해 작은 chunk 시 I/O 수 감소.
- **Byte-addressable storage management**: zone write pointer·storage space를 byte 단위로 노출해 압축 공간 절감을 최대한 harvest하는 storage 관리.
- **Padding zone**: 마지막 chunk를 압축 후 logical block 경계에 자동 align하는 새 zone 추상화. CW와 전통 write를 같은 zone에서 조화(평균 0.8% amplification).
- **Feedback size**: CW가 host에 보고하는 크기(post-compression size와 다를 수 있음). 같은 zone 전통 write 조화·flash page 경계 chunk 제한 등 최적화에 활용.
- **Balloon-ZNS**: SOTA host-transparent 압축 ZNS. zone당 일관된 CR locality를 이용해 여러 zone을 한 superblock에 묶어 chunk를 비례 축소 배치. flash 병렬성 제한이 단점.

## 강점 · 한계 · 열린 질문
**강점**
- ZNS의 address coupling이라는 구조적 사실을 정확히 짚어 SSD 내부 인덱스를 제거 → ZNS 저비용·DRAM-less 목표를 훼손하지 않으면서 fine-grained 압축 관리 달성.
- 압축 실행은 SSD HW에, 인덱싱은 host SW에 두는 분업이 명료하고, NVMe driver 수정이 수 줄로 minimal, host SW 적응도 ~1100 LoC로 moderate.
- 평가가 raw access·system-level(RocksDB)·micro/macro·CPU/memory·latency·extended까지 포괄적이고 SOTA(Balloon-ZNS)·다양한 압축 baseline과 직접 비교.

**한계**
- FEMU 에뮬레이터 기반(실 ASIC HW 미사용); 하드웨어 압축 latency를 통계로 emulate하고 emulation에 32 core 사용(실 제품엔 없음이라 밝히나, 실측 검증은 future work).
- padding으로 인한 minor extra write volume(ZSTD 대비 ~8%); index block 확장으로 host memory 증가.
- Zone Append 기반 CW는 호환 host SW 부재로 미평가(future work); FDP로의 확장도 vision 단계.
- QAT는 다른 HW라 head-to-head 비교 부적절하다고 명시(external/on-chip 대표값으로만 사용).

**열린 질문**
- 실 ASIC 압축 SSD에서 latency·area·throughput 수치가 emulation과 얼마나 부합하는가?
- multi-chunk DR의 prefix-chunk prediction이 CR이 chunk마다 크게 변동하는 workload에서 NLB 과/소추정으로 인한 손실은?
- byte-addressable write pointer·capacitor buffer가 power-loss·crash 시 데이터 정합성을 어떻게 보장하는가(OOB metadata 외)?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 host-transparent in-storage 압축(TCSSD)이 ZNS에서 sub-optimal한 두 가지 본질적 이유는?
> > (1) 압축 chunk 위치 추적을 위한 fine-grained index가 필요한데, 이것이 ZNS의 저비용·on-board DRAM 절감 목표와 충돌(coarse-grained 대안은 공간 낭비·double read로 write amplification·read 성능 희생). (2) ZNS의 명시적 zone 경계 때문에 압축 공간 절감을 harvest하기 어렵다(logical zone 확대 시 낭비, 물리 flash 축소 시 per-zone bandwidth 제한).

> [!question]- Q2. CCZNS의 핵심 통찰(key insight)을 한 문장으로?
> > ZNS에서 logical/physical address가 zone 내에서 coupled되어 있으므로, host가 이미 가진 fine-grained index를 그대로 써서 SSD 내부 압축 데이터를 직접 관리하면 SSD 내부 중복 인덱스 없이 두 challenge를 모두 해결할 수 있다.

> [!question]- Q3. CW와 DR 명령은 전통 NVMe read/write와 어떻게 다르며 어떤 정보를 추가로 주고받는가?
> > reserved bit(최대 2개)로 식별되는 전통 read/write의 변형이다. CW는 intra-block offset(byte-granular write 위치)을 주고 SSD가 post-compression size를 metadata field에 덮어써 host로 돌려준다(인덱스 갱신용). DR은 post-compression size로 byte-granular read 범위를 지정한다. 이를 위해 Linux NVMe driver를 bi-directional metadata transfer로 수정(수 줄).

> [!question]- Q4. single-chunk가 아니라 multi-chunk CW/DR이 왜 필요한가?
> > RocksDB(4KB)/F2FS(16KB)/BtrFS(4KB)처럼 chunk가 작으면 single-chunk는 chunk마다 I/O가 생겨 I/O 수가 폭증해 성능 저하. multi-chunk CW는 write buffer flush와, multi-chunk DR은 prefetching과 협력해 한 I/O에 여러 chunk를 담는다. 평가에서 single-chunk DR만 쓰면 throughput이 55.2%로 떨어지고 single-chunk CW는 공간 할당에 실패.

> [!question]- Q5. 같은 zone에 압축 write(CW)와 전통 write를 섞을 때 생기는 문제와 해법은?
> > write granularity 불일치 문제가 생긴다. 해법은 padding zone — 압축 후 마지막 chunk를 logical block 경계에 자동 align하는 새 zone 추상화(explicit zone open command의 reserved bit로 지정). CR 4·256KB I/O 가정 시 평균 공간 amplification은 0.8%에 불과.

> [!question]- Q6. byte-addressable storage management에서 OOB metadata에 무엇을 저장하고 왜 유용한가?
> > 각 chunk의 (page 내 offset, post-compression size, feedback size)를 flash page OOB(128-256B/4KB)에 둔다. DR의 주소 정합성 검사(불법 read 거부), chunk 경계·독립 해제(효율적 multi-chunk DR), feedback/actual size 분리로 인한 resilience·최적화(예: flash page 경계 chunk 제한)에 쓰인다. CR 4 가정 시 4KB당 40B로 오버헤드 미미.

> [!question]- Q7. RocksDB+ZenFS에서 CCZNS는 어떤 부분에 CW/DR을 적용하고 어떤 부분은 전통 명령을 쓰는가?
> > SST의 data block은 CW/DR로 (de)compression을 offload하고, index block·filesystem metadata 등 그 외는 전통 read/write를 쓴다. SST zone은 padding mode로 open. index entry는 post-compression size에 더해 pre-compression size(R) 필드를 추가하고, data block index entry 구성은 모든 data block persist 후로 지연(CW 완료 후에만 post-compression size 보고 가능).

> [!question]- Q8. RocksDB 평가에서 CCZNS의 대표 수치(write volume, throughput, CPU)는?
> > YCSB Load 기준 write volume을 NO/Snappy/Balloon 대비 평균 44.5%/25.2%/12.6% 감소, throughput을 NO 대비 3.6×·ZSTDPC 대비 75.1%·Balloon 대비 65.3% 향상, host CPU를 ZSTDPC 대비 평균 73.1%/최대 75.4% 절감. host memory는 ZSTDPC 대비 +2.9%로 minor.

## 🔗 Connections
[[ZNS]] · [[HPCA]] · [[2025]]

## References worth following
- **Balloon-ZNS** (Wang et al., DAC 2024, [93]): SOTA host-transparent 압축 ZNS. CCZNS의 직접 비교 대상이자 차별점 출발점.
- **eZNS** (Min et al., OSDI 2023, [74]) / **ZNS: Avoiding the Block Interface Tax** (Bjørling et al., ATC 2021, [30]): ZNS 인터페이스 한계·기본 설계 이해.
- **Zone Append** (SDC 2020, [3]): index를 chunk보다 늦게 persist하는 원칙의 기반, CW의 Append 확장 근거.
- **FusionFS / CISCOps** (Zhang et al., FAST 2022, [99]): in-storage computing offload 추상화. CCZNS의 압축 특화 설계와 대조.
- **QATzip / Intel QAT** ([11][18]) & **ASIC zstd accelerator** (Chen et al., IPDPSW 2021, [33]): external/on-chip vs in-storage 압축 성능 비교 맥락.
- **[47] To zip or not to zip** (Holmberg, FAST 2013): multi-chunk DR의 prefix-chunk-based compression ratio prediction 출처.

## Personal annotations
<본인 메모 영역>
