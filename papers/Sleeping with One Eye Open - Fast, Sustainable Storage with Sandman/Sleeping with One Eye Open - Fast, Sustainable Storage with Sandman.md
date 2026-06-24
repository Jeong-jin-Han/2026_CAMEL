---
title: "Sleeping with One Eye Open: Fast, Sustainable Storage with Sandman"
aliases: [Sleeping with One Eye Open]
description: "All-flash 서버의 폴링 스택 에너지 낭비를 cache-coherence 기반 얕은 sleep과 NIC 큐 기반 마이크로초 burst 감지로 줄여 SPDK급 성능에 전력 39.38%·에너지 33.36%까지 절감하는 스케줄러 Sandman."
venue: SOSP
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/sustainability
  - topic/storage
  - venue/sosp
  - year/2025
---

# Sleeping with One Eye Open: Fast, Sustainable Storage with Sandman

> **SOSP 2025** · `cluster/fs` · Source: [Sleeping with One Eye Open - Fast, Sustainable Storage with Sandman.pdf](Sleeping%20with%20One%20Eye%20Open%20-%20Fast,%20Sustainable%20Storage%20with%20Sandman.pdf)

저자: Yanbo Zhou (UC San Diego), Erci Xu (Shanghai Jiao Tong University, 교신저자), Anisa Su (UC San Diego), Jim Harris (Samsung Semiconductor, 현 NVIDIA), Adam Manzanares (Samsung Semiconductor), Steven Swanson (UC San Diego)

## TL;DR
All-flash 서버는 고성능을 위해 **busy-polling 스택(SPDK)** 을 쓰면서 워크로드가 한가할 때도 CPU 코어를 100% 점유해 막대한 전력을 낭비한다. Sandman은 (1) cache-coherence 기반 **user-level wait (monitorx/mwaitx)** 로 코어를 얕은 sleep(C-1)에 두었다가 system call/interrupt 없이 즉시 깨우는 **fast resource scaling**, (2) **NIC 수신 큐의 들어오는 I/O 개수**를 마이크로초 단위로 세어 burst를 감지하는 정책을 결합한다. 그 결과 SPDK 대비 성능 격차는 corner case에서도 5% 이내(평균 4.8%)이면서 평균 전력 최대 39.38%, 에너지 최대 33.36%를 절감한다.

## 문제 & 동기
최신 NVMe PCIe 5.0 SSD는 드라이브당 2,500K IOPS·14 GB/s 이상, 용량 120 TB 이상에 도달했다. 이 성능을 끌어내려면 SSD당 CPU 코어가 SATA 대비 32배, PCIe 3.0/4.0 대비 4배/2배 더 필요하고, 고성능을 위해 SPDK 같은 **polling-mode 스택**을 쓰면 CPU가 idle 대비 1.82배 전력을 소비한다. 문제는 실제 클라우드 워크로드(Google, Meta, Alibaba)가 짧은 burst 중심이라 nonstop polling이 불필요하다는 점이다 — required compute capacity가 peak 대비 훨씬 낮은데도, peak를 처리하려고 예약한 CPU 때문에 낭비 에너지가 required의 최대 3.4배에 이른다(Figure 2). 기존 대안(Linux interrupts, Governor, Dynamic Scheduling, Hybrid Polling) 중 어느 것도 SPDK 수준 성능과 에너지 효율을 동시에 달성하지 못한다.

> [!quote]- 📄 원문 표현 (paper)
> "Through a motivational study, we discover that the culprit is the inefficiency in the software stack, and existing power-saving methods fail to deliver comparable performance, especially under workload bursts." (Abstract, p.1)
>
> "supporting a single PCIe 5.0 NVMe SSD (capable of 2,500K IOPS) under this setup requires at least three logical cores on the storage server with the CPU-efficient SPDK stack. This is 32×, 4×, and 2× higher than running a SATA SSD, and NVMe PCIe 3.0 and 4.0 SSDs, respectively." (§2.2 R1, p.3)
>
> "The total wasted energy can amount to 3.4× the required energy (i.e., the ratio of the red area to the blue area in Figure 2)." (§2.2 R3, p.3)

## 핵심 통찰

> [!note]- 토글: 핵심 통찰 펼치기
> - **에너지 낭비의 근본 원인은 polling, 그리고 cascading effect.** CPU가 polling으로 온도가 65°C→80°C로 오르고 서버 팬이 18K→28K RPM으로 올라가 메모리·냉각 등 other components도 비슷하게 전력을 더 쓴다(§2.2 R2, Table 1). 즉 CPU 활동이 시스템 전체로 파급된다.
> - **기존 power-saving 기법의 5가지 한계(Observation #1~#5).** ① Interrupts는 context switch로 polling보다 더 많은 전력을 쓸 수도 있다. ② **하드웨어 transition overhead** — C-state/주파수 전환에 수백 μs(P-state는 base까지 최소 400 μs, Figure 5)가 걸려 burst 대응이 늦다. ③ thread load(CPU cycle)로 마이크로초 단위 burst를 감지하면 부정확해 800 μs 이상 latency가 추가된다. ④ **Software wakeup overhead** — system call·interrupt로 sleeping 코어를 깨우면 27 μs(Dynamic Scheduling 기준)가 critical path에 든다. ⑤ 잦은 low/high power 전환은 코어가 충분히 오래 sleep하지 못하게 해 절감을 제한한다.
> - **두 가지 결정적 novelty.** ① burst를 전통적 CPU-cycle 계수 대신 **NIC 큐의 I/O를 분석**해 모델링한다(마이크로초 단위 감지). ② **cache-coherence 메커니즘(monitorx/mwaitx)을 lightweight thread와 결합**해 system call/interrupt 없는 wakeup을 구현한다.
> - **설계 가이드라인(D1~D4)이 관찰에서 직접 도출된다.** D1: 주파수 낮추지 말고 코어를 sleep시켜라(exit latency가 낮은 shallow sleep 우선, Table 2). D2: scheduling critical path에서 system call/interrupt 금지. D3: sibling 코어를 함께·오래 sleep시켜라. D4: burst는 CPU cycle이 아니라 NIC 큐에서 측정하라.

## 설계 / 메커니즘

> [!note]- 토글: 설계 / 메커니즘 펼치기
> Sandman은 SPDK 위에서 동작하며 코어를 **main core 1개 + worker core 여러 개**로 나눈다(Figure 6). 둘 다 polling mode이지만 main core가 추가로 scheduler를 돌려 metric을 모으고 I/O thread 재분배를 결정한다.
>
> **① Fast Resource Scaling (§4.3)** — 두 기술.
> - *Lightweight I/O threads*: I/O thread를 user-level lightweight thread로 구현(SPDK의 reactor/thread 프레임워크 재사용). 코어는 ring list에 든 I/O thread를 round-robin으로 실행하며, thread를 다른 코어의 ring list로 옮겨 빠르게 scaling한다(Figure 7).
> - *Shallow sleep with instant transition*: idle 코어는 **monitorx** 로 event queue의 다음 entry 주소를 모니터링하고 **mwaitx** 로 C-1 shallow sleep에 든다. 다른 코어가 그 cache line에 **store** 하면(modified 상태로 변경) cache-coherence 프로토콜로 sleeping 코어가 **즉시** 깨어난다 — system call·interrupt 불필요(Figure 8(b)). 기존 kernel 기반(epoll_wait/eventfd, Figure 8(a))과 대비된다.
>
> **② Resource Monitoring & Burst Detection (§4.4)** — 두 가지 시간 단위.
> - *Coarse-grained (1초)로 낭비 모니터링*: core load = busy cycles / total cycles(SPDK와 동일). idle threshold T_idle, healthy threshold T_healthy 정의. 기본 T_healthy=80%(20% CPU를 burst 버퍼로 예약), T_idle = T_healthy의 절반. load < T_idle이면 thread를 다른 코어로 consolidate하고 그 코어를 sleep. **Core selection**: target 코어가 healthy를 유지(core load ≤ T_healthy)하면 수용하고, D3에 따라 sibling(hyper-thread) 쌍이 함께 sleep하도록 옮긴다(Figure 9, 10).
> - *Fine-grained (10 μs)로 burst 감지*: NIC 큐의 incoming I/O count를 metric으로 사용(D4). **Workload Variation Detection Model** — 이전 scheduling event들의 I/O count로 Moving Average(MA)와 Standard Error(SE)를 계산해 신뢰구간 CI = MA ± Z·SE(95% 신뢰수준, Z≈1.96)를 만든다. 현재 polled I/O count가 CI 상한을 넘으면 workload 급증으로 보고 thread를 새 코어로 reschedule. 성능을 위해 **unused sleeping 코어 전체를 깨워** 통째로 할당(static allocation처럼 최고 성능 보장), hyper-thread sibling을 우선.
>
> **③ Implementation (§4.5)** — SPDK 기반. Data plane은 SPDK NVMe-oF + RDMA InfiniBand user-space lib로 storage·network 모두 polling. Control plane: SPDK 컴포넌트 재사용 + 확장(fine/coarse-grained event 282 LoC, RDMA RX 큐 보고하는 poller 192 LoC, reactor event 큐 234 LoC) + 신규 추가(user-level wait 추상화 layer 128 LoC, burst 감지 모듈 192 LoC, scheduler 884 LoC).

## 평가

> [!note]- 토글: 평가 펼치기
> **셋업(§5)**: AMD EPYC 9454P 48-core(3.65 GHz max / 400 MHz min), 756 GB DRAM, 16× Samsung PM1743 3.84 TB PCIe 5.0 SSD, Mellanox ConnectX-6 2×200 Gbps. Ubuntu 22.04.3(Linux 6.8.7), SMT 활성. 두 서버 직결(NVMe-oF over RDMA). powerstat(CPU)·IPMI(system) 1초 단위 측정. 비교 대상: SPDK(성능 baseline), Linux, Governor, Dynamic Scheduling, Hybrid Polling.
>
> - **종합 성과(Abstract, §1)**: 평균 전력 최대 **39.38%**, 에너지 최대 **33.36%** 절감, 성능은 SPDK 대비 corner case에서도 5% 이내. 실제 클라우드 block storage 워크로드에서 Linux/SPDK 대비 각각 에너지 **30.23%·33.36%** 절감, latency 분포는 SPDK와 동등(p.2).
> - **Stable workload (§5.1)**: Sandman은 SPDK 대비 P99.9 tail latency 차이 **4.8%** 에 불과(SPDK에 가장 근접). Governor는 5210K IOPS에서 SPDK 대비 최대 **161.5%** 높은 latency, Dynamic Scheduling은 640K IOPS 초과 시 thread migration으로 latency 급증(p.10).
> - **Burst workload (§5.2)**: Sandman은 다중 코어를 더 오래 sleep시켜 system 전력 최대 **39.38%** 절감, latency·IOPS는 SPDK와 동등. Governor가 최악(주파수 scaling 비효율), Linux가 차악(context switch)(p.10).
> - **Attribution / ablation (§5.3, Figure 11)**: 성능 — SF(주파수 scaling) 기준에 sleeping cores(+SC) 추가로 latency **41.34%** 감소, NIC 큐 burst 감지(+DQ) **25.13%** 추가 감소, user-level wait(+NI) **17.69%** 추가 감소로 SPDK에 근접. 전력 — Hybrid Polling(+HP) **22.41%** 절감, sleeping cores(+SC) **15.25%** 추가, scheduling siblings(+SS) **16.03%** 추가 절감(p.11).
> - **Burst 감지 정확도(§5.3)**: stable에서 불필요한 migration 회피로 **93.45%~95.78%** 정확도, burst 감지는 thread movement 기준 **97.84%** 정확도(p.11).
> - **오버헤드(Table 3, §5.3)**: 통계 수집 79.03 ns(CPU 0.78%), scheduling 알고리즘 1.99 μs(16.60%), thread movement 106.52 ns(1.05%). 10 μs interval에서 main core는 알고리즘에 CPU의 16.6%를 쓰며, 최악의 경우 active thread를 돌리면 IOPS가 16.6% 감소(p.11).
> - **CPU state 비교(Table 2)**: P-0(3.65 GHz) 244 W/716 W, C-1(shallow) 134 W/412 W·exit 3 μs, C-N(deep) 52 W/252 W·exit 800 μs → exit latency 때문에 shallow sleep 선호(p.6).
> - **Application(§5.5)**: RAID-5 flash array, RocksDB(YCSB) 모두에서 SPDK급 성능·에너지 절감. RAID-5 write 워크로드는 write amplification·parity 계산으로 sleep 기회가 적어 절감 폭이 작다(p.12).
> - **Cloud trace(§5.6)**: Alibaba/Tencent block-level trace 24시간 replay. Alibaba에서 SPDK에 가장 근접한 latency 분포로 Linux/SPDK 대비 에너지 **30.23%·33.36%** 절감(Figure 15)(p.13).

## 섹션 노트
- **§1 Introduction**: 4개 기존 접근의 한계 요약, 4개 design guideline 도출, 기여·novelty 정리.
- **§2 The Dilemma in Storage Evolution**: all-flash 서버 진화(2.1)와 에너지 소비 3가지 원인 R1(고처리능력 수요)·R2(I/O 처리 고전력)·R3(현장 burst 빈번)(2.2). Table 1(전력 breakdown), Figure 2(reserved/required/wasted).
- **§3 Limitations of Existing Approaches**: SPDK·Linux·Governor·Dynamic Scheduling·Hybrid Polling을 stable/burst로 비교, Observation #1~#5 도출. Figure 3·4·5.
- **§4 Sandman**: Design guideline D1~D4(4.1), system overview(4.2), Fast Resource Scaling(4.3), Resource Monitoring & Burst Detection(4.4), Implementation(4.5). Figure 6~10.
- **§5 Evaluation**: latency·power(5.1), burst resilience(5.2), 내부 메커니즘·ablation(5.3), 민감도(5.4), application(5.5), cloud trace(5.6). Figure 11~16, Table 3.
- **§6 Discussion**: 하드웨어 의존성(Intel 4th Gen/AMD 3rd Gen 이후 user wait 지원), 특수 network stack 호환성, DPU 적용 가능성.

## 핵심 용어
- **busy-polling / polling-mode stack (SPDK)**: CPU 코어를 고정 할당해 I/O 완료를 끊임없이 polling하는 방식. 최고 성능이지만 워크로드와 무관하게 100% 점유 → 고전력.
- **monitorx / mwaitx**: 최신 CPU의 unprivileged user-level wait 명령. cache-coherence로 특정 메모리 범위(예: event queue entry 주소)를 모니터링하다 store가 일어나면 sleeping 코어를 system call/interrupt 없이 즉시 깨운다.
- **C-state / P-state**: C-state는 코어 sleep 깊이(C-1 shallow=exit 3 μs, C-N deep=exit 800 μs), P-state는 동작 주파수(P-0 max, P-N min). exit latency가 작은 C-1을 선호.
- **lightweight I/O thread**: user-level로 구현된 I/O thread. 여러 task를 묶은 추상화이며 ring list에 담겨 round-robin 실행, 코어 간 빠르게 이동 가능.
- **core load**: busy cycles / total cycles (scheduling interval 동안). SPDK와 동일한 기본 metric. idle/healthy threshold로 consolidation 판단.
- **Workload Variation Detection Model**: NIC 큐 incoming I/O count의 Moving Average와 Standard Error로 신뢰구간(CI=MA±Z·SE, Z≈1.96)을 만들어 burst를 감지하는 모델.
- **coarse-grained(1초) vs fine-grained(10 μs) scheduling**: 전자는 resource wasting 모니터링·consolidation, 후자는 burst 감지·rapid scaling.
- **sibling cores**: 같은 물리 코어를 공유하는 두 logical core(hyper-thread). 함께 sleep시켜야 실제 절전(D3).

## 강점 · 한계 · 열린 질문
- **강점**: 성능-에너지 trade-off를 거의 없앤다(SPDK 대비 4.8% latency 차이로 30%대 에너지 절감). burst를 CPU cycle이 아닌 NIC 큐에서 감지하는 발상이 마이크로초 단위 정확도를 만든다. monitorx/mwaitx로 wakeup의 27 μs software overhead를 제거. SPDK 위에 ~1,900 LoC로 구현돼 실용적. 실제 Alibaba/Tencent trace, RAID-5, RocksDB까지 폭넓게 평가.
- **한계**: ① **하드웨어 의존** — monitorx/mwaitx는 Intel 4th Gen Xeon/AMD 3rd Gen EPYC 이후에만 지원, 구형에서는 software interrupt fallback으로 latency 증가. ② write-heavy 워크로드(RAID-5)는 write amplification·parity로 sleep 기회가 적어 절감 폭이 작다. ③ scheduling 알고리즘이 main core CPU의 16.6%를 소비, 최악 시 active thread IOPS 16.6% 감소 가능. ④ 완전 idle 가정에선 Hybrid Polling이 더 절전할 수 있으나 실제론 거의 idle하지 않다고 주장.
- **열린 질문**: T_healthy(20% 버퍼) 같은 고정 파라미터가 다양한 burst 패턴에 robust한가? 95% 신뢰구간이 latency-sensitive 워크로드에서 burst를 놓칠 위험은? DPU/SmartNIC로 offload된 환경에서 절감 폭은? read/write 혼합 비율이 더 높은 trace에서도 30%대 절감이 유지되는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. all-flash 서버에서 에너지가 낭비되는 근본 원인은?
> 고성능을 위해 busy-polling 스택(SPDK)이 CPU 코어를 워크로드와 무관하게 100% 점유하기 때문. 실제 워크로드는 짧은 burst 중심인데도 peak 처리용으로 CPU를 예약해, 낭비 에너지가 required의 최대 3.4배에 이른다. polling으로 CPU 온도·팬 RPM이 올라 other components까지 전력을 더 쓰는 cascading effect도 있다.

> [!question]- Q2. 기존 4개 접근(Linux/Governor/Dynamic Scheduling/Hybrid Polling)이 실패하는 이유는?
> Linux interrupts는 context switch로 polling보다 더 쓸 수도 있고 latency 높음. Governor(주파수 scaling)는 transition에 수백 μs 걸려 burst 대응이 늦고 tail latency가 SPDK 대비 최대 161.5% 높음. Dynamic Scheduling은 CPU cycle 기반 load 추정이 마이크로초 단위에서 부정확해 latency 800 μs 추가·과도한 migration. Hybrid Polling은 잦은 sleep/wake 전환으로 절감이 제한적.

> [!question]- Q3. Sandman의 두 가지 핵심 novelty는?
> ① burst를 전통적 CPU-cycle 계수 대신 **NIC 큐의 incoming I/O를 분석**해 마이크로초 단위로 모델링. ② **cache-coherence 메커니즘(monitorx/mwaitx)을 lightweight thread와 결합**해 system call/interrupt 없는 wakeup 구현.

> [!question]- Q4. monitorx/mwaitx로 sleeping 코어를 어떻게 즉시 깨우나?
> idle 코어가 monitorx로 event queue의 다음 entry 주소(cache line)를 모니터링하고 mwaitx로 C-1 shallow sleep에 든다. 다른 코어가 그 주소에 store하면 cache line이 modified 상태로 바뀌고, cache-coherence 프로토콜이 sleeping 코어를 즉시 깨운다. system call·interrupt가 필요 없어 27 μs의 software wakeup overhead를 제거한다.

> [!question]- Q5. coarse-grained와 fine-grained 스케줄링의 역할 차이는?
> coarse-grained(1초)는 core load(busy/total cycles)로 resource wasting을 모니터링해 idle 코어의 thread를 consolidate하고 sibling을 함께 sleep시킨다. fine-grained(10 μs)는 NIC 큐 I/O count로 Workload Variation Detection Model의 신뢰구간(MA±1.96·SE)을 만들어 burst를 감지하고, unused 코어를 통째로 깨워 빠르게 scaling한다.

> [!question]- Q6. burst를 thread load(CPU cycle)가 아닌 NIC 큐로 감지하는 이유는?
> 마이크로초 단위에서는 코어당 CPU cycle이 제한돼 cycle 기반 load 추정이 부정확하다(Observation #3). 어떤 함수든 CPU cycle을 소비하면 thread-load 추정을 왜곡한다. NIC 수신 큐의 incoming I/O 개수는 워크로드의 source를 직접 세므로 마이크로초 단위 burst를 정확히 감지한다(D4).

> [!question]- Q7. Sandman은 SPDK 대비 성능·에너지에서 어떤 수치를 냈나?
> stable에서 P99.9 latency 차이 4.8%(SPDK에 가장 근접). 평균 전력 최대 39.38%, 에너지 최대 33.36% 절감. cloud trace(Alibaba)에서 Linux/SPDK 대비 에너지 30.23%·33.36% 절감하면서 latency 분포는 SPDK와 동등.

> [!question]- Q8. ablation에서 각 기술이 기여한 바는?
> 성능: sleeping cores(+SC) latency 41.34%↓, NIC 큐 burst 감지(+DQ) 25.13%↓, user-level wait(+NI) 17.69%↓. 전력: Hybrid Polling(+HP) 22.41%↓, sleeping cores(+SC) 15.25%↓, scheduling siblings(+SS) 16.03%↓. 즉 모든 구성요소가 성능·전력 양쪽에 의미 있게 기여한다.

## 🔗 Connections
[[File System]] · [[SOSP]] · [[2025]]

## References worth following
- **SPDK [54]** — Storage Performance Development Kit. Sandman의 기반이자 성능 baseline. busy-polling 스택의 표준.
- **Hybrid Polling [32]** (Le Moal, I/O Latency Optimization with Polling) — Sandman이 integrate한 adaptive sleep 정책의 출처.
- **Caladan [11] / Shenango [37] / ZygOS [43] / Shinjuku [23]** — 마이크로초 단위 thread scheduling 선행 연구. context switch·preemption 최적화 비교군.
- **Skyloft [22] / uProcess [28]** — user-space interrupt로 core rescheduling 효율을 높인 연구. resource utilization 관점.
- **Hunt et al. [17]** (User Wait Instructions Power Saving for DPDK PMD) — user-wait로 polling 전력을 줄인 선행 기법(sleep-active 모드, Sandman과 대비).
- **Wysocki [66]** (CPU Idle Time Management) — Linux C-state/P-state 관리 배경 지식.

## Personal annotations
<본인 메모 영역>
