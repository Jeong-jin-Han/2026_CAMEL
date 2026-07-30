---
title: "Silo: Speculative Hardware Logging for Atomic Durability in Persistent Memory"
description: "PM 트랜잭션의 atomic durability를 위해 로그를 '백업'이 아닌 '데이터'로 취급하여, crash가 없는 common case에서 로그를 PM에 쓰지 않고 온칩 로그의 새 데이터로 직접 PM data region을 in-place update하는 투기적(speculative) 하드웨어 로깅 기법"
venue: HPCA
year: 2023
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/reliability
  - topic/persistent-memory
  - topic/crash-consistency
  - topic/hardware-logging
  - topic/write-amplification
  - venue/hpca
  - year/2023
  - list/26s-v2
---

# Silo: Speculative Hardware Logging for Atomic Durability in Persistent Memory

> **HPCA 2023** · cluster/reliability · Source: [Silo - Speculative Hardware Logging for Atomic Durability in Persistent Memory.pdf](<Silo - Speculative Hardware Logging for Atomic Durability in Persistent Memory.pdf>)

저자: Ming Zhang, Yu Hua* (교신저자) — Wuhan National Laboratory for Optoelectronics, School of Computer, Huazhong University of Science and Technology

## TL;DR
Persistent memory(PM) 트랜잭션의 atomic durability를 보장하려는 기존 하드웨어 로깅(undo/redo/undo+redo)은 로그를 "백업(backup)"으로 취급해 데이터를 2–3배 더 쓰고, 커밋 전에 로그·캐시라인 flush를 기다려야 하는 ordering 제약을 진다. Silo는 실제 시스템이 대부분 crash 없이 동작한다는 관찰에서 출발해, 온칩 로그 버퍼에 undo+redo 로그를 유지하다가 커밋 후에는 (드문 crash 상황이 아니면) 로그를 PM 로그 영역에 쓰지 않고, 로그 안의 new data로 PM data region을 직접 in-place update하는 "Log as Data" 방식을 제안한다. 이로써 로그 쓰기 자체가 사라지고 커밋이 로그/캐시라인 flush를 기다릴 필요가 없어지며(ordering 제약 제거), overflow와 crash 같은 드문 경우에만 필요한 로그만 선택적으로 PM에 flush해 정확성을 보장한다. Gem5+NVMain 기반 평가에서 state-of-the-art(MorLog) 대비 트랜잭션 처리량 4.3배, 메모리 쓰기 76.5% 감소를 달성했다.

## 문제 & 동기
PM은 `load`/`store`로 직접 접근 가능하지만 non-volatility 때문에 crash 시 일부만 갱신된 데이터가 남을 수 있어, 트랜잭션 단위로 all-or-nothing(atomicity)과 커밋 후 지속성(durability)을 보장하는 atomic durability가 필요하다(p.1–2). 이를 위해 write-ahead logging(WAL)이 널리 쓰이며 소프트웨어(clwb/sfence로 로그 후 데이터를 persist)나 하드웨어(Tx_begin/Tx_end만 지정하고 나머지는 HW가 처리, Fig.1)로 구현된다. 그러나 기존 하드웨어 로깅(ATOM, DHTM, FWB, MorLog, ASAP 등)은 로그를 PM 로그 영역에 보수적으로 써서 데이터를 2–3배 추가로 쓰고(Heavy Writes), undo/redo/undo+redo 각각의 커밋 전 flush 순서 제약(Ordering Constraints: undo는 데이터 flush 후 로그 persist를 기다림, redo는 로그 전부 persist 후 in-place update, undo+redo의 FWB는 로그를 데이터보다 먼저 PM에 강제, MorLog는 커밋 전 L1/로그버퍼의 전체 로그 flush를 대기)로 인해 성능이 저하된다. 저자들은 실제로는 단일 머신이 crash 없이 동작하는 경우가 대부분이며(common failure-free case), 이 경우 트랜잭션 커밋 후 로그가 전부 truncate(폐기)되어 실제로 쓰이지 않는데도 write 오버헤드만 유발한다는 점을 지적한다. 또한 PMDK 워크로드(Array/Btree/Hash/Queue/RBtree/Ctrie)와 TATP·은행 트랜잭션을 측정한 결과 트랜잭션당 write size는 대체로 0.5KB 미만으로 작다(Fig.4, p.4).

> [!quote]- 📄 원문 표현 (paper)
> - "Due to conservatively writing logs to the PM log region in each transaction, legacy hardware logging schemes incur high overheads in two aspects... Writing logs supports crash recovery, but the write traffic significantly increases, which exacerbates the write endurance of PM and hence shortens the PM lifetime." (p.1)
> - "In practice, a single machine normally runs without crash or power failure in most of the time [6], [18]... Although these logs are not used, they incur high write overheads." (p.1)
> - "As shown in Fig. 4, the write size is generally less than 0.5 KB per transaction." (p.4)

## 핵심 통찰 (Key Insight)

> [!note]- 통찰 1 — "Log as Data": 로그를 백업이 아니라 데이터 원본으로 재해석
> undo+redo 로그 한 벌만 온칩 로그 버퍼에 유지하면 이미 트랜잭션이 만든 new data 전체가 로그 안에 들어있다. 기존 설계처럼 이를 PM 로그 영역에 "쓸모없는 백업"으로 또 쓰는 대신, 커밋 후 그 new data로 PM data region을 직접 in-place update하면 로그를 PM에 쓸 필요 자체가 사라진다(Fig.2e, p.3). crash가 드물다는 관찰과 맞물려, common case(로그가 결국 안 쓰이는 경우)의 쓰기 트래픽을 근본적으로 없앤 것이 이 논문의 핵심 기여다.

> [!note]- 통찰 2 — 온칩 로그의 in-place update가 ordering 제약도 함께 제거한다
> 로그를 데이터로 쓰면 커밋 시점에 ① 캐시라인을 데이터 영역에 flush할 필요도, ② 로그를 로그 영역에 persist할 필요도 없다. 로그 생성기(log generator)에서 로그 컨트롤러(log controller)로 가는 경로가 CPU 캐시 계층을 거치지 않고, 로그가 캐시라인보다 먼저 persist되도록 보장되기 때문이다(§III-D, p.5). 이 덕분에 트랜잭션 커밋은 배터리 백업 로그 버퍼(persistent buffer)에만 로그가 도달하면 되고, 실제 PM data region으로의 lazy update는 백그라운드에서 처리된다.

> [!note]- 통찰 3 — 온칩 로그 축소(reduction)와 word 단위 write coalescing으로 이중으로 트래픽을 줄인다
> 트랜잭션 내에서 같은 주소를 여러 번 쓰는 temporal locality를 이용한 log merging(가장 오래된 값+최신 값만 유지)과, 값이 실제로 바뀌지 않은 write를 버리는 log ignorance로 온칩 로그 엔트리 수 자체를 평균 64.3% 줄인다(§III-C, §VI-D). 또한 로그의 new data(8B word)를 PM DIMM의 on-PM buffer(라인 크기 256B)에서 서로 겹치거나 인접한 write끼리 병합(coalescing)해 미디어에 쓰는 read-modify-write 횟수와 write amplification을 추가로 줄인다(§III-E, Fig.9, p.6).

## 설계 / 메커니즘 (Design)

> [!abstract]- 아키텍처와 로그 엔트리 구조 (§III-B, Fig.5·6, p.4)
> - **Log Generator**: 각 코어의 L1D 캐시 컨트롤러에 위치. `Tx_begin` 실행 시 현재 tid를 기록하고 레지스터의 txid를 증가시킨다. 캐시라인이 수정될 때(in-flight write request) new data와 물리 주소를 캡처하고, 태그 매칭과 오버랩되어 추가 지연 없이 old data도 함께 얻는다. 이렇게 만든 로그 엔트리(flush-bit, tid, txid, addr, old data, new data)를 메모리 컨트롤러의 로그 버퍼로 보낸다. CPU store는 로그 전송 완료를 기다리지 않고 다음 명령을 실행한다.
> - **로그 엔트리 구조**(Fig.6): flush-bit 1bit, tid 8bit, txid 16bit, addr(물리 주소) 48bit, old data 1 word(8B), new data 1 word(8B).
> - **Log Buffer**: 메모리 컨트롤러 안에 위치한 코어별 작은 배터리 백업(persistent) 버퍼. §VI-D 결과에 근거해 코어당 20 엔트리(680B)를 유지하며, 각 엔트리 옆의 64-bit 하드웨어 comparator로 빠른 주소 비교(<1ns)를 수행한다. 트랜잭션 커밋 후 엔트리는 deallocate되어 다음 트랜잭션에 재사용된다.
> - **Log Controller**: 메모리 컨트롤러 내에서 로그 버퍼를 관리. 트랜잭션 실행 중 log merging을 수행하고, 커밋 후 새 데이터를 data region에 flush해 in-place update한다(§III-D). PM 미디어에 쓰기 전 on-PM buffer에서 coalescing한다(§III-E). overflow·crash 같은 드문 경우에만 로그 영역에 로그를 flush한다(§III-F, §III-G).
> - **Log Region**: 스레드별 로그 영역을 분리해 두는 distributed 스킴으로 스레드 간 write contention을 피한다.

> [!abstract]- On-Chip Log Reduction: Log Ignorance & Log Merging (§III-C, Fig.7, p.5)
> - **Log Ignorance**: 값이 실제로 바뀌지 않는 write(예: data copy, value assignment)는 로그를 생성하지 않는다.
> - **Log Merging**: 같은 물리 주소를 여러 번 쓰면(temporal locality), 새 로그 엔트리가 도착할 때 로그 버퍼의 64-bit comparator들이 병렬로 기존 엔트리의 addr과 비교한다. 매치되면(Fig.7 예: Log_{A1+A2} 도착 시 기존 Log_{A0+A1}과 매치) 기존 엔트리의 new data를 새 값으로 교체하고(oldest+newest만 유지), 새 로그 엔트리는 버린다. 매치가 없으면 새 엔트리를 append한다. 이 병합은 스레드/트랜잭션 경계를 넘지 않으며 백그라운드에서 트랜잭션 실행에 영향 없이 처리된다.

> [!abstract]- Exploiting Logs for In-Place Updates: log-update 스킴 (§III-D, Fig.8, p.5–6)
> 트랜잭션 실행 중 undo+redo 로그가 로그 버퍼에 유지된다. 캐시라인이 커밋 전 PM으로 evict되면 불필요한 쓰기가 되므로, 로그의 flush-bit를 1로 세팅해 커밋 후 new data가 단순히 discard되도록 한다(캐시 라인이 이미 실제로 나갔으므로). 커밋 시(`Tx_end`) log generator가 log controller에 알리고, log controller는 ACK와 동시에 new data를 flush한다 — 이 온칩 제어 메시지 교환은 수 사이클(cycle)만 소요된다. 캐시라인 evict(CE)와 in-place update(IPU) 두 갱신 경로가 race 없이 정확성을 유지함을 세 가지 타이밍 시나리오로 증명한다: ① CE가 IPU보다 먼저 발생 → flush-bit=1, 로그는 온칩에서 폐기, CE가 data region 갱신; ② CE와 IPU가 커밋 중 동시 발생 → 로그의 new data가 evict된 캐시라인에 병합됨(Fig.9); ③ CE가 IPU보다 나중 발생 → bit-level write reduction 덕에 같은 값의 재기록이므로 안전. 이 스킴은 CPU 캐시 계층을 우회하므로 cache coherence 오버헤드가 없고, 트랜잭션이 항상 같은 메모리 컨트롤러(MC)에서 처리되므로 다중 MC 환경에서도 조정(coordination) 비용이 없다.

> [!abstract]- Coalescing Writes to PM: on-PM buffer (§III-E, Fig.9, p.6)
> PM DIMM 내부의 on-PM buffer(ADR로 보호됨, 라인 크기 예: 256B)는 word 단위(8B) new data를 모아 미디어에 쓸 때의 read-modify-write 증폭을 줄인다. 세 가지 케이스: Case 1) 같은 라인 내 겹치는 바이트(W1–W3)는 로그 병합으로 없앨 수 없어도 on-PM buffer에서 낮은/높은 4B를 순서대로 덮어써 정확성 보장; Case 2) 겹치지 않는 같은 라인의 word(W4, W5)는 미디어를 두 번 쓰지 않고 함께 저장; Case 3) 다른 라인의 word(W6)는 다른 캐시라인과 함께 buffer를 공유해 한 번에 미디어로 기록.

> [!abstract]- Handling Log Overflow (§III-F, p.6–7)
> 큰 트랜잭션에서 로그 버퍼가 undo+redo 로그를 다 담지 못하면 overflow가 발생한다. flush-bit=1인(이미 캐시라인 flush된) 오버플로 undo 로그는 온칩에서 폐기하고, flush-bit=0인 로그는 1로 세팅한 뒤 new data를 data region에 flush(durability 보장)한다. 물리 주소가 인접한 undo 로그(18B: 로그 메타데이터+old data)를 온-PM 버퍼 라인 크기 $S$에 맞춰 $N=\lfloor S/18 \rfloor$개씩 batch로 flush한다(예: $S=256$B면 14개씩). overflow는 오버플로 로그 flush와 다음 로그 추가가 병렬로 처리되고, undo/new data 사이 ordering 제약이 없으며, batch flush로 write amplification이 크게 늘지 않아 성능 저하가 크지 않다.

> [!abstract]- Selective Log Flushing for Crash Recovery (§III-G, p.7)
> crash가 발생하면 트랜잭션 상태에 따라 필요한 로그만 선택적으로 flush한다. ① 커밋 전 실패 트랜잭션: atomicity를 위해 undo 로그 전체를 로그 영역에 flush하고 new data는 온칩에서 폐기. ② 커밋 후이지만 new data가 아직 flush 안 된 트랜잭션: flush-bit=0이고 (tid, txid)를 가진 redo 로그만 로그 영역에 flush해 durability를 보장. 로그 영역의 ID tuple로 커밋된 트랜잭션을 식별하고, 나머지 로그 중 ID tuple이 없는 것(undo 로그, 실패 트랜잭션의 old data)은 데이터를 되돌리는(revoke) 데 사용하고, ID tuple이 있는 것(redo 로그)은 replay해 data region을 갱신한다. 이 selective flushing 로직은 배터리로 전원 공급되는 간단한 gate/mux로 하드웨어에 구현된다.

> [!abstract]- Putting It All Together — 예시 워크스루 (§IV, Fig.10, p.8)
> Fig.10(a–h)는 코어1의 Tx1(A,B 갱신), 코어2의 Tx2(D 갱신), Tx3(A 재갱신, C 갱신)이 동시 실행되다 crash가 발생하는 시나리오를 단계별로 보여준다. Tx1 커밋 시 로그 A1/B1이 data region을 in-place update하고(10b), Tx2가 F를 갱신하며 캐시라인 D1이 evict되어 LogD의 flush-bit가 1이 되고(10c), Tx3가 A를 재갱신해 로그가 병합되며(10d) Tx3의 undo 로그가 overflow하고(10e), crash 발생 시 이미 커밋된 트랜잭션(T1,T3)의 redo 로그만 flush-bit=0 조건으로 로그 영역에 flush되며 미커밋 Tx2는 실패한다(10f). 복구 시 (tid,txid)=(1,3) 같은 ID tuple로 커밋 여부를 식별해 redo 로그(A1→A2, C0→C1)는 replay하고, 미커밋 Tx2의 부분 갱신(D1→D0, F1→F0)은 undo 로그로 되돌린다(10g). 복구 후 PM은 일관된 상태가 된다(10h).

> [!abstract]- Hardware Overhead (Table I, p.7)
> 코어당 log buffer(SRAM, 20 entries, 680B), 20개의 64-bit comparator(CMOS cells), 배터리(lithium thin-film, $2.125\times10^{-4}\,mm^3$/로그버퍼), log head/tail(flip-flops, 16B/코어). 캐시라인 evict을 막거나 순서를 바꾸지 않으므로 추가 스케줄링 오버헤드가 없고, 로그가 물리 주소를 기록하므로 context switch 시 주소 앨리어싱 문제도 없다.

## 평가 (Evaluation)

> [!success]- 실험 환경 (Table II·III, §VI-A, p.8)
> Gem5 시뮬레이터 + NVMain으로 구현. 8코어 x86-64 2GHz, L1 64KB/8-way, L2 256KB/16-way(코어별), L3 8MB/16-way(shared), 메모리 컨트롤러 FRFCFS 64-entry queue(ADR domain), 로그 버퍼 680B/코어 FIFO 8-way(배터리 백업). PM: 16GB PCM, read/write latency 50/150ns. 마이크로벤치마크: Array/Btree/Hash/Queue/RBtree(데이터 원소 64B). 매크로벤치마크: TPCC(New-Order), YCSB(read/update 20%/80%). 비교 대상: Base(naive undo+redo flush), FWB, MorLog, LAD(logless atomic durability, [18]). ASAP는 커스텀 atomic region 대상이라 durability 보장 방식이 달라 직접 비교하지 않음(p.9).

> [!success]- Write Traffic 감소 (Fig.11, p.8–9)
> Base 대비 각 write마다 로그+수정 캐시라인을 flush하는 구조라 트래픽이 가장 높다. MorLog/FWB는 로그를 로그 영역에 백업해 중복 쓰기를 낳고, LAD는 캐시라인만 써서 트래픽이 낮지만 Silo가 로그를 데이터로 재사용해 대부분 시나리오에서 가장 낮다. 8코어에서 Silo는 MorLog/FWB 대비 write를 76.5%/82% 감소시킨다(p.9).

> [!success]- 트랜잭션 처리량 (Fig.12, p.9)
> Base가 로깅·캐시라인 flush 오버헤드로 가장 낮다. 8코어에서 MorLog는 FWB보다 1.5배 높은 처리량을 보이고(중간 redo 데이터 축소), LAD는 짧은 write path 덕에 FWB/MorLog보다 높으나 L1→LLC→MC로 이어지는 긴 flush 경로 때문에 Silo보다는 낮다. 코어 수가 늘수록 Silo의 ordering 제약 제거 효과가 커져 scalability가 좋아지며, 8코어에서 Silo는 LAD/MorLog/FWB 대비 각각 1.5배/4.3배/6.4배 처리량을 개선한다(p.9). Array·Queue처럼 spatial locality가 낮은 워크로드에서 LAD를 특히 크게 앞선다.

> [!success]- 로그 버퍼 용량 (Fig.13, p.9–10)
> 온칩 로그 축소 기법(§III-C)이 코어당 로그 수를 평균 64.3% 줄인다. Array는 데이터 원소가 64B(여러 word)라 로그가 많이 생기지만 90.4%가 실제로 수정되지 않아 무시된다. 최대 remaining 로그 수는 Hash에서 20개로, 이 값을 근거로 코어당 20 엔트리(680B, 트랜잭션당 26B×20+8B head/tail 물리주소 레지스터)를 설정했다(8코어 총 5,440B).

> [!success]- 배터리/에너지 요구량 (Table IV, §VI-E, p.10)
> 5,440B 로그버퍼를 crash 후 flush하는 데 필요한 에너지는 62 μJ(11.228 nJ/byte 모델 기준). 8코어 기준 eADR은 10,496KB 캐시(dirty block 45%)를 flush(54,377 μJ), BBB는 16KB 버퍼(194 μJ)를 flush해야 하는 반면 Silo는 5.3125KB/62 μJ로 훨씬 작다. 슈퍼커패시터(Cap)/리튬박막(Li) 배터리 부피·면적 기준으로 eADR/BBB는 Silo보다 각각 888.2배/3.2배 더 큰 부피(volume)의 Cap이 필요하다.

> [!success]- 대형 트랜잭션 처리 (Fig.14, p.10)
> write set이 로그 버퍼 크기의 1×~16×인 경우를 평가. 16× 큰 트랜잭션에서도 평균 처리량 저하는 7.4%에 불과(오버플로 undo 로그 flush와 새 로그 생성이 병렬 처리되기 때문). write traffic은 오버플로 undo 로그를 batch로 flush하는 덕에 평균 최대 1.9배만 증가(Btree/Hash/Queue/RBtree에서 다소 증가, Array·TPCC·YCSB는 locality가 좋아 안정적).

> [!success]- 로그 버퍼 지연 민감도 (Fig.15, p.10)
> 로그 버퍼 접근 지연을 8~128 사이클로 바꿔도(SRAM부터 다양한 버퍼 타입 커버) 처리량은 대체로 안정적. 128사이클 버퍼는 8사이클 대비 평균 3.3%만 처리량 저하 — 로그 버퍼 read/write가 critical path에 있지 않기 때문.

## 섹션 노트
- §I Introduction: 문제 배경(Heavy Writes, Ordering Constraints)과 기여 4가지(speculative hardware logging, log-as-data in-place update, overflow/crash 처리, 실험).
- §II Background and Motivation: atomic durability 정의, hardware logging의 필요성, eADR로 software logging을 지원하는 것의 비용(대형 배터리, cache pollution), 기존 hardware logging들의 ordering 제약 비교(Fig.2, Fig.3), "Log as Data" 아이디어 제안.
- §III The Silo Design: 가정(§III-A, isolation은 SW lock 등 별도 메커니즘, nested transaction 미지원), 아키텍처(§III-B), 온칩 로그 축소(§III-C), in-place update를 위한 log-update 스킴(§III-D), PM 쓰기 coalescing(§III-E), 로그 오버플로 처리(§III-F), crash 복구를 위한 선택적 flush(§III-G).
- §IV Putting It All Together: 전체 예시(Fig.10) 및 하드웨어 오버헤드(Table I).
- §V Discussions: eADR/BBB와의 배터리 요구량 비교, LAD(logless)와의 설계 차이(3가지 이유), 기존 persistent buffer 기반 hardware logging(ATOM, MorLog, ASAP, Proteus)과의 차이 — 이들은 여전히 로그를 backup으로 취급.
- §VI Performance Evaluation: 실험 설정, write traffic, 처리량, 로그 버퍼 용량, 에너지/배터리, 대형 트랜잭션, 지연 민감도.
- §VII Related Work: WAL(ARIES 등 SW 로깅), hardware logging(undo/redo/persistent buffer), multi-versioning(Kiln, LAD, HOOP, Kamino-Tx), 단일 연산 crash consistency(SW: NVTree/Fast&Fair/Level Hashing/MOD; HW: eADR/BBB).
- §VIII Conclusion: common failure-free case를 빠르게, crash 시에만 필요한 로그만 flush하는 speculative hardware logging으로 정확성 손실 없이 성능·수명을 개선.

## 핵심 용어 (Key terms)
- **Atomic durability**: 트랜잭션의 모든 갱신이 all-or-nothing으로 PM에 반영되고(atomicity), 커밋 후 확실히 persist(durability)되는 성질.
- **Log as Data**: Silo의 핵심 아이디어. 로그를 PM에 쓸 백업이 아니라, 커밋 후 data region을 in-place update할 새 데이터의 원천으로 재사용하는 방식.
- **Speculative logging**: crash가 드물다는 관찰에 근거해, common case에서는 로그를 PM에 쓰지 않고 온칩에만 유지하다가 실제 crash가 발생했을 때만 PM에 flush하는 로깅 전략.
- **Log generator / Log controller**: 각각 L1D 캐시 컨트롤러(로그 생성)와 메모리 컨트롤러(로그 병합·in-place update·overflow/crash 처리)에 위치하는 Silo의 두 하드웨어 컴포넌트.
- **Log merging / Log ignorance**: 같은 주소에 대한 반복 write를 oldest+newest만 남기고 병합하거나, 값이 안 바뀐 write의 로그 생성을 생략해 온칩 로그 수를 줄이는 기법.
- **On-PM buffer / write coalescing**: PM DIMM 내부의 ADR로 보호되는 버퍼로, word 단위 new data를 라인 단위로 모아 미디어 write amplification을 줄인다.
- **Log overflow / batch flushing**: 큰 트랜잭션에서 로그 버퍼 용량을 초과할 때, 물리 주소가 인접한 undo 로그를 batch로 로그 영역에 flush하는 처리.
- **Selective log flushing**: crash 시 트랜잭션 커밋 상태(flush-bit, ID tuple)에 따라 필요한 undo 또는 redo 로그만 골라 로그 영역에 persist하는 복구 메커니즘.
- **eADR / BBB (Battery-Backed Cache)**: 배터리로 전체 CPU 캐시를 persistent하게 만드는 기법. 단일 write에는 durability를 제공하나 atomicity는 보장하지 않으며, 대용량 배터리가 필요.
- **LAD (Logless Atomic Durability)**: 메모리 컨트롤러에서 갱신된 캐시라인을 버퍼링해 커밋 시까지 유지하는 logless 방식. Silo와 달리 L1→LLC→MC 경로의 캐시라인 flush를 커밋이 기다려야 한다.

## 강점 · 한계 · 열린 질문
- **강점**: "Log as Data"로 common failure-free case의 로그 쓰기를 원천 제거하면서도 crash 시 정확성(atomicity+durability)을 그대로 보장(CE/IPU 3-케이스 증명, §III-D). 커밋이 로그·캐시라인 flush를 기다리지 않아 ordering 제약을 없애고 코어 수 증가에 따른 scalability를 확보(8코어에서 최대 6.4배 처리량). 배터리 요구량이 eADR/BBB 대비 수백 배 작아(Table IV) 실용적 지속가능성이 높다. 오버플로·crash도 트랜잭션 abort 없이 처리한다.
- **한계**: isolation은 Silo 자체가 제공하지 않고 fine-grained locking 같은 소프트웨어 메커니즘에 의존하며(§III-A), nested transaction을 지원하지 않는다. 로그 버퍼가 코어당 20 엔트리(680B)로 작아 큰 트랜잭션에서는 overflow가 발생해 batch flush 오버헤드(write traffic 최대 1.9배 증가, Fig.14b)가 생긴다. 여전히 배터리 백업 SRAM 로그 버퍼에 의존하므로 BBB류 기법과 개념적으로 완전히 무관하지는 않다(다만 규모가 훨씬 작다). Gem5+NVMain 시뮬레이션 기반 평가로, 실제 PM 하드웨어(Optane 등)나 실제 배터리 회로 구현 검증은 제시되지 않는다.
- **열린 질문**: crash 확률이 평소보다 높은 환경(불안정 전원, 임베디드 등)에서 speculative 접근의 이득이 줄어드는 임계점은? 20 엔트리보다 훨씬 큰 write set을 갖는 워크로드(대형 batch 트랜잭션)에서 overflow 빈도가 늘어날 때의 실제 수명 영향은? SecPB/Thoth(같은 HPCA 2023, secure NVM)처럼 integrity tree 등 보안 메커니즘과 결합할 때 log generator의 캐시 우회 경로가 보안 오버헤드와 어떻게 상호작용하는지는 다루지 않는다.

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 hardware logging(ATOM, DHTM, FWB, MorLog, ASAP)의 근본적 비효율은 무엇인가?
> 답: 로그를 항상 "백업"으로 취급해 PM 로그 영역에 보수적으로 쓴다는 점이다. 이 때문에 매 트랜잭션마다 데이터 쓰기가 2–3배로 증가하고(Heavy Writes), 방식별 ordering 제약(undo는 데이터 flush 후 로그 persist 대기, redo는 전체 로그 persist 후 in-place update, FWB는 로그를 데이터보다 먼저 강제 flush, MorLog는 커밋 전 전체 로그 flush 대기)이 성능을 저하시킨다.

> [!question]- Q2. Silo가 "Log as Data"로 write traffic을 줄이는 원리는?
> 답: crash가 실제로는 드물다는 관찰에서, 온칩 로그 버퍼에 유지된 undo+redo 로그의 new data가 이미 트랜잭션이 만든 데이터 전체를 담고 있으므로, 커밋 후 이를 그대로 PM data region에 in-place update하고 로그 자체는 PM 로그 영역에 쓰지 않는다. 로그를 PM에 쓰는 것은 overflow나 crash 같은 드문 경우에만 발생한다.

> [!question]- Q3. Silo가 ordering 제약을 제거하는 이유는?
> 답: 로그가 log generator에서 log controller로 갈 때 CPU 캐시 계층을 우회하고 캐시라인보다 먼저 persist(배터리 백업 로그 버퍼 도달)됨이 보장되므로, 커밋이 캐시라인 flush나 로그 영역 persist를 기다릴 필요가 없다. new data는 로그 버퍼(배터리 백업)에 안전하게 남아 있으므로 data region으로의 실제 갱신은 백그라운드에서 lazy하게 처리해도 된다.

> [!question]- Q4. 캐시라인 evict(CE)과 in-place update(IPU)가 동시에 발생해도 정확성이 유지되는 이유는?
> 답: 두 경로(evict된 캐시라인, 로그의 new data)가 항상 같은 값을 담고 있기 때문이다. CE가 먼저 발생하면 flush-bit=1로 세팅되어 해당 로그는 온칩에서 폐기되고(중복 쓰기 방지), CE와 IPU가 동시에 발생하면 on-PM buffer에서 병합되며, IPU가 먼저 발생한 뒤 CE가 나중에 일어나도 bit-level write reduction 덕에 같은 값이 재기록될 뿐이라 데이터가 손상되지 않는다(§III-D, 3가지 시나리오).

> [!question]- Q5. Log overflow 시 Silo는 어떻게 정확성과 성능을 동시에 지키는가?
> 답: flush-bit=1인(이미 캐시라인이 flush된) undo 로그는 온칩에서 폐기하고, flush-bit=0인 로그는 flush-bit를 1로 세팅한 후 new data를 data region에 flush해 durability를 확보한다. 물리 주소가 인접한 로그들을 온-PM buffer 라인 크기에 맞춰 배치(batch)로 flush($N=\lfloor S/18\rfloor$개)하여 write amplification을 줄이고, 이 flush가 다음 트랜잭션의 새 로그 추가와 병렬로 진행되어 성능 저하를 최소화한다(§III-F, 평균 처리량 저하 7.4% at 16× 큰 트랜잭션).

> [!question]- Q6. crash 발생 시 Silo의 복구 절차는?
> 답: (tid, txid) ID tuple을 로그 영역에서 확인해 커밋된 트랜잭션을 식별한다. ID tuple이 없는 로그는 커밋 실패 트랜잭션의 undo 로그이므로 old data로 되돌려(revoke) atomicity를 지키고, ID tuple이 있는 로그는 커밋된 트랜잭션의 redo 로그이므로 replay하여 data region을 최신 상태로 만들어 durability를 보장한다(§III-G, Fig.10f–h).

> [!question]- Q7. Silo의 배터리 요구량이 eADR/BBB보다 훨씬 작은 이유는?
> 답: eADR/BBB는 crash 시 전체 CPU 캐시(eADR: 10,496KB, 45% dirty)나 대형 버퍼(BBB: 16KB×8코어)를 flush해야 하므로 큰 배터리가 필요하다. Silo는 트랜잭션당 최소한의 undo+redo 로그만 코어당 680B(총 5,440B, 8코어)로 유지하면 되므로 flush 에너지가 62 μJ에 불과해, 필요한 Cap/Li 배터리 부피가 eADR/BBB 대비 각각 888.2배/3.2배 작다(Table IV, §VI-E).

> [!question]- Q8. Silo가 성능 면에서 LAD(logless atomic durability)를 능가하는 이유는?
> 답: LAD는 메모리 컨트롤러에 도달하기까지 갱신된 L1 캐시라인을 LLC를 거쳐 MC로 flush해야 커밋할 수 있어(Prepare phase), 특히 spatial locality가 낮은 워크로드(Array, Queue)에서 dirty 캐시라인이 많아 긴 write path 지연이 크다. Silo는 로그가 캐시를 우회해 곧바로 로그 버퍼에 도달하므로 이 flush를 기다릴 필요가 없어, 8코어에서 LAD 대비 1.5배 높은 처리량을 보인다.

## 🔗 Connections
[[Reliability]] · [[HPCA]] · [[2023]]
관련: [[Root Crash Consistency of SGX-style Integrity Trees in Secure Non-Volatile Memory Systems]] · [[SecPB - Architectures for Secure Non-Volatile Memory with Battery-Backed Persist Buffers]] · [[Thoth - Bridging the Gap Between Persistently Secure Memories and Memory Interfaces of Emerging NVMs]]

## References worth following
- [52] Wei, Deng, Tong, Liu, Ye. "Morlog: Morphable hardware logging for atomic persistence in non-volatile main memory." ISCA 2020 — Silo가 직접 비교하는 state-of-the-art 하드웨어 로깅(MorLog), 4.3배/76.5% 개선의 기준선.
- [18] Gupta, Daglis, Falsafi. "Distributed logless atomic durability with persistent memory." MICRO 2019 — Silo와 성능 비교되는 logless 접근(LAD), §V에서 설계 차이 상세 논의.
- [38] Ogleari, Miller, Zhao. "Steal but no force: Efficient hardware undo+redo logging for persistent memory systems." HPCA 2018 — undo+redo 로깅 원조격 연구(FWB), Silo가 극복하려는 ordering 제약의 대표 사례.
- [2] Abulila, Hajj, Jung, Kim. "ASAP: architecture support for asynchronous persistence." ISCA 2022 — durability를 비동기적으로 relax하는 다른 접근, Silo와 비교 불가 이유(커스텀 atomic region)를 §VI에서 논의.
- [22] Intel. "eADR: New opportunities for persistent memory applications." — Silo가 배터리 요구량을 비교하는 하드웨어 persistence 표준.
- [61] Zhao, Li, Yoon, Xie, Jouppi. "Kiln: Closing the performance gap between systems with and without persistence support." MICRO 2013 — multi-versioning 기반 atomic durability의 대표적 선행 연구, related work 비교 대상.

## Personal annotations
<!-- 본인 메모 영역 -->
