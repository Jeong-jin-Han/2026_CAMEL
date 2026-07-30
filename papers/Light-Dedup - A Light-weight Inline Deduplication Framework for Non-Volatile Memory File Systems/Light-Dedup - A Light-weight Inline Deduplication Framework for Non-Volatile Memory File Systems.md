---
title: "Light-Dedup: A Light-weight Inline Deduplication Framework for Non-Volatile Memory File Systems"
aliases: [Light-Dedup]
description: "NVM의 read/write 비대칭·긴 media read latency·coarse access granularity를 정면으로 고려해, non-cryptographic hash + speculative prefetch(LRBI)와 region 기반 in-NVM metadata table(LMT)로 inline deduplication 성능과 aged 상태에서의 metadata I/O amplification을 동시에 개선한 NVM 파일시스템용 경량 dedup 프레임워크."
venue: ATC
year: 2023
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/fs
  - topic/persistent-memory
  - topic/deduplication
  - topic/filesystem
  - topic/prefetching
  - venue/atc
  - year/2023
  - list/26s-v2
---

# Light-Dedup: A Light-weight Inline Deduplication Framework for Non-Volatile Memory File Systems

> **ATC 2023** · cluster/fs · Source: [Light-Dedup - A Light-weight Inline Deduplication Framework for Non-Volatile Memory File Systems.pdf](<Light-Dedup - A Light-weight Inline Deduplication Framework for Non-Volatile Memory File Systems.pdf>)

저자: Jiansheng Qiu†* §, Yanqi Pan†*, Wen Xia†✉ (교신저자), Xiaojia Huang†, Wenjun Wu†, Xiangyu Zou†, Shiyi Li† (Harbin Institute of Technology, Shenzhen), Yu Hua‡ (Huazhong University of Science and Technology). (†* 공동 1저자, § 현재 Tsinghua University 재직)

## TL;DR
NVM(예: Optane DCPMM)은 비싸서 deduplication으로 논리적 용량을 늘리고 비용을 낮추는 것이 유망하지만, 기존 NVM dedup 연구는 NVM의 I/O 메커니즘(읽기/쓰기 비대칭, 긴 media read latency, coarse access granularity)을 충분히 고려하지 못했다. Light-Dedup은 (1) non-cryptographic hash(xxHash)와 speculative-prefetch 기반 byte-by-byte content-comparison을 결합한 **LRBI**로 NVM의 메모리 인터페이스를 이용해 비동기 read를 실현하고 content-comparison의 media read latency를 숨기며, (2) region(4KiB) 단위 linked list로 조직된 in-NVM metadata table **LMT**로 aged(fragmented) 파일시스템에서도 metadata I/O amplification을 낮게 유지한다. NOVA 기반 Linux kernel 5.1.0 구현으로 NOVA·NV-Dedup·DeNOVA 대비 1.01–8.98배 throughput, speculative prefetch 자체는 0.3–118% 성능 향상을 가져온다.

## 문제 & 동기 (p.101–104)
NVM(DCPMM)은 HDD/SSD보다 훨씬 비싸 비용 절감이 시급하고, deduplication은 이를 위한 유망한 접근이다. 그러나 전통적 disk 기반 dedup(cryptographic hash)은 NVM처럼 빠른 장치에서 CPU 오버헤드가 병목이 되고, 기존 NVM 전용 dedup(NV-Dedup, DeWrite 등)도 NVM의 두 I/O 특성을 충분히 이용하지 못한다: (1) **read/write asymmetry** — NVM의 read bandwidth가 write보다 최대 3배 높고, write는 buffer가 media write latency를 숨겨주지만 read는 그렇지 않음 (2) **coarse media access granularity**(예: DCPMM XPLine 256B) 대비 dedup metadata(16–64B/block)의 크기 불일치로 metadata I/O amplification 발생. 저자들의 직접 실험: NV-Dedup은 duplication ratio 0→75%에서 sequential write bandwidth가 52.5% 하락하며, 그 원인은 MD5 cryptographic hash 계산이 전체 쓰기 시간의 64.9%를 차지하기 때문(p.104). content-comparison 기반 접근(LD-w/o-P)에서는 두 번째 쓰기(100% 중복)의 content-comparison 시간이 3263.0ns로 전체 dedup latency의 78.8%를 차지(Table 1, p.104). Aged 파일시스템에서 entry-based metadata management(NV-Dedup/LO-Dedup 방식)를 재현한 실험에서는 블록당 read/write amplification이 각각 약 19.35×/9.86×까지 치솟음(Table 2, p.104–105).

> [!quote]- 📄 원문 표현 (paper)
> - "As the next-generation storage media, deduplication for expensive NVM is profitable and urgent." (§3.1, p.103)
> - "The read bandwidth of NVM is up to 3× than its write. [...] This feature is common for persistent storage media such as Phase Change Memory (PCM), STT-RAM, memristor, 3D-XPoint, NAND flash, etc." (§2.1, p.102)
> - "In summary, the severe metadata I/O amplification observed in Table 2 wears out NVMs and leads to performance degradation under aging file systems." (§3.3, p.105)

## 핵심 통찰 (Key Insight)

> [!note]- 통찰 1 — NVM의 memory interface는 CPU prefetch로 read를 비동기화할 수 있다
> NVM은 memory bus에서 CPU load/store로 접근되므로, storage 프로토콜과 달리 prefetch 명령어(예: x86 `prefetcht0`)로 media read를 CPU 연산과 겹칠 수 있다. content-comparison이 dedup latency의 78.8%를 차지하는 상황(Table 1)에서, 이 prefetch 가능성을 활용하면 content-comparison에 의한 read latency를 상당 부분 숨길 수 있다는 것이 LRBI의 출발 동기다.

> [!quote]- 📄 원문 인용
> "We can leverage memory prefetch instructions to enable asynchronous NVM reads and thus accelerate content-comparison." (§3.2, p.104)

> [!note]- 통찰 2 — region(page) 단위로 metadata를 관리하면 aging에도 locality가 무너지지 않는다
> mimalloc의 page 단위 free-list 샤딩에서 착안해, dedup metadata entry를 개별(entry) 단위가 아니라 4KiB region 단위로 순차 할당하면, 파일시스템이 aged(fragmented)되어도 접근이 region 내에서는 여전히 sequential에 가깝다. entry-based(free-list) 관리는 aging 시 할당 위치가 무작위가 되어 metadata read/write가 랜덤 NVM 접근이 되지만, region 기반은 이를 회피한다.

> [!quote]- 📄 원문 인용
> "managing deduplication metadata in the region (i.e., 4 KiB block) granularity to maintain access locality, which elegantly reduces metadata I/O amplification." (§4, p.105)

> [!note]- 통찰 3 — non-cryptographic hash + content-comparison의 결합이 NVM의 비대칭 자체를 이용한다
> Cryptographic hash(MD5/SHA)는 계산 비용이 커 CPU를 굶주리게 하지만, non-cryptographic hash(xxHash)만 쓰면 hash collision을 검증할 수 없다. LRBI는 non-cryptographic hash로 대부분의 non-duplicate 블록을 빠르게 걸러내고, 동일 fingerprint를 가진 소수 블록만 byte-by-byte content-comparison으로 검증한다 — 즉 "느린 duplicate write"를 "더 빠른 read(content-comparison)"로 바꿔치기해, read가 write보다 빠른 NVM의 비대칭을 이용한다.

> [!quote]- 📄 원문 인용
> "read/write asymmetry can be leveraged to improve NVM deduplication performance by combining non-cryptographic hash with content-comparison, trading the slow duplicate writes for the faster reads." (§4.1, p.105)

## 설계 / 메커니즘 (Design)

> [!abstract]- 전체 아키텍처 (Figure 2, p.105)
> Light-Dedup은 두 핵심 컴포넌트로 구성된다: (1) **LRBI(Light-Redundant-Block-Identifier)** — non-cryptographic hash(xxHash) fingerprinting + speculative prefetch 기반 content-comparison으로 duplicate block 식별. (2) **LMT(Light-Meta-Table)** — fingerprint→physical block 매핑과 speculative-prefetch hint를 저장하는 region 기반 in-NVM metadata table. In-DRAM index는 Linux 커널의 `rhashtable`(동적 리사이즈 해시테이블, RCU lock 기반 동시성 제어)로 fingerprint→LMT 위치를 검색한다.

> [!abstract]- 기본 dedup 로직: write/read/deletion (§4.2, p.106)
> **Write**: 입력 4KiB 블록의 xxHash fingerprint 계산 → rhashtable에서 LMT entry 검색 → entry가 있으면 byte-by-byte content-comparison. 동일하면 duplicate로 판정해 refcnt 증가 + 파일시스템 메타데이터에 duplicate block number 기록; 다르면 non-dedup block으로 취급(LMT에 등록 안 함); entry가 없으면 unique block으로 정상 기록 후 새 entry(refcnt=1) 할당. **Read**: 비-dedup 경로와 동일. **Deletion**: block number→metadata offset의 역방향 매핑 테이블(추가 8B write)을 유지, refcnt를 감소시켜 0이 되면 entry 회수. 이상적으로는 블록당 40B NVM read/write(fingerprint 32B+blocknr 8B)가 필요하지만, entry-based 관리는 aging 시 이를 크게 웃돈다(Table 2).

> [!abstract]- LRBI의 speculative prefetch: In-Block Prefetch(IBP) + Cross-Block Prefetch(CBP) (§4.3, Figure 3–4, p.106–107)
> - **IBP**: 한 블록 안에서 2단계로 prefetch한다. 먼저 XPLine(256B) stride로 16개의 `prefetcht0` 명령을 발행해 블록 전체를 NVM read buffer/CPU cache에 로드하고, 이어서 64B(cache line) stride로 두 번째 wave를 발행해 read buffer의 데이터를 CPU cache로 옮긴다. 이는 하드웨어 prefetcher가 한 번에 앞선 2 cache line만 프리페치하는 한계와, CPU 코어가 동시에 처리 가능한 in-flight prefetch 수가 제한적인 문제(예: 8–16개)를 우회한다.
> - **CBP**: 현재 블록의 content-comparison이 진행되는 동안, 다음에 비교될 것으로 예측되는 블록을 미리 prefetch한다. 각 LMT entry의 61-bit **hint** 필드가 다음 speculated block의 entry 위치를, 3-bit **trust degree**(0–7, threshold=4)가 그 hint의 신뢰도를 기록한다. 예측이 맞으면 trust degree +1, 틀리면 -2. 추가로 CPU별 **stream trust degree**를 유지해 워크로드의 locality를 반영하고, trust degree가 threshold 이상일 때만 prefetch를 신뢰·수행한다.
> - **Prefetch-Current(PC)/Speculation(SP)/Prefetch-Next(PN)**: PC는 fingerprint 계산·인덱싱과 현재 블록 NVM read를 병렬화; SP는 hint로 fingerprint 계산·인덱싱 자체를 skip; PN은 가장 공격적으로 다음 입력 블록과 비교될 블록을 미리 prefetch해 CPU-NVM I/O 병렬성을 최대화한다(Figure 4).
> - **Transition**: 동시에 NVM에 접근하는 스레드 수가 커널 모듈 파라미터 임계값(기본 6) 이상이면 다음 블록 prefetch를 비활성화해 NVM read buffer contention을 줄인다. 최종 CBP = PN + Transition.
> - IBP와 CBP는 상보적이다: 워크로드의 duplication continuity가 좋으면(hint 신뢰도 높음) CBP가 자주 트리거되고, 그렇지 않으면 IBP로 fallback한다.

> [!quote]- 📄 원문 인용
> "Speculative prefetch leverages NVM's memory interface and uses In-Block and Cross-Block Prefetch techniques to asynchronously load speculated data into CPU/NVM buffers, which exploits the parallelism of NVM I/O and CPU computation and thus markedly hides read latency." (§4, p.105)

> [!abstract]- LMT: region 기반 in-NVM metadata layout (§4.4, Figure 5–6, p.108)
> LMT는 fingerprint→physical block 매핑과 speculative-prefetch hint를 저장하는 in-NVM table로, **region(4KiB, 블록 크기와 정렬) 기반 linked list**로 조직된다. 각 metadata entry는 32B(8B blocknr + 8B fp(xxHash) + 8B refcnt + 8B hint)이고, 하나의 region은 최대 64개 entry(4KiB/32B/2, 절반만 사용 가능하게 제한)를 담을 수 있다. DRAM 변수 **Cur Region**이 현재 사용 중인 region을 가리키고, entry는 이 region 안에서 거의 순차적으로 할당된다(Entry Allocation: 빈 entry 탐색 → 없으면 Cur Region eviction → Region Queue에서 재사용 가능한 region 획득 또는 파일시스템 block allocator로 새 region 할당). Region은 entry의 절반 이하가 사용 중일 때만 "allocatable"로 간주되어 Region Queue(DRAM, XArray로 개수 추적)에 들어간다. **Entry Deletion**: 대상 entry의 blocknr을 0으로 설정하고, 해당 region의 유효 entry가 절반 이하가 되면 Region Queue에 되돌린다. 이 설계는 **GC(garbage collection)를 완전히 회피**한다 — region을 절반이 빌 때만 재사용 가능하게 하는 대신, NVM 용량 $x$ 기준 최대 $\frac{2x}{4\text{KiB}}$개 entry(entry당 32B)만큼, 즉 최대 $\frac{2x}{4\text{KiB}}\times32\text{B}/x \approx 1.56\%$의 NVM 공간을 GC-free 설계의 대가로 지불한다(worst case). 실측으로는 128GiB non-duplicate 파일 기록 시 region 공간이 데이터 크기의 0.79%였다(p.109).

> [!quote]- 📄 원문 인용
> "Light-Dedup does not reclaim allocated regions and allows to reuse them when half of the entries are free. In other words, Light-Dedup trades more NVM space for GC-free design." (§4.4, p.108)

> [!abstract]- Crash consistency & 구현 (§4.5–4.6, p.109)
> Light-Dedup은 NVM 파일시스템(NOVA)의 recovery 과정과 협력하는 **lazy** crash consistency 전략을 쓴다. Normal recovery(clean unmount)에서는 in-DRAM rhashtable 항목과 유효 entry 카운트를 NVM 예약 영역에 저장했다가 리마운트 시 재구성한다. Failure recovery에서는 dedup metadata를 스캔해 두 불일치(파일시스템만 참조하는 block이 미등록된 경우 / metadata table만 참조하는 잘못된 entry)를 교정하고 in-DRAM 구조를 재구축한다. Light-Dedup은 Linux kernel 5.1.0 기반 NOVA 위에 구현되었으며, 코드는 https://github.com/Light-Dedup/Light-Dedup 에 공개되어 있다.

## 평가 (Evaluation)

> [!success]- 실험 환경 (§5.1, p.109)
> Intel Xeon Gold 5218(16 core/32 thread, 2.3GHz, 22MiB L3, `clwb` 지원), 512GiB Optane DCPMM(2×256GiB, non-interleaved AppDirect), 128GiB DRAM(4×32GiB), CentOS + kernel 5.1.0(NOVA 수정). 비교 대상: NOVA, NV-Dedup(소스 미공개라 재구현), DeNOVA(DeNOVA-Immediate, background Dedup Daemon 적극 실행), LD-w/o-P(Light-Dedup에서 speculative prefetch만 제거한 버전). FIO(`sync` I/O engine)로 측정, 각 측정 5회 반복, 변동계수(CV) 5% 미만.

> [!success]- Microbenchmark (§5.2, Figure 7, p.109–110)
> - 4KiB block I/O: duplication ratio ≥75%에서 Light-Dedup은 NV-Dedup 대비 **1.70–4.58×** throughput(NV-Dedup의 cryptographic hash 오버헤드 때문), NOVA 대비 **1.05–2.28×** throughput. 0% duplication ratio에서는 NOVA보다 **3–15% 느림**.
> - IBP만으로 LD-w/o-P 대비 single thread에서 **1–52%** 성능 향상.
> - 2MiB I/O, single thread에서 duplication ratio ≥75%일 때 Light-Dedup은 LD-w/o-P 대비 **72–118%** 성능 향상(CBP가 syscall 내 locality를 활용).
> - Read-write interference 실험(50% duplication, 8 threads, 4 reader+4 writer 혼합): mixed workload 전체 bandwidth가 2368 MiB/s로, non-interfered 시스템 대비도 개선됨을 보여 read-write 간섭 속에서도 이득 유지.

> [!success]- 실세계 워크로드 (§5.3, Table 3, Figure 8–9, p.110–111)
> Copy(Linux kernel 복사, 100% dup), Homes(63.52GiB, 84% dup, FIU), WebVMs(54.53GiB, 47% dup, FIU), Mails(173.27GiB, 95% dup, FIU) 네 워크로드로 `trace-replayer` 도구를 사용해 재현. Light-Dedup이 대부분의 경우 최고 성능. 단일 스레드 Mails에서 Light-Dedup은 NOVA 대비 **1.28×** write throughput. 반면 LD-w/o-P는 NOVA조차 따라잡지 못함.

> [!success]- Speculative Prefetch 효율 (§5.4, Figure 10–11, p.112)
> - 128GiB 파일 두 번 쓰기(single thread, FIO): PN이 다른 변형들보다 **1.11–2.19×** 우수.
> - Multi-thread: thread 수 ≤5에서 PN이 SP·LD-w/o-P 대비 각각 **1.03–1.29×**, **1.51–2.19×**. thread 수 ≥6에서는 prefetch I/O가 in-NVM buffer contention을 악화시켜 PN의 content-comparison 시간이 SP 대비 최대 1.65× 증가 → Transition(threads≥6에서 next-block prefetch 비활성화)으로 완화한 CBP(PN+Transition)가 thread 수 증가에도 꾸준히 확장.

> [!success]- LMT의 metadata I/O amplification (§5.5, Table 4, p.112)
> Aging 실험(fresh 128GiB 기록 → `fallocate`로 절반을 hole로 만듦 → 다시 64GiB 기록)에서, aged 시스템 기준 블록당 read amplification이 **region-based 6.10×** vs **entry-based 19.35×**, write amplification이 **region-based 3.43×** vs **entry-based 9.86×**. Throughput도 region-based가 entry-based 대비 약 **11.6%** 높음(1336.72 vs 1197.76 MiB/s).

> [!success]- Recovery overhead 및 기타 (§5.6, §6, Table 5, p.112)
> Failure recovery time은 파일 32×1/32×2/32×4에서 NOVA 0.315/0.488/0.829s 대비 Light-Dedup 1.260/2.372/4.604s — 절대값은 크지만 파일 크기에 선형으로 증가하는 추세는 NOVA와 동일. rhashtable 메모리 사용량은 128GiB 데이터 기준 duplication 0/25/50/75%에서 각각 1.26GiB/1.08GiB/658MiB/331MiB로 데이터 크기의 1% 미만. 두 개의 256GiB DCPMM을 interleave한 확장성 실험(32GiB, 75% dup)에서는 thread 1→16개에 따라 throughput이 952→6238 MiB/s로 스케일.

> [!quote]- 📄 원문 인용
> "Experimental results suggest Light-Dedup achieves 1.01–8.98× throughput over the state-of-the-art NVM deduplication file systems. Here, the speculative prefetch technique used in LRBI improves Light-Dedup by 0.3–118%." (Abstract, p.101)

## 섹션 노트
- §1 Introduction: NVM 비용 절감을 위한 dedup의 필요성과, 기존 연구가 놓친 NVM I/O 메커니즘 세 가지(read/write asymmetry, coarse access granularity, long media read latency)를 소개하고 Light-Dedup의 두 기여(LRBI, LMT)를 요약.
- §2 Background: NVM의 다섯 가지 공통 I/O 특성(asymmetry, buffer로 인한 read/write 차이, coarse access granularity, long media read latency, memory interface) 정리, NOVA 배경, inline dedup 기술(redundant block identification, in-storage metadata management) 및 in-NVM index 기법(rhashtable 채택 근거) 리뷰.
- §3 Observations and Motivations: FIU Mails/WebVMs trace에서 각각 95%/47% 4KiB-granularity 중복 관찰(p.103), NOVA/LD-w/o-P 두 파일 쓰기 실험으로 I/O asymmetry·read latency 영향 분석(Table 1), All-in-NVM/entry-based 두 metadata 관리 방식의 amplification 실측 비교(Table 2).
- §4 Design and Implementation: LRBI(기본 dedup 로직 + speculative prefetch: IBP/CBP), LMT(region 기반 layout, entry 할당/삭제, GC 회피), crash consistency, 향후 이식성(미래 NVM 장치, CXL, ARM PRFM) 논의.
- §5 Performance Evaluation: microbenchmark(§5.2), 실세계 시나리오(§5.3), speculative prefetch 효율(§5.4), LMT 효율(§5.5), recovery overhead(§5.6) 5개 질문에 답.
- §6 Discussion: rhashtable 메모리 소비 확인, hardware cryptographic hash accelerator는 널리 보급되지 않아 고려 대상에서 제외했다고 명시.
- §7 Conclusion and Future Work: LRBI+LMT로 NVM dedup 성능 극대화 요약, 향후 memory-efficient hash table 인덱스 최적화 계획.

## 핵심 용어 (Key terms)
- **Inline Deduplication**: 데이터를 write 경로 상에서 즉시(offline이 아니라) 중복 판정·제거하는 기법. 파일시스템 쓰기 성능과 NVM 내구성을 동시에 개선하려면 오프라인 방식이 아니라 inline이 필요.
- **LRBI (Light-Redundant-Block-Identifier)**: non-cryptographic hash와 speculative-prefetch 기반 byte-by-byte content-comparison을 결합한 duplicate block 식별 기법.
- **LMT (Light-Meta-Table)**: region 기반 linked list로 조직된 in-NVM dedup metadata table. fingerprint→physical block 매핑과 speculative-prefetch hint를 저장.
- **xxHash**: 빠른 non-cryptographic hash 함수. LRBI의 fingerprint 계산에 사용되어 cryptographic hash 대비 CPU 오버헤드를 줄임.
- **In-Block Prefetch (IBP)**: 하나의 4KiB 블록 안에서 XPLine(256B) stride prefetch로 NVM read buffer를 채운 뒤, 64B stride prefetch로 CPU cache까지 로드하는 2단계 prefetch 기법.
- **Cross-Block Prefetch (CBP)**: 현재 블록 비교 도중 다음에 비교될 것으로 예측되는 블록을 hint/trust degree 기반으로 미리 prefetch하는 기법.
- **Region**: LMT의 4KiB 정렬 관리 단위. metadata entry를 이 안에서 순차 할당해 locality를 유지하고 metadata I/O amplification을 줄임.
- **rhashtable**: Linux 커널의 동적 리사이즈 해시테이블 구현. Light-Dedup은 fingerprint→LMT 위치의 in-DRAM index로 사용.
- **XPLine**: Intel Optane DCPMM의 256바이트 media access granularity. dedup metadata(수십 바이트)와의 크기 불일치가 metadata I/O amplification의 원인.
- **Metadata I/O amplification**: 논리적으로 필요한 metadata 바이트 수 대비 실제 NVM에서 읽고/쓴 바이트 수의 배율.
- **Trust degree**: CBP의 hint가 신뢰할 만한지 나타내는 0–7 카운터. threshold(4) 이상일 때만 prefetch를 수행.

## 강점 · 한계 · 열린 질문
- **강점**: 실제 Optane DCPMM 하드웨어에서 NVM I/O 메커니즘(asymmetry, coarse granularity, long read latency)을 정량적으로 분석(Table 1–2, Figure 1)한 뒤 그 관찰에서 직접 설계를 도출하는 탄탄한 motivation. GC-free region 설계로 aged 파일시스템에서도 안정적 성능 유지(§5.5). NOVA 기반 오픈소스 구현과 artifact evaluated(Available/Functional/Reproduced, p.101) 배지 획득.
- **한계**: Optane DCPMM이 상용에서 단종되어(§4.6, p.109에서 저자들도 이를 인지하고 미래 NVM 이식성을 논의) 실험 플랫폼의 장기적 대표성에 의문. CBP는 context switch로 CPU load queue가 flush되면 syscall 경계에서 효과가 줄어들고(§5.2, p.110), 높은 concurrency(≥6 threads)에서는 PN 단독으로는 buffer contention 때문에 성능이 오히려 저하되어 Transition 같은 보완 장치가 필요(§5.4). Hardware cryptographic hash accelerator는 아직 널리 보급되지 않았다는 이유로 비교 대상에서 제외(§6).
- **열린 질문**: CXL 기반 storage 장치로의 이식(저자들이 future work로 명시, §4.6, p.109) 시 LRBI의 prefetch 전제(메모리 인터페이스, XPLine 크기)가 어떻게 바뀔까? ARM PRFM instruction으로의 이식 가능성(§4.6)은 실제로 검증되지 않음. rhashtable을 대체할 memory-efficient hash table(§7에서 제안한 미래 작업)이 도입되면 DRAM index 오버헤드가 얼마나 더 줄어들까?

## ❓ Q&A (자가 점검)

> [!question]- Q1. Light-Dedup이 지적하는, 기존 NVM dedup 연구가 놓친 NVM I/O의 핵심 두 가지 특성은?
> > (1) read/write asymmetry — NVM read bandwidth가 write보다 최대 3배 높고 write buffer가 write latency를 숨겨주는 반면 read는 그렇지 않음. (2) coarse media access granularity(예: DCPMM XPLine 256B)와 dedup metadata(16–64B)의 크기 불일치로 인한 metadata I/O amplification.

> [!question]- Q2. LRBI가 cryptographic hash 대신 non-cryptographic hash + content-comparison을 결합하는 이유는?
> > cryptographic hash(MD5 등)는 계산 비용이 커 CPU 시간을 지배(실측 64.9%, p.104)하지만, non-cryptographic hash만 쓰면 hash collision을 확실히 검증할 수 없다. xxHash로 대부분의 non-duplicate 블록을 빠르게 걸러내고, 동일 fingerprint를 가진 소수만 byte-by-byte content-comparison으로 검증해, "느린 duplicate write"를 "더 빠른 read"로 바꿔치기함으로써 NVM의 read/write 비대칭을 이용한다.

> [!question]- Q3. In-Block Prefetch(IBP)가 2단계로 나뉘는 이유는?
> > CPU 코어가 동시에 처리 가능한 in-flight prefetch 명령 수가 제한적(예: 8–16개)이라 64개 cache line을 한 번에 prefetch(P64)하면 병렬성이 제한된다. 그래서 먼저 XPLine(256B) stride 16개로 블록 전체를 NVM read buffer에 로드(대부분 병렬 처리됨)하고, 그다음 64B stride로 read buffer의 데이터를 CPU cache로 옮기는 2단계로 나눠 실질적 병렬성을 확보한다.

> [!question]- Q4. Cross-Block Prefetch(CBP)의 hint/trust degree는 어떻게 동작하나?
> > 각 LMT entry의 hint 필드가 다음에 비교될 것으로 예측되는 블록의 entry 위치를 가리키고, 3-bit trust degree(0–7)가 그 예측의 신뢰도를 나타낸다. 예측이 맞으면 신뢰도 증가, 틀리면 감소하며 threshold(4) 이상일 때만 실제로 prefetch를 수행한다. CPU별 stream trust degree도 함께 유지해 워크로드 locality를 반영한다.

> [!question]- Q5. LMT가 region 기반 layout을 택해 얻는 이득과 그 대가는?
> > Aged(fragmented) 시스템에서도 metadata entry가 4KiB region 안에서 순차 할당되어 locality가 유지되므로, entry-based(free-list) 관리 대비 read/write amplification이 크게 줄어든다(실측 19.35×/9.86× → 6.10×/3.43×, Table 4). 대가는 region을 절반 이하로 비어야만 재사용해 GC를 완전히 회피하는 설계로, 최대 데이터 크기의 약 1.56%(실측 0.79%)의 NVM 공간을 추가로 소모한다.

> [!question]- Q6. Light-Dedup이 GC(garbage collection)를 아예 피하는 방법과 그 근거는?
> > region이 entry의 절반 이하만 사용 중일 때만 "allocatable"로 표시해 재사용 가능하게 하고, 절반 이상 차 있으면 재사용하지 않는다. 이렇게 하면 log-structured 파일시스템에서 흔히 필요한 복잡하고 시간이 많이 드는 GC 로직 없이도 region을 free할 수 있어, GC 오버헤드 대신 최대 2배(worst case) 정도의 여분 NVM 공간을 지불하는 트레이드오프를 택했다.

> [!question]- Q7. 고농도(high duplication ratio) 워크로드에서 Light-Dedup이 NOVA/NV-Dedup 대비 얻는 성능 이득의 크기는?
> > Microbenchmark(4KiB I/O)에서 duplication ratio ≥75%일 때 NV-Dedup 대비 1.70–4.58×, NOVA 대비 1.05–2.28× throughput(Figure 7). 전체적으로는 SOTA NVM dedup 파일시스템 대비 1.01–8.98× throughput(Abstract). Speculative prefetch 자체의 기여는 0.3–118%.

> [!question]- Q8. 왜 높은 concurrency(스레드 수↑)에서 CBP가 PN 단독보다 나은가?
> > PN(Prefetch-Next)은 다음 비교 대상 블록을 적극적으로 prefetch하지만, 동시 스레드가 많아지면 대량의 추가 prefetch I/O가 NVM read buffer의 contention을 악화시켜(threads 16에서 content-comparison 시간이 SP 대비 최대 1.65× 증가) 오히려 성능이 떨어진다. Transition 메커니즘이 동시 접근 스레드 수가 임계값(기본 6) 이상이면 다음 블록 prefetch를 비활성화해 이 문제를 완화하며, 최종 CBP(=PN+Transition)는 스레드 수 증가에도 꾸준히 확장된다(Figure 11).

## 🔗 Connections
[[File System]] · [[ATC]] · [[2023]]
관련: [[Ethane - An Asymmetric File System for Disaggregated Persistent Memory]] · [[Multi-Granularity Shadow Paging with NVM Write Optimization for Crash-Consistent Memory-Mapped I-O]]

## References worth following
- [66] Xu & Swanson, **NOVA: A Log-structured File System for Hybrid Volatile/Non-volatile Main Memories** (FAST 2016) — Light-Dedup이 구현 기반으로 삼은 SOTA NVM 파일시스템.
- [58] Wang et al., **NV-Dedup: High-performance Inline Deduplication for Non-volatile Memory** (IEEE TC 2017) — cryptographic hash 기반 NVM dedup, 주요 비교 baseline이자 hash 계산 오버헤드 사례.
- [75] Zuo, Hua, Zhao, Zhou, Guo, **Improving the Performance and Endurance of Encrypted Non-volatile Main Memory through Deduplicating Writes (DeWrite)** (MICRO 2018) — non-cryptographic hash + byte-by-byte comparison을 결합한 선행 아이디어, LRBI의 직접적 출발점.
- [28] Kwon, Cho et al., **DeNOVA: Deduplication Extended NOVA File System** (IPDPS 2022) — background deduplication daemon 기반 NVM dedup FS, 주요 비교 baseline.
- [64] Xiang, Zhao, Xu, Jiang, **Characterizing the Performance of Intel Optane Persistent Memory: A Close Look at Its On-DIMM Buffering** (EuroSys 2022) — DCPMM의 read buffer/write buffer 동작을 규명한 근거 연구, Light-Dedup의 NVM I/O 특성 분석에 인용.
- [31] Leijen, Zorn, de Moura, **Mimalloc: Free List Sharding in Action** (APLAS 2019) — region(page) 단위 free-list 관리 아이디어의 영감원.

## Personal annotations
<!-- 본인 메모 영역 -->
