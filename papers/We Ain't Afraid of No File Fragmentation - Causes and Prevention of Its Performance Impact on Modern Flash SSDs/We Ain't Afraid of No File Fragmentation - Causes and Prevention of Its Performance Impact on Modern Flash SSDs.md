---
title: "We Ain't Afraid of No File Fragmentation: Causes and Prevention of Its Performance Impact on Modern Flash SSDs"
aliases: [We Ain't Afraid of No File Fragmentation]
description: "Flash SSD에서 파일 단편화의 read 성능 저하 진짜 원인은 request splitting이 아니라 die-level collision이며, NVMe 확장+page-to-die 매핑으로 defrag 없이 해결"
venue: FAST
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/fragmentation
  - topic/filesystem
  - venue/fast
  - year/2024
---

# We Ain't Afraid of No File Fragmentation: Causes and Prevention of Its Performance Impact on Modern Flash SSDs

> **FAST 2024** · `cluster/fs` · Source: [We Ain't Afraid of No File Fragmentation - Causes and Prevention of Its Performance Impact on Modern Flash SSDs.pdf](We%20Ain't%20Afraid%20of%20No%20File%20Fragmentation%20-%20Causes%20and%20Prevention%20of%20Its%20Performance%20Impact%20on%20Modern%20Flash%20SSDs.pdf)

저자: Yuhun Jun (Sungkyunkwan Univ. / Samsung Electronics), Shinhyun Park (Sungkyunkwan Univ.), Jeong-Uk Kang (Samsung Electronics), Sang-Hoon Kim (Ajou Univ.), Euiseong Seo (Sungkyunkwan Univ., 교신저자)

## TL;DR
Flash SSD에서 파일 단편화(file fragmentation)가 sequential read 성능을 떨어뜨리는 진짜 원인은 기존 통념인 **request splitting**(kernel I/O path / interface 오버헤드)이 아니라, SSD 내부에서 발생하는 **die-level collision의 급증**이다. SSD FTL이 page를 쓰여진 순서대로 round-robin 배치하기 때문에, 단편화나 overwrite가 일어나면 연속된 file block의 page들이 연속된 die에 놓이지 못해 read 시 같은 die에 충돌이 몰린다. 저자들은 write command에 hint를 추가하는 **NVMe command extension + page-to-die 매핑 알고리즘**을 제안해, defrag 없이 단편화 상태를 그대로 두고도 read 성능 저하를 막는다. 162MB SQLite DB를 10,011조각으로 단편화시킨 사례에서 기존 시스템은 40% 저하, 제안 기법은 3.5% 저하에 그쳤다 (Abstract, p.1).

## 문제 & 동기
- HDD 시대의 통념: 단편화 성능 저하의 원인은 흩어진 sector를 찾는 **seek time**. SSD는 seek/rotation이 없으니 단편화에 강하고 defrag 불필요(심지어 write로 수명만 깎음)하다고 SSD 제조사들이 주장했다.
- 그러나 최근 연구들(Conway et al., Hahn et al. 등)은 SSD도 단편화된 파일 read 시 2~5배 느려짐을 발견. 이들은 그 원인을 kernel I/O path의 **request splitting**으로 가설을 세웠다.
- 이 논문의 반박: 기존 연구의 실험 설정이 (1) 단편화가 직접 유발한 저하와 (2) 단편화가 SSD 내부 data placement에 끼친 간접 영향을 구분하지 못했다. 진짜 원인은 후자, 즉 die-level collision이다.

> [!quote]- 📄 원문 표현 (paper)
> "our analysis reveals that, contrary to assertions in existing literature, the primary cause of the degraded performance is not due to request splitting but stems from a significant increase in die-level collisions." (Abstract, p.1)
>
> "In SSDs, when overwrites come between writes of neighboring file blocks, the file blocks are not placed on consecutive dies, resulting in random die allocation. This randomness escalates the chances of die-level collisions, causing deteriorated read performance later. We also reveal that this may happen when a file is overwritten." (Abstract, p.1)
>
> "the previous claim suggesting file fragmentation adversely impacts sequential read performance also in flash SSDs due to request splitting is based on inaccurate experiment settings and analyses." (Introduction, p.1)

## 핵심 통찰

> [!note]- 통찰 1 — request splitting 오버헤드는 무시할 수 있다
> 단편화로 하나의 sequential I/O가 여러 device command로 쪼개지면 kernel I/O path에서 iomap/bio/request 생성 비용이 늘지만, 극단적 DoF=256에서도 kernel path는 약 9.7ms 수준. 게다가 **command queueing**(NVMe 65,535 queue × 65,536 depth, SATA NCQ 32) 덕분에 후속 fragment 처리 시간에 대부분 가려진다(overlap). ramdisk 실험에서 queue depth=128이면 DoF가 read time에 거의 영향 없음 (§3.1, Fig.4/Fig.6, p.7).

> [!note]- 통찰 2 — 진짜 원인은 page-to-die misalignment → die-level collision
> FTL은 write 들어온 순서대로 page를 die에 round-robin 배치한다. die mapping은 file system level이 아니라 **"flash 안에 실제로 쓰이는 순간"** 결정된다. 따라서 다른 파일 write가 끼어들면(단편화) 연속 file block의 page들이 연속 die에 못 가고, 한 파일 page들이 특정 die에 몰린다. sequential read 시 같은 die에 read가 직렬화되어(die-level collision) 내부 parallelism이 무너진다 (§2.3, §3.2, p.6, p.9).

> [!note]- 통찰 3 — overwrite도 단편화 없이 misalignment를 만든다
> in-place update가 불가능한 flash 특성상, 연속 파일의 중간 block을 overwrite하면 새 page가 round-robin에 따라 "직전 할당 die의 다음 die"에 배치된다. file system level에서는 여전히 contiguous인데도 page-to-die 순서가 깨져 die-level collision이 생긴다. 즉 단편화는 misalignment의 충분조건이 아니다 (§4, Fig.9, p.10).

> [!note]- 통찰 4 — 단편화 그대로 두고도 read 성능 보존 가능
> 성능 저하가 단편화의 불가피한 결과가 아니라 die 배치만 바로잡으면 회피 가능하다는 것이 핵심. 따라서 비싼 defrag(느린 매체에서 대량 read/write, GC 유발, 수명 소모) 없이도 해결할 수 있다 (Introduction, p.2).

## 설계 / 메커니즘

> [!abstract]- NVMe command extension + page-to-die 매핑 정책
> **핵심 아이디어**: file system은 flash page 위치를 모르고 SSD firmware는 page 간 file 관계를 모르는 mismatch를 해소하기 위해, host가 write command에 hint를 실어 SSD가 올바른 die를 고르게 한다.
>
> **write 분류**: kernel I/O stack이 write를 (a) 새 data block 할당이 필요한 **append write**와 (b) 기존 block에 쓰는 **overwrite**로 구분해 NVMe에 추가 정보를 전달한다.
>
> **append write hint**: host가 write command와 함께 "바로 앞 file block의 LBA"를 전달. SSD firmware는 round-robin을 따르지 않고, 앞 file block의 page가 놓인 die의 **바로 다음 die**에 새 page를 배치한다(Fig.10). write 크기가 die allocation granularity를 넘으면 추가 page들은 기존 round-robin을 따른다.
>
> **overwrite hint**: write command에 overwrite flag를 세팅. firmware는 기존 LBA의 page를 invalidate하고, **원래 page가 있던 die와 같은 die**에 새 page를 할당해 die-level contiguity를 보존한다.
>
> **프로토콜 구현**: 추가 프로토콜 오버헤드 없이 NVMe write command의 미사용 reserved bit 재활용 — CDW12의 24/25번째 bit로 append/overwrite/일반 write 구분, append 시 CDW2/CDW3로 직전 file block의 LBA 전달.
>
> **부작용 고려**: 작은 block에 반복 overwrite가 몰리면 특정 die만 free page 고갈 → 이른 GC. 다만 overwrite는 valid page를 invalidate하므로 valid copy가 줄어 WAF와 GC 시간이 오히려 짧아진다. uneven wear가 심해지면 해당 LBA를 다른 die로 재배치(성능 희생)하는 메커니즘으로 완화 가능 (§4, p.10-11).

## 평가

> [!success]- 실험 환경 & 분석 결과
> **환경**: Intel Xeon Gold 6138, Ubuntu 20.04 (kernel 5.15.0), PCIe Gen3 x4 / SATA 3.0. ext4 사용. 실제 commercial SSD 6종 — NVMe-A: Samsung 980 PRO, NVMe-B: WD Black SN850, NVMe-C: SK Hynix Platinum P41, NVMe-D: Crucial P5 Plus, SATA-A: Samsung 870 EVO, SATA-B: WD Blue SA510 (Table 1, p.6).
>
> **단편화 직접 측정** (8MB 파일, DoF 1~256): NVMe-A는 DoF 128/256에서 DoF 1 대비 2.7배/4.4배 read time 증가, NVMe-B는 1.3배/1.9배 (§3, Fig.3, p.7). DoF가 64를 넘으면서 저하 시작. NVMe 기본 max request 1MB라 fragment가 1MB 넘는 DoF≤8에서는 차이 없음.
>
> **request splitting은 무관**: ramdisk에서는 DoF=256에서도 read time 1.5배 증가에 그치고, queue depth=128이면 거의 영향 없음. kernel path는 극단 케이스도 ~9.7ms (§3.1, Fig.4-6, p.7-8).
>
> **die allocation granularity / stripe size 측정** (raw device, FOB 상태): NVMe-A/B는 32KB granularity, stripe size NVMe-A 1MB·NVMe-B 256KB. interval 확대 시 sustained throughput이 NVMe-A 2600→166 MB/s, NVMe-B 3020→480 MB/s로 급락(64KB부터). SATA-A 4KB granularity, SATA-B 16KB granularity (§3.2, Fig.8, p.9).
>
> **제안 기법 검증 (emulation on commercial SSD)**: 256 fragment로 8MB 단편화 파일. append 단편화 read 저하가 NVMe-A 79%·NVMe-B 76%였으나, 제안 기법 적용 시 평균 1% 저하로 회복. overwrite: SATA-A/B 27%/16% 저하 → 제안 기법으로 contiguous 수준 회복, 최대 저하 SATA-B 1.2% (§5.1, Fig.11, p.12).
>
> **실제 구현 (ext4 + NVMe driver + NVMeVirt emulator, Table 2: 4채널/채널당 2die, 32KB unit, read 36µs/write 185µs)**:
> - Worst case(전 block 단일 die): fragmented 19%, overwritten 18.2%로 폭락 → 제안 기법 Append Worst 3.5%·Overwrite Worst 2.3% 저하 (Fig.12, p.13).
> - Random disturbance: 59.4%/62% 저하 → 5.8%/1.6% 저하.
> - **SQLite** (10,000 records × 16KB, DoF 5,005): select 성능 60% → 제안 기법으로 1.6배 향상, contiguous 대비 3.5% 저하.
> - **Filebench fileserver** (10,000 × 128KB, 평균 DoF 15.7, 11GB): contiguous 대비 80% 수준 → 93%로 회복.
> - **fileserver small** (16KB append < 32KB page): flash page level 단편화로 제안 기법 효과 8.2%에 그침 — 한계 노출 (§5.2, Fig.12, p.13-14).

## 섹션 노트
- §1 Introduction: HDD 통념 vs SSD 현실, 기존 연구의 request-splitting 가설 반박, die-level collision이 진짜 원인이라는 주장과 NVMe extension 제안 예고.
- §2 Background and Motivation: §2.1 단편화의 옛 통념(seek time)과 defrag tools(e4defrag, defrag.f2fs, xfs_fsr, FragPicker), §2.2 SSD 시대의 단편화 논쟁(file system aging), §2.3 modern flash SSD 내부(die parallelism, FTL round-robin, GC).
- §3 Analysis of File Fragmentation: §3.1 request splitting의 영향이 미미함을 ramdisk/blktrace로 입증, §3.2 page misalignment와 die allocation granularity·stripe size 측정.
- §4 Our Approach: append/overwrite 구분, hint 전달, page-to-die 매핑 규칙, NVMe reserved bit 활용, GC/wear 부작용 분석.
- §5 Evaluation: §5.1 commercial SSD emulation 검증, §5.2 ext4+NVMeVirt 실구현으로 SQLite/Filebench application 효과.
- §6 Conclusion: 원인은 die-level misalignment, NVMe extension으로 defrag 없이 수% 이내 저하로 억제.

## 핵심 용어
- **File fragmentation / DoF (Degree of Fragmentation)**: 한 파일의 logical block이 연속되지 않고 흩어진 정도. extent 수 ÷ 이상적 extent 수 (Cheng Ji et al. 정의).
- **Request splitting**: 단편화로 하나의 sequential I/O가 여러 bio/request/device command로 쪼개지는 현상. 기존 연구가 지목한 저하 원인이나 본 논문은 부정.
- **Die-level collision**: read 대상 page들이 같은 die에 몰려 read가 직렬화되며 내부 parallelism이 무너지는 현상. 본 논문이 지목한 진짜 원인.
- **Page-to-die mapping**: FTL이 logical page를 물리 die에 배치하는 매핑. 기본은 write 순서 기반 round-robin.
- **Die allocation granularity**: 한 die에 한 번에 할당되는 page 묶음 크기(예: NVMe 32KB, two-plane program으로 die당 2 page).
- **Stripe size**: 첫 die로 돌아오기 전 모든 die에 걸쳐 쓰이는 data 총량(예: NVMe-A 1MB, NVMe-B 256KB).
- **Delayed allocation / block preallocation / reservation**: ext4가 단편화를 줄이려 쓰는 기법(write 시점 지연 할당, inode당 free block 예약).
- **NVMeVirt**: software-defined 가상 NVMe device emulator (Kim et al., FAST '23). 제안 기법 FTL을 구현한 플랫폼.

## 강점 · 한계 · 열린 질문
- **강점**: 기존 통념(request splitting)을 ramdisk·blktrace·6종 실제 SSD로 정량 반박하고, die-level collision을 진짜 원인으로 실증. defrag 없이 단편화 상태를 유지하면서 성능 보존하는 발상이 신선. overwrite까지 misalignment 원인으로 포착. NVMe reserved bit만 써서 프로토콜 오버헤드 0. artifact(kernel/NVMeVirt) 공개·평가 통과.
- **한계**: 실제 SSD firmware 수정은 불가능해 제안 기법은 emulation(write pattern 모방) + NVMeVirt로만 검증, 상용 검증 부재. write 크기가 flash page보다 작은 경우(fileserver small)는 page-level 단편화라 효과 8.2%로 제한 — 별도 page allocation 전략 필요. 특정 die에 overwrite 집중 시 GC/uneven wear 부작용은 완화책 제시에 그침.
- **열린 질문**: write가 page보다 작을 때의 die/page 배치 전략은? hint 기반 비-round-robin 배치가 장기적으로 wear leveling·GC efficiency에 미치는 영향의 정량 분석은? 다양한 file system(F2FS, Btrfs, XFS)으로의 일반화 가능성은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 연구가 지목한 SSD 단편화 성능 저하의 원인은 무엇이며, 본 논문은 왜 그것을 반박하는가?
> 답: 기존 연구는 kernel I/O path에서의 **request splitting**(하나의 I/O가 여러 command로 쪼개지며 생기는 오버헤드)을 원인으로 가설했다. 본 논문은 (1) ramdisk 실험에서 DoF 256에서도 read time이 1.5배만 증가하고 queue depth를 키우면 거의 사라지며 (2) command queueing이 splitting 오버헤드를 후속 fragment 처리에 가려버리므로(극단도 ~9.7ms) request splitting은 무시 가능하다고 반박한다.

> [!question]- Q2. 단편화가 실제로 SSD read 성능을 떨어뜨리는 진짜 원인은 무엇인가?
> 답: **die-level collision**의 급증이다. FTL이 page를 write 순서대로 round-robin 배치하므로, 단편화로 다른 파일 write가 끼어들면 연속 file block의 page가 연속 die에 배치되지 못하고 특정 die에 몰린다. sequential read 시 같은 die에서 read가 직렬화되어 내부 parallelism이 깨지고 성능이 떨어진다.

> [!question]- Q3. 파일이 file system level에서 완전히 contiguous인데도 read가 느려질 수 있는 이유는?
> 답: **overwrite** 때문이다. flash는 in-place update가 안 되므로 중간 block을 overwrite하면 새 page가 round-robin상 "직전 할당 die의 다음 die"에 배치된다. file system에서는 여전히 연속이지만 page-to-die 순서가 깨져 die-level collision이 발생한다. 즉 단편화 없이도 misalignment가 생긴다.

> [!question]- Q4. 제안 기법은 append write와 overwrite를 어떻게 다르게 처리하는가?
> 답: append write는 host가 "직전 file block의 LBA"를 hint로 보내고, firmware는 그 page가 놓인 die의 **바로 다음 die**에 새 page를 배치한다. overwrite는 write command에 flag를 세팅해 기존 page를 invalidate하고 **원래 die와 같은 die**에 새 page를 할당해 die-level contiguity를 유지한다.

> [!question]- Q5. 추가 프로토콜 오버헤드 없이 hint를 전달하는 방법은?
> 답: NVMe write command의 미사용 reserved bit를 재활용한다. CDW12의 24·25번째 bit로 append/overwrite/일반 write를 구분하고, append write 시 reserved인 CDW2·CDW3로 직전 file block의 LBA를 전달한다.

> [!question]- Q6. 162MB SQLite DB를 10,011조각으로 단편화한 대표 결과는?
> 답: 기존 시스템은 read 성능이 40% 저하됐지만, 제안 기법은 3.5% 저하에 그쳤다(Abstract). 별도 SQLite 실험(10,000×16KB, DoF 5,005)에서도 select 성능이 60% 수준에서 1.6배 향상되어 contiguous 대비 3.5% 저하로 회복됐다.

> [!question]- Q7. 제안 기법의 한계가 드러나는 시나리오는?
> 답: write 크기가 flash page 크기(예: 32KB)보다 작은 경우(fileserver small, 16KB append). 두 file의 write가 한 page에 묶여 **page-level 단편화**가 생기는데, die 배치만 고치는 본 기법으로는 효과가 8.2%에 그친다. 이를 위해서는 별도의 page allocation 전략이 필요하다(future work).

> [!question]- Q8. 특정 die에 overwrite가 몰릴 때의 부작용과 완화책은?
> 답: 같은 die의 free page가 빨리 고갈돼 이른 GC가 발생할 수 있다. 다만 overwrite는 기존 page를 invalidate하므로 GC 시 복사할 valid page가 줄어 WAF와 GC 시간이 오히려 짧아진다. uneven wear가 심해지면 해당 LBA의 page를 다른 die로 재배치하는(성능 일부 희생) 메커니즘으로 완화할 수 있다.

## 🔗 Connections
[[File System]] · [[FAST]] · [[2024]]

## References worth following
- Jonggyu Park, Young Ik Eom. "FragPicker: A new defragmentation tool for modern storage devices." SOSP '21. — 접근된 fragment만 online migration하는 defrag, 본 논문이 반박/비교하는 핵심 선행 연구 [31].
- Yuhun Jun, Jaehyung Park, Jeong-Uk Kang, Euiseong Seo. "Analysis and mitigation of patterned read collisions in flash SSDs." IEEE Access, 2022. — die-level collision 개념의 저자 선행 연구 [18].
- Sang-Hoon Kim et al. "NVMeVirt: A versatile software-defined virtual NVMe device." FAST '23. — 제안 기법 FTL을 구현·평가한 emulator [22].
- Alex Conway et al. "Filesystem aging: It's more usage than fullness." HotStorage '19 / "File systems fated for senescence?" FAST '17. — SSD에서 aging으로 인한 read 5배 저하를 관측한 연구 [4,5].
- Feng Chen, Rubao Lee, Xiaodong Zhang. "Essential roles of exploiting internal parallelism of flash memory based SSDs." HPCA '11. — SSD 내부 die parallelism의 중요성 [3].
- Jonggyu Park, Young Ik Eom. "File fragmentation from the perspective of I/O queueing." HotStorage '22. — fragment 간 거리가 SSD 성능에 영향 준다는 상반된 주장 [32].

## Personal annotations
<본인 메모 영역>
