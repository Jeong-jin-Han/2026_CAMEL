---
title: "WriteGuards: Distributed Storage Support for Strongly Consistent Caches"
description: "키 범위 단위의 fencing 토큰(WriteGuard)으로 delayed-writes anomaly를 막아, 스토리지와 느슨하게 결합된 채로 linearizable 캐시 읽기를 제공하는 메커니즘과 이를 이용한 CLINK/CRINK 캐시 설계"
venue: OSDI
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/infra
  - venue/osdi
  - year/2026
  - list/26s-v2
  - topic/caching
  - topic/linearizability
  - topic/distributed-storage
  - topic/write-fencing
---

# WriteGuards: Distributed Storage Support for Strongly Consistent Caches

> **OSDI 2026** · cluster/infra · Source: [WriteGuards - Distributed Storage Support for Strongly Consistent Caches.pdf](<WriteGuards - Distributed Storage Support for Strongly Consistent Caches.pdf>)

저자: Ziming Mao (UC Berkeley), Atul Adya (Databricks), Jonathan Ellithorpe (Databricks), Rishabh Iyer (UC Berkeley), Matei Zaharia (UC Berkeley), Scott Shenker (UC Berkeley, ICSI), Ion Stoica (UC Berkeley)

## TL;DR
대규모 서비스는 낮은 tail latency와 linearizable read를 동시에 원하지만, 기존 캐시는 eventual consistency만 제공하거나(Memcached류) 매 read/write마다 스토리지에 재확인해야 해서(Chubby식 lease, Chrono) 비싸다. 이 논문은 auto-sharded 환경에서 키 소유권이 바뀔 때 이전 owner의 "지연된 쓰기(delayed write)"가 새 owner 배정 이후 스토리지에 뒤늦게 커밋되어 캐시가 stale해지는 **delayed-writes anomaly**를 지적하고, 이를 막는 경량 fencing primitive **WriteGuards**를 제안한다. WriteGuards는 개별 키가 아니라 키 범위(key range) 단위로 opaque guard 값을 스토리지에 설치·검사하며, read 경로에는 어떤 coordination도 추가하지 않는다. 이를 바탕으로 in-process 캐시 CLINK와 원격 캐시 CRINK-R/CRINK-L을 TiDB 위에 구현했고, production trace(Meta, Twitter, Databricks Unity Catalog) 평가에서 CLINK는 P90 read latency를 직접 스토리지 접근 대비 최대 3자리수(10.3ms→4.2μs) 줄이고, CRINK는 기존 강한 일관성 원격 캐시(Chrono) 및 직접 스토리지 접근 대비 2.2–2.4배 낮은 latency를 보인다.

## 문제 & 동기
대규모 데이터센터 서비스는 permission 체크, 세션 상태 추적처럼 latency-critical한 경로에서 최신 committed 값을 읽는 linearizable read를 필요로 하는 경우가 많다(financial trading, ad bidding 등, p.1). 하지만 트랜잭션 스토리지는 network round trip·serialization·트랜잭션 처리 때문에 multi-millisecond tail latency를 갖고(p.1), 이를 피하려고 대부분의 서비스는 lookaside 캐시(Memcached/Redis)나 in-process 캐시를 쓰지만 이들은 보통 eventual consistency만 제공한다(p.1, §3.4). 특히 auto-sharder가 부하 분산을 위해 키 범위 소유권을 동적으로 재배정할 때, 이전 owner가 보낸 쓰기가 네트워크에서 지연되다가 새 owner 배정 이후 스토리지에 커밋되면 캐시가 stale한 값을 계속 서빙하는 **delayed-writes anomaly**가 발생한다(§4). 이는 이론적 가능성이 아니라 실제 production에서 관찰된 현상이며, GitHub는 아웃테이지 중 패킷이 약 90초간 in-flight 상태로 남아있었다고 보고했다(p.1).

> [!quote]- 📄 원문 표현 (paper)
> - "Write operations have been observed in production systems [18, 67] to stall for extended periods, with GitHub reporting packets remaining in flight for approximately ninety seconds during an outage [82]." (p.1)
> - "**Delayed-write anomaly.** A delayed write for key K is a write that commits in the storage system after a new owner has already assumed ownership of K in storage." (p.4)
> - "Reads often suffer 1–5 ms tails due to queuing and concurrency, despite sub-millisecond network RTTs." (p.4)

## 핵심 통찰 (Key Insight)

1. **캐시 레이어의 소유권만으로는 부족하다 — 스토리지 레이어에서도 소유권이 확립되어야 한다.** Auto-sharder가 캐시 pod에 exclusive ownership을 부여해도, 이전 owner의 write가 스토리지에 늦게 도착해 커밋될 수 있는 한 캐시는 안전하지 않다. 저자들은 문제의 근원이 "캐싱 계층"이 아니라 "스토리지 계층이 소유권 변경을 인지하지 못한다"는 데 있음을 짚어내고, fencing을 스토리지 레이어에 심는다(§4, §5).

> [!quote]- 📄 원문 표현 (paper)
> - "Ownership at the caching layer is not enough. A pod must also become the owner in the storage layer before that no other writes can succeed." (p.4)

2. **개별 키가 아니라 키 범위(range) 단위로 fencing한다.** Chubby의 sequencer 방식은 키/lock마다 상태를 유지해야 해서 수억~수십억 키 규모에서 비용이 폭증한다(§4.2.3). WriteGuards는 auto-sharder가 이미 range 단위로 소유권을 관리한다는 사실(Assignment Consistency, §2.3)을 활용해, range당 하나의 opaque guard 값만 설치·검사하면 되므로 read 경로에 아무 코스트도 추가하지 않고 write 경로에만 가벼운 조건부 체크 하나를 추가한다(§5, §9.3).

> [!quote]- 📄 원문 표현 (paper)
> - "WriteGuards apply to key ranges instead of individual keys for scalability, add only a conditional check on the write path, and require no coordination on reads." (p.1)

3. **소프트 스테이트(soft state) + total order 보장만으로 정확성을 얻는다.** WriteGuard는 tablet 서버에 비영속(soft) in-memory interval map으로만 존재하고 재시작 시 사라져도 무방하다. 대신 ownership-contained·non-overlapping·unique-valued라는 세 규칙(§6.2.2)으로 "WriteGuard Ordering" 정리를 도출해, 모든 relevant guard가 소유권 순서와 정확히 일치하는 total order를 이루도록 하여 지연된 쓰기가 항상 최신 guard와 불일치해 reject됨을 보장한다(§6.2.3).

> [!quote]- 📄 원문 표현 (paper)
> - "WriteGuard Ordering. All GuardHandles form a total order, and their corresponding WriteGuards in storage follow the same order. We call the WriteGuards associated with these handles relevant. All others are irrelevant." (p.7)

## 설계 / 메커니즘 (Design)

**WriteGuard 프리미티브 (§5).** 스토리지는 `SetGuard(range, guardValue)` API를 노출하며, tablet 서버는 non-overlapping key range → guard value의 in-memory interval map을 유지한다. 배정된 slice가 여러 tablet에 걸치면 caller가 splitpoint 기준으로 range를 쪼개 각 tablet에 SetGuard를 보낸다(Fig.6, p.6). Tablet split/merge 시 guard는 상속되며(merge 시 서로 다른 원본 range의 guard 값들이 유지됨, §5.2), tablet migration 시엔 단순화를 위해 guard 메타데이터를 버린다(정확성에는 영향 없음, §5.2). Write는 이 guard 값과 일치할 때만 accept되어 이전 owner의 delayed write를 reject한다(§5.3, Fig.6 예시).

**CLINK: in-process linearizable 캐시 (§6).** 애플리케이션 프로세스 주소공간에 값을 직접 캐싱하는 write-through 캐시로, `GuardMap`(range→GuardHandle), `CacheMap`(key→(GuardHandle,Value)), `OpMap`(key→진행 중 op 배열) 세 자료구조를 유지한다(§6.1, p.7). 소유권을 새로 받으면 Algorithm 1(`HandleNewAssignment`)이 해당 range에 유일한 WriteGuard 값을 생성해 `SetGuard`로 설치하고, 그 설치 호출이 하나의 ownership period 안에서 시작·끝나야 GuardHandle을 인정한다(ownership-contained·non-overlapping·unique-valued, §6.2.2, Fig.7). Write(Algorithm 2)는 GuardMap에 guard가 없으면 즉시 reject하고, 있으면 `IsStillAssigned`로 연속 소유권을 재확인한 뒤 스토리지에 쓰고, 결과가 **LSI(Latest State Invariant: 캐시가 서빙하는 값은 스토리지의 최신 committed 값과 같아야 한다)** 를 만족할 때만 캐시에 반영한다(§6.3.1, p.8). Read(Algorithm 3)는 캐시에 continuous-ownership 값이 있으면 바로 반환하고, 없으면 스토리지에서 읽어 `SatisfiesLSI`(Algorithm 4, previous/current/future owner에 의한 mutation을 모두 배제)를 통과할 때만 캐싱한다(§6.3.2–6.3.3, Fig.8). Hot key는 asymmetric replication으로 여러 pod에 복제되며, primary가 write를 처리하고 two-phase invalidation(무효화 메시지→ack 확인→스토리지 write→finish 메시지)으로 replica의 cacheability를 관리한다(§6.5, Fig.9의 primary 구조는 아니고 §6.5 텍스트).

**CRINK-R / CRINK-L: 원격 캐시 (§7).** CRINK-R(Fig.9)은 CLINK 아키텍처를 standalone 원격 캐싱 서비스로 배포한 것으로, write-through 패턴과 auto-sharder/WriteGuard 메커니즘을 그대로 유지한다. CRINK-L(Fig.10)은 값 저장을 일반 lookaside 캐시(예: Redis)에 맡기고 별도의 경량 version service가 auto-sharded assignment consistency만 유지하도록 분리한다(consistency metadata와 value storage의 decoupling). 읽기는 클라이언트가 version과 캐시를 병렬로 조회해 버전이 일치하면 캐시값을 쓰고, 불일치하면 스토리지로 폴백한다(§7.2).

**구현 (§8).** TiDB 위에 구현: TiKV에 SetGuard API 추가(+717 Rust lines), TiDB SQL layer(+229 Go), PD(+57 Go). 캐시(GuardMap/CacheMap/OpMap 및 프로토콜)는 약 6000 lines C++. Centrifuge에서 영감을 받은 strong-ownership auto-sharder는 약 12000 lines Scala. TiDB 수정은 대략 1인·월 소요(p.9).

> [!quote]- 📄 원문 표현 (paper)
> - "The GetSliceHandle API returns a SliceHandle for a key range [lowInclusive, highExclusive) where a single key is simply a singleton range." (p.3)
> - "Latest State Invariant (LSI). A key K may be served from the cache only if it equals the latest committed value of K in storage." (p.5)
> - "The entire TiDB modification took roughly 1 person-month, showing that changes are fast and lightweight." (p.9)

## 평가 (Evaluation)
실험은 production 클러스터(TiKV pod 9개·15GB/30vCPU, TiDB pod 6개, PD pod 3개, 애플리케이션 서버 6개·16vCPU/16GB)에서 Meta·Twitter public trace와 Databricks 내부 Unity Catalog(UC) trace로 진행되었다(§9.1).

- **처리량**: CLINK는 코어 수에 거의 선형으로 스케일하며 24코어에서 22.8M QPS(코어당 약 1M ops/s)를 달성(§9.2, Fig.11a).
- **WriteGuard 오버헤드**: write-only 워크로드로 스토리지 75% CPU 이용률에서 측정한 결과, WriteGuard 유무에 따른 throughput/latency 차이가 사실상 없음(§9.3, Fig.11b) — SetGuard가 write마다가 아니라 assignment 변경 시에만 발생하기 때문.
- **Read latency (production trace, §9.4.1, Fig.12a)**: 모든 시스템·percentile 중 CLINK가 최저 latency(P90 0.5μs–4.2μs)로, Storage(P90 4.8ms–10.3ms)를 몇 자리수 앞섬. CRINK-R/CRINK-L(P90 0.65ms–3.2ms)은 Chrono(P90 2.3ms–5.5ms)와 Storage를 모든 워크로드에서 능가. Version 방식은 P90 3.3ms–9.0ms.
- **Write 트래픽 민감도**: UC trace에서 write QPS를 늘려도 CLINK/CRINK-R/CRINK-L latency는 안정적인 반면, Chrono는 write 빈도가 늘수록 timestamp bound 갱신 때문에 캐시 가능 구간이 좁아져 latency가 상승(§9.4.2, Fig.12b).
- **단일 hot key 마이크로벤치마크(Fig.12c)**: CLINK는 write 시 캐시 population을 막으므로 hit rate가 target percentile 아래로 떨어지면 latency가 스토리지 수준(2–2.8ms)까지 상승; P99는 write QPS 80에서, P90/P50은 200/400에서 spike — 이는 실제 production 단일 키 write QPS를 크게 상회하는 수준.
- **Resharding 민감도(§9.4.3, Fig.13a)**: 트래픽 처른(churn) 0.5% 상태의 resharding 이벤트 동안에도 CLINK는 100% availability와 낮은 latency를 유지(재배정이 20ms 이내로 빨라 재시도가 충분히 성공).
- **실제 운영 처른(Fig.14)**: 3개 production 서비스(ACL, rate limit, remote cache)에서 분당 부하 이동 비율이 대체로 0.2% 미만.
- **비계획 재시작의 가용성 영향(§2.3.2, Fig.4)**: 86개 sharded 서비스에서 월별 unplanned restart 비율은 대체로 0.14% 이하이며, 100-pod 서비스 기준 약 두 달에 한 번 크래시가 발생하는 빈도. 단일 키 unavailability는 order tens of seconds이나 전체 daily availability는 99.999%를 초과.
- **Asymmetric replication(§9.5, Fig.13b–c)**: 부하가 늘면 auto-sharder가 replica를 동적으로 추가하며(Fig.13b), replica가 늘수록 write latency는 coordination 오버헤드로 소폭 증가(Fig.13c). 표준 16코어 pod에서 5-replica를 트리거하려면 단일 키에 80M+ QPS가 필요.

> [!quote]- 📄 원문 표현 (paper)
> - "CLINK reduces tail read latency by three orders of magnitude, and CRINK improves tail latency by 2.2–2.4×." (p.9, §9 개요)
> - "Across all systems and percentiles, CLINK demonstrates the lowest read latency (P90 of 0.5μs–4.2μs), remaining well below remote caches and significantly outperforming Storage (P90 of 4.8ms–10.3ms)." (p.10)
> - "Even if a single pod becomes unavailable for 30 seconds and temporarily affects 1% of keys, overall daily availability still exceeds 99.999%, making the practical impact negligible." (p.4)

## 섹션 노트
- **§1 Introduction**: linearizable read 필요성과 delayed-writes 문제를 개괄하고 WriteGuards·CLINK·CRINK·구현/평가라는 3대 기여를 제시.
- **§2 Background**: 애플리케이션/스토리지 모델, lookaside vs linked 캐시 구분, auto-sharder의 hot-key replication과 strong ownership(Assignment Consistency) 개념, 계획/비계획 재시작의 가용성 영향을 다룸.
- **§3 Motivation**: 낮은 tail latency의 필요성, linearizable read의 필요성, 스토리지 직접 접근의 비용, 기존 캐시의 eventual-consistency 한계를 각각 정량·사례로 뒷받침.
- **§4 The Delayed-Writes Anomaly**: resharding 시나리오(Fig.5)로 delayed-write anomaly를 구체적으로 설명하고, version check·metadata write 변환·Chubby sequencer·TrueTime·timeout 기반 근사 등 기존 해법의 한계를 분석.
- **§5 The WriteGuard Primitive**: SetGuard API, tablet의 interval map, split/merge 시 guard 상속, write 시 guard 검사, delayed write 방지 예시를 제시.
- **§6 CLINK**: GuardMap/CacheMap/OpMap 자료구조, Algorithm 1–4(assignment/write/read/cacheability 체크), WriteGuard ordering 정리 및 그 증명, asymmetric replication을 통한 멀티 pod serving, 향후 최적화 아이디어를 정리.
- **§7 CRINK**: CRINK-R(모놀리식 원격 배포)과 CRINK-L(version service와 value cache 분리)의 아키텍처와 트레이드오프.
- **§8 System implementation**: TiDB/TiKV/PD에 대한 구체적 코드 변경량과 auto-sharder/캐시 구현 규모.
- **§9 Evaluation**: throughput, WriteGuard 오버헤드, write 민감도, resharding 민감도, asymmetric replication 비용의 5가지 질문에 답함.
- **§10 Related Work**: Chrono·Chubby 같은 강한 일관성 캐시, 스토리지 레이어 내장 캐시, TTL 기반 약한 일관성 캐시(TxCache 포함)와 비교하며 차별점을 명확히 함.
- **§11 Conclusion**: WriteGuards가 지연 쓰기 방지와 loose coupling을 동시에 달성하는 경량 primitive임을 재강조하고, 스토리지 시스템 제공자들이 이를 표준 write-fencing primitive로 노출할 것을 제안.

## 핵심 용어 (Key terms)
- **WriteGuard**: 키 범위에 결합된 opaque한 fencing 토큰. 스토리지가 write 시 이 값을 검사해 이전 owner의 지연된 write를 거부한다.
- **Delayed-writes anomaly**: 키 소유권이 바뀐 뒤에도 이전 owner의 write가 스토리지에 뒤늦게 커밋되어 새 owner가 stale 값을 캐싱·서빙하게 되는 race.
- **Latest State Invariant (LSI)**: 캐시가 서빙하는 키 값이 스토리지 상의 최신 committed 값과 항상 같아야 한다는 CLINK의 정합성 불변식.
- **Assignment Consistency**: auto-sharder가 생성하는 배정(어느 pod가 어떤 range의 primary/replica인지)에 대해 모든 pod가 합의하는 성질.
- **Auto-sharder / Slicer**: 키 범위(slice)를 pod에 동적으로 재배정하며 hot key를 감지해 asymmetric replication을 수행하는 샤딩 계층(Centrifuge/Slicer 계열에서 영감).
- **SliceHandle / SetGuard / IsStillAssigned / IsPrimary**: auto-sharder가 노출하는 lease 기반 API로, 특정 range의 배정이 여전히 유효한지·자신이 primary인지 확인하는 데 쓰임.
- **GuardMap / CacheMap / OpMap**: CLINK가 pod마다 유지하는 세 자료구조로 각각 range→GuardHandle, key→(GuardHandle,값), key→진행 중 op 목록을 관리.
- **CLINK (Consistent Linked In-memory Key-value cache)**: 애플리케이션 프로세스 주소공간에 값을 직접 저장하는 in-process linearizable 캐시.
- **CRINK-R / CRINK-L (Consistent Remote In-memory Key-value cache)**: 원격 강한 일관성 캐시로, R은 모놀리식 write-through, L은 version service와 일반 lookaside 캐시(Redis 등)를 분리한 계층형 설계.
- **Chrono**: Dropbox의 원격 강한 일관성 캐시 baseline으로, 커밋 타임스탬프 상한을 부여해 linearizability를 보장하지만 매 write 이후 일정 시간 캐시를 서빙 불가로 만든다.
- **Asymmetric replication / Two-phase invalidation**: hot key를 여러 pod에 복제하되 하나만 primary로 write를 처리하고, write 시 무효화→ack→스토리지 write→finish 순으로 replica cacheability를 관리하는 프로토콜.

## 강점 · 한계 · 열린 질문
**강점**: (1) WriteGuard는 soft state·range 단위 검사만으로 동작해 read 경로에 어떤 coordination도 추가하지 않는다(§9.3에서 실측 확인). (2) 스토리지와 캐시 계층을 loose coupling으로 유지해 두 계층이 독립적으로 진화 가능(§1). (3) WriteGuard 값이 없는 기존 애플리케이션과의 하위 호환성을 자연스럽게 확보(§5.3). (4) TiDB에 약 1인·월 수준의 경량 수정으로 통합 가능함을 실제로 보임(§8, p.9).

**한계**: (1) 저자들이 명시적으로 스코프를 "single-datacenter 환경"으로 한정하며, geo-distributed/WAN 환경으로의 확장은 다루지 않는다(p.2, "we limit the scope of this paper to single-datacenter environments"). (2) 정확성이 auto-sharder가 제공하는 Assignment Consistency라는 외부 보장에 의존하는데, 이는 별도 컴포넌트(§2.3)의 신뢰를 전제한다. (3) tablet migration 시 guard 메타데이터를 그냥 버리는 설계 선택(§5.2)은 "정확성에는 영향 없다"고 주장하지만 그 근거(비용 대비 이득 분석)는 상세히 제시되지 않는다. (4) hot key에 대한 asymmetric replication의 two-phase invalidation은 write latency를 소폭 늘린다(§9.5, Fig.13c).

**열린 질문**: WriteGuard 프리미티브가 Spanner류 TrueTime 기반 timestamp fencing과 결합되거나, 여러 데이터센터에 걸친 key range 소유권 이전에도 동일한 total-order 보장을 유지할 수 있는지는 미해결 과제로 남는다. 또한 tablet 서버 재시작 시 guard가 완전히 휘발되는데(soft state), 재시작 직후 아직 auto-sharder가 새 assignment를 인지하지 못한 짧은 창(window)에서 여전히 안전한지에 대한 명시적 증명은 제시되지 않는다.

## ❓ Q&A (자가 점검)

> [!question]- WriteGuard는 왜 개별 키가 아니라 key range 단위로 동작하도록 설계되었나?
> Chubby식 per-key lease/sequencer는 수억~수십억 키 규모에서 lease 서비스·애플리케이션 서버·스토리지 백엔드에 과도한 상태 유지 비용을 요구해 scalable하지 않다(§4.2.3). Auto-sharder가 이미 key range(slice) 단위로 소유권을 관리하므로(§2.3), WriteGuard도 같은 단위로 동작시켜 coordination 오버헤드를 range 개수만큼으로 amortize한다(§1, §5).

> [!question]- Latest State Invariant(LSI)는 구체적으로 무엇을 보장하며, CLINK는 이를 어떻게 강제하나?
> LSI는 "캐시가 서빙하는 키 값은 스토리지의 최신 committed 값과 같아야 한다"는 불변식이다(p.5). CLINK는 두 메커니즘으로 이를 강제한다: LSI-at-entry(모든 write/read의 반환값을 SatisfiesLSI로 즉시 검사해 캐시에 넣을지 결정, Algorithm 2·3의 15–17행/13–14행)와 LSI-at-serve(매 write 시 캐시값을 evict하고, cache hit 반환 전 continuous ownership을 재확인)이다(§6.4).

> [!question]- CLINK가 delayed write로부터 안전한 이유를 WriteGuard Ordering 성질과 연결해 설명하라.
> Auto-sharder가 ownership period를 total order로 정렬하고, GuardHandle 생성 규칙(ownership-contained·non-overlapping·unique-valued)이 이 순서를 그대로 storage 상의 WriteGuard 순서에 반영하기 때문에(§6.2.2 WriteGuard Ordering, p.7), 새 owner가 설치한 relevant guard는 모든 이전 owner의 relevant guard보다 항상 나중에 설치된다. 값이 모두 유일(unique-valued)하므로 지연된 write가 들고 있는 이전 guard 값은 현재 guard와 결코 일치할 수 없어 reject된다(§6.2.3).

> [!question]- CRINK-R과 CRINK-L의 핵심 차이는 무엇이고, CRINK-L이 얻는 이점은?
> CRINK-R은 CLINK 아키텍처를 그대로 원격 서비스로 배포해 값과 consistency metadata를 함께 하나의 write-through 캐시로 관리한다(§7.1). CRINK-L은 이 둘을 분리해, 작은 version 정보만 담당하는 version service와 실제 값은 일반 lookaside 캐시(Redis 등)에 맡긴다(§7.2). 이를 통해 independent scaling(값 캐시만 별도로 스케일), layered consistency(기존 eventual-consistency 캐시에 linearizability를 얹을 수 있음), flexible consistency(필요에 따라 eventual read 또는 version 확인 read를 선택 가능)를 얻는다.

> [!question]- Chrono 대비 CLINK/CRINK가 갖는 latency 이점의 근본 원인은 무엇인가?
> Chrono는 매 write마다 미래 시점(기본 5초)의 commit timestamp 상한을 요청·기록하고, 그 상한이 지날 때까지 해당 키의 read를 캐시에서 서빙하지 못하게 blocking한다(§4.2.4, §10.1.1). 이는 read/write 모두에 원격 Chrono 서비스 왕복을 추가하고 write 후 일정 시간 캐시를 무력화한다. 반면 CLINK/CRINK는 WriteGuard를 range 배정 변경 시에만 설치하고 read 경로에는 스토리지/원격 서비스 왕복을 요구하지 않으므로(§9.4.1, Fig.12b), write 빈도가 늘어도 latency가 거의 변하지 않는다.

> [!question]- WriteGuard 메타데이터가 tablet migration 시 삭제되는 설계가 왜 정확성에 영향을 주지 않는다고 주장되는가?
> Tablet이 다른 서버로 옮겨가면 새 서버는 lazy initialization으로 해당 range를 empty guard로 초기화한다(§5.1). 이후 새로 배정된 owner가 어차피 자신의 새 WriteGuard를 설치해야 하므로(Algorithm 1), migration 시점에 이전 guard 값을 유지하지 않아도 owner가 새 guard를 설치하기 전까지는 write가 여전히 거부되거나(guard 불일치) 새 guard 설치 후에만 accept된다는 논리다. 다만 논문은 이 트레이드오프를 "guard 상태 이전 복잡도 대비 이득이 미미하다"는 수준으로만 정당화한다(§5.1).

> [!question]- 실험에서 WriteGuard 자체의 성능 오버헤드는 어떻게 측정되었고 결과는?
> Write-only 워크로드로 스토리지 계층 CPU 이용률을 75%까지 올려 WriteGuard 체크가 병목이 되는지 스트레스 테스트했다(§9.3). Fig.11b는 WriteGuard 유무에 따른 throughput(kQPS)과 write latency(ms)를 비교했는데, 둘 다 유의미한 차이가 없었다 — SetGuard 호출이 write마다가 아니라 assignment 변경(resharding) 시에만 발생하기 때문(Fig.15가 보여주듯 resharding은 분 단위로 드물게 일어남).

> [!question]- Asymmetric replication을 사용하는 hot key에서 write availability는 왜 유지되는가?
> Write는 반드시 primary를 통해서만 처리되고, replica membership이 바뀌면(오토샤더가 통지하거나 SliceHandle로 감지) continuous group ownership이 깨져 replica가 즉시 해당 range를 uncacheable로 만든다(§6.5 Failure Handling). 이 무효화는 두 그룹(primary-replica) 사이에서 직접 이뤄지고 스토리지 시스템이 lease recall에 관여할 필요가 없어(Chubby식 lease-recall과 달리) write availability에 미치는 영향이 적다(§6.5, §9.5).

## 🔗 Connections
[[Infra]] · [[OSDI]] · [[2026]]

## References worth following
- **Adya et al., "Slicer: Auto-Sharding for datacenter applications", OSDI 2016 [9]** — 이 논문의 auto-sharder 모델(hot-key isolation, asymmetric replication)이 직접 기반하는 원본 시스템.
- **Adya, Dunagan, Wolman, "Centrifuge: Integrated Lease Management and Partitioning for Cloud Services", NSDI 2010 [7]** — CLINK/CRINK 구현에 쓰인 strong-ownership auto-sharder 설계가 영감을 받은 시스템.
- **Dropbox Tech Team, "Meet Chrono: Our Scalable, Consistent Metadata Caching Solution", 2023 [81]** — 본 논문의 핵심 baseline이자 비교 대상인 원격 강한 일관성 캐시.
- **Burrows, "The Chubby lock service for loosely-coupled distributed systems", OSDI 2006 [20]** — sequencer 기반 fencing의 원형으로, WriteGuards가 스케일 문제를 해결하고자 하는 출발점.
- **Ports, Zhang, Madden, Liskov, "Transactional Consistency and Automatic Management in an Application Data Cache" (TxCache), OSDI 2010 [70]** — 유사하게 캐시에 스토리지 검증을 붙이는 접근이지만, latest-value 보장이나 auto-sharded 재배정 대응은 다루지 않는다는 점에서 대비되는 related work.
- **Corbett et al., "Spanner: Google's globally-distributed database", OSDI 2012 [30, 31]** — TrueTime 기반 timestamp fencing 대안(§4.2.4)의 비교 기준.

## Personal annotations
<!-- 본인 메모 영역 -->
