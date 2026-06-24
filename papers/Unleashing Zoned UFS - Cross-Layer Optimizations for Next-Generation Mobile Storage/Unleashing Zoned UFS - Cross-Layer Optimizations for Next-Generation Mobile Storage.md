---
title: "Unleashing Zoned UFS: Cross-Layer Optimizations for Next-Generation Mobile Storage"
aliases: [Unleashing Zoned UFS]
description: "ZUFS(Zoned UFS)를 상용 스마트폰에 실제 배포하며 SRAM·쓰기순서·GC 3대 난제를 Android~펌웨어 cross-layer 최적화로 해결한 FAST 2026 논문."
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/zns
  - topic/zns
  - topic/ufs
  - topic/mobile
  - venue/fast
  - year/2026
---

# Unleashing Zoned UFS: Cross-Layer Optimizations for Next-Generation Mobile Storage

> **FAST 2026** · `cluster/zns` · Source: [Unleashing Zoned UFS - Cross-Layer Optimizations for Next-Generation Mobile Storage.pdf](<Unleashing Zoned UFS - Cross-Layer Optimizations for Next-Generation Mobile Storage.pdf>)

저자: Jungae Kim, Sungjin Park, Jinwoo Kim, Jieun Kim, Iksung Oh (SK hynix Inc.) · Jaegeuk Kim, Chul Lee, Bart Van Assche, Daeho Jeong, Konstantin Vyshetsky (Google) · Kyu-Jin Cho, Jin-Soo Kim (Seoul National University)

## TL;DR
ZUFS(Zoned UFS)는 zone 내 순차쓰기를 강제해 conventional UFS(CUFS)의 거대한 L2P 매핑 오버헤드를 zone 단위(1TB에 약 8KB ZMT)로 줄이지만, 실제 상용폰 배포 시 (1) 다중 open zone에 걸친 제한된 SRAM 관리, (2) end-to-end 쓰기 순서 보장, (3) 큰 zone으로 인한 심각한 GC 오버헤드라는 3대 난제가 발생한다. 저자들은 SCSI/UFS driver·block layer·F2FS·Android framework·device firmware를 가로지르는 cross-layer 최적화(ZABM 동적 버퍼, 쓰기순서 보장, proactive GC)로 이를 해결했고, Google Pixel 10 Pro 실기에서 fragmentation 하에 쓰기 처리량 2배 이상, Genshin Impact 게임 로딩 시간 14% 단축을 달성했다.

## 문제 & 동기
모바일 스토리지(UFS)의 page-level L2P 매핑 테이블은 용량의 약 0.1%(1TB → 약 1GB)를 차지하는데, UFS 컨트롤러의 SRAM은 2017년 이후 약 1MB 수준에 머물러 있어 용량 증가(32GB→1TB)와의 격차가 scalability 한계를 만든다. ZUFS는 zone 단위 매핑으로 이 격차를 해소하지만, 개념적 단순함과 달리 상용 배포에는 SRAM 부족, 쓰기 순서 위반, 큰 zone GC라는 비자명한 난제가 따른다.

> [!quote]- 📄 원문 표현 (paper)
> "While the concept appears straightforward, deploying ZUFS in commercial smartphones introduces non-trivial challenges across the mobile storage stack. In this paper, we identify three key obstacles: managing limited SRAM across multiple open zones, ensuring end-to-end write ordering guarantees, and mitigating severe garbage collection overhead caused by large zones." (Abstract, p.1)

> [!quote]- 📄 원문 표현 (paper)
> "the SRAM allocated for the L2P mapping table in UFS devices has remained at around 1 MB or less since 2017, despite the exponential growth in device capacity during the same period." (§2.2, p.4)

## 핵심 통찰

> [!note]- 통찰 토글
> - **Zone 단위 매핑이 SRAM 격차를 닫는다.** Page-level 매핑(1TB→약 1GB)을 zone 단위로 바꾸면 ZMT(Zone Mapping Table)는 1TB에 약 8KB만 필요해 SRAM에 전부 cache 가능 → map cache miss 제거 → fragmentation 하에서도 안정적 random read. (§4.1, p.6)
> - **ZUFS의 잠재력은 cross-layer 재설계로만 발현된다.** 단일 layer 최적화가 아니라 Android framework~F2FS~block layer~SCSI/UFS driver~device firmware를 coordinated하게 고쳐야 진짜 이득이 나온다. (Abstract / §6, p.1, p.10)
> - **모바일 전력관리(clock gating)가 쓰기 순서를 깬다.** Datacenter ZNS SSD와 달리, 모바일의 공격적 power management(clock gating)가 요청을 requeue하며 dispatch 순서를 바꿔 zoned semantics를 silent하게 위반한다 → 데이터센터에 없는 모바일 고유 문제. (§3.3, p.6)
> - **큰 zone은 양날의 검.** 큰 zone은 device-level parallelism·device GC 제거에는 좋지만, section이 physical zone과 정렬되면서 victim section당 migrate해야 할 live data가 늘어 F2FS-level GC 오버헤드(WAF)를 키운다. (§3.4 / §4.1, p.6)
> - **In-device 해법이 host-side 개입보다 가볍다.** 선행연구 ZMS는 host-side IOTailor layer로 트래픽을 reshape하지만, 본 논문은 device 내부(SGBM)에서 fine-grained 버퍼 관리로 해결 → 칩 면적 약 0.4%로 negligible, host stack 침습 최소화. (§4.2, p.8)

## 설계 / 메커니즘

> [!abstract]- 설계 토글
> **전체 구조 (§4.1, p.6).** 평가 device는 multiple TLC NAND die(각 4 plane), page 16KB. ZUFS는 die들에 걸쳐 block을 묶어 zone을 구성하며 effective zone size는 약 1,056MB(physical superblock 단위와 정렬, sequential I/O에서 device parallelism 최대 활용). 컨트롤러 on-chip SRAM은 주로 write buffer, ZMT(Zone Mapping Table), ZML(Zone Mapping Log)에 사용.
>
> **ZMT/ZML (§4.1, p.7).** ZMT 엔트리는 8-byte: 4-byte 시작 physical address + 4-byte length(zone 내 유효 데이터 마지막 위치 = host 가시 write pointer). 1TB ZUFS는 ZMT 전체가 8KB(CUFS는 약 1GB). 업데이트는 ZML에 먼저 staging 후 ZMT로 checkpoint → consistency 관리 단순화. zone remapping(read reclaim/wear-leveling) 시 ZML이 목적지 zone의 시작/길이를 임시 기록해 마이그레이션 중에도 victim zone read 가능.
>
> **(1) ZABM — Zone-Aware Buffer Management (§4.2, p.8).** 각 open zone에 full superpage 버퍼(768KB)를 주는 naive 설계는 SRAM 부족. SLC 임시저장은 SLC-TLC migration으로 WAF 증가. 해법: **SGBM(Scatter-Gather Buffer Manager)** 하드웨어 컨트롤러가 reserved SRAM을 4KB slot으로 분할하고, open zone마다 slot table을 두어 slot index를 추적. 한 die를 program할 만큼 모이면 즉시 flush, 전 die에 걸쳐 full superpage가 모이면 parallel flush. zone별 최소 버퍼 보장 + 부하 큰 zone은 동적으로 더 많은 slot 확보. ZABM은 칩 면적의 약 0.4%만 차지.
>
> **(2) End-to-End 쓰기 순서 보장 (§4.3, p.8-9).** clock-gated 상태에서 거부·requeue된 요청이 순서를 깨는 문제 → UFS driver의 *synchronous ungating mechanism* 교체: 새 I/O 도착 시 clock이 완전히 ungate될 때까지 driver가 대기 후 dispatch. block layer corner case 수정: mq-deadline의 `next_rq` 포인터가 requeue 후 stale → 다른 요청 선택으로 순서 깨짐, FUA 플래그 요청이 scheduler ordering path를 우회, `ionice`/`blk-ioprio` cgroup priority가 zoned write를 잘못된 순서로 제출해 unaligned write error 유발 → 모두 수정.
>
> **(3) Proactive GC (§4.4, p.9-10).** F2FS background GC를 `/sys/fs/f2fs/<dev>/` 파라미터로 노출되는 tunable knob(Table 1)로 3단계 제어: free section 비율 > `gc_no_zoned_gc_percent`(기본 60%)면 **No-GC**(GC 비활성, 응답성 우선), 그 아래면 **Normal-GC**(cost-benefit 알고리즘), `gc_boost_zoned_gc_percent`(기본 25%) 아래면 **Boosted-GC**(greedy 알고리즘, migration window를 `gc_boost_gc_multiple`=5배 확대). `reserved_segments`(기본 6336 = 6 open zone 수용에 필요한 segment 수의 2배)로 segment 단위 OP 예약. user read 발생 시 background GC 즉시 일시정지(read I/O 항상 우선). 모든 변경은 upstream Linux kernel에 반영.

## 평가

> [!success]- 평가 토글
> **환경 (§5.1, p.6).** Google Pixel 10 Pro(12GB LPDDR5X, 512GB ZUFS), Android OS 16 + kernel 6.6, F2FS. Baseline CUFS는 conventional LU를 전체 볼륨에 할당. ZUFS는 pure LFS mode F2FS.
>
> **I/O 처리량 (§5.2, Fig.7a, p.7).** clean device에서 ZUFS는 seq/rand read·write 모두 CUFS와 comparable(fragmentation·GC 없으면 둘 다 raw NAND 대역폭 활용).
>
> **Wide-range random read (§5.3, Fig.7b/7c, p.7).** access range를 4GB→256GB로 키우면 CUFS는 map cache miss로 처리량 급감, ZUFS는 ZMT가 SRAM에 다 들어가 모든 range에서 안정적. 256GB 파일에서 block size 4KB~512KB 변화 시, 128KB 이하 small/medium block에서 ZUFS가 일관되게 CUFS 능가(gap이 128KB 이하에서 가장 큼).
>
> **Fine-grained buffer (§5.4, Fig.8, p.7).** ZUFS(192KB chunk = 단일 die program 단위)가 ZMS(768KB chunk)보다 쓰기 처리량 26% 높음(ZMS는 768KB 다 모일 때까지 flush 지연·stall). ZUFS는 chunk를 768KB로 제한해도 ZMS와 처리량 차이 negligible → die-level flush는 성능 패널티 없이 SRAM 유연 활용.
>
> **Fragmentation 영향 (§5.5, Fig.9/10, p.7-8).** 32,768개 128KB 파일 생성 후 격번 삭제로 fragmentation 누적. CUFS는 쓰기 처리량 거의 100MB/s로 폭락, read 약 35% 감소(90th iteration에서 free segment 고갈→foreground GC가 user thread에서 실행). ZUFS는 40·80 iteration 근처 dip은 있으나 쓰기 처리량 200MB/s 이상 유지, read는 안정적. F2FS segment 분석상 ZUFS는 No-GC→Normal-GC(약 40 iteration)→Boosted-GC로 전이하며 free section을 확보.
>
> **App-level: Genshin Impact (§5.6.1, p.8).** 약 40GB 게임 리소스 verification + 로딩을 aged device(40GB 미만까지 fragmentation)에서 측정. ZUFS 30초 vs CUFS 35초 → **14% 개선**. CUFS는 read 요청의 66.3%가 4~8KB(SSR feature로 인한 small fragmented read), ZUFS는 다수 요청이 512KB 초과(순차).
>
> **App-level: Photo Gallery scrolling (§5.6.2, Table 2, p.8-9).** 1,300장(평균 약 4.5MB) aged device에서 30회 swipe. Jank Rate 0.60%(CUFS)→0.26%(ZUFS), Avg Fragments/File 46.29→2.31, Avg Fragment Length 99KB→1,979KB(약 20배), p99 Frame Time 16ms→11ms.

## 섹션 노트
- **§1 Introduction (p.2-3):** 2024년 기준 스마트폰 사용자 58억+, UFS가 eMMC 대체 de facto. ZUFS는 JEDEC 표준(JESD220-5). 기여: (1) 1만 대 실측 fragmentation user study, (2) ZUFS 채택 신규 난제 및 해법, (3) 실기 종합 평가, (4) Android framework·Linux kernel 변경 오픈소스/upstream. 본 논문 features는 2025년 출시 Pixel 10 Pro에 탑재 — zoned storage의 상용 하이엔드폰 최초 배포 주장.
- **§2 Background (p.3-6):** UFS의 hierarchical L2P(L1/L2 map), map cache, compressed mapping(huge page 유사, contiguity 의존). ZUFS는 2023.11 JEDEC 비준, zone 상태(EMPTY/OPEN/CLOSED/FULL), 최소 6 open zone 동시 지원 요구. Android storage stack: ZBD 추상화, F2FS LFS mode, mq-deadline, ZWL(Zone Write Locking, zone당 1 write in-flight). ZUFS support는 Android 16 + GKI 6.6/6.12부터 공식.
- **§2.5 Related Work (p.6):** ZNS+ (OSDI 21), eZNS (OSDI 23), ZNSwap. Yan et al. ZUFS[46]는 FTL을 host로 이전(GB-scale DRAM 필요), 본 논문은 FTL을 device에 유지. ZMS(ATC 24)가 가장 근접 — IOTailor host-side layer로 reshaping하나 device geometry 정보 노출 필요·CPU/DRAM 오버헤드·per-file encryption으로 cross-file merge 제약.
- **§3 Motivation (p.6-8):** fragmentation level = dirty segment / (dirty+free segment). 1만 대 중 약 30%가 severe fragmentation(level>0.7), low utilization에서도 fragmentation 발생(r≈0.74). read 평균 66ms/MB(worst 344), write 평균 175ms/MB(99p 678, worst 약 2s/MB).
- **§6 Conclusion (p.10):** ZUFS를 mature/deployable 기술로 확립하는 broader effort의 시작이라 규정.

## 핵심 용어
- **ZUFS (Zoned UFS):** zone 내 순차쓰기를 강제해 page-level L2P 매핑을 zone 단위로 축소한 차세대 모바일 스토리지 표준(JEDEC JESD220-5).
- **CUFS (Conventional UFS):** 기존 page-level L2P 매핑 기반 UFS. 대용량 매핑 테이블(약 1GB/1TB)로 SRAM 부족·map cache miss 발생.
- **L2P mapping / map cache:** logical block→physical page 변환 테이블과 그 일부만 SRAM에 cache하는 메커니즘. miss 시 NAND read 유발.
- **ZMT (Zone Mapping Table):** zone 단위 8-byte 엔트리(시작주소+길이) 매핑 테이블. 1TB에 약 8KB로 SRAM에 전부 상주.
- **ZML (Zone Mapping Log):** ZMT 업데이트를 batch staging하고 zone remapping 중 임시 매핑을 기록하는 로그.
- **ZABM / SGBM:** Zone-Aware Buffer Management. 핵심은 SGBM(Scatter-Gather Buffer Manager) 하드웨어 컨트롤러로, SRAM을 4KB slot으로 쪼개 open zone별 동적 할당.
- **superpage:** 모든 die의 plane에 걸친 program 단위. 본 device에서 768KB.
- **ZWL (Zone Write Locking):** zone당 1개 write만 in-flight 허용해 순서를 강제하는 메커니즘.
- **clock gating:** idle 시 device clock을 끄는 모바일 전력관리 기법. requeue로 dispatch 순서를 바꿔 zoned write 순서를 위반시킬 수 있음.
- **No-GC / Normal-GC / Boosted-GC:** free section 비율에 따라 F2FS background GC 강도를 3단계로 제어하는 proactive GC 정책.
- **SSR (Selective Segment Reuse):** F2FS가 dirty segment의 invalid block을 재사용하는 IPU 최적화. write amplification은 줄이나 data scatter로 small fragmented read 유발.
- **WAF (Write Amplification Factor):** 실제 NAND 쓰기량/host 쓰기량 비율.

## 강점 · 한계 · 열린 질문
- **강점:** 상용 하이엔드폰(Pixel 10 Pro) 실기 배포·실측 — zoned storage 최초 commercial deployment. cross-layer로 SRAM·순서·GC 3대 난제를 종합 해결. 변경사항을 Android framework·Linux kernel에 upstream/오픈소스. in-device 해법으로 칩 면적 약 0.4%, host 침습 최소. app-level(게임 14%, gallery jank 0.26%)로 user-perceived 이득 입증.
- **한계:** 단일 device(특정 SK hynix ZUFS, 512GB) 평가로 일반화 한계. ZMS 비교는 소스 비공개라 IOTailor를 emulation. 절대 throughput 수치 다수가 그래프(수치 라벨 일부만 본문)에 의존. zone size·die 구성은 본 device 고정값(1,056MB)으로 다른 구성 영향 미검증.
- **열린 질문:** ZUFS 도입 후 endurance/lifespan 장기 영향은? proactive GC knob들의 자동 튜닝(워크로드 적응) 가능성? open zone 6개 초과 워크로드에서 ZABM slot 경쟁은? 다양한 vendor·zone size 일반화는?

## ❓ Q&A (자가 점검)

> [!question]- Q1. ZUFS가 CUFS 대비 줄이는 핵심 오버헤드는 무엇이고 수치는?
> > L2P 매핑 테이블 크기. CUFS는 page-level로 1TB당 약 1GB SRAM 필요, ZUFS는 zone 단위 ZMT로 1TB당 약 8KB만 필요 → SRAM에 전부 상주, map cache miss 제거.

> [!question]- Q2. 상용 ZUFS 배포의 3대 난제는?
> > (1) 다중 open zone에 걸친 제한된 SRAM 관리, (2) end-to-end 쓰기 순서 보장, (3) 큰 zone으로 인한 심각한 GC 오버헤드. (Abstract, p.1)

> [!question]- Q3. SGBM은 SRAM을 어떻게 관리하나?
> > reserved SRAM을 4KB slot으로 분할하고 open zone마다 slot table을 둬 동적 할당. 한 die program 분량이 모이면 즉시 flush, 전 die superpage가 모이면 parallel flush. zone별 최소 버퍼 보장 + 부하 큰 zone에 추가 slot. 칩 면적 약 0.4%. (§4.2, p.8)

> [!question]- Q4. 모바일에서 쓰기 순서가 깨지는 근본 원인과 해법은?
> > clock gating(전력관리)으로 clock-gated 상태에서 거부·requeue된 요청이 dispatch 순서를 바꿈. 해법: UFS driver의 synchronous ungating(clock 완전 복원까지 대기 후 dispatch) + mq-deadline next_rq stale·FUA 우회·ionice/blk-ioprio 순서 문제 수정. (§3.3/§4.3, p.6, p.8-9)

> [!question]- Q5. 큰 zone이 GC를 악화시키는 이유와 proactive GC의 대응은?
> > section이 physical zone과 정렬되며 victim section당 migrate live data가 늘고 free section 고갈이 빨라 foreground GC가 잦아짐. 대응: free section 비율 기반 No-GC(>60%)/Normal-GC/Boosted-GC(<25%, greedy+window 5배) 3단계 + segment 단위 reserved_segments OP + user read 시 background GC 즉시 일시정지. (§3.4/§4.4, p.6, p.9-10)

> [!question]- Q6. ZMS 대비 ZUFS의 접근 차이와 정량 이점은?
> > ZMS는 host-side IOTailor로 트래픽 reshaping(geometry 노출·CPU/DRAM 오버헤드), ZUFS는 device 내부 SGBM으로 fine-grained 처리. ZUFS(192KB chunk)가 ZMS(768KB)보다 쓰기 처리량 26% 높음. (§4.2/§5.4, p.8, p.7)

> [!question]- Q7. 실기 app-level 결과 핵심 수치는?
> > Genshin Impact 로딩 30초 vs CUFS 35초(14% 개선). Photo gallery jank rate 0.60%→0.26%, fragment 길이 99KB→1,979KB(약 20배), p99 frame 16ms→11ms. (§5.6, p.8-9)

> [!question]- Q8. fragmentation이 실제 스마트폰에서 얼마나 심각한가?
> > 1만 대 실측에서 약 30%가 severe fragmentation(level>0.7), low utilization에서도 발생(util-frag 상관 r≈0.74). write 99p latency 678ms/MB, worst 약 2s/MB. (§3.1, p.7)

## 🔗 Connections
[[ZNS]] · [[FAST]] · [[2026]]

## References worth following
- ZMS: Zone abstraction for mobile flash storage (USENIX ATC 24) — 가장 근접한 선행연구, host-side IOTailor 비교 대상. [21]
- Yan et al. ZUFS: Enhancing Stability and Endurance ... with Integrated Zoned Namespaces in UFS (CCGrid 24) — host로 FTL 이전하는 대조적 접근. [46]
- ZNS+: Advanced zoned namespace interface for in-storage zone compaction (OSDI 21) — filesystem-level GC를 device로 오프로드. [18]
- eZNS: An elastic zoned namespace for commodity ZNS SSDs (OSDI 23) — adaptive zone sizing. [35]
- F2FS: A new file system for flash storage (FAST 15) — ZUFS가 의존하는 LFS 파일시스템 원전. [33]
- JEDEC. Zoned Storage for UFS (JESD220-5, 2023) — ZUFS 표준 사양. [23]

## Personal annotations
<본인 메모 영역>
