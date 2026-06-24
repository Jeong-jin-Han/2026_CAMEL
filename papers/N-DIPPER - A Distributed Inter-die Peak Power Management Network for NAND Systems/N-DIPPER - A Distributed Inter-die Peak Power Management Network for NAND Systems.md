---
title: "N-DIPPER: A Distributed Inter-die Peak Power Management Network for NAND Systems"
aliases: [N-DIPPER]
description: "다이 간 경량 패브릭과 토큰 기반 스케줄링으로 NAND 패키지의 peak-current 위반을 제거하면서 성능 손실을 최소화하는 분산 전력관리 구조"
venue: HPCA
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/reliability
  - topic/nand
  - topic/power-management
  - venue/hpca
  - year/2026
---

# N-DIPPER: A Distributed Inter-die Peak Power Management Network for NAND Systems

> **HPCA 2026** · `cluster/reliability` · Source: [N-DIPPER - A Distributed Inter-die Peak Power Management Network for NAND Systems.pdf](<N-DIPPER - A Distributed Inter-die Peak Power Management Network for NAND Systems.pdf>)

저자: Jinwoo Park, John Kim (KAIST)

## TL;DR
3D NAND이 고층화·고전압화되면서 여러 die의 high-current(HC) phase가 겹쳐 PMIC의 peak-current limit을 위반하고 $V_{CC}$ droop·신뢰성 저하를 일으킨다. N-DIPPER는 패키지 내부에 경량 die-to-die 패브릭(예: unidirectional ring)을 두고, 단일 token을 순환시켜 어느 die가 HC phase에 진입할지를 분산 스케줄링하며, eFuse 기반의 device-aware 정보로 PVT·wear에 따른 peak-current를 정확히 추정한다. 결과적으로 peak-current 위반을 100% 제거하면서 baseline 대비 throughput의 97%를 유지한다(약 4% 성능 저하).

## 문제 & 동기
- NAND이 word-line(WL) stack을 쌓고 program 전압을 키우면서 die당 peak current가 지난 10년간 30배 이상 증가했고, 패키지 내 여러 die의 charge pump가 동시에 동작하면 전류 spike가 중첩되어 PMIC의 current limit을 초과한다.
- 한 공유 power rail을 나누는 die들의 전류 합이 예산을 넘으면 $V_{CC}$ droop이 발생해 charge pump의 구동 능력이 떨어지고, $V_{prog}$/$V_{pass}$의 상승 기울기가 둔화되어 WL이 제때 목표 전압에 도달하지 못한다 → $V_{th}$ 분포 확대, read error, 최악의 경우 brownout/NAND reset.
- 기존 정적(static) 기법은 (1) controller throttling이나 (2) slope control(waveform shaping)인데, PVT(Process/Voltage/Temperature) 변동과 wear로 인한 비결정적 inter-die timing mismatch와 peak-current magnitude 변동을 포착하지 못해 보수적 guardband를 둘 수밖에 없고, 이는 die-level parallelism과 성능을 제한한다.

> [!quote]- 📄 원문 표현 (paper)
> "Such overlaps cause power-management-IC (PMIC) limit violations, voltage droops, and reliability degradation. Traditional static solutions – such as controller throttling or slope control – cannot fully address these issues because dynamic process, voltage, and temperature (PVT) variations and wear out-induced drift cause both nondeterministic inter-die timing mismatches and peak-current magnitude fluctuations, forcing conservative guardbands that limit system parallelism and performance." (p.1, Abstract)
>
> "Exceeding the PMIC limit induces a $V_{CC}$ droop on the shared supply rail [1], [23] which compromises the driving capability of internal charge pumps, reducing the rising slope of $V_{prog}$ and $V_{pass}$ ... Under severe overlap, $V_{CC}$ may fall below the internal reference $V_{REF}$, triggering an on-die brownout/reset operation [11]." (p.4)

## 핵심 통찰

> [!note]- 토글: 핵심 통찰
> - **두 가지 drift 모드를 구분한다.** (1) *Inter-die Timing Drift*(수평 이동): HC phase가 *언제* 발생하는지가 PVT·wear로 die마다 어긋난다. selective-verify 정책으로 slow die와 fast die의 verify 횟수가 달라지며 loop마다 LC phase imbalance가 누적되어 HC가 결국 겹친다. (2) *Peak-Current Magnitude Drift*(수직 이동): HC phase가 *얼마나* 많은 전류를 끄는지가 wear·온도로 변한다(예: die당 peak이 200mA에서 220mA까지 상승).
> - **정적 관리의 보수성은 표현(representation)에서도 증폭된다.** 토큰을 "전류 demand"로 표현하는 선행연구(MTPM)는 coarse quantization으로 rounding-up하여 실제 전류 합이 예산 안에 있어도 조기 throttling을 유발하고, 병렬 die 수 확장을 제한한다(32-die에서 정적 약 18 die vs 동적 최대 24 die).
> - **핵심 발상의 전환: 토큰을 "전류"가 아니라 "스케줄링 권한(HC 진입 권한)"으로 쓴다.** 단일 global token이 패브릭을 순환하며 한 번에 하나의 die만 HC phase에 진입하도록 직렬화한다. 토큰은 HC 동안 *보유되지 않으므로* 안전한 overlap은 허용되어 die 간 동시 실행이 가능하다(on-demand 전류관리가 아님).
> - **device-aware 추정**: eFuse에 저장된 per-chip nominal peak bound와 PVT/wear offset(PPOT table)을 이용해 worst-case guardband 없이 실제 peak current를 정밀 추정한다.

## 설계 / 메커니즘

> [!note]- 토글: 설계 / 메커니즘
> **(1) Die-to-Die Fabric**
> - NAND 패키지에는 원래 die 간 직접 통신이 없으므로, 매우 low-radix topology(unidirectional ring 등)의 경량 in-package 패브릭을 추가하고 NAND datapath를 재사용한다. 두 종류 패킷만 지원: **token packet**과 **metadata packet**(1-bit header로 구분, Fig. 9).
> - 일반 flow control 불필요(in-flight 패킷이 token 하나뿐). 확장성을 위해 패키지를 $K$ subgroup으로 나누는 multi-token 확장 제공 — 각 subgroup은 독립 패브릭과 $I_{PKG}/K$ 예산을 갖는다(Fig. 8c).
>
> **(2) Token-based Scheduling**
> - 단일 token이 die들을 순환. die는 token을 잡아 자신의 HC 진입 가능 여부를 판단하고, 진입 시 전체 die의 peak current 합 $I_{sum}$을 PMIC limit $I_{pkg}$와 비교. token packet은 TK=1로만 전송(TK=0은 전송 안 함).
>
> **(3) Metadata**
> - token을 얻어 HC 진입이 승인되면 metadata packet을 다른 모든 die에 broadcast하여 현재 스케줄된 HC phase 정보를 공유. 전체 peak offset 값(약 300mA에 1mA 해상도면 9-bit 필요)을 보내는 대신, HC phase 종류 등 이산 정보(source ID, HC type, P/T/W 상태)를 전송해 원격 die가 로컬 look-up으로 peak current를 추정.
>
> **(4) Microarchitecture (Fig. 10, die당 N-DIPPER controller)**
> - **eFuses**: `eFuse.I_p[k]`($\mu+3\sigma$ baseline peak 상한, die-common, wafer final test 시 프로그램), `eFuse.t_HC[k]`(HC phase 지속시간). HC Base eFuse(magnitude 1mA/LSB, duration 80ns/LSB), P/T/W Offset eFuse(135 entries = 5×3×3×3, 8-bit signed, 1mA/LSB) — Table I.
> - **PCSR (Per-Die Peak Current Status Registers)**: 다른 $N-1$ die의 현재 peak 사용량 $I_p$와 잔여 HC 시간 $t_{HC}$를 로컬에 유지(register, 매 cycle 갱신).
> - **PPOT (PVT-Wear Peak-Current Offset Table)**: device 상태(P=process corner, T=temperature, W=wear state; Table II)로 인덱싱되는 signed offset lookup table(eFuse로 한 번만 프로그램).
>
> **(5) Controller 동작 (Algorithm 1, 3 stage)**
> - *Stage 1 — token 처리*: HC 요청이 있으면 local state로 PPOT 조회 → $I_{p\_sum}=\sum PCSR[k].I_p$ 계산 → `I_{p_sum} + eFuse.I_p[k] + PPOT[sel] > I_{pkg}`이면 stall(예산 확보까지 대기), 아니면 metadata broadcast(blocking) 후 HC 시작(non-blocking), token을 이웃으로 전달.
> - *Stage 2 — metadata 처리*: 수신 metadata로 `PCSR[src]`를 nominal peak + offset, 지속시간으로 갱신 후 forward.
> - *Stage 3 — PCSR 갱신*: 각 die의 $t_{HC}$를 매 cycle 감소시키고 0이 되면 해당 die의 peak 기여를 0으로 reset.

## 평가

> [!note]- 토글: 평가
> **방법론** (p.8): custom NAND device-level simulator + SimpleSSD 아키텍처 시뮬레이터. SK Hynix 176-layer TLC NAND 기반($t_{PROG}$=425$\mu$s). 8/16/32 die 패키지, PMIC budget = 1.2A/2.4A/4.8A(nominal HC peak 200mA에 비례). PCIe 4.0 ×4, 8 channels, 4 ways. Workload: synthetic sequential write(R100~R0), 실트레이스 Web1/TPC-C/MSR1(SNIA). 비교군: Baseline(관리 없음), SCT(SSD Controller Throttling), DWS(Device Waveform Shaping), N-DIPPER(ring/bi-ring/mesh, single/multi-token).
>
> **핵심 결과**
> - **peak-current 위반 100% 제거**, baseline throughput의 **97% 유지**(약 4% 성능 저하) — Abstract, p.2. PPOT(전류 offset)를 쓰지 않으면 slowdown이 약 7%로 증가(p.10).
> - **$t_{PROG}$ overhead (Fig. 12)**: write-only sequential에서 N-DIPPER(uni-ring)는 8/16/32 die에서 445/465/511$\mu$s (nominal 426 대비). bi-ring 439/452/472, **mesh 434/439/445**로 mesh가 hop count를 줄여 $T_{meta}$ 감소. baseline DWS는 520$\mu$s(고정 overhead). SCT는 약 23%, DWS는 약 20% 성능 저하 vs N-DIPPER 약 4%.
> - **read/write mix (Fig. 12b)**: read 비중이 늘수록(R20→R50→R80) overhead 감소 — read는 전류 spike가 작아 $T_{tk}$(token waiting) 단축.
> - **multi-token 확장성 (Fig. 13)**: 32-die unidirectional ring에서 single token 511$\mu$s → multi-token 446$\mu$s로 $t_{PROG}$ 15% 감소. multi-token으로 topology 간 격차도 15%→3%로 축소.
> - **PMIC provision factor $\alpha$ (Fig. 14)**: 타이트한 예산(낮은 $\alpha$)에서 DWS는 HC peak를 공격적으로 clip해 $t_{PROG}$가 40~50% 증가; N-DIPPER는 모든 $\alpha$에서 DWS보다 낮음. $\alpha=1$이면 DWS는 overhead 없음(N-DIPPER가 약 5% 높음).
> - **End-to-end IOPS (Fig. 15)**: N-DIPPER throughput은 baseline의 3% 이내. read-intensive(R100/Web1/TPC-C)는 영향 미미. write-intensive에서 SCT/DWS는 최대 22%(19%) 저하; N-DIPPER는 mesh/multi-token으로 SCT(DWS) 대비 최대 16%(22%) 높은 throughput.
> - **HW overhead (Table IV)**: 45nm 합성 기준 controller 면적 총 **0.7456 mm²**(PCSR register 0.6, PPOT eFuse 0.1161, Tx/Rx 0.0192, logic 0.0017). die 면적 88.6mm² 대비 약 **0.84%**(보수적 상한).

## 섹션 노트
- **I. Introduction**: scaling으로 peak current 30배↑, 동시 HC overlap이 PMIC limit 초과·$V_{CC}$ droop·신뢰성 위협. 기여 3가지(두 drift 모드 식별, 경량 in-package 관리 구조, multi-token 확장).
- **II. Background**: NAND $V_{th}$ 분포·read reference, SSD/PMIC 아키텍처(공유 rail), program/read/erase의 HC/LC phase(Fig. 4). program은 BL/WL setup에서 HC, erase는 channel 일정 bias로 peak가 WL 수에 둔감.
- **III. Motivation**: (A) Inter-die Timing Drift(selective-verify로 slow/fast die의 verify-time 불균형 누적, Fig. 5), (B) Peak-Current Magnitude Drift(wear/온도로 magnitude 변동, Fig. 7). 정적 기법의 보수성과 확장성 한계.
- **IV. Architecture**: fabric/token/metadata/microarch(eFuse·PCSR·PPOT)·Algorithm 1·multi-token.
- **V. Evaluation**: peak 감소, $t_{PROG}$ overhead, multi-token, $\alpha$ 영향, end-to-end, HW overhead.
- **VI. Discussion**: in-package 한정(inter-package는 future work), multi-token의 load imbalance(정적 분할로 한 domain 과부하 가능), single token의 dual role(grant+broadcast) 분리 가능성. Related work(peak current management, SSD interconnect, token 개념, MTPM 대비 fine-grain).

## 핵심 용어
- **Peak current / HC phase**: charge pump가 WL/BL을 목표 전압으로 충전할 때 발생하는 짧고 강한 전류 spike(high-current). 정상상태 bias-hold/sensing은 LC(low-current).
- **PMIC limit ($I_{pkg}$)**: 패키지가 공유 rail에서 끌 수 있는 peak current 상한. 초과 시 $V_{CC}$ droop 발생.
- **$V_{CC}$ droop / brownout**: 공유 공급 전압이 보호 임계 아래로 떨어져 voltage-monitor가 reset/abort를 trigger하는 현상.
- **Inter-die Timing Drift**: HC phase가 *언제* 발생하는지가 die마다 어긋나는 수평 drift(PVT·wear·selective-verify 기인).
- **Peak-Current Magnitude Drift**: HC phase가 *얼마나* 많은 전류를 끄는지가 변하는 수직 drift(wear·온도 기인).
- **Token-based scheduling**: 단일 token을 순환시켜 한 번에 하나의 die만 HC 진입을 직렬화하되, HC 동안 token을 보유하지 않아 안전한 overlap을 허용하는 분산 스케줄링.
- **eFuse**: NAND peripheral의 일회성 프로그래머블 비휘발 비트. 작은 read-only 메모리처럼 동작하며 device-specific 파라미터 저장.
- **PCSR / PPOT**: 각각 다른 die의 현재 peak·잔여시간을 담는 register, PVT-wear offset lookup table(eFuse).
- **Selective-verify**: $t_{PROG}$ 단축을 위해 fast die가 특정 state(예: P1)의 verify를 건너뛰는 정책. die 간 verify-time 불균형의 원인.
- **DWS / SCT / MTPM**: 비교군 — Device Waveform Shaping(slew-rate throttling), SSD Controller Throttling(병렬 op 제한), Multi-Token Power Management(전류 demand를 토큰화한 선행연구).

## 강점 · 한계 · 열린 질문
- **강점**: peak-current 위반을 완전히 제거하면서 성능 손실 약 4%로 최소화; 패키지 내부 분산 처리라 SSD controller 개입 불필요; device-aware(eFuse/PPOT) 추정으로 worst-case guardband 회피; multi-token으로 die 수 확장에도 latency overhead 완화; HW overhead 약 0.84%로 낮음.
- **한계(저자 인정)**: in-package(공유 rail) 한정 — inter-package peak 관리는 hierarchical로 가정만 하고 미해결; multi-token의 정적 분할은 workload skew 시 한 domain 과부하/다른 domain 미활용의 load imbalance 유발; single token이 grant와 metadata broadcast의 dual role을 맡아 $T_{tk}$/$T_{meta}$ 증가 가능.
- **열린 질문**: 동적 peak-current budget 재할당으로 multi-token imbalance를 풀 수 있는가? token 분리(grant용/broadcast용) 시 실제 이득은? 실측 silicon에서 PPOT offset의 정확도와 eFuse 프로그램 비용은? mesh의 추가 Tx/Rx port·in-package routing 비용 대비 이득의 손익분기는?

## ❓ Q&A (자가 점검)

> [!question]- Q1. N-DIPPER가 해결하는 근본 문제는 무엇인가?
> > 고밀도 3D NAND 패키지에서 여러 die의 high-current(HC) phase가 중첩되어 PMIC peak-current limit을 위반하고, 이로 인한 $V_{CC}$ droop이 charge pump 구동 능력을 떨어뜨려 신뢰성 저하·brownout을 일으키는 문제. 기존 정적 기법은 PVT·wear 변동을 못 잡아 보수적 guardband로 병렬성을 희생한다.

> [!question]- Q2. 토큰을 "전류 demand"로 쓴 선행연구(MTPM)와 N-DIPPER의 토큰 사용은 어떻게 다른가?
> > MTPM은 토큰이 사전 정의된 전류량을 나타내고 HC 진입 시 여러 토큰을 잡아야 하는 on-demand·coarse-grain 방식이라 rounding-up으로 조기 throttling을 유발한다. N-DIPPER는 토큰을 "HC 진입 스케줄링 권한"으로 쓰고 HC 동안 토큰을 보유하지 않아 안전한 overlap을 허용하며, device-aware 추정으로 fine-grain하다.

> [!question]- Q3. 두 가지 drift 모드는 각각 무엇이고 무엇이 원인인가?
> > Inter-die Timing Drift(HC가 *언제* 발생하는지의 수평 어긋남, selective-verify로 slow/fast die의 verify-time 불균형 누적)와 Peak-Current Magnitude Drift(HC가 *얼마나* 전류를 끄는지의 수직 변동, wear·온도 기인, 예: 200mA→220mA).

> [!question]- Q4. die가 HC phase 진입을 승인받는 판정식은 무엇인가?
> > token 보유 시 PPOT로 offset을 조회하고 $I_{p\_sum}=\sum PCSR[k].I_p$를 더한 뒤, `I_{p_sum} + eFuse.I_p[k] + PPOT[sel] > I_{pkg}`이면 stall(예산 확보까지 대기), 아니면 metadata broadcast 후 HC 시작하고 token을 이웃으로 전달한다(Algorithm 1).

> [!question]- Q5. metadata에 full peak offset 대신 이산 정보를 보내는 이유는?
> > 약 300mA를 1mA 해상도로 표현하면 9-bit가 필요해 fabric 부담이 크다. 대신 source ID·HC type·P/T/W 상태 같은 이산 정보를 보내면 원격 die가 로컬 PPOT look-up으로 peak를 추정할 수 있어 패킷이 경량화된다.

> [!question]- Q6. multi-token 확장은 무엇을 개선하고 어떤 trade-off가 있는가?
> > 패키지를 $K$ subgroup으로 나눠 독립 패브릭으로 hop count를 $1/K$로 줄여 latency overhead를 완화한다(32-die ring $t_{PROG}$ 511→446$\mu$s, 15%↓). 단 $I_{pkg}/K$로 정적 분할하므로 workload가 한 domain에 몰리면 load imbalance로 불필요한 stall이 생긴다.

> [!question]- Q7. SCT·DWS 대비 N-DIPPER의 성능 우위는 수치로 얼마인가?
> > write-only에서 SCT 약 23%, DWS 약 20% 저하 vs N-DIPPER 약 4%. end-to-end write-intensive에서 N-DIPPER는 SCT 대비 최대 16%, DWS 대비 최대 22% 높은 throughput을 mesh/multi-token으로 달성하며, baseline의 3% 이내를 유지한다.

> [!question]- Q8. HW overhead는 얼마이고 무엇이 지배적인가?
> > 45nm 합성 기준 controller 총 0.7456 mm²로, die 면적 88.6mm² 대비 약 0.84%. 대부분은 다른 die 상태를 담는 PCSR register(0.6 mm²)이고 PPOT eFuse(0.1161), Tx/Rx pad(0.0192), logic(0.0017)이 뒤따른다.

## 🔗 Connections
[[Reliability]] · [[HPCA]] · [[2026]]

## References worth following
- [11] N. Sudo, "Power down detection circuit and semiconductor memory device," US Patent 2022 — $V_{CC}$ droop/brownout 감지·reset 메커니즘의 근거.
- [21] T. You et al., "Multitoken-based power management for nand flash storage systems," IEEE TCAD 39(11), 2020 — 본 논문이 직접 비교하는 token 기반 선행연구(MTPM).
- [10] K.-S. Yoon et al., "Fully integrated digitally assisted low-dropout regulator for a nand flash memory system," IEEE TPELS 2018 — DWS(charge-pump slew-rate throttling) 비교군의 device-level 근거.
- [43] M. Jung et al., "SimpleSSD: Modeling solid state drives for holistic system simulation," IEEE CAL — 평가에 사용한 아키텍처 시뮬레이터.
- [29] J. K. Park, S. E. Kim, "A review of cell operation algorithm for 3d nand flash memory," 2022 — selective-verify·LC phase imbalance 등 verify-time variation 설명의 근거.
- [38] W. J. Dally, B. Towles, *Principles and Practices of Interconnection Networks* — die-to-die fabric topology 설계 배경.

## Personal annotations
<본인 메모 영역>
