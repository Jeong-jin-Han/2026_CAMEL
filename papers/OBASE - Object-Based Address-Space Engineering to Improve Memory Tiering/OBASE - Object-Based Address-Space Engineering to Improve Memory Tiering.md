---
title: "OBASE: Object-Based Address-Space Engineering to Improve Memory Tiering"
description: "객체(object) 단위 hotness를 추적·군집화하여 페이지 단위 OS memory tiering/reclamation 백엔드를 무수정으로 더 효과적으로 만드는 compiler-runtime frontend 시스템"
venue: OSDI
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/cxl
  - venue/osdi
  - year/2026
  - list/26s-v2
  - topic/memory-tiering
  - topic/address-space-engineering
  - topic/lock-free-migration
  - topic/unmanaged-languages
---

# OBASE: Object-Based Address-Space Engineering to Improve Memory Tiering

> **OSDI 2026** · cluster/cxl · Source: [OBASE - Object-Based Address-Space Engineering to Improve Memory Tiering.pdf](<OBASE - Object-Based Address-Space Engineering to Improve Memory Tiering.pdf>)

저자: Vinay Banakar¹,³, Suli Yang³, Kan Wu²*, Andrea C. Arpaci-Dusseau¹, Remzi H. Arpaci-Dusseau¹, Kimberly Keeton³ — ¹University of Wisconsin-Madison, ²xAI, ³Google (*work done at Google)

## TL;DR
데이터센터 memory tiering이 이론적 절감 대비 실제 절감이 낮은 근본 원인은 **hotness fragmentation**—할당자가 객체를 접근 패턴이 아닌 크기 기준으로 배치해 hot/cold 객체가 같은 페이지에 섞이는 현상—이다. OBASE는 unmanaged 언어(C++)를 위한 compiler-runtime 시스템으로, `guide`라는 포인터 간접 계층을 통해 객체 접근을 경량으로 추적하고, ATC(Active Thread Count)·epoch 기반 lock-free 프로토콜로 객체를 NEW/HOT/COLD 힙 사이에 안전하게 마이그레이션하여 주소공간을 재구성한다. 이 재구성만으로 페이지가 균일하게 hot 또는 cold가 되므로, kswapd·TMO·TPP·AutoNUMA·Memtis 같은 기존 page-based 백엔드를 전혀 수정하지 않고도 효과적으로 동작하게 만든다. 10개 concurrent 자료구조와 Meta/Twitter production trace 평가에서 page utilization 2–4배 개선, 메모리 footprint 최대 70% 절감을 오버헤드 2–5%로 달성했다.

## 문제 & 동기
Google production 6개 워크로드 분석 결과 trace 기간 동안 1.7%–21.3%의 바이트만 접근되었고(4/6 워크로드는 3% 미만), 이론적으로는 DRAM의 80–98%를 다른 티어로 옮길 수 있어야 한다(p.1, Fig.1). 그러나 실제로는 Meta가 20–32%, Google이 20% 수준의 절감만 달성해 이론-실현 간 큰 격차가 존재한다(p.1). 원인은 애플리케이션이 *객체(object)* 단위로 데이터를 조직하는 반면 OS는 *페이지(4KB/2MB/1GB)* 단위로 메모리를 관리한다는 granularity mismatch다. 논문은 **page utilization**을 $\text{Utilization}(T)=\dfrac{\sum_{p\in P(T)} U(p,T)}{\sum_{p\in P(T)} \text{Size}(p)}$로 정의해 이 fragmentation을 정량화한다(p.3, §2.1). Fig.2 CDF에서 페이지 utilization 중앙값은 워크로드별 8%(Tahoe)~약 50%(Sierra)이며, 2MB huge page에서는 Tahoe/Bravo/Yankee가 90% 이상의 huge page에서 utilization 10% 미만으로 훨씬 악화된다(p.3). Fig.3의 Meta/Twitter object-level trace 분석은 hotness가 시간에 따라 phased(Meta) 또는 sporadic(Twitter)하게 지속적으로 변함을 보여, 정적 배치나 allocation-time 힌트로는 해결 불가능함을 시사한다(p.3, §2.3).

> [!quote]- 📄 원문 표현 (paper)
> - "Hardware and OS mechanisms for memory tiering are widely deployed, yet datacenters still overprovision DRAM. The root cause is hotness fragmentation" (p.1)
> - "Our analysis of Google production workloads shows that up to 97% of the bytes in active pages are cold and unreclaimable." (p.1)
> - "Active pages are mostly cold: 70–90% of bytes in pages the OS considers hot receive no accesses." (Finding 1, p.3)
> - "Hotness is transient; object hotness is neither knowable at allocation time nor stable over time." (Finding 2, p.3)

## 핵심 통찰 (Key Insight)

**1. Frontend(layout)/Backend(reclamation) 분리** — OBASE는 reclamation 정책을 전혀 건드리지 않고 오직 가상주소 공간의 *layout*만 재구성한다. 객체 온도가 유사한 것끼리 군집시켜 페이지를 균일하게 만들면, 어떤 page-based 백엔드(swap 기반이든 byte-addressable 기반이든)도 그 위에서 더 좋은 결정을 내릴 수 있다. 이 분리 덕분에 하이퍼스케일러가 기존 티어링 인프라를 바꿀 필요 없이 채택 장벽이 낮아지고, 향후 reclamation 메커니즘 발전과도 독립적으로 호환된다.

> [!quote]- 📄 원문 표현 (paper)
> - "This separation allows us to reuse existing page-based tiering infrastructures—both swap-based [37, 53] and byte-addressable [38, 42, 47]—and leverage future improvements in reclamation mechanisms." (p.2)

**2. Guide 추상화를 통한 unmanaged 언어에서의 객체 이동성** — C++ 등 unmanaged 언어는 객체 주소가 안정적이라고 가정하므로 동적 재배치가 어렵다. OBASE는 raw pointer 대신 `guide`(논리적 identity와 물리 위치를 분리하는 경량 간접 계층)를 통해 객체에 접근하게 하고, 컴파일러의 type-level/instrumentation/validation 3개 pass가 어노테이션된 포인터의 역참조를 guide 호출로 자동 재작성한다. 개발자는 포인터 시맨틱을 그대로 유지한 채 점진적으로(코드베이스 일부부터) 도입할 수 있다.

> [!quote]- 📄 원문 표현 (paper)
> - "OBASE decouples an object's logical identity from its location using a lightweight guide abstraction." (p.4)

**3. ATC/epoch 기반 optimistic lock-free 마이그레이션** — GC의 load barrier나 stop-the-world 없이, 객체별 Active Thread Count(ATC)가 0일 때만(즉 그 객체를 조작 중인 스레드가 없을 때만) 마이그레이션을 허용한다. 마이그레이션은 복사 후 guide를 CAS로 원자적으로 스왑하는 2단계 optimistic protocol이며, 도중 접근이 발생하면 CAS 실패로 조용히 abort되어 스레드는 절대 stale pointer를 보지 않는다. RCU/epoch-based reclamation에서 영감을 받았지만, GC처럼 forwarding pointer로 해결하는 대신 충돌 시 마이그레이션 자체를 거부(veto)하는 방식을 택했다.

> [!quote]- 📄 원문 표현 (paper)
> - "Migration is permitted only when ATC reaches zero, indicating that no thread is currently executing an operation that could read or modify the object." (p.6)
> - "OBASE instead adopts a quiescence-based approach inspired by epoch-based reclamation [20, 25] and RCU [43]: it tracks the number of threads actively using each object (ATC) and only relocates when this count reaches zero. Any concurrent access aborts migration via CAS failure rather than requiring forwarding-pointer resolution." (p.12)

## 설계 / 메커니즘 (Design)

**전체 아키텍처 (Fig.4, p.4)**: 애플리케이션(Trees/Hash Tables/Skip Lists 등 pointer-based 자료구조)이 Guide를 통해 접근하면 SODA가 접근을 기록하고, 백그라운드 Object Collector(OC)가 주기적으로 객체를 NEW→HOT→COLD 힙으로 재분류·마이그레이션하며(SAMA 사용), 재구성된 주소공간을 OS tiering 백엔드(kswapd/TMO/TPP/Memtis 등)에 그대로 노출한다.

- **Guide metadata encoding (§4.1, p.7)**: 48비트 canonical x86-64 주소의 상위 16비트를 재활용해 ATC(7비트, 객체당 최대 128 concurrent thread), CIW(5비트, 최대 32 스캔윈도우 추적), 현재 힙 ID(2비트, NEW/HOT/COLD), access/migration-lock 플래그(2비트)를 인라인으로 저장한다. 별도 side table 없이 포인터 역참조 자체에 access recording이 포함된다.
- **SAMA (Spatially-Aware Memory Allocator, §3.4/§4.1)**: jemalloc의 extent management를 기반으로 각 힙(NEW/HOT/COLD)마다 연속된 가상주소 range를 예약하고 그 안에서 객체를 sub-allocate한다. 이 contiguity 덕분에 backend는 개별 객체가 아니라 힙 전체 단위로 coarse-grained madvise 힌트(MADV_HUGEPAGE/MADV_COLD/MADV_PAGEOUT)를 적용할 수 있다(Fig.5, p.6).
- **SODA (Sparse Object Data Activity, §4.3, p.7)**: guide 포인터 존재 위치를 추적하는 2단계 비트맵(coarse block + 64비트 word 단위 슬롯 표시). 객체 주소가 아니라 guide 자체를 추적하므로 객체가 이동해도 유효하다. 오버헤드는 potential guide slot당 1비트.
- **Cold Threshold Controller (§4.4, p.7)**: 승격률(promotion rate) $PR=\dfrac{\text{unique COLD pages accessed}}{\text{working set size}}\times\dfrac{60}{\text{scan interval(s)}}$을 목표치 1%에 맞추도록 cold threshold $C_t$(비활성 스캔윈도우 수)를 스캔마다 ±1 조정한다. 기본 스캔 간격 120초, $C_t$ 초기값 3, 범위 $[1,32]$. CIW(Consecutive Inactive Window)가 $C_t$를 넘으면 COLD로 강등, COLD 중 접근되면 즉시 HOT으로 복귀한다(Fig.5 state diagram).
- **Scope Guard tracking (§4.2, Fig.6, p.7)**: 컴파일러가 public API 진입/종료 지점에 `createTAG`/`addToTAG(guide)`/`destroyTAG` 훅을 삽입해 Thread-local Active Scope Guard(TAG)로 중첩된 내부 호출에서도 ATC를 정확히 관리한다. `BaseDeltaPtrSet`으로 포인터 locality를 활용해(2 cache line 내 최대 16개 근접 포인터를 base+32비트 delta로 그룹화) 삽입 비용을 낮췄고, 10개 자료구조에서 연산당 median unique guide 수는 3(hash table)~12(B+Tree traversal), per-op TAG 오버헤드 100ns 미만이다.
- **Epoch protocol (§4.5, Fig.6, Table 1, p.8)**: 전역 epoch counter + Thread Activity Index(TAI)로 INACTIVE→PREPARE→ACTIVE 3상태를 전이한다. OC가 마이그레이션을 시작하면 epoch을 증가시키고 PREPARE에 진입, 모든 스레드가 새 epoch에 동기화(모든 슬롯이 새 epoch 기록)될 때까지 TAI를 스캔한 뒤 ACTIVE로 전환한다. ACTIVE 상태에서만 ATC==0 & migration-lock clear인 후보를 2단계 CAS(락 설정→SAMA로 복사·새 guide 구성→publish)로 이동시키며, 어느 CAS든 실패하면 마이그레이션은 중단(abandon)된다.

> [!quote]- 📄 원문 표현 (paper)
> - "OBASE embeds access tracking directly into guide pointer dereferences, yielding object-level information without significant overhead." (p.6)
> - "Contiguity is a deliberate design choice that allows coarse-grained OS hints to be applied to whole heaps rather than individual objects." (p.5)
> - "Callers never follow stale pointers and do not need explicit synchronization." (p.6)

## 평가 (Evaluation)
평가 환경: Intel Xeon Gold 5218(16코어, SMT off), Ubuntu 22.04, Linux 6.12; fast tier 2×16GB DDR4-2400 DRAM, slow tier 4×128GB Optane PMEM-2666 (Memory Mode, ~로컬 DRAM 대비 2.5배 지연); swap 실험은 512GB Optane P4800X SSD 사용(p.8, §5.1). 테스트베드는 자체 구현 in-memory KV store **CrestDB**, Table 2의 10개 concurrent 자료구조(Hash Table Harris/Pugh/Java CHM, SkipList Coarse/Fraser/Herlihy, B+Tree Coarse/OCC/MassTree, ART) 사용.

- **E1 효과성 (§5.2, Fig.7, p.9)**: YCSB A/B/C(10M키, 30B키/1024B값, 13GiB 데이터셋, Zipfian) 기준 page utilization이 워크로드 A(쓰기 50%) 2배, B(쓰기 5%) 약 3배, C(read-only) 최대 4배 개선. 수렴 후 절대 utilization은 40%(A)~80%(C). RSS는 모든 자료구조·워크로드에서 65–72% 감소(예: 워크로드 B 10M키에서 baseline RSS 12.4GiB → 수렴 후 3.5–4.0GiB).
- **E2 백엔드 시너지 (§5.3, Fig.8·9, p.10)**: MassTree+YCSB-C(4GiB working set, 13GiB footprint)에서, OBASE 없이는 메모리-성능 트레이드오프가 강제됨 — Kswapd 1.8배 절감(처리량 손실 없음, 다만 3GiB cold 데이터가 mixed 페이지에 갇힘), Cgroup 3.2배(처리량 38% 붕괴), TMO 2배(PSI가 페이지 수준 압력만 보므로 추가 회수 불가). OBASE 적용 시 4개 백엔드 모두 4GiB(가장 공격적 수준)에 도달하면서 처리량 저하 없음. 티어링 백엔드(TPP/AutoNUMA/Memtis, MassTree+YCSB-B 50M키/67GiB, DRAM:CXL 1:4/1:8/1:16)에서는 TPP 단독 1.65배(1:4)→1.25배(1:16)에서 OBASE+TPP는 1.85배→1.45배(가장 제약적인 1:16에서 16%p 개선). AutoNUMA 단독은 1:16에서 1.05배(거의 무의미)였으나 OBASE+AutoNUMA는 TPP 단독 수준을 따라잡음. Memtis는 이미 PEBS 기반 신호를 정교하게 활용해 이득 폭이 작음(3–10%p vs TPP/AutoNUMA의 12–29%p). 핵심: OBASE+TPP(1:16, 1.45배)가 TPP 단독(1:8, 1.55배)에 근접 — hot set을 2.5배 축소시켜 DRAM 절반으로 동등 성능 달성.
- **E3 오버헤드/확장성 (§5.4, Fig.10, p.11)**: reclamation/tiering 비활성 상태에서 uninstrumented 대비 평균 처리량 2.5% 감소, p90 지연 5% 증가(hash table 1.5–3%로 최소, skiplist/B+Tree/ART 3–5%). OC 전용 스레드는 CPU 1% 미만 사용. 스레드 2→32로 확장 시 처리량은 거의 선형 확장, 오버헤드는 1–8%로 상한 유지(Hashtable Pugh, Skiplist Fraser, MassTree OCC 대표 측정).
- **E4 실제 trace (§5.5, Fig.11·12, p.11–12)**: Meta CacheLib, DBench Mixgraph, Twitter Cluster 7(고편중 α=1.07), Cluster 23(저편중 α=0.274, 쓰기중심) 4개 trace를 CrestDB+ART로 재생. Page utilization 1.8–3.4배 개선(Cluster 23이 최대 3.4배 — 저편중이라 baseline utilization이 매우 낮았음; Cluster 7이 최소 1.8배 — 고편중이라 baseline이 이미 준수). OBASE Hinted는 RSS를 36–58% 감소(Cluster 7 최대 58%, DBench 최소 36%). TMO에 OBASE를 추가하면 15–30%p 추가 절감(TMO는 회수 가능 페이지를 식별하고 OBASE는 그 페이지가 실제로 균일하게 cold하도록 보장 — 상호보완적). Fig.12: Meta CacheLib trace 2.3시간 관찰에서 $C_t$가 초기 3(승격률 14%로 급등)에서 25분 내 18로 수렴(승격률 1% 미만), t=5400s 워크로드 변화 시 재수렴 — 컨트롤러가 워크로드 진화를 지속 추적함을 입증.

> [!quote]- 📄 원문 표현 (paper)
> - "Takeaway #2: OBASE eliminates the memory-vs-performance trade-off that forces datacenter operators to balance aggressive reclamation against SLO compliance." (p.10)
> - "Takeaway #4: OBASE imposes 2–5% overhead that remains constant as thread count increases, a modest cost relative to backend improvements demonstrated in §5.2–5.3." (p.11)
> - "Takeaway #5: OBASE delivers consistent memory savings across real world workloads with diverse characteristics, even provides benefits over state-of-the-art backends like TMO, and dynamically adapts to shifting access patterns." (p.12)

## 섹션 노트
- **§1 Introduction**: hotness fragmentation을 문제로 정의하고 layout(frontend)/reclamation(backend) 분리라는 설계 철학을 제시. 기존 티어링 인프라 재사용 및 unmanaged 언어(C/C++) 대상임을 명시.
- **§2 Case for Dynamic Object Reorganization**: page utilization 메트릭 정의, Google 6개 production 워크로드로 fragmentation 정량화(Finding 1), Meta/Twitter object trace로 hotness의 시간적 비정상성 입증(Finding 2), unmanaged 언어에서 객체 이동의 어려움 설명.
- **§3 Object-Based Address-Space Engineering**: 시스템 개요(Fig.4), guide를 통한 object mobility, 4가지 핵심 challenge(mobility/tracking overhead/dynamic adaptation/safe concurrent migration) 제시, NEW/HOT/COLD 힙 구조와 SAMA/cold threshold 적응, safe concurrent migration(ATC/epoch) 개념 소개.
- **§4 Implementation**: guide metadata bit layout, TAG 기반 scope tracking, SODA bitmap, cold threshold controller의 구체적 수치, epoch protocol과 race resolution(Table 1) 상세.
- **§5 Evaluation**: E1(효과성)~E4(실사용 trace 적응성) 4개 질문에 답하는 구조. CrestDB/YCSB/production trace 실험.
- **§6 Related Work**: AIFM/MIRA(object-level 관리하지만 custom runtime), Alaska(handle indirection이지만 hotness 미추적), 정적 allocation-time 배치, TPP/Memtis/HawkEye/TMO(page-level tiering, OBASE가 보완), GC 진영(ZGC/Shenandoah의 load barrier 방식과 OBASE의 quiescence 기반 차이)과 비교.
- **§7 Conclusion**: 가상주소 공간을 "저장소"가 아닌 애플리케이션-OS 간 "커뮤니케이션 채널"로 재정의; generational GC, NUMA false-sharing 완화, trust-level isolation 등으로의 확장 가능성을 미래 방향으로 제시. 한계로 relocatable pointer에 대한 개발자 annotation 필요와 pointer-based 구조 한정을 명시.

## 핵심 용어 (Key terms)
- **Hotness fragmentation**: 객체의 실제 접근 빈도와 무관하게 크기 기준으로 배치되어 hot/cold 객체가 같은 페이지에 섞이는 현상
- **Page utilization**: 터치된 페이지들에서 실제 접근된 바이트 수 대비 전체 페이지 용량의 비율 (fragmentation 정량화 지표)
- **Address-space engineering**: 접근 패턴이 유사한 객체를 가상주소 공간상에서 동적으로 재배치·군집화하는 기법
- **Guide**: 객체의 논리적 identity와 물리 위치를 분리하는 경량 포인터 간접 계층; 역참조 시 접근 기록도 함께 수행
- **SODA (Sparse Object Data Activity)**: guide 존재 위치를 추적하는 2단계 sparse bitmap
- **SAMA (Spatially-Aware Memory Allocator)**: 힙별로 연속 가상주소 range를 예약·서브할당하는 jemalloc 기반 할당기
- **Object Collector (OC)**: 주기적으로 객체 온도를 재분류하고 힙 간 마이그레이션을 수행하는 백그라운드 컴포넌트
- **ATC (Active Thread Count)**: 특정 객체를 현재 조작 중인 스레드 수 카운터; 0이어야 마이그레이션 허용
- **TAG (Thread-local Active Scope Guard)**: public API 진입~종료 구간 동안 유지되는 스레드-로컬 nesting-aware 가드
- **CIW (Consecutive Inactive Window)**: 객체가 연속으로 접근되지 않은 스캔 윈도우 수; cold threshold와 비교되어 강등 여부 결정
- **Cold threshold ($C_t$)**: promotion rate 목표(1%)에 맞춰 적응적으로 조정되는 비활성 임계 윈도우 수
- **Promotion rate (PR)**: 스캔 윈도우당 COLD 힙에서 재접근된 페이지 비율 — $C_t$ 적응 컨트롤러의 피드백 신호
- **Epoch protocol (INACTIVE/PREPARE/ACTIVE)**: 마이그레이션 안전성을 위한 3상태 프로토콜; RCU/epoch-based reclamation에서 영감을 받은 quiescence 기반 접근

## 강점 · 한계 · 열린 질문
**강점**: (1) frontend/backend 분리로 기존 OS 티어링 인프라(kswapd, TMO, TPP, AutoNUMA, Memtis)를 전혀 수정하지 않고도 시너지를 낸다는 점이 채택 장벽을 크게 낮춤. (2) lock-free/non-blocking 마이그레이션으로 stop-the-world 없이 다양한 동시성 메커니즘(lock-free~coarse global lock, Table 2)에 걸쳐 균일하게 동작함을 10개 자료구조로 실증. (3) synthetic(YCSB) + production trace(Meta/Twitter) 양쪽에서 일관된 이득과 낮은(2–5%) 오버헤드.

**한계**: (1) relocatable 포인터에 개발자가 수동으로 annotation을 달아야 하며 pointer-based 자료구조에만 적용 가능(비-pointer 자료구조나 mmap된 raw buffer 등은 범위 밖). (2) unmanaged 언어(C++) 특화 설계이며 managed 런타임(JVM/Go) 적용은 "일반화 가능하다"는 주장만 있고 실증되지 않음(p.4). (3) 평가가 자체 구현 CrestDB(연구용 KV store)에 한정되어 실제 프로덕션 시스템(Redis, RocksDB 등)에서의 검증은 없음. (4) 단일 노드/단일 서버 실험만 존재하며 멀티노드 CXL fabric이나 disaggregated 환경은 다루지 않음. (5) promotion rate target(1%)과 cold threshold 초기값 등은 경험적으로 고정되어 있고, "최적 promotion rate는 하드웨어에 의존적"이라며 hardware-aware 튜닝을 future work로 남김(p.9).

**열린 질문**: Conclusion에서 제시된 확장 방향—generational GC(age 대신 access intensity로 grouping), NUMA 노드 간 false sharing 감소(access pattern별 분리), trust-level 격리—이 실제로 얼마나 잘 작동할지는 미검증. 128비트 addressing 시대에 guide metadata encoding을 어떻게 확장할지도 미래 과제로 남아있다(p.7 각주 성격 언급).

## ❓ Q&A (자가 점검)

> [!question]- 왜 allocation-time hint나 static 배치로는 hotness fragmentation을 해결할 수 없는가?
> Finding 1/2(§2.2–2.3)에 따르면 같은 코드 경로가 매우 다른 lifecycle의 객체를 할당하고(예: Redis SET 핸들러가 세션 토큰과 평생 안 읽히는 프로필을 동일하게 할당), object hotness 자체가 Meta는 phased하게, Twitter는 sporadic하게 시간에 따라 계속 변한다(Fig.3). 따라서 할당 시점의 정적 판단으로는 근본 해결이 불가능하다.

> [!question]- OBASE가 백엔드를 전혀 수정하지 않고도 이득을 주는 메커니즘은?
> Frontend(layout)/backend(reclamation) 문제를 분리해, address space만 재구성하여 균일하게 hot/cold한 페이지를 만든다. 백엔드는 자신이 이미 쓰던 신호(PTE access bit, PEBS 샘플, PSI 신호)를 그대로 관찰하지만, 그 페이지들이 이제 실제로 균일한 온도이므로 더 정확한 결정을 내리게 된다.

> [!question]- lock 없이 어떻게 동시성 안전성을 보장하는가?
> guide는 원자적 CAS로 갱신되며, ATC(해당 객체를 조작 중인 스레드 수)가 0이고 migration-lock이 clear일 때만 마이그레이션이 허용된다. 복사 후 guide를 스왑하는 CAS 도중 어떤 스레드가 그 객체를 참조하면 CAS가 실패해 마이그레이션은 조용히 abort되며(Table 1), 스레드는 절대 stale pointer를 보지 않는다.

> [!question]- page utilization 메트릭의 수식은?
> $\text{Utilization}(T)=\dfrac{\sum_{p\in P(T)} U(p,T)}{\sum_{p\in P(T)} \text{Size}(p)}$ (p.3), 여기서 $P(T)$는 시간창 $T$ 동안 터치된 페이지 집합, $U(p,T)$는 페이지 $p$에서 실제 접근된 고유 바이트 수.

> [!question]- Cold threshold $C_t$는 어떻게 적응되는가?
> Promotion rate $PR$(스캔당 COLD 힙에서 재접근된 unique 페이지 비율)을 목표치 1%와 비교해, 관측된 rate가 target을 넘으면 $C_t$를 1 윈도우 증가시키고 낮으면 1 감소시킨다. $[1,32]$ 범위로 bound되며 기본 스캔 간격은 120초다.

> [!question]- reclamation backend 실험(§5.3.1)에서 OBASE가 없앤 트레이드오프는 무엇인가?
> Kswapd/Cgroup/TMO 각각 메모리 절약과 처리량 사이 트레이드오프에 갇혀 있었다(Cgroup은 3.2배 절감이지만 처리량 38% 붕괴 등). OBASE 적용 시 4개 백엔드 모두 가장 공격적인 절감 수준(4GiB RSS)에 도달하면서 처리량 저하가 없어, 데이터센터 운영자가 aggressive reclamation과 SLO 준수 사이에서 타협할 필요가 없어진다(Takeaway #2).

> [!question]- 오버헤드는 어디서 비롯되는가?
> 두 가지: (1) 매 guide 역참조마다 발생하는 tagged-pointer atomic read-modify-write(4–5ns, L1 캐시 히트 수준), (2) ACTIVE epoch 동안의 TAG/ATC bookkeeping. 평균적으로 처리량 2.5% 감소, p90 지연 5% 증가이며 스레드 수(2→32)에 따라 증가하지 않는다(§5.4).

> [!question]- Memtis에서 OBASE의 개선 폭이 TPP/AutoNUMA보다 작은 이유는?
> Memtis는 PEBS 하드웨어 샘플링으로 이미 상당히 정교한 page-level hotness 신호를 포착하고 있어서, OBASE가 제공하는 object-level 정보의 한계 이득이 상대적으로 작다(3–10%p 개선 vs TPP/AutoNUMA의 12–29%p, p.10).

## 🔗 Connections
[[CXL]] · [[OSDI]] · [[2026]]

## References worth following
- **TMO: Transparent Memory Offloading in Datacenters** (Weiner et al., ASPLOS 2022, ref [53]) — OBASE가 §5.3/§5.5에서 직접 결합·비교하는 Meta의 PSI 기반 proactive reclamation 백엔드; OBASE의 보완적 관계(15–30%p 추가 절감)를 이해하려면 필독.
- **TPP: Transparent Page Placement for CXL-Enabled Tiered Memory** (Maruf et al., ASPLOS 2023, ref [42]) — OBASE의 주요 tiering 백엔드 실험 대상; hysteresis 기반 promotion/demotion 정책의 기준선.
- **Memtis: Efficient Memory Tiering with Dynamic Page Classification and Page Size Determination** (Lee et al., SOSP 2023, ref [38]) — PEBS 기반 최신 page-level tiering으로, OBASE 이득이 가장 작게 나타나는 강력한 baseline; object-level 정보의 한계 이득을 가늠하는 비교점.
- **AIFM: High-Performance, Application-Integrated Far Memory** (Ruan et al., OSDI 2020, ref [48]) — object-level 원격 메모리 관리의 대표 선행 연구; OBASE와 달리 custom runtime을 요구한다는 점에서 설계 철학 차이를 대조하기 좋음.
- **Tidying Up the Address Space** (Banakar et al., DIMES 2025, ref [16]) — 같은 저자진의 선행 workshop 논문으로, page utilization 저하 현상(Redis/MongoDB YCSB)을 처음 지적한 동기 부여 자료.

## Personal annotations
<!-- 본인 메모 영역 -->
