---
title: "Characterizing and Emulating FDP SSDs with WARP"
aliases: [WARP, Characterizing FDP SSDs]
description: "두 상용 FDP SSD를 cross-device/cross-workload로 특성화하고, FDP 동작을 재현·탐구하는 최초의 오픈 에뮬레이터 WARP를 제안한 논문"
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/fdp
  - topic/emulation
  - topic/characterization
  - venue/fast
  - year/2026
---

# Characterizing and Emulating FDP SSDs with WARP

> **FAST 2026** · `cluster/fs` · Source: [Characterizing and Emulating FDP SSDs with WARP.pdf](<Characterizing and Emulating FDP SSDs with WARP.pdf>)

저자: Inho Song, Shoaib Asif Qazi (Virginia Tech); Javier González (Samsung Electronics); Matias Bjørling (Western Digital); Sam H. Noh, Huaicheng Li (Virginia Tech)

## TL;DR
FDP(Flexible Data Placement)는 RUH(Reclaim Unit Handle) 단위로 write를 분리해 WAF를 1.0 근처로 낮추겠다고 약속하지만, 실제 효과는 vendor·workload에 따라 크게 달라진다. 이 논문은 두 상용 FDP SSD를 종합적으로 특성화해 *Noisy RUH*와 *Save Sequential*이라는 미보고 현상을 발견하고, FDP 동작을 충실히 재현하면서 II/PI·OP·RU 크기 등 firmware 정책을 tunable knob으로 노출하는 최초의 오픈 에뮬레이터 **WARP**(FEMU 기반)를 제시한다.

## 문제 & 동기
FDP는 NVMe TP4146으로 표준화된 best-effort interface로, host가 placement hint(RUH)를 주되 GC는 전적으로 device가 관리한다. OpenChannel/ZNS와 달리 block interface 호환성을 유지하지만, RU size·OP ratio·RUH 개수·격리 방식(II vs PI)이 vendor마다 hard-code되어 host에 불투명하다. 그 결과 동일 workload가 한 device에서는 near-1 WAF를, 다른 device에서는 collapse를 보인다. 기존 연구는 단일 application stack에 집중되어 FDP가 *언제 작동하고 언제 실패하며 왜 device마다 다른지*를 설명하지 못한다.

> [!quote]- 📄 원문 표현 (paper)
> "FDP only reduces WAF when workload lifetimes align with RUH isolation; when they do not, the expected benefit vanishes." (p.2)
>
> "This gap between specification and practice is the major barrier to FDP adoption. Our study addresses this gap by asking three key questions: When does FDP deliver near-1 WAF, and when does it fail? Which device-level configurations drive these differences? What internal mechanisms explain the variation across devices?" (p.2)

## 핵심 통찰

> [!note]- 통찰 토글
> - **FDP는 flexible하지만 foolproof는 아니다.** placement model에 workload lifetime이 정렬되면 near-1 WAF를 주지만, misclassification·co-location·adversarial invalidation에서는 conventional SSD로 퇴화한다.
> - **두 가지 미보고 현상.** (1) *Noisy RUH*: 한 handle의 invalidation이 GC 압력을 통해 다른 handle의 WAF를 증폭. (2) *Save Sequential*: capacity-dominant sequential stream이 device 내부 정책과 충돌해 조기에 reclaim되어 device-friendly 특성을 잃음.
> - **격리의 진짜 피해자는 noisy stream이 아니라 capacity-dominant stream.** WAF breakdown에서 RUH 0(sequential overwrite)이 트래픽의 88%를 흡수하며, RUH 2의 invalidation이 RUH 1을 간접 증폭(Observation #6, #7).
> - **II vs PI는 OP slack에 의해 결정.** PI(Persistently Isolated)는 OP가 충분할 때만 II(Initially Isolated)를 이기고, OP가 부족하면 fragmentation 때문에 오히려 더 나쁘다.
> - **WARP로 hardware 한계를 넘는 정책 탐구 가능.** small RU(single-channel-mapped) 최적화로 kvcache WAF를 1.37 → 1.16까지 추가 절감.

## 설계 / 메커니즘

> [!note]- 설계 토글
> **WARP = Write Amplification Research Platform.** FEMU의 pre-FDP FTL을 확장해 FDP를 first-class·configurable·observable 설계축으로 만든 오픈 에뮬레이터. FEMU에 upstream됨 (github.com/MoatLab/FEMU).
>
> **세 가지 핵심 capability (p.8):**
> - **Isolation semantics**: II(모든 GC copy를 공유 GC-RUH로 redirect, 원래 tag 폐기)와 PI(per-RUH write pointer로 reclamation 후에도 RUH identity 보존) 양쪽을 명시적으로 구현.
> - **Configurable geometry & policies**: RU size(128/256/512MB), OP ratio(1~28%), RUH count, GC heuristic, lazy threshold를 runtime knob으로 노출.
> - **Per-RUH observability**: host bytes / GC copy bytes / remapped blocks / allocations / evictions를 RUH별로, GC event별로 structured log 기록 → Noisy RUH·Save Sequential 같은 hidden dynamics 노출.
>
> **GC architecture (p.9):** 두 결정으로 일반화. (1) 어느 RUH를 reclaim할지(greedy = 최고 압력, 또는 pressure-based), (2) 그 RUH 내 어느 RU를 reclaim할지(greedy = valid page 최소, 또는 cost-benefit `u×age/(1−u)`). 추가로 Lazy GC(occupancy가 5~10% 임계 이하일 때까지 연기), background GC(약 90% allocation에서 trigger, foreground는 ~99%), block remapping(전부 invalid인 block은 migration 없이 destination RU로 재매핑).
>
> **Placement model (p.8):** 각 RUH는 II 시 host write가 RUH X로, GC copy는 dedicated GC-RUH로 흘러 legacy FTL 최소 변경. PI는 per-RUH write pointer로 host/GC 데이터를 엄격 분리하나 OP 공간을 fragment.

## 평가

> [!note]- 평가 토글
> **Testbed (p.4):** 상용 FDP SSD 2종 — SSD_A(7.68TB, U.3), SSD_B(3.84TB, E1.S), 둘 다 PCIe Gen5/NVMe 2.1, 8 RUH, peak ~5GB/s. Intel Xeon Gold 5416S, 500GB DRAM, Linux v6.8 + FDP I/O passthrough patches. WARP 기본: RU 256MB, OP 10%, lazy GC 5%, 8채널·8die/채널·4KB page.
>
> **Single-stream baseline (Fig.2, p.5):** 128KB uniform random write에서 baseline WAF가 vendor geometry로 고정 — SSD_A ≈ 2.0×, SSD_B ≈ 3.5×.
>
> **Three-stream (Fig.3, p.5–6):** SSD_A는 Zipfian(α=1.2/2.2)에서 near-1 WAF, 그러나 80/20 패턴에서 collapse. SSD_B는 전반적으로 높아 skewed에서도 1.3~1.5, 80/20에서 3.0 초과.
>
> **Worst-case uniform random invalidation (Fig.5, p.6):** SSD_A는 4× device capacity에서 **2.58×**, SSD_B는 **4.49×**(전체 연구 최고치). FDP가 marginal improvement만 제공.
>
> **CacheLib kvcache (Fig.6, p.6–7):** NoFDP는 20% SOC에서 1.64, 40%에서 1.85까지 상승(4.6TB 이상 내부 재기록). FDP는 20%에서 **WAF 1.08**(8% 초과 write)로 거의 1.0 유지. hit ratio는 양쪽 모두 보존(40% SOC에서 ~82%, Fig.8).
>
> **Noisy neighbor (Fig.9, p.7–8):** co-location에서 NoFDP는 WAF 1.28 → ~3.0(7배 증가). FDP는 최고 2.6으로 억제(baseline 이하).
>
> **F2FS (Fig.10–11, p.7–8):** FDP/NoFDP 모두 2.3~2.5 WAF(개선 없음). eBPF 추적 결과 user data의 **99%가 WRITE_LIFE_NOT_SET**으로 라벨링 → 사실상 단일 RUH로 collapse. OLTP는 TRIM 덕에 1.0.
>
> **WARP fidelity (Table 2, Fig.12, p.10–11):** WARP_2-7 구성이 128KB random write에서 2.0~3.5 범위를 span하며 두 상용 device를 재현. CacheLib kvcache에서 SSD_A 1.85 → 1.27(FDP), WARP가 2.00 → 1.37로 동일 방향 재현. 복합 workload에서 WAF를 실제 device와 0.2~0.3 이내로 일치(Observation #5).
>
> **II vs PI (Fig.18–19, p.12):** 256MB RU에서 crossover ≈ 7~9% OP — OP 5%에서 II 2.187 < PI 2.365, OP 10%에서 II 1.338 vs PI 1.181(PI 우세). 128MB RU는 crossover가 더 높게 이동(작은 RU일수록 PI에 더 큰 OP 필요).
>
> **Per-RUH breakdown (Fig.16–17, p.11–12):** 80/20에서 RUH 0이 트래픽의 88%(4.42× capacity). RUH 1 기여 0.131(26%), RUH 2 기여 0.070(14%) — RUH 2의 invalidation이 RUH 1 WAF를 3.8×까지 증폭.
>
> **WARP-guided 최적화 (Fig.20, p.12–13):** small RU(single-channel-mapped) 적용으로 kvcache 40% SOC에서 WAF 1.37 → **1.16**.
>
> **Latency (Table 3, p.13):** 4KB QD=1 random read에서 WARP 335K IOPS vs SSD_A 460K. median latency 동일(70μs), p99.999 tail은 457μs vs 967μs로 현실적.

## 섹션 노트
- **§1 Introduction**: WAF의 비용·수명·탄소 영향, hyperscaler(Google/Meta)의 FDP 채택, 세 가지 핵심 질문 제시.
- **§2 Background & Motivation**: FDP primer(RU/RG/RUH 추상화), II vs PI, OpenChannel/ZNS/multi-stream 대비 FDP의 middle ground 위치, WAF의 workload-dependent 특성.
- **§3 Characterization**: testbed, microbenchmark(single/two/three-stream), CacheLib(kvcache/cdn/twitter, multi-tenant), F2FS(Fileserver/OLTP). Observation #1~#4.
- **§4 WARP Design**: design contract, interface/placement model, GC architecture, configurable geometry, observability/calibration, FEMU 확장.
- **§5 Evaluation**: WARP fidelity 검증, three-stream 분석(Noisy RUH/Save Sequential), II vs PI OP tradeoff. Observation #5~#8.
- **§6 WARP Guided Optimization**: small RU WAF 최적화, latency 최적화(VM-exit/doorbell 제거).
- **§7 Use Cases**: hardware 너머 정책 탐구, cross-layer co-design, multi-tenancy.

## 핵심 용어
- **FDP (Flexible Data Placement)**: NVMe TP4146으로 표준화된 best-effort placement interface. host가 hint를 주되 GC는 device 관리, block interface 호환.
- **RU (Reclaim Unit)**: GC granularity, 보통 NAND superblock으로 구성.
- **RG (Reclaim Group)**: 함께 관리되는 RU 집합, 보통 NAND die별 그룹화.
- **RUH (Reclaim Unit Handle)**: host write를 특정 RU로 steer하는 logical identifier.
- **WAF (Write Amplification Factor)**: GC로 인한 flash 초과 write. `rHMW = Host Media Written / Device Capacity` 축으로 표현.
- **II (Initially Isolated)**: GC copy를 공유 GC-RUH로 co-locate(원래 tag 폐기). legacy FTL 최소 변경, 상용 device의 초기 설계.
- **PI (Persistently Isolated)**: reclamation 후에도 RUH identity 보존(per-RUH write pointer). 엄격 격리하나 OP fragment.
- **Noisy RUH**: 한 handle의 invalidation이 GC 압력을 통해 다른 handle의 WAF를 증폭하는 현상.
- **Save Sequential**: capacity-dominant sequential stream이 조기 reclaim되어 device-friendly 특성을 잃는 현상.
- **OP (Over-Provisioning)**: GC를 위한 예비 용량 비율. II/PI tradeoff의 핵심 변수.
- **WARP**: Write Amplification Research Platform. FEMU 기반 오픈 FDP 에뮬레이터.

## 강점 · 한계 · 열린 질문
- **강점**: 최초의 cross-device/cross-workload FDP 특성화 + 오픈 에뮬레이터. 미보고 현상(Noisy RUH, Save Sequential) 발견·기전 설명. FEMU upstream으로 재현성·확장성 확보. fidelity를 실제 device와 0.2~0.3 WAF 이내로 검증, latency까지 현실적.
- **한계**: 상용 device 단 2종, RUH 8개로 제한된 표본. 실제 device의 II semantics만 노출되어 PI는 에뮬레이터로만 검증. F2FS 결과는 OS-level tagging 미성숙에 의존(99% NOT_SET).
- **열린 질문**: adaptive RU sizing(dynamic), OP에 따른 II↔PI 동적 전환, FDP-aware request scheduler, file system의 정확한 lifetime classification 방법은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. FDP가 약속한 near-1 WAF가 실패하는 세 가지 조건은?
> > 답: (1) misclassification(workload lifetime이 RUH isolation과 불일치), (2) RUH interference / co-located traffic, (3) adversarial(uniform random) invalidation. 이 경우 FDP는 conventional SSD 동작으로 퇴화한다.

> [!question]- Q2. Noisy RUH와 Save Sequential은 각각 무엇인가?
> > 답: Noisy RUH는 한 handle의 invalidation이 GC 압력을 통해 다른 handle의 WAF를 증폭하는 현상이고, Save Sequential은 capacity-dominant sequential stream이 device 내부 GC 정책과 충돌해 조기 reclaim되어 device-friendly 특성을 잃는 현상이다. 둘 다 실제 device에서도 관찰됨(Fig.3c&f).

> [!question]- Q3. WARP는 무엇을 기반으로 하며 핵심 기여 capability 셋은?
> > 답: FEMU의 pre-FDP FTL을 확장. (1) II/PI isolation semantics 명시 구현, (2) RU size·OP·RUH count·GC heuristic의 configurable geometry, (3) per-RUH observability(host/GC bytes, GC event log).

> [!question]- Q4. II와 PI 중 어느 것이 언제 유리한가?
> > 답: PI는 OP가 충분할 때(256MB RU 기준 ~7~9% 초과)만 II를 이긴다. OP가 부족하면 PI는 per-RUH 격리가 OP를 fragment해 fragmentation penalty로 더 나빠진다. II는 GC-RUH로 GC 트래픽을 흡수해 scarce OP·heterogeneous workload에서 robust하다.

> [!question]- Q5. F2FS에서 FDP가 효과 없는 이유는?
> > 답: eBPF 추적 결과 user data의 99%가 WRITE_LIFE_NOT_SET으로 라벨링되어 generic NoFDP 동작에 매핑, 사실상 단일 RUH로 collapse한다. node/metadata segment는 분리하지만 user data 분류가 불충분.

> [!question]- Q6. Worst-case(uniform random invalidation)에서 두 device의 WAF는?
> > 답: SSD_A 2.58×, SSD_B 4.49×(연구 내 최고치). 이 adversarial workload에서 FDP의 이점은 거의 사라지고 conventional SSD로 수렴(Fig.5).

> [!question]- Q7. WARP-guided 최적화의 핵심 아이디어와 효과는?
> > 답: small RU(single-channel-mapped)로 RUH 0의 write를 한 NAND block에 완전히 채운 뒤 다음 channel로 이동. GC overhead 감소 + small-object cache를 단일 channel로 throttle해 Noisy RUH 억제. kvcache 40% SOC에서 WAF 1.37 → 1.16.

> [!question]- Q8. WARP의 fidelity는 어떻게 검증되었나?
> > 답: 128KB random write에서 WARP_2-7 구성이 2.0~3.5(두 device span) 재현, CacheLib kvcache에서 실제 device의 방향(1.85→1.27)을 WARP가 2.00→1.37로 재현, 복합 skewed/multi-tenant workload에서 0.2~0.3 WAF 이내 일치. latency도 median 동일·tail 현실적(Table 3).

## 🔗 Connections
[[File System]] · [[FAST]] · [[2026]]

## References worth following
- **[17] Allison et al., "Towards Efficient Flash Caches with Emerging NVMe FDP SSDs", EuroSys 2025** — FDP를 CacheLib에 통합한 선행 연구, 본 논문 trace의 기반.
- **[53] Li et al., "The CASE of FEMU", FAST 2017** — WARP가 확장한 에뮬레이터.
- **[16] Bjørling et al., "ZNS: Avoiding the Block Interface Tax", ATC 2021** — FDP와 대비되는 host-managed interface.
- **[15] Bjørling et al., "LightNVM: OpenChannel SSD", FAST 2017** — placement/GC 모두 host 위임 설계.
- **[11] Allison, "Flexible Data Placement — Specification Perspective"** — NVMe TP4146 사양 배경.
- **[23] Lee et al., "F2FS", FAST 2015** — §3.4 F2FS 평가의 data classification 기준.

## Personal annotations
<본인 메모 영역>
