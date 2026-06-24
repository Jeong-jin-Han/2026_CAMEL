---
title: "Discard-Based Garbage Collection for Distributed Log-Structured Storage Systems in ByteDance"
aliases: [DisCoGC, Discard-Based GC]
description: "compaction-only GC의 write/space amplification trade-off를 깨기 위해 discard와 compaction을 결합한 DisCoGC를 제안, ByteDance 프로덕션에서 TCO 약 20% 절감."
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/garbage-collection
  - topic/log-structured
  - topic/distributed
  - venue/fast
  - year/2026
---

# Discard-Based Garbage Collection for Distributed Log-Structured Storage Systems in ByteDance

> **FAST 2026** · `cluster/fs` · Source: [Discard-Based Garbage Collection for Distributed Log-Structured Storage Systems in ByteDance.pdf](<Discard-Based Garbage Collection for Distributed Log-Structured Storage Systems in ByteDance.pdf>)

저자: Runhua Bian, Liqiang Zhang, Jinxin Liu, Jiacheng Zhang, Jianong Zhong, Jiahao Gu, Hao Guo, Zhihong Guo, Yunhao Li, Fenghao Zhang, Jiangkun Zhao, Yangming Chen, Guojun Li, Ruwen Fan, Haijia Shen, Chengyu Dong, Yao Wang, Rui Shi, Jiwu Shu, Youyou Lu (ByteDance / Tsinghua University; 교신저자 Youyou Lu)

## TL;DR
ByteDance의 분산 append-only 스토리지 **ByteStore**는 초기에 **compaction-only GC**를 사용했는데, valid data를 새 LogFile로 옮겨 쓰는 과정에서 write amplification(WA)과 space amplification(SA)이 **상충(trade-off)**하여 월 수백만 달러의 TCO를 유발했다. 본 논문은 valid data 이동 없이 stale 영역의 물리 공간을 상수 시간에 회수하는 **discard**를 compaction과 결합한 **DisCoGC(Discard-and-Compaction GC)**를 제안한다. 다층 스토리지 스택(ByteDrive + ByteStore)에 discard를 도입할 때 생기는 boundary loss, metadata overhead, fragmentation, 제한된 trim IOPS 문제를 boundary extension, discard-friendly EC stripe, batching/scheduling/flow control, trim filter/merger로 해결하여 프로덕션 클러스터에서 **logical WA 32% 감소, total WA 25% 감소, TCO 약 20% 절감**을 성능 저하 없이 달성했다.

## 문제 & 동기
append-only 분산 스토리지는 stale data가 차지하는 공간을 주기적으로 회수하는 GC가 필요하다. 기존 ByteDrive+ByteStore는 **compaction**만으로 GC를 수행했다. compaction은 garbage ratio가 높은 LogFile을 스캔하여 valid data를 새 LogFile로 다시 쓰고 옛 LogFile을 삭제한다. 이때 두 가지 비용이 상충한다: SA를 낮추려고 aggressive하게 compaction하면 valid data 재기록으로 **WA가 증가**하고 foreground I/O와 경쟁하며 SSD 수명을 깎는다. 반대로 compaction을 덜 하면 stale data가 쌓여 SA가 커진다. WA = LWA × PWA(logical × physical)이며, 이 trade-off가 월 수백만 달러의 추가 TCO를 만든다.

ByteDance의 프로덕션 trace 분석 결과, AI 모델 다운로드/추론, inverted index 빌딩/업데이트, 분산 컴퓨팅(Spark) 등 워크로드는 write sequentiality가 높아 **전체 write의 절반 이상이 256KiB를 넘는 연속 범위를 수정**하고, 수 초 내에 잦은 overwrite가 발생해 **길고 연속적인(stale) garbage 영역**을 LogFile에 남긴다. 이런 영역은 valid data를 옮길 필요 없이 곧바로 버릴 수 있으므로, GC에 **discard**를 도입하는 것이 합리적이다.

> [!quote]- 📄 원문 표현 (paper)
> "our early works on optimizing compaction present a fundamental trade-off (Fig. 1) between write amplification and space amplification. Write amplification arises from rewriting valid data to new LogFiles during compaction, and space amplification stems from stale data on the SSD." (p.2)
>
> "we find that the workloads with AI model download/inference, inverted index building/updating, and distributed computing (e.g., Spark) show high write sequentiality. In these cases, over half of the writes modify contiguous ranges exceeding 256KiB. These workloads also show frequent overwrites that occur within seconds. This results in long, contiguous ranges of stale data on the LogFiles." (p.2)
>
> "Instead of moving valid data and deleting the entire LogFile, a discard request directly reclaims the space occupied by stale data on LogFiles, efficiently reclaiming long, contiguous ranges of garbage on LogFiles." (p.3)

## 핵심 통찰

> [!note]- 통찰 1: discard와 compaction은 상보적(complementary)이다
> discard는 valid data 이동 없이 stale 영역을 상수 시간에 회수하므로 WA를 늘리지 않고 SA를 줄인다. 단점은 LogFile/chunk에 구멍을 내 fragmentation을 유발하는 것이다. compaction은 반대로 데이터를 옮겨 fragmentation을 막지만 WA를 일으킨다. DisCoGC는 **discard를 고빈도 주 메커니즘**으로, **compaction을 저빈도 보조 메커니즘**으로 조합해 두 기법의 장점을 취한다. (§4.4)

> [!note]- 통찰 2: 다층 스택에서 discard의 진짜 적은 "경계(boundary)"다
> ByteDrive(Volume/Segment Layer) → ByteStore(LogFile/Chunk) → UFS(cluster)로 내려가며 allocation unit이 서로 어긋나(misaligned) 있고, EC stripe·compression·sector header 때문에 discard 범위 경계의 garbage를 회수하지 못하는 **boundary loss**가 누적된다. 이 누적 손실이 회수 불가능한 공간으로 쌓이는 것이 핵심 난제다. (§4.2)

> [!note]- 통찰 3: trim은 양날의 검 — trim IOPS가 부족하면 오히려 독이다
> UFS에서 공간을 "해제"해도 SSD에 trim을 내려야 물리 공간이 실제로 회수된다. 그러나 SSD의 max trim IOPS는 write IOPS보다 훨씬 낮다(예: Model B는 write의 ~3%). trim이 밀리면 invalid data가 SSD에 남아 aggressive SSD GC와 PWA 증가, foreground 품질 저하를 부른다. 따라서 trim을 무조건 많이 내리는 게 아니라 **큰 범위만, 합쳐서** 내려야 한다. (§4.5)

> [!note]- 통찰 4: SSD의 2단계 trim 구현이 임계치 기반 trim filter의 근거다
> trim 크기가 일정 임계치(예: Model A는 128MiB) 이하이면 FTL이 foreground에서 write cache를 무효화하고 LBA-PBA 매핑은 background로 갱신해 latency가 1ms 미만으로 낮고 일정하다. 큰 trim은 write cache flush가 필요해 foreground latency가 튄다 — 라기보다, 작은 trim이 너무 많으면 IOPS 한계에 부딪힌다. 이 관찰이 작은 범위를 모아 큰 trim으로 보내는 filter/merger 설계의 근거다. (§4.5)

## 설계 / 메커니즘

> [!abstract]- DisCoGC 전체 구조와 5단계 discard flow
> **스택 배경(§2):** ByteDrive는 Volume Layer(random read/write/trim, 4KiB)와 Segment Layer(compression + append-only 변환, LSM-Tree로 LBA→물리주소 매핑)로 구성. ByteStore는 LogFile(최대 2GiB, append/read/seal) 단위로 데이터를 관리하며, LogFile은 여러 chunk(수십~수백 MiB, EC 또는 replication)로, chunk replica는 ChunkServer의 **UFS(Userspace Filesystem)**에 저장. UFS는 4KiB sector마다 앞 32B를 header(CRC, data length), 나머지 4064B를 data로 쓰는 self-contained layout을 쓰고, 4 sector(4×4064B)를 1 cluster로 묶어 allocation unit으로 삼으며 MetaPage zone으로 cluster 할당·매핑을 관리.
>
> **Discard 실행 흐름(§4.1, 5단계):** ① BlockServer가 각 segment의 LSM-tree를 스캔해 invalid LogFile 범위 식별 → ② ByteStore SDK로 각 범위에 discard 요청 → ③ SDK가 LogFile 범위를 chunk-level 범위로 매핑하고 chunk replica 위치 파악 → ④ ChunkServer의 UFS가 해당 cluster를 찾아 MetaPage를 수정해 cluster 해제 → ⑤ BlockServer가 성공적으로 discard된 범위를 기록(중복 discard 방지). 회수된 cluster는 새 append에 재사용. discard는 top-down·asynchronous.

> [!abstract]- Boundary loss 완화: boundary extension + discard-friendly EC stripe (§4.2)
> **Boundary loss 두 종류:** (1) **EC loss** — fault tolerance용 EC는 완전한 EC stripe 단위(예: 4+1 EC, 16KiB stripe면 64KiB 배수)로만 discard 가능한데 BlockServer가 보내는 범위는 임의 크기라 정렬되지 않음. (2) **Cluster loss** — UFS는 cluster(4×4064B) 단위로 discard해야 하지만 EC stripe 패킷은 보통 4KiB 배수라 cluster-aligned가 아님.
> **Boundary extension:** discard 연산은 같은 범위에 재진입 가능(reentrant)하므로, BlockServer는 현재 discard 범위가 이전에 discard된 인접 범위와 맞닿으면 현재 범위를 인접 범위 쪽으로 **수 MiB 확장**해 경계 garbage까지 회수하고 새 경계 손실도 막음. (Fig.13)
> **Discard-friendly EC stripe:** EC stripe unit size를 cluster size에 맞춰 **n×4×4064B**로 설정 → cluster loss 제거. LogFile이 임의 크기 요청을 수용하므로 write/discard 효율에 영향 없음. (Fig.14: 16KiB misaligned stripe는 25.6% cluster loss, 4×4064B aligned는 0 loss)

> [!abstract]- Batching / Scheduling / Flow control (§4.3)
> **Discard batching:** 같은 LogFile의 여러 discard 범위를 하나의 discard 요청으로 묶어 UFS의 MetaPage 수정을 한 번만 하도록 함(같은 chunk면 MetaPage 1회만 수정). 한 요청당 최대 범위 수는 1~64로 configurable.
> **Parallelism-aware scheduling:** discard task는 segment 단위로 생성, 병렬도 P로 제한. 주기적으로 discard 범위가 가장 큰 top-k segment 선택(k는 P와 진행 중 task 수로 계산). 단, LogFile 수가 임계치 초과 시 compaction은 LogFile이 가장 많은 top-k segment로 전환해 fragmentation/metadata 압박 완화.
> **Flow control:** batching/parallelism control에도 discardable 범위가 많으면 burst가 생기므로 최대 discard IOPS를 제한해 성능 변동 억제.

> [!abstract]- Compaction-Discard 조정 + Trim filter/merger (§4.4–4.5)
> **조정:** discard를 주 메커니즘(고빈도, lightweight), compaction을 보조(저빈도, anti-fragmentation)로 운영. compaction-only 대비 fragmentation이 약간 늘어 PWA가 2~10% 증가하지만, discard로 data 이동이 줄어 NAND 총 기록 byte가 감소해 SSD 수명은 오히려 연장. **Garbage ratio** = 1 − Valid Data/(Total Data + LPB×Boundaries), LPB(loss per boundary)=EC stripe 길이의 절반으로 추정.
> **Trim filter:** 큰 범위(예: ≥128KiB)만 trim 발행 → trim 수를 줄여 trim IOPS 한계 회피(PWA는 소폭 증가).
> **Trim merger:** LBA상 인접한 작은 범위들을 하나의 큰 범위로 병합해 한 번에 trim.
> **Crash consistency:** discard 메타데이터는 BlockServer 메모리에만 유지, per-segment WAL의 discard LogFile에 "issued"(단계①후)·"successfully discarded"(단계⑤후) 범위를 persist. 재시작 시 둘을 비교해 중단된 discard를 재시도. **Memory:** LogFile별 두 bitmap(issued I / failed F)을 roaring bitmap으로 압축, S = I & (~F)로 success bitmap 유도해 bitmap 크기 25~45% 추가 절감.

## 평가

> [!success]- 프로덕션 클러스터 (§6.2)
> mixed 워크로드(online/SAR/offline) 클러스터. 서버당 dual 24C48T CPU, 256GiB DRAM, 200Gbps network, 16 SSD. **SA: baseline(compaction-only) 1.37 → DisCoGC 1.23** 유지(p.502). **Invalid range size:** 90% 이상이 128KiB 초과, 70% 이상이 1MiB 초과 → mixed 워크로드가 DisCoGC에 적합(Fig.16). **Logical WA: 32% 감소**, SA 10% 감소, PWA는 최대 10% 증가하나 **total WA 25% 감소**(Fig.15a). **Latency/bandwidth: 무시할 수준의 영향**(Fig.15b,c) → discard가 lightweight하기 때문. **TCO: 프로덕션에서 약 20% 절감.**

> [!success]- Overall performance & trace별 효과 (§6.3)
> 3개 trace + FIO(32MiB granularity, 64KiB block random write)로 SA 고정 후 LWA·foreground latency 측정. SA-LWA 곡선이 4개 trace 모두 좌하단으로 이동(더 나은 trade-off, Fig.17). **SAR trace가 가장 큰 개선**(large·contiguous garbage) → TCO 25% 이상 절감 추정. **online은 가장 작은 개선**(fragmented garbage)이나 여전히 향상되어 robustness 입증. 최악의 경우에도 compaction-only로 fallback 가능하며 fallback 모드에서도 **TCO 2~5% 절감**. write latency 영향은 무시할 수준(Fig.18).

> [!success]- Factor analysis (§6.4)
> SA 고정(online 1.2, SAR 1.05, offline 1.2)에서 각 기법 기여 측정(baseline=compaction-only, Fig.19). **+Discard(+flow control):** LWA를 모든 trace에서 8.4~13.9% 감소, discard ratio 0.45~0.88(flow control이 일부 throttle). **+Batch(batch size 64):** LWA를 추가 2.7~11.7% 감소, discard ratio가 거의 1에 근접. **+BoundExt:** LWA를 추가 5.5~16.1% 감소(경계 garbage 회수). foreground bandwidth/latency는 모든 구성에서 안정.

> [!success]- Sensitivity & Trim & 자원 (§6.5–6.7)
> **SSD space usage(60/70/80%):** 높은 사용률일수록 PWA 증가(더 aggressive SSD GC)하나 LWA·latency는 불변(Fig.20). **Discard IOPS limit:** 높을수록 LWA 감소(garbage 더 회수), 단 비-제로 한계에서 IOPS↑는 latency 악화(CPU 경쟁, Fig.21). **Trim(Model A):** trim만으로 PWA 1.4→1.3, delete latency +600us(허용 수준). +Filter(128KiB)는 PWA 1.35, +Merge는 PWA 1.33. **Trim(Model B, trim IOPS 부족):** trim만이면 PWA·delete latency 매우 높음 → +Filter(128KiB)로 PWA 1.74·trim IOPS 2.8K·delete latency 34ms, +Merge로 PWA 1.65(Fig.22). **CPU/Memory(§6.7):** offline trace에서 CPU는 compaction-only의 **82.9%**(compaction 감소), memory는 102.9%(discard bitmap). 튜닝 후 batch size ≈10, CPU overhead 약 1%(평균 1.2%).

## 섹션 노트
- **§1 Introduction:** ByteStore는 ByteDance의 foundational append-only 스토리지. ByteDrive(Elastic Block Storage)+ByteStore 스택에 집중. compaction-only GC의 WA/SA trade-off가 동기. discard 도입의 4대 난제(boundary misalignment, metadata overhead, fragmentation/구멍, trim IOPS 부족) 제시. 4대 기여(trace 특성화, DisCoGC 설계·구현, 프로덕션+testbed 입증, 실전 가이드라인).
- **§2 System Overview:** Fig.2 계층형 스택(Apps → Storage Services(ByteDrive/TOS/NAS/ByteGraph) → ByteStore). §2.1 ByteDrive(Volume/Segment Layer), §2.2 ByteStore(LogFile/Chunk/MetaServer/SDK), §2.2.2 UFS layout(self-contained sector, 4064B data + 32B header, 4-sector cluster). §2.3 [20KiB,200KiB) write의 7단계 data flow 예시.
- **§3 Motivation:** §3.1 compaction GC 한계(WA=LWA×PWA, WA/SA 음의 상관). §3.2 trace 분석 — Table 1(online/SAR/offline trace, W:R ratio·IOPS·BW). Takeaway 1: GC를 off-peak에 돌리는 것은 SAR/offline에 부적합(predictable daily cycle 없음). §3.2.2 write sequentiality(SAR 65% >256KiB). Takeaway 2: SAR/offline은 sequential·frequent overwrite → large contiguous garbage; online은 fragmented.
- **§4 Design:** §4.1 discard mechanism(5단계), §4.2 boundary loss(EC/cluster loss, boundary extension, discard-friendly EC stripe), §4.3 batching/scheduling/flow control, §4.4 compaction-discard 조정, §4.5 trim filter/merger.
- **§5 Deployment & Implementation:** 단계적 배포(offline→per-volume), canary 단계의 mock discard(공간 미해제, 정합성만 검증), crash consistency(WAL), memory management(roaring bitmap).
- **§6 Evaluation:** §6.1 setup(Table 2 SSD Model A/B), §6.2 프로덕션, §6.3 overall, §6.4 factor, §6.5 sensitivity, §6.6 trim, §6.7 CPU/Memory.
- **§7 Lessons Learned:** DisCoGC 채택 판단(sequential+overwrite→주저없이, random+fragmented→득실 따져, mixed→robustness 신뢰). 파라미터 튜닝(batch size 32/64, discard IOPS로 CPU 2% 미만, avg CPU 1.2%). trim 최적화 필요성은 SSD별 trim IOPS에 의존(target SSD를 trim IOPS로 선택). 모니터링(discard ratio<0.4가 10분 이상 & SSD space>85% & 증가 시 SRE alert).
- **§8 Related Work:** write/space amplification(FTL, file system, KV store), cross-layer GC(Open-Channel SSD, trim/discard 활용 IPLFS·Slack Space Recycling). DisCoGC는 cloud 다층 스토리지의 고유 난제를 다루고 실전 경험 제공.

## 핵심 용어
- **DisCoGC (Discard-and-Compaction GC)**: discard를 주, compaction을 보조로 결합한 GC 기법. 핵심 기여.
- **Discard / Trim**: 본 논문에서 "discard"는 소프트웨어 계층 간 연산, "trim"은 SSD 하드웨어 연산을 지칭(상호 교환적 사용). 둘 다 stale 영역을 회수.
- **Compaction**: garbage ratio 높은 LogFile의 valid data를 새 LogFile로 재기록하고 옛 것을 삭제하는 GC. WA를 유발.
- **Write Amplification (WA)**: WA = LWA × PWA. LWA(logical)는 compaction의 valid data 재기록, PWA(physical)는 SSD 내부 GC·wear leveling에서 발생.
- **Space Amplification (SA)**: stale data가 SSD를 점유해 생기는 공간 증폭. WA와 음의 상관.
- **Boundary loss**: 계층 간 allocation unit misalignment·compression으로 discard 범위 경계의 garbage가 회수되지 못해 누적되는 손실. EC loss와 cluster loss로 구분.
- **Boundary extension**: 현재 discard 범위를 이전 discard된 인접 범위 쪽으로 수 MiB 확장해 경계 garbage를 회수하는 기법.
- **LogFile**: ByteStore의 append-only 기본 단위(최대 2GiB), GC의 기본 대상. 여러 chunk로 구성.
- **UFS (Userspace Filesystem)**: ChunkServer의 user-space 파일시스템. 4064B data+32B header sector, 4-sector cluster를 allocation unit으로 사용, MetaPage로 관리.
- **Trim filter / merger**: 큰 범위(≥128KiB)만 trim 발행(filter)하고 인접 작은 범위를 병합(merger)해 SSD trim IOPS 한계를 회피.
- **Roaring bitmap**: issued/failed discard 범위를 압축 표현하는 bitmap(success = issued & ~failed로 유도).

## 강점 · 한계 · 열린 질문
- **강점:** 실제 ByteDance 프로덕션(exabyte-scale)에서 검증된 trace-driven 설계로 TCO 약 20% 절감을 성능 저하 없이 달성. 다층 스토리지 스택에 discard를 도입할 때의 실전 난제(boundary loss, trim IOPS)를 구체적으로 규명하고 해결. 단계적 배포·mock discard·crash consistency 등 production engineering 디테일이 풍부.
- **한계:** online(fragmented) 워크로드에서는 개선폭이 작음(최악 fallback 시 TCO 2~5%). SSD trim IOPS가 부족한 모델(Model B)에서는 filter/merger 추가 튜닝이 필수이며 PWA가 높음. boundary extension의 확장 폭, LPB 추정(EC stripe 절반) 등 휴리스틱에 의존. 평가가 ByteStore/ByteDrive 고유 구조에 강하게 결합되어 일반화 가능성은 제한적.
- **열린 질문:** read latency·read amplification 분석은 향후 과제로 남김(저자도 언급). discard ratio가 낮은 fragmented 워크로드를 위한 적응형 메커니즘은? trim IOPS가 높은 차세대 SSD에서 filter/merger의 필요성은 사라지는가? boundary extension 폭의 자동 튜닝은 가능한가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. compaction-only GC의 근본적 trade-off는 무엇이며 왜 비싼가?
> > 답: valid data를 새 LogFile로 옮겨 SA를 줄이려면 재기록으로 WA가 증가하고 foreground I/O 경쟁·SSD 수명 저하를 일으킨다. 반대로 compaction을 덜 하면 stale data가 쌓여 SA가 커진다. WA와 SA가 음의 상관이라 동시에 최적화할 수 없고, 이 trade-off가 월 수백만 달러의 TCO를 만든다.

> [!question]- Q2. discard가 compaction보다 유리한 워크로드 조건은?
> > 답: write sequentiality가 높고 잦은 overwrite로 **길고 연속적인 stale 영역**이 생기는 워크로드(SAR, offline). 이런 영역은 valid data 이동 없이 상수 시간에 회수 가능. 프로덕션에서 invalid range의 90%+가 128KiB 초과, 70%+가 1MiB 초과여서 적합했다. 반대로 online처럼 fragmented한 경우 효과가 작다.

> [!question]- Q3. boundary loss는 왜 생기고 두 종류는 무엇인가?
> > 답: 다층 스택의 allocation unit이 서로 어긋나고 compression이 끼어 discard 범위 경계의 garbage를 회수하지 못해 누적된다. (1) EC loss: discard는 완전한 EC stripe 단위로만 가능한데 BlockServer 범위는 임의 크기라 미정렬. (2) Cluster loss: UFS는 cluster(4×4064B) 단위로만 discard 가능한데 EC stripe 패킷이 4KiB 배수라 cluster-aligned가 아님.

> [!question]- Q4. boundary loss를 어떻게 두 가지 방법으로 해결했나?
> > 답: (1) **Boundary extension** — discard가 reentrant함을 이용해 현재 범위가 이전 discard 인접 범위와 맞닿으면 수 MiB 확장해 경계 garbage를 회수하고 새 경계 손실도 방지. (2) **Discard-friendly EC stripe** — EC stripe unit size를 cluster size(n×4×4064B)에 맞춰 cluster loss를 0으로 제거.

> [!question]- Q5. trim을 무조건 많이 내리지 않는 이유와 해법은?
> > 답: SSD의 max trim IOPS가 write IOPS보다 훨씬 낮아(Model B는 ~3%) trim이 밀리면 invalid data가 SSD에 남아 aggressive SSD GC·PWA 증가·foreground 저하를 부른다. 해법은 trim filter(≥128KiB 큰 범위만 발행)와 trim merger(인접 작은 범위 병합)로 trim 수를 줄이는 것. Model A는 trim만으로 충분하지만 Model B는 filter/merger가 필수.

> [!question]- Q6. discard와 compaction의 역할 분담은?
> > 답: discard는 lightweight·고빈도 **주 메커니즘**으로 stale 공간을 회수, compaction은 저빈도 **보조 메커니즘**으로 discard가 만든 fragmentation을 정리해 LogFile/chunk 수 증가와 metadata 압박을 완화. 둘은 상보적이며, compaction-only 대비 PWA가 2~10% 늘지만 data 이동 감소로 NAND 총 기록은 줄어 SSD 수명은 오히려 연장.

> [!question]- Q7. crash consistency는 어떻게 보장하나?
> > 답: discard 메타데이터는 BlockServer 메모리에만 두고, per-segment WAL의 discard LogFile에 "issued"(단계①후)와 "successfully discarded"(단계⑤후) 범위를 persist. 재시작 시 두 집합을 비교해 중단된 discard를 찾아 재시도한다. ByteStore의 crash consistency는 기존 설계로 보장.

> [!question]- Q8. 프로덕션에서의 핵심 정량 효과는?
> > 답: SA 1.37→1.23, logical WA 32% 감소, SA 10% 감소, PWA 최대 10% 증가에도 total WA 25% 감소, latency/bandwidth 영향 무시 수준, **TCO 약 20% 절감**. CPU는 compaction-only의 82.9%, avg CPU overhead 약 1.2%.

## 🔗 Connections
[[File System]] · [[FAST]] · [[2026]]

## References worth following
- **[29] IPLFS: Log-Structured File System without Garbage Collection** (Kim et al., USENIX ATC'22) — discard/trim으로 무한 logical address를 가상화해 GC를 없앤 접근. DisCoGC의 discard 활용과 직접 비교 대상.
- **[35] Optimizations of LFS with Slack Space Recycling and Lazy Indirect Block Update** (Oh et al., SYSTOR'10) — stale data를 direct overwrite(discard)로 재사용해 compaction을 회피하는 Slack Space Recycling. discard 기반 GC의 사상적 기반.
- **[17] LightNVM: The Linux Open-Channel SSD Subsystem** (Bjørling et al., FAST'17) — single-layer GC를 위한 host-side 직접 flash 관리. cross-layer GC redundancy 문제의 대안.
- **[25] AegonKV** (Duan et al., FAST'25) — SmartSSD 기반 GC offloading으로 KV-separated LSM의 WA/tail latency/cost 개선. GC offloading 관점의 인접 연구(이 폴더에도 노트 존재).
- **[32] Extending the Lifetime of Flash-based Storage through Reducing Write Amplification from File Systems** (Lu et al., FAST'13) — file system 계층 WA 감소. WA = LWA×PWA 분해의 배경.
- **[18] Windows Azure Storage** (Calder et al., SOSP'11) — append-only 분산 스토리지의 강한 일관성·history 보존 설계. ByteStore의 append-only 선택 근거.

## Personal annotations
<본인 메모 영역>
