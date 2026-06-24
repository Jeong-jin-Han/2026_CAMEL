---
title: "Fast, Transparent Filesystem Microkernel Recovery with Ananke"
aliases: [Fast, Transparent Filesystem]
description: "마이크로커널 파일시스템 프로세스 크래시(p-crash)를 p-log + AIM으로 투명·고속 복구해 애플리케이션이 손실 없이 계속 실행되게 하는 시스템 Ananke"
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/crash-recovery
  - topic/microkernel
  - topic/filesystem
  - venue/fast
  - year/2025
---

# Fast, Transparent Filesystem Microkernel Recovery with Ananke

> **FAST 2025** · `cluster/fs` · Source: [Fast, Transparent Filesystem Microkernel Recovery with Ananke.pdf](Fast,%20Transparent%20Filesystem%20Microkernel%20Recovery%20with%20Ananke.pdf)

저자: Jing Liu (Microsoft Research), Yifan Dai, Andrea C. Arpaci-Dusseau, Remzi H. Arpaci-Dusseau (University of Wisconsin–Madison)

## TL;DR
Ananke는 마이크로커널(user-space) 파일시스템 서버가 프로세스 단위로 죽는 **process crash (p-crash)** 를 전체 시스템 크래시(s-crash)와 분리해 다룬다. 정상 동작 중 시스템 콜의 핵심 정보를 in-memory **P-Crash Log (p-log)** 에 기록해 두고, p-crash가 나면 host OS의 조율 아래 새 프로세스를 띄워 **AIM (Act-Ignore-Modify)** 알고리즘으로 p-log를 재생, on-disk 상태와 애플리케이션이 인식하는 상태 사이의 **state gap** 을 정확히 복원한다. 30,000+ fault-injection으로 무손실(lossless)·투명 복구를 보였고, common-path 성능 오버헤드는 대부분 2% 미만, 복구는 보통 수백 ms(≤400ms)다.

## 문제 & 동기
마이크로커널 파일시스템은 파일시스템 서버가 별도 user-space 프로세스로 동작하므로, 서버가 죽어도(=p-crash) 나머지 시스템과 클라이언트 애플리케이션은 살아있다. 그러나 서버를 그냥 재시작해도 애플리케이션이 인식하던 파일시스템 상태(버퍼링된 업데이트, 열린 file descriptor 등 휘발성 상태)와 on-disk 상태 사이에 **state gap** 이 생겨, 기존 s-crash 복구(journaling 등 on-disk consistency만 보장)로는 메워지지 않는다. 예: 빈 파일에 10번 write 후 close한 직후 서버가 죽고 재시작하면, on-disk는 빈 파일이지만 애플리케이션은 10번 write가 반영된 파일을 기대한다 → 심각한 데이터 손실. p-crash는 power failure 같은 s-crash보다 빈번하므로, p-crash를 전역 장애 시나리오로 격상시키지 않고 별도로 빠르고 투명하게 복구하는 메커니즘이 필요하다.

> [!quote]- 📄 원문 표현 (paper)
> "We refer to the difference between applications' perceived filesystem state and the state recovered by s-crash recovery (i.e., persisted) as the state gap." (p.1)
>
> "Performing s-crash recovery is insufficient because it ensures that the on-disk states are consistent, but does not address the state gap." (p.2)
>
> "filesystem failures and p-crashes occur frequently, whereas power failures are relatively rare." (p.2)

## 핵심 통찰

> [!note]- 통찰 1 — p-crash 복구는 s-crash 복구와 분리하면 "전체 머신이 살아있는" 기회를 활용할 수 있다
> p-crash 시점에는 OS와 다른 시스템 서비스가 정상 동작하므로, OS가 이상적인 coordinator 역할을 하고 server memory에 남은 정보를 복구에 활용할 수 있다. 이 기회를 살려, full-system 복구 때는 얻을 수 없는 핵심 정보(p-log)를 미리 기록해 두면 fast·transparent 복구가 가능하다.

> [!note]- 통찰 2 — state gap은 POSIX의 세 가지 추상화(fd, inode, path↔inode)에 대한 변경의 집합으로 정확히 추적 가능하다
> 파일시스템 API는 file descriptor(fd), file inode, pathname→inode 매핑 중 일부를 바꾼다(Figure 4). 이 중 fd는 ephemeral하고 나머지는 persistent하다. 각 p-log 항목이 관여한 fd/inode를 `targets` 배열과 `targets_status` 비트맵으로 기록하면, 어떤 변경이 아직 state gap에 속하는지(아직 영속화 안 됨)를 정확히 가려낼 수 있다.

> [!note]- 통찰 3 — 메모리 손상(memory corruption) 가능성을 가정한 현실적 fault model이 필요하다
> 기존 복구 시스템들은 fail-stop만 가정하거나, 실패한 커널/메모리/메타데이터를 재사용해 추가 오류 위험이 있었다. Ananke는 transient fault로 인한 메모리 손상까지 다루며, 실패 후엔 checksum·replication으로 보호된 p-log만 신뢰하고(limited trust in memory), 손상된 메모리는 그냥 버린다.

> [!note]- 통찰 4 — 깨끗한 새 프로세스로의 robust restart + 투기적(speculative) 사전 준비로 복구 비용을 숨긴다
> 실패한 프로세스의 자원을 재사용하지 않고, host OS가 조율해 clean address space의 새 프로세스를 띄운다. 새 파일시스템 프로세스가 시작될 때 secondary passive 프로세스도 미리 만들어, 수 초가 걸리는 device connection 초기화를 main과 병렬로 미리 수행(speculative restart)해 복구 지연을 숨긴다.

## 설계 / 메커니즘

> [!abstract]- 베이스 아키텍처 (uFS) 와 5대 도전 과제(C1~C5)
> Ananke는 고성능 오픈소스 마이크로커널 파일시스템 **uFS** 위에 구축된다. uFS는 POSIX-compliant, multi-threaded user-space FS로 SPDK 드라이버로 스토리지에 직접 접근하고, thread-per-core 구조(각 thread = worker)로 확장된다. 애플리케이션은 shared memory의 message ring buffer(App-Worker MsgRing)로 시스템 콜을 보낸다. uFS는 s-crash 복구용 journaling(s-log)을 가진다.
> 이상적 복구의 5대 도전: **C1** state gap 복구, **C2** common-path 성능 영향 최소화, **C3** memory corruption 처리, **C4** robust restart(실패 자원 재사용 금지), **C5** prompt error detection. (Table 1: Ananke만 5개 항목 모두 best)

> [!abstract]- P-Crash Log (p-log) 설계
> 클라이언트 연산 중 state gap에 기여하는 것을 추적하는 in-memory circular buffer. per-core(코어당 하나, 단일 writer thread)로 lock-free하게 운영. 각 항목(Figure 3a)은 operation(syscall + args + return value)과 descriptor(pid, written data page 논리 포인터 `pages[]`, op_crc, timestamp, ipc_idx, op checksum, 그리고 핵심인 `targets[5]`와 `targets_status` 비트맵 + self_crc)를 담는다. `targets`의 첫 원소는 fd, 나머지는 inode(self, parent, dst_self, dst_parent). 연산 완료 시 로깅되며, 이후 close나 영속화로 클라이언트 뷰가 disk와 일치하면 해당 `targets_status` 비트를 클리어해 garbage collection 대상이 되게 한다. **no force flush** (C1,C2): common-path 성능을 위해 버퍼 업데이트를 디스크로 강제하지 않는다. p-log는 메모리 손상에 대비해 **replication(2 copies, pointer-less) + checksum** 으로 보호되며, 실패 후 유일하게 신뢰되는 영역이다. p-log는 데이터 자체가 아니라 page cache 페이지로의 논리 포인터만 저장(end-to-end 논리상 데이터 손상은 애플리케이션 책임).

> [!abstract]- AIM (Act-Ignore-Modify) 알고리즘
> p-log를 입력받아, fresh 프로세스가 실행할 새 시스템 콜 집합을 출력하는 재생 알고리즘. 각 연산을 세 가지로 분류:
> - **Ignore**: 관련 fd가 모두 close되고 inode 상태 변경이 모두 영속화됨(targets_status 비트 전부 0) → 재생 불필요.
> - **Act**: 다음 셋 모두 참이면 원본 그대로 재생 — ① fd가 갱신됐고 fd가 아직 열려있거나 닫혔어도 해당 inode가 sync 안 됨, ② 참조 inode 중 sync된 게 없음, ③ path 기반 연산은 의존 path↔inode가 이후 변경으로 영속화되지 않음.
> - **Modify**: Act도 Ignore도 아니면, 일부 효과는 이미 영속화됐으므로 연산을 변형해 재생(예: write→lseek, creat→open). path 기반 연산은 재생 시 pathname 인자를 영속 상태에 맞게 보정. 내부 API 확장으로 creat/mkdir이 inode 번호를, open이 fd 번호를 지정 가능하게 하고 `CreatReuseDInode`를 도입해 directory inode가 sync 안 된 상황을 처리.
> AIM은 garbage collection(common path)과 replay(recovery) 양쪽에 쓰인다. p-log concurrency는 timestamp를 linearizability point로 삼아 per-core private p-log 간 전역 순서를 형성(critical path의 추가 inter-core 통신 회피).

> [!abstract]- Kernel-coordinated Speculative Restart & Lightweight Corruption Detection
> (C4) host OS가 main 프로세스의 종료(hardware exception, checksum/assertion 실패, segfault 등)를 즉시 감지하고 제어권을 잡아, 실패 프로세스와 fresh 프로세스 간 전환을 조율한다. IPC 연결·driver connection을 fresh 프로세스가 올바로 다시 맺도록 보장하고, fresh 프로세스 시작 시 secondary passive 프로세스를 미리 띄워 수 초짜리 device 초기화를 병렬로 숨긴다(2.9초 overlap). fresh 프로세스 알림은 `PTHREAD_MUTEX_ROBUST` mutex로 구현. 현재 구현은 main 프로세스가 signal handler(`sigaltstack`)로 data page를 rescue.
> (C5) heap의 file descriptor, DMA-able memory의 metadata/data 등 in-memory semantic 구조에 CRC checksum을 달아 read/write마다 검증·갱신 → fault를 조기 감지해 fail fast. memory overhead 0.2%, performance overhead 2.85%. 이 checksum은 복구 시 신뢰 판단엔 쓰지 않고(replicated p-log만 신뢰) 손상 발생 감지에만 사용.

## 평가

> [!success]- 평가 요약 (수치)
> 환경: 128GB RAM, AMD 2.80GHz, NVMe SSD(Samsung PM173X, raw latency ~70us), p-log GC threshold 4MB.
> 비교 대상: uFS(s-crash만), uFS-Sync(매 fsync/creat마다 전체 flush 강제), Membrane-style replay.
>
> - **투명 복구 vs 성능 (LevelDB, Figure 1)**: uFS는 p-crash 후 LevelDB가 exit→수동 개입 필요. uFS-Sync는 투명 복구하나 write 워크로드에서 ~5x(YCSB-Load), ~3x(YCSB-A) 느림. Ananke는 양쪽 다 달성(투명+고성능). 복구 시간: Load=uFS-Sync 285ms / Ananke 114ms; YCSB-A=310ms / 171ms; YCSB-C=108ms / 102ms (p.3).
> - **실제 애플리케이션 투명 복구 (Figure 7)**: gnu-sort, cp(CpDir), unzip, SQLite, LevelDB 5종에 각 시스템 콜 후/중에 p-crash 주입(30,000+ 케이스). uFS는 F_OK/F_BAD/S_BAD 다수 발생(애플리케이션이 잘못된 결과/데이터). **Ananke는 전부 S_OK** (올바른 return code + 무손실 데이터) (p.10–11).
> - **memory corruption (Table 2)**: stack/heap/DMA-metadata/p-log 4개 영역에 18,100 케이스 corruption 주입. fault가 error로 manifest한 3,373 케이스 전부 successful restart + correct FS metadata. correct FS data는 stack 73%(11/15), heap 99.8%, 나머지 100% (현 구현 한계: signal handler가 stack/heap의 data page rescue 시 일부 corrupt). 각 영역 detection 비율 0.71%/20.4%/73.5%/100% (p.11).
> - **common-path 성능 오버헤드 (Figure 8)**: uFS 대비, full memory protection(replication+CRC) 포함 시 대부분 워크로드 <2%, 매우 write-intensive(copy, LevelDB-Load)도 ≤7%. 비교로 uFS-Sync는 최대 ~6x 느리고 Membrane은 dir/file 생성 많은 워크로드에서 3.4x 느림 (p.11).
> - **memory overhead**: p-log는 garbage되지 않은 요청에 비례, 4MB GC threshold로 제한. CRC overhead는 최대 0.02%, p-log 복제까지 합쳐도 작음. p-log 항목당 ~8MB 미만, CRC는 workload memory의 0.01% 미만 (p.1, p.12).
> - **복구 시간 (Figure 10)**: uFS/uFS-Sync/Ananke 모두 <400ms. kernel-coordinated speculative restart가 device connection 재구축(2.9s)을 워크로드 실행과 overlap. Ananke 복구 시간은 p-log 내 연산 수에 비례, fsync 시 p-log가 축소되어 짧아짐 (p.12).

## 섹션 노트
- **§1 Introduction**: p-crash vs s-crash 구분, state gap 정의, Ananke 개요(p-log + AIM + kernel-coordinated speculative restart + lightweight checksum). 30,000+ fault-injection, <2% 오버헤드, ≤400ms 복구.
- **§2 Background & Motivation**: monolithic FS 실패=full-system crash(s-crash, journaling으로 on-disk consistency만), microkernel FS 실패=p-crash. §2.3 separate p-crash recovery 주장(가용성↑, 빈번함, 전역 장애 격상 회피). §2.4 대안들(s-crash로 변환/client-side recovery/write buffering 제거=uFS-Sync)의 비용. §2.5 기존 시스템 한계와 C1~C5.
- **§3 Ananke Design**: §3.1 목표(transparent·fast common-path·realistic fault model·fast recovery·few codebase mods), §3.2 uFS 베이스, §3.3 설계 원칙(no force flush/limited trust/clean restart/fail fast), §3.4 4단계 복구 흐름 + 메커니즘, §3.5 p-log 상세, §3.6 AIM 상세(Figure 4 직관, Figure 5 예제 16개).
- **§4 Implementation**: uFS에 ~4K LoC 추가. per-core p-log, exactly-once semantics(replby 전 p-log 갱신), 4-step careful update protocol(compiler barrier로 reordering 방지), p-log replication(primary→replica, CRC 검증), datablock/inode/bitmap/dentry CRC, kernel-coordinated speculative restart(현재는 main 프로세스 signal handler).
- **§5 Evaluation**: §5.2 투명복구 vs 성능, §5.3 state gap 복구(real apps + multiple processes), §5.4 memory corruption, §5.5 common-case overhead(성능/메모리), §5.6 recovery performance.
- **§6 Related Work**: 하드웨어/소프트웨어 fault 동기, Membrane/Rio/Nova-Fortis/Otherworld/microreboot/Rx/Nooks/Shadow Driver/Recovery Domain/CuriOS/OSIRIS/TxIPC/Redleaf/Theseus 대비.
- **§7 Conclusion**: 다양한 fault를 투명·고성능 복구, fault tolerance가 과도한 비용을 요구하지 않음을 입증.

## 핵심 용어
- **p-crash (process crash)**: 마이크로커널 파일시스템 서버 프로세스만 죽고 OS와 나머지 시스템은 살아있는 장애 모델.
- **s-crash (system crash / full-system crash)**: monolithic 커널 FS 실패처럼 OS 포함 전체가 죽어 휘발성 상태가 모두 손실되는 장애(power failure와 동일하게 취급).
- **state gap**: 애플리케이션이 인식하는 파일시스템 상태와 s-crash 복구가 복원하는 영속(on-disk) 상태 간의 차이.
- **p-log (P-Crash Log)**: state gap에 기여하는 클라이언트 연산을 기록하는 in-memory circular buffer. per-core, replicated, checksummed, pointer-less.
- **AIM (Act-Ignore-Modify)**: p-log를 재생해 state gap을 재구성하는 알고리즘. 각 연산을 그대로 실행(Act)/무시(Ignore)/변형 실행(Modify)으로 분류.
- **targets / targets_status**: p-log 항목이 관여한 fd·inode(targets[5])와 각 target 변경이 아직 state gap에 속하는지 나타내는 비트맵.
- **kernel-coordinated speculative restart**: host OS가 실패 감지·조율해 clean한 새 프로세스로 재시작하며, device 초기화를 미리 병렬 수행해 지연을 숨기는 기법.
- **uFS**: Ananke의 베이스가 되는 SPDK 기반 고성능 user-space POSIX 파일시스템 마이크로커널.
- **no force flush**: common-path 성능 유지를 위해 버퍼된 업데이트를 디스크로 강제하지 않고 p-log로 state gap을 포착하는 원칙.
- **S_OK / S_BAD / F_OK / F_BAD**: fault-injection 결과 분류. S/F=성공/실패 return code, OK/BAD=데이터가 무장애 실행과 동일한지 여부. S_OK가 이상적.

## 강점 · 한계 · 열린 질문
- **강점**: p-crash와 s-crash를 분리한다는 명확한 문제 정의와 state gap이라는 정밀한 추상화. 30,000+ + 18,100 fault-injection으로 무손실·투명 복구를 실증. common-path 오버헤드가 매우 낮음(<2~7%). 메모리 손상까지 현실적 fault model로 포괄.
- **한계**: 현 구현은 stack/heap corruption 시 signal handler가 data page를 rescue하다 일부 실패(stack data 73%, heap 99.8%) → eBPF/read-only kernel space로 rescue를 옮겨 개선 예정. App-Worker MsgRing(애플리케이션과 공유) 손상은 설계 대상 외(보안 계약상 FS가 책임지지 않음). data page corruption은 애플리케이션 책임(end-to-end). transient fault 가정(같은 fault가 replay 중 재발하지 않음).
- **열린 질문**: deterministic(재발) fault나 악의적 애플리케이션의 MsgRing 손상에 대한 보호는? POSIX 외 다른 API 모델(예: io_uring 스타일)로 AIM이 일반화되는가? 다중 코어/다중 디바이스 규모에서 per-core p-log 전역 순서화 비용은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. p-crash와 s-crash의 차이, 그리고 왜 s-crash 복구만으로는 부족한가?
> > p-crash는 FS 서버 프로세스만 죽고 시스템·클라이언트는 살아있는 장애, s-crash는 OS 포함 전체가 죽는 장애다. s-crash 복구(journaling)는 on-disk consistency만 보장할 뿐, 애플리케이션이 인식하던 버퍼된 업데이트·열린 fd 등 state gap을 복원하지 못해, 그냥 서버를 재시작하면 데이터 손실/혼란이 생긴다.

> [!question]- Q2. state gap이란 무엇이고 어떤 추상화로 추적되는가?
> > 애플리케이션이 인식하는 FS 상태와 영속(on-disk) 상태의 차이다. POSIX의 세 추상화 — file descriptor(fd, ephemeral), file inode, path↔inode 매핑 — 에 대한 변경 집합으로 표현되며, p-log의 targets/targets_status가 어떤 변경이 아직 영속화되지 않아 gap에 속하는지 추적한다.

> [!question]- Q3. p-log는 무엇을 기록하며 어떻게 메모리 손상으로부터 보호되는가?
> > state gap에 기여하는 시스템 콜(연산+인자+반환값), 관여한 fd·inode(targets), 영속 여부 비트맵(targets_status), data page로의 논리 포인터, checksum을 per-core circular buffer에 기록한다. pointer-less하게 설계되고 2-copy replication과 CRC로 보호되어, 실패 후 유일하게 신뢰되는 영역이 된다.

> [!question]- Q4. AIM의 Act / Ignore / Modify는 각각 언제 적용되는가?
> > Ignore: 관련 fd가 모두 close되고 inode 변경이 모두 영속화되어 재생 불필요할 때. Act: fd가 아직 열려있거나(또는 inode 미sync), 참조 inode 중 sync된 게 없고, path↔inode 의존이 깨지지 않아 원본 그대로 재생 가능할 때. Modify: 효과 일부가 이미 영속화되어 원본을 변형해야 할 때(예: 이미 persist된 write→lseek로, creat→open으로).

> [!question]- Q5. no force flush가 왜 중요하며 uFS-Sync와 어떻게 다른가?
> > uFS-Sync는 투명 복구를 위해 매 fsync/creat마다 dirty data를 디스크로 강제 flush해 write 워크로드에서 3~6x 느려진다. Ananke는 디스크로 강제하지 않고 작은 p-log로 state gap만 포착하므로 common-path 성능을 유지(오버헤드 <2~7%)하면서도 투명 복구를 달성한다.

> [!question]- Q6. kernel-coordinated speculative restart는 어떻게 복구 지연을 숨기는가?
> > host OS가 실패를 즉시 감지·조율해 clean address space의 새 프로세스로 재시작하고, fresh 프로세스 시작 시 secondary passive 프로세스를 미리 띄워 수 초(약 2.9s)짜리 device connection 초기화를 워크로드 실행과 병렬로 미리 수행한다. 그래서 전체 복구가 <400ms로 유지된다.

> [!question]- Q7. Ananke가 가정하는 fault model과 그 한계는?
> > fail-stop p-crash와 transient memory corruption(같은 fault가 replay 중 재발하지 않음)을 다룬다. checksum/assertion으로 손상을 조기 감지(fail fast)하고 손상 메모리는 버린 뒤 신뢰되는 p-log로만 복구한다. 한계: deterministic fault, App-Worker MsgRing 손상, data page 자체 손상(애플리케이션 책임)은 설계 대상 외이며, 현 구현은 stack/heap data rescue가 일부 실패한다.

> [!question]- Q8. 평가에서 Ananke의 무손실·투명 복구를 가장 잘 보여주는 결과는?
> > Figure 7: 5개 실제 애플리케이션에 30,000+ p-crash를 주입했을 때 uFS는 F_OK/F_BAD/S_BAD가 다수 나오는 반면 Ananke는 전 케이스 S_OK(올바른 return code + 무장애 실행과 동일한 데이터). Table 2: 18,100 memory corruption 케이스에서 error로 manifest한 3,373건 전부 successful restart + correct FS metadata.

## 🔗 Connections
[[File System]] · [[FAST]] · [[2025]]

## References worth following
- Sundararaman et al., **Membrane: Operating System Support for Restartable File Systems**, ACM TOS 2010 [63] — restartable kernel FS의 대표격, Ananke의 주요 비교/replay 기법 출처.
- Chen et al., **The Rio File Cache: Surviving Operating System Crashes**, ASPLOS VII 1996 [10] — warm reboot용 file cache 보존, memory corruption 취약점 대비 비교.
- Depoutovitch & Stumm, **Otherworld: Giving applications a chance to survive OS kernel crashes**, EuroSys 2010 [13] — 애플리케이션 주소공간 재사용 microreboot 접근.
- Liu et al., **Scale and Performance in a Filesystem Semi-Microkernel (uFS)**, SOSP 2021 [36] — Ananke의 베이스 시스템.
- Li et al., **Efficiently recovering stateful system components of multi-server microkernels (TxIPC)**, ICDCS 2021 [33] — instruction-level undo log 기반 microkernel 복구 비교.
- Qin et al., **Rx: Treating Bugs As Allergies**, SOSP 2005 [52] — checkpoint 재시도형 복구, crash-only 설계 비교.

## Personal annotations
<본인 메모 영역>
