---
title: "FastCommit: resource-efficient, performant and cost-effective file system journaling"
aliases: [FastCommit]
description: "JBD2 physical journaling의 byte/IO 과다 소비를 hybrid(logical+physical) 저널링으로 해소해 fsync를 2.8배 빠르게 한 Ext4 메인라인 머지 작업"
venue: ATC
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/journaling
  - topic/filesystem
  - venue/atc
  - year/2024
---

# FastCommit: resource-efficient, performant and cost-effective file system journaling

> **ATC 2024** · `cluster/fs` · Source: [FastCommit - resource-efficient, performant and cost-effective file system journaling.pdf](<FastCommit - resource-efficient, performant and cost-effective file system journaling.pdf>)
> 저자: Harshad Shirwadkar, Saurabh Kadekodi, Theodore Tso (Google)

## TL;DR
Ext4의 기본 저널인 **JBD2(physical journaling)**는 변경된 metadata 블록 전체를 복사·기록해 byte/IO/cache-flush 오버헤드가 크다. FastCommit은 5초 단위 느린 JBD2 commit 사이에 **logical journaling**을 끼워 넣는 **hybrid(logical+physical) 저널**로, (1) 다수의 metadata 변경을 4KB 한 블록에 packing하는 *compact logging(FCLog/FCTag)*, (2) FUA로 cache flush를 생략하는 *selective flushing*, (3) fsync를 호출한 user thread가 직접 commit해 context switch를 없애는 *inline journaling* 을 결합한다. 그 결과 fsync 지연을 약 **2.8배** 단축하고, byte/IO/flush 오버헤드를 각각 최대 **63%/42%/79%** 줄이며, 저널 간섭을 **2배** 감소시키고 클라우드 월 비용을 **22~57%** 낮춘다. 대부분의 코드는 Linux 메인라인 Ext4(`fs/ext4/fast_commit.c`)에 머지되었다.

## 문제 & 동기
- JBD2는 commit마다 최소 3블록(12KB), 평균 6블록(24KB)을 두 번의 IO로 기록하는 **physical journal**이다. metadata-heavy workload나 NFS처럼 fsync가 빈번하면 오버헤드가 폭증한다.
- mail-server workload 측정에서 JBD2가 디스크 write 대역폭의 **76%**를 소비, end-to-end throughput을 45% 떨어뜨리고 fsync 지연을 280% 증가시켰다(p.1, p.2).
- NFS는 close-to-open 시 모든 파일 변경에 fsync를 발생시켜(mail-server에서 26배 많은 fsync) JBD2의 약점을 극대화 → throughput 8배 하락(p.2).
- 클라우드 VBD(Google PD, Amazon EBS 등)는 capacity/bandwidth/IOPS를 독립 과금하므로, 자원을 많이 먹는 저널은 throttling 또는 추가 비용 구매를 강제한다(p.2~3).
- 순수 logical journal(XFS류)은 byte/IO는 최적이나 Ext4 저널링 서브시스템과 on-disk 포맷을 전면 재설계해야 하고 성숙한 recovery 구현에 수년이 걸려 비현실적(p.2).

> [!quote]- 📄 원문 표현 (paper)
> "JBD2, the current physical journaling mechanism in Ext4 is bulky and resource-hungry. ... In a simple multi-threaded mail-server workload, JBD2 consumed approximately 76% of the disk's write bandwidth." (p.1)
> "We present FASTCOMMIT: a hybrid logical+physical journal that is resource-efficient and minimizes journal overhead." (p.3)
> "A natural alternative is to build purely logical journaling in Ext4 (similar to XFS), ... However, this option would require a comprehensive redesign of the current JBD2 journaling subsystem, ... effectively making adoption impractical." (p.3)

## 핵심 통찰

> [!note]- 통찰 1 — fsync를 JBD2 commit에서 decouple하라
> JBD2 commit은 5초 주기 또는 fsync마다 트리거되는데, fsync가 잦으면 commit이 잦아져 bulky한 physical 기록이 반복된다. FastCommit은 연속된 두 느린 commit 사이에 **logical journal(FCLog)**을 추가해 fsync마다 vfs 수준의 *operation*만 기록하고, 무거운 JBD2 checkpointing은 5초 주기로 미룬다. fsync와 JBD2 commit의 분리가 핵심.

> [!note]- 통찰 2 — logical은 작지만 recovery가 복잡하다 → hybrid로 양다리
> 순수 logical은 byte/IO 최소이나 복구가 어렵고 Ext4 재설계가 필요. 순수 physical은 단순하나 무겁다. FastCommit은 복잡·희귀한 연산(파일 시스템 resize 등)은 기존 JBD2(physical)로 fallback("slow commit")하고, 흔한 연산만 logical로 처리해 두 방식의 장점만 취한다.

> [!note]- 통찰 3 — 실용성이 곧 채택 가능성이다
> 학술적으로 완전 재설계는 매력적이나, 파일시스템 성숙에 ~10년이 걸리고 클라우드 마이그레이션 사용자는 기존 setup을 그대로 옮긴다. FastCommit은 **JBD2 API/Ext4 on-disk 포맷을 바꾸지 않고**, 1.5% JBD2 공간만 빌려 쓰며, 별도 reformat 없이 Ext4+JBD2 위에서 켤 수 있도록 설계 → 코드 대부분이 메인라인 Linux에 머지(p.3).

> [!note]- 통찰 4 — VFS의 idempotent 결과를 기록하면 recovery가 단순해진다
> FCTag는 연산 자체가 아니라 *연산의 결과(예: 삭제 후 inode reference count=2)*를 저장해 idempotent replay를 보장한다. 별도 recovery 로직 대신 Linux **VFS API를 재사용**해 각 FCTag를 replay한다(p.8).

## 설계 / 메커니즘

> [!abstract]- Hybrid journaling 구조 (§5.1)
> - JBD2는 여전히 5초마다 physical commit("slow commit") 수행. 그 사이 fsync 시점에 FastCommit이 logical commit 시도, 불가능하면 JBD2로 fallback.
> - **Logical journaling area(FC area)**: JBD2 예약 공간의 기본 **1.5%**만 마킹해 logical 로그 저장. 매우 공간 효율적이라 추가 디스크 공간 불필요, Ext4 on-disk 포맷 그대로(p.5).
> - file-level(logical, inode 단위)과 block-level(physical, JBD2) 저널링을 layering violation 없이 동시 지원. JBD2가 Ext4 block-mutating API를 inode-mutating 루틴 뒤에 shim으로 감싸 둔 점을 활용(§5.6).

> [!abstract]- Compact logging — FCLog / FCTag (§5.2)
> - FastCommit commit = 여러 파일에 대한 작은 **FCTag**들의 묶음 = **FCLog**. 대개 FCLog 하나가 4KB 한 블록에 들어간다(p.5).
> - FCTag 필드: type(2B) + length(2B) + value(가변). FCLog는 head tag(12B, 직전 slow commit의 commit ID 포함)로 시작, tail tag(12B, FCLog 전체 checksum)로 끝남(p.6).
> - 8종 FCTag만으로 대부분 연산 표현: HEAD, ADD_RANGE(데이터 추가), DEL_RANGE(데이터 삭제), CREAT(파일 생성), LINK(symlink/rename), UNLINK(삭제), INODE(inode 저장), TAIL. (XFS는 ~40종 필요 → Ext4 재설계 불가피)
> - 예) 4KB append: HEAD(12B)+ADD_RANGE(20B)+INODE(136B)+TAIL(12B) = **168B**. 동일 작업 JBD2는 6블록 = **24KB**(p.6).
> - 예) rename: HEAD+LINK+UNLINK+INODE+TAIL = **192B**. JBD2는 7블록 = **28KB**(p.6).

> [!abstract]- Selective flushing (§5.3)
> - cache flush는 휘발성 캐시를 강제로 내려 무관한 데이터까지 디스크에 쓰게 만들어 비싸다. JBD2는 commit당 최소 1회 flush 필요(데이터 durable, descriptor/metadata durable).
> - FastCommit은 (1) FCLog가 4KB 한 블록에 들어가고 (2) 함께 써야 할 user data가 없으면, **FUA(Force Unit Access)** write로 단일 블록만 non-volatile에 내려 cache flush를 **생략**한다(p.7). FUA는 1블록 write에만 가능.

> [!abstract]- Inline journaling (§5.4)
> - 기존 JBD2 commit엔 context switch 2회 발생: fsync→전용 JBD2 thread 깨우기, JBD2 thread→user thread 깨우기. JBD2 thread는 높은 IO 우선순위를 가져 깨우는 지연(S_f)이 큼.
> - FastCommit commit은 작고 빠르므로 **fsync를 호출한 user thread가 직접 commit을 수행**. commit 동안만 user thread의 scheduling/IO 우선순위를 JBD2 thread 수준으로 일시 상승, 완료 후 원복 → context switch 제거(p.7).

> [!abstract]- Crash recovery & 호환성 (§5.5~5.8)
> - 복구 시 먼저 JBD2 area를 복구한 뒤 FC area 순회, 각 FCLog 내 FCTag를 순차 replay(VFS API 재사용). 직전 slow commit ID와 head tag가 다른 FCTag는 폐기(p.8).
> - **Replay idempotency**: FCTag는 결과를 저장하므로 복구 중 재크래시해도 처음부터 안전하게 재시작(p.8).
> - **API 무변경**(§5.6), **backward compatibility**: FASTCOMMIT enabled/present 두 플래그로 구/신 커널 마운트 호환 보장(§5.7).
> - **Upstream readiness**: XFSTests(auto·log group) 전부 통과, XFSTestsbld로 일일 연속 테스트(§5.8).

## 평가

> [!example]- 실험 셋업 & 비교군
> - GCE n2-standard-32 VM (32 vCPU, 128GB RAM, 32Gbps), Debian 11, Linux 5.19. 시나리오 3종: 로컬 SSD(375GiB), NFS+SSD, NFS+VBD(Google Hyperdisk: 150MB/s·15000 IOPS·2TB).
> - 비교: Ext4+FastCommit(FC) vs JBD2 / Async(JBD2 비동기 commit) / CJFS(FAST'23, JBD2 SOTA) / XFS(순수 logical). iJournaling은 코드 비공개로 제외(p.9).
> - 벤치: micro(create+append+fsync, fsync 빈도 1024→1배), macro = Varmail, Fileserver, Postmark, FSMark(p.9).

> [!example]- fsync 지연 (§6.1)
> - 단일 파일 micro 분해: JBD2 fsync **458µs** vs FC **162µs** → **2.8배** 단축. 항목별: descriptor+metadata 182µs, commit marker FC 144µs vs JBD2 99µs, context switch+misc JBD2 177µs vs FC 18µs(Table 1, p.10).
> - Varmail/NFS 1M ops: JBD2 mean 281µs vs FC **119µs**(2.3배↓), 50th 272 vs **79µs**(3배↓). p99는 FC 1.47ms로 JBD2 0.47ms보다 큼(slow commit fallback 꼬리 지연). 분포는 FC가 훨씬 tight·예측가능(Fig.12, p.11).
> - macro 4종 median fsync: FC가 JBD2 대비 **2.5배 이상** 빠름(Fig.13, p.11).

> [!example]- 자원 효율 (byte/IO/flush) (§6.2, §6.3.2)
> - Abstract 요약: byte/IO/cache-flush 오버헤드를 JBD2 대비 각각 최대 **63%/42%/79%** 감소, 순수 logical XFS보다도 낮음(p.3).
> - byte: 모든 벤치·시나리오에서 FC가 최소, Fileserver 제외 JBD2 대비 거의 **2배** 적게 기록(Fig.8/11, p.10·11). 로컬 SSD에서 commit의 98.5%가 단일 블록, 81%가 metadata+commit marker를 1 IO로 처리(p.12).
> - IO/flush: FC·XFS가 최저. CJFS는 fsync당 3 IO(JBD2보다 50% 많음). FC는 fsync당 ~1 IO(Fig.9/10/14, p.10·12).

> [!example]- throughput / scalability / 간섭 / 비용 (§6.3~6.4)
> - app throughput: FC가 대부분 최고, JBD2 대비 **1.75~2배**(IO-intensive, fsync 병목). NFS+VBD에서 특히 효과 큼(Fig.15, p.12).
> - scalability: 1~40 thread, 40 thread에서 Varmail throughput이 타 FS 대비 **2배 이상**(Fig.17, p.13).
> - journal 간섭(FSMark+FIO 멀티테넌트): JBD2가 대역폭 50% 소비, FC는 ~60MB/s만 소비하며 JBD2보다 **19% 빠름**. FSMark·FIO throughput 모두 상승(Fig.18, p.13). Abstract: 간섭 2배↓, 두 동시 workload throughput 80%·23%↑, 결합 runtime 20%↓(p.3).
> - 클라우드 비용: p95 bandwidth/IOPS 프로비저닝 최소. VBD 공개 과금 기준 월 비용 JBD2 대비 **22~57%** 절감(Fig.16/19, p.13~14).

## 섹션 노트
- **§1 Introduction**: 문제(JBD2 76% 대역폭, NFS 8배 throughput 하락), 순수 logical 재설계의 비현실성, hybrid 제안과 3대 기법 소개.
- **§2 Background**: logical vs physical journaling, JBD2(고정 on-disk 포맷, multi-block atomic, checkpointing), NFS CTO/async·sync, 클라우드 VBD/PBD 과금.
- **§3 Related work**: XFS(logical), CJFS(JBD2 확장, IO 3배), iJournaling(Hyrum's Law 위반·fsync 의미 변경), Fine-Grained Journaling(NVM·전면 재구조화 필요)와 차별화.
- **§4 Motivation**: fsync 빈도 1024→1 스윕으로 JBD2 byte/IO/flush 급증 측정(Fig.2~4), NFS가 JBD2 최악 케이스, 클라우드 과금 모델이 JBD2에 불리.
- **§5 Design**: 5.1 hybrid, 5.2 FCLog/FCTag(compact logging), 5.3 selective flushing(FUA), 5.4 inline journaling, 5.5 crash recovery·idempotency, 5.6 API 무변경, 5.7 backward compat, 5.8 upstream 테스트.
- **§6 Evaluation**: 6.1 fsync 성능, 6.2 자원 효율(byte/IO), 6.3 throughput·scalability·간섭, 6.4 클라우드 비용.
- **§7 Conclusion / §8 Ack / Appendix A**: 기여 요약, artifact(대부분 upstream `fs/ext4/fast_commit.c`, 미머지 패치·벤치 스크립트 GitHub).

## 핵심 용어
- **JBD2 (Journaling Block Device v2)**: Ext4의 기본 physical journal. 변경된 metadata 블록 전체를 복사해 트랜잭션으로 기록, multi-block atomic 보장. bulky하지만 복구 단순.
- **Physical journaling**: 변경된 metadata 블록의 복사본을 저장하는 방식. 포맷 독립·복구 단순하나 byte/IO 큼.
- **Logical journaling**: 파일/디렉터리 조작(연산) 자체를 inode 수준에서 기록. 작고 빠르나 복구 복잡(replay 필요).
- **Hybrid journaling**: FastCommit의 logical+physical 결합. 흔한 연산은 logical(fast commit), 복잡·희귀 연산은 JBD2 physical(slow commit)로 fallback.
- **FCLog (FastCommit log)**: 한 fast commit 단위. 여러 FCTag를 4KB 한 블록에 packing, head/tail tag로 감쌈.
- **FCTag**: 단일 파일 변경을 표현하는 최소 단위 태그(type/length/value). 8종(HEAD/ADD_RANGE/DEL_RANGE/CREAT/LINK/UNLINK/INODE/TAIL). 결과를 저장해 idempotent.
- **Compact logging**: 다수 metadata 변경을 FCTag로 압축해 4KB 한 블록에 담아 byte 오버헤드 축소.
- **Selective flushing**: FCLog가 단일 블록이고 user data가 없을 때 FUA write로 cache flush를 생략하는 기법.
- **FUA (Force Unit Access)**: 단일 블록 write를 디스크 휘발성 캐시를 건너뛰어 non-volatile에 바로 기록하는 명령(full cache flush 아님).
- **Inline journaling**: fsync를 호출한 user thread가 직접 commit을 수행해 JBD2 전용 thread 기상에 따른 context switch를 제거.
- **Slow commit / Fast commit**: 5초 주기 또는 fallback 시의 JBD2 physical commit이 slow, 그 사이 fsync마다의 logical commit이 fast.
- **CJFS**: FAST'23의 JBD2 확장(concurrent journaling). byte는 동급이나 fsync당 IO가 JBD2보다 50% 많음.

## 강점 · 한계 · 열린 질문
- **강점**: 실용성 최우선 — API/on-disk 포맷 무변경, 1.5% 공간만 사용, reformat 불필요, 코드 대부분 Linux 메인라인 머지. fsync 2.8배·자원 오버헤드 대폭 절감과 backward compat 동시 달성. 멀티테넌트 간섭 감소와 클라우드 비용 절감까지 실측.
- **한계**: p99 fsync 꼬리 지연은 JBD2보다 큼(slow commit fallback 시 평균보다 큰 지연 발생). FC area가 작으면 fallback 빈도↑, 크게 잡으면 slow commit 비용↑ — 튜닝 트레이드오프 존재. logical 표현 가능 연산이 8종 FCTag로 제한, 복잡 연산은 여전히 JBD2 의존.
- **열린 질문**: p99 꼬리 지연을 줄이기 위한 FC area 크기·slow commit 빈도 자동 튜닝? FCTag 집합 확장으로 fallback 비율을 더 낮출 수 있나? 다른 파일시스템/NVMe ZNS·CXL 등 신매체에서 selective flushing(FUA) 이득은? NFS 외 분산 환경에서 hybrid 저널의 일관성 보장 범위?

## ❓ Q&A (자가 점검)

> [!question]- Q1. JBD2가 자원을 많이 쓰는 근본 원인은?
> > physical journaling이라 변경된 metadata 블록 전체 복사본을 commit마다 기록한다. commit당 최소 3블록(12KB)·평균 6블록(24KB)을 2회 IO + cache flush로 써서, fsync가 잦으면(NFS·metadata-heavy) byte/IO/flush가 폭증한다(mail-server에서 write 대역폭 76% 소비).

> [!question]- Q2. FastCommit이 "hybrid"인 이유와 fallback 기준은?
> > logical(fast commit)과 physical(JBD2 slow commit)을 결합하기 때문. 5초마다 JBD2 physical commit을 유지하고, 그 사이 fsync마다 logical FCLog를 기록한다. 8종 FCTag로 표현 불가한 복잡·희귀 연산(예: 파일시스템 resize)은 기존 JBD2 physical commit으로 fallback한다.

> [!question]- Q3. compact logging이 byte를 어떻게 줄이나? 구체 수치는?
> > 여러 파일 변경을 작은 FCTag로 압축해 4KB 한 블록(FCLog)에 packing한다. 4KB append가 JBD2 24KB → FC 168B, rename이 JBD2 28KB → FC 192B. 로컬 SSD에서 commit의 98.5%가 단일 블록으로 처리됐다.

> [!question]- Q4. selective flushing이 가능한 조건과 핵심 명령은?
> > FCLog가 4KB 단일 블록에 들어가고 함께 durable화할 user data가 없을 때 가능하다. 이때 cache flush 대신 단일 블록을 FUA(Force Unit Access) write로 휘발성 캐시를 건너뛰어 non-volatile에 직접 기록해 비싼 flush를 생략한다.

> [!question]- Q5. inline journaling이 제거하는 오버헤드는?
> > JBD2 commit 경로의 2번의 context switch(fsync→JBD2 thread, JBD2 thread→user thread)다. FastCommit은 fsync를 호출한 user thread가 직접 commit하면서 commit 동안만 우선순위를 일시 상승시킨다. 결과적으로 micro 분해에서 context switch+misc 지연이 JBD2 177µs → FC 18µs.

> [!question]- Q6. crash recovery에서 idempotency를 어떻게 보장하나?
> > FCTag가 연산 자체가 아니라 연산의 *결과*(예: 삭제 후 inode reference count 값)를 저장하기 때문이다. 동일 FCTag를 여러 번 적용해도 같은 상태가 되므로, 복구 중 재크래시해도 처음부터 안전하게 replay를 재시작할 수 있다. replay는 별도 로직 없이 VFS API를 재사용한다.

> [!question]- Q7. fsync는 빨라졌는데 p99 꼬리 지연은 왜 JBD2보다 큰가?
> > FastCommit은 약 5초마다 slow commit(JBD2 physical)으로 fallback하는데, 이때 누적 변경을 처리하느라 평균 fast commit보다 큰 지연이 발생한다. 그래서 mean·median은 크게 낮지만(2.3~3배) p99는 FC 1.47ms로 JBD2 0.47ms보다 높다. FC area 축소나 잦은 JBD2 commit으로 꼬리를 완화할 수 있으나 평균 지연 증가와 트레이드오프다.

> [!question]- Q8. 왜 순수 logical(XFS류)로 가지 않고 hybrid를 택했나?
> > 순수 logical은 byte/IO는 최소지만 Ext4의 JBD2 서브시스템과 on-disk 포맷 전면 재설계, ~40종 태그·복잡한 recovery가 필요해 채택이 비현실적이다. FastCommit은 API/포맷 무변경·1.5% 공간·backward compat을 지켜 메인라인 머지를 달성했고, byte/IO 효율은 XFS보다도 낮게 얻었다.

## 🔗 Connections
[[File System]] · [[ATC]] · [[2024]]

## References worth following
- CJFS: Concurrent journaling for better scalability — Oh et al., FAST'23 [25]. FastCommit의 직접 비교 대상(JBD2 SOTA).
- iJournaling: Fine-Grained journaling for improving fsync latency — Park & Shin, ATC'17 [26]. hybrid 영감의 원류이나 fsync 의미 변경 문제.
- Fine-grained metadata journaling on NVM — Chen et al., MSST'16 [6]. byte-level logical journal, NVM 대상.
- Barrier-Enabled IO Stack for Flash Storage — Won et al., FAST'18 [34]. order-preserving IO와 flush 비용.
- RFLUSH: Rethink the flush — Yeon et al., FAST'18 [36]. selective flushing의 근거.
- Journaling the linux ext2fs filesystem — Tweedie et al., 1998 [32]. JBD/JBD2의 기원.

## Personal annotations
<본인 메모 영역>
