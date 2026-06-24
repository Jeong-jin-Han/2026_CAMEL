---
title: "Ares-Flash: Efficient Parallel Integer Arithmetic Operations Using NAND Flash Memory"
aliases: [Ares-Flash]
description: "페이지 버퍼 latch만 미세 수정해 NAND flash 내부에서 정수 덧셈·누산·곱셈을 BL 병렬로 수행하는 IFP 구조(0.84% 면적 오버헤드)."
venue: MICRO
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/in-flash
  - topic/arithmetic
  - venue/micro
  - year/2024
---

# Ares-Flash: Efficient Parallel Integer Arithmetic Operations Using NAND Flash Memory

> **MICRO 2024** · `cluster/isc` · Source: [Ares-Flash - Efficient Parallel Integer Arithmetic Operations Using NAND Flash Memory.pdf](<Ares-Flash - Efficient Parallel Integer Arithmetic Operations Using NAND Flash Memory.pdf>)

저자: Jian Chen (Tsinghua), Congming Gao (Xiamen, 교신), Youyou Lu (Tsinghua), Yuhao Zhang (Tsinghua), Jiwu Shu (Tsinghua, 교신)

## TL;DR
Ares-Flash(Ares)는 3D NAND flash의 **page buffer latch 회로를 최소한으로 수정**해 flash 매체 내부에서 정수 산술(덧셈·누산·곱셈)을 수행하는 **In-Flash Processing(IFP)** 구조다. ADC 같은 별도 연산 하드웨어 없이 BL-level 병렬성을 살리면서, 기존 bitwise 전용 IFP(ParaBit, Flash-Cosmos)가 못 하던 N-bit full adder를 page buffer 안에서 구성하고, **intra-plane transmission peripheral**로 internal bus 없이 carry/bit-shift를 전달한다. 면적 오버헤드는 **0.68~0.84%**에 불과하며, 누산에서 ISP 대비 평균 **8.2×/4.5×**, OSP/ISP 대비 **89×/13.2×** 성능, VVM에서 OSP/ISP 대비 **2.6×/1.78×** 향상을 보인다.

## 문제 & 동기
데이터 집약 응용(데이터베이스, 대규모 이미지 처리, NN)에서 덧셈·곱셈은 기본 연산이지만, 전통 구조에서는 storage↔memory↔compute 사이 데이터 이동이 병목이다. NDP 중 **IFP는 flash 매체 가장 아래에서 연산을 시작**해 PIM/ISP보다 데이터 이동을 더 줄이지만, 기존 IFP는 (1) ADC 등 비싼 추가 하드웨어로 면적/전력 오버헤드가 크거나(예: [66]은 137% 추가 면적, 253× 전력), (2) ParaBit/Flash-Cosmos처럼 추가 하드웨어 없이도 **bitwise 연산 수준에 머물러** 복잡한 산술 구성이 비효율적이다. 핵심 난제 두 가지: **Efficient Adder Logic**(모든 BL이 동시에 같은 연산을 하지만 비트별 carry 의존성이 순차적), **Transfer-free Bit-Shift**(internal bus 없이 shift 수행).

> [!quote]- 📄 원문 표현 (paper)
> "Distinguished from processing-in-memory (PIM) and in-storage-processing (ISP), IFP reduces the data movement starting from the most bottom flash memory medium." (p.1, Abstract)
>
> "But their accomplishments still remain at the level of simple bitwise operations, and thus it prevents their work from being widely adopted." (p.1, Abstract)
>
> "we have to mainly tackle two challenges: (1) Efficient Adder Logic ... (2) Transfer-free Bit-Shift: Implementing bit-shifts in both addition and multiplication without using the internal bus for data transfer is a significant hurdle." (p.2)

## 핵심 통찰
> [!note]- 토글: 핵심 통찰
> - **Page buffer가 곧 연산 플랫폼**: 3D NAND의 page buffer는 BL마다 5개의 latch(현대 TLC/QLC는 BL당 5~6 latch 지원)를 갖고, BL-level 병렬성과 충분한 연산 공간을 제공한다. 여기에 latch 회로를 비침습적으로(read/program 기능 유지) 수정해 4종의 유연한 bitwise 연산(AND, in-place AND, OR, in-place OR + XOR/NAND/NOR 파생)을 만든다. (p.2, p.4)
> - **연산을 sensing 단계가 아닌 latch 내부에서 수행**: ParaBit/Flash-Cosmos는 sensing 과정 중에 bitwise를 처리해 중간 결과를 cell에 다시 쓰고 여러 번 read/program이 필요한 반면, Ares는 모든 계산을 page buffer 안에서 수행해 read/program/transfer 횟수를 줄인다. (p.2)
> - **Carry 의존성 회피 재배열**: N-bit 덧셈에서 carry-in이 디지트 간 순차 의존을 만드는데, Ares는 **모든 디지트의 carry 값을 먼저 계산**한 뒤 sum을 계산하도록 단계를 재배열한다. 중간값 t₁, t₂(AND/XOR 부분)는 carry 계산과 독립이므로 latch에 보관해 재사용한다. (p.6, Fig.10)
> - **Bit-shift를 transmission peripheral로 해결**: lower digit의 carry-out을 latch L4에 저장 후, 인접 BL의 L1 반전단에 2개 트랜지스터로 연결해 internal channel 없이 higher digit으로 전달한다. (p.6~7, Fig.11)
> - **곱셈 = shift + accumulate**: A는 flash에, B는 controller에 두고, B의 각 비트(LSD→MSD)에 따라 `shift`(b=0) 또는 `shift_and_add`(b=1) 명령을 내려 부분곱을 누산한다. (p.7~8, Fig.13)

## 설계 / 메커니즘
> [!note]- 토글: 설계 / 메커니즘
> **전체 구조 (Fig.5)**: page buffer(BL당 5 latch L1~L5) + transmission peripheral. 사용자는 NAND program 인터페이스로 데이터를 미리 저장하고, 연산 시 flash controller가 Ares operational command를 전송 → cell read 후 모든 연산이 page buffer 내부에서 수행 → 결과만 internal channel로 SSD controller에 전달. (p.4)
>
> **기본 latch 연산 (Fig.6, §IV-B)**: 상단 트랜지스터(M₁, M₄)는 SO→L1 설정, 하단(M₂, M₃)은 SO 기반 L1 설정 담당. M_pre로 SO를 high 초기화. 4종 연산:
> - AND: M₁,M₅ 동시 활성 → A,B 모두 1일 때만 SO high 유지, 결과를 free latch L3에 저장.
> - In-place AND: M₈,M₃ 순차 활성, 원래 latch에 결과 저장(공간 절약).
> - OR / In-place OR: M₄,M₈ 또는 M₁,M₆로 큰 값 선택. (p.4~5)
>
> **XOR (Fig.7)**: A⊕B = (¬A∧¬B)∧(A∨B). 먼저 A AND B를 free latch L3에, 다음 in-place OR을 L2에, 마지막 in-place AND(한 입력은 M9로 반전)로 결과를 L2에 저장. (p.5)
>
> **1-bit Full Adder (Fig.8)**: L1~L4 사용, 입력 x,y는 L1,L2. ①AND(t₁→L3) ②XOR(t₂→L2, 이전 AND 재사용) ③세번째 입력 z를 L1로, AND(t₃→L4) ④t₂ XOR z = Sum(→L2) ⑤t₁ OR t₃ = Carry(→L4). SPICE(Samsung 65nm CMOS)로 검증. (p.5~6, Fig.9)
>
> **N-bit Addition (Fig.10, §IV-D)**: 두 operand를 같은 WL, 같은 BL에 정렬. 단계 재배열로 모든 디지트의 carry를 먼저 계산 → sum 계산. 중간값 t₁,t₂는 L3,L2에 보관. intra-plane transmission peripheral(Fig.11)로 carry-out을 인접 BL로 전달.
>
> **Transmission control (Fig.11b)**: 4 BL마다 transmission 회로 → 4-bit 기본 지원. `shf`(4-bit), `EN_8`, `EN_16` enable 신호 조합으로 8/16/32-bit operand 지원(32-bit는 모든 enable 활성). 데이터 격리를 위해 인접 데이터 그룹 간 연결 차단. (p.6~7)
>
> **Accumulation (§IV-E)**: 첫 덧셈 결과를 L2에 두고 다음 operand를 L1로 read해 반복. M개 operand → M-1 덧셈 사이클.
>
> **Multiplication (§IV-F, Fig.13)**: A는 flash, B는 controller. B 비트가 0이면 `shift`(w/o ADD), 1이면 `shift_and_add`. 누산 결과는 free latch L5에 임시 저장, shift된 A는 L1에 복사. 채널 공유로 plane당 multiplier 1개 제약 → 칩 간 interleaved 전송으로 병렬화. (p.7~8)
>
> **Ares-V (Vertical layout, §IV-G)**: operand를 같은 BL에 vertical 분산, N-bit full adder를 반복. intra-die transmission(Fig.15)으로 divide-and-conquer 누산(plane 간 결과 전달, pipelining). die-level 병렬, 최대 256K operand 누산(16KB page). BL-level 병렬 극대화, carry 전파 중 idle 제거. (p.8~9)
>
> **시스템 지원 (§V)**: API 3종 — `Ares_write`(operand size/logical addr/layout/group_id 지정), `Ares_input`(곱셈용 operand), `Ares_read`(add/accumulate/mul/mul&add). NVMe 드라이버 확장(예약 공간에 메타 인코딩). flash command 5종: `init`, `add`, `shift`, `shift_and_add`, `end`(Fig.16). FTL은 group_id 기반으로 같은 plane에 데이터 배치, GC는 copy-back으로 동일 plane 유지. (p.9~10)
>
> **신뢰성 (§V-B)**: Flash-Cosmos의 **Enhanced-SLC-mode-Programming(ESP)** 채택, 두 전압 상태를 더 구분(Fig.17). 추가 reference voltage(V_ref2)로 margin 점검, idle 시 재프로그램. (p.10)

## 평가
> [!note]- 토글: 평가
> **방법론 (§VI-A, Table I)**: OSP(Intel Xeon Gold 5220 2.2GHz, 64GB DDR4), ISP(Zu3T FPGA 400MHz, 21.2Mb BRAM), Ares(SSD: 8채널, 채널당 8칩, 4 plane/칩, 16KB/page, tR=25µs, tPROG=200/700µs SLC/TLC). 시뮬레이터 MQSim + HSpice(트랜지스터 활성 0.4µs). 에너지: Intel RAPL, DRAMSim3, Samsung 990 Pro SSD 전력. (p.10~11)
>
> **면적 오버헤드 (§VI-G)**: latch당 추가 트랜지스터 4개(A, ¬A 각 2개), 트랜지스터당 0.4µm×0.4µm(65nm). page buffer 추가 면적 **0.34%~0.42%**, 배선 여유 포함 시 **0.68%~0.84%** (Abstract의 0.84%와 일치). 비교: [66] ADC는 137% 추가, [43] ICE는 1.6% 추가. (p.10, p.1)
>
> **Addition (Fig.18, §VI-B)**: page당 덧셈 latency — Ares-H는 32/16/8/4-bit에서 **129µs/65µs/33µs/17µs**, Ares-V는 모든 타입에서 **10µs**. ParaBit/Flash-Cosmos 대비 최소 **160×/125×** 가속. int32 덧셈 throughput: ParaBit/Flash-Cosmos는 OSP의 11%/13%, int8에서는 33%/37%로 OSP보다 느림(internal bus 의존, 40%+ latency). (p.12, p.4)
>
> **Vector Addition (Fig.19, §VI-C)**: Ares-H가 OSP 대비 **2.55×**, ISP 대비 **1.75×** 평균; Ares-V는 OSP/ISP 대비 **2.6×/1.78×**. 데이터 타입 작아질수록 Ares-H 이득 증가(int4). (p.12)
>
> **Accumulation (§VI-D)**: K-Means(Fig.20) — Ares-H가 OSP **18.58×**, ISP **9.89×**; Ares-V는 **13.58×/7.22×**. Page Rank(Fig.21, 실데이터 LJ/WK/FB9/FB11) — Ares-H가 OSP/ISP **12.47×/6.82×**, Ares-V **11.66×/6.4×**. 최악(LJ int32)에도 OSP/ISP 대비 **6×/3.29×** 향상. Abstract의 "8.2×/4.5×"(누산), "89×/13.2×"는 평균/대표 수치. (p.12~13)
>
> **Multiplication / VVM (Fig.22, §VI-E)**: VSS 사례(MS-Celeb 10M, ImageNET 14M, Celeb-500K 50M). int4에서 Ares-V가 OSP/ISP 대비 **2.74×/1.4×**, int16 Ares-H가 OSP **2.12×**·ISP **1.18×**. int8/int4에서 OSP/ISP **7×/3.84×**, **16×/8.79×** 평균. 칩당 병렬 128K VVM(int4)/64K(int8)/32K(int16). vector size 512 기준 VVM당 평균 0.24µs/1.26µs/8.68µs로 SOTA [43] 대비 **42×**. (p.12~13)
>
> **에너지 (§VI-F)**: Page Rank 누산(Fig.23) — Ares-H가 OSP/ISP 대비 **97.9×/14×**, Ares-V **92.36×/13.2×** 절감. VVM(Fig.24) — Ares-H가 OSP/ISP **89×/13.2×**, int4에서 최대 **220×/32.6×** 절감. (p.13)
>
> **데이터 압축 (§VI-H)**: Ares 실행시간이 OSP 전송시간의 절반 미만(최악 VVM 52%, 최선 10%, 누적 3.8%). OSP는 zstd 압축해도 decompression이 host CPU/메모리 부담. (p.13~14)

## 섹션 노트
- **§I Introduction**: IFP 동기, ParaBit/Flash-Cosmos 한계, 2대 난제(adder logic, transfer-free bit-shift), 기여 3가지(Ares 소개·0.84% 오버헤드, Ares-based SSD 설계·API, 실데이터 평가). (p.1~2)
- **§II Background**: 3D NAND 구조(channel/chip/die/plane 4단 병렬), read operation, page buffer/latch, PIM/ISP/IFP 비교(Fig.3 latency: IFP가 internal+external transfer 모두 감소). (p.2~3)
- **§III Motivation**: OSP/ISP/IFP 덧셈 latency 분석, 기존 IFP 한계(hardware-mod IFP는 면적/전력 과다, intrinsic IFP는 bitwise 한정). (p.3~4)
- **§IV Design**: 본 노트 "설계/메커니즘" 참조. (p.4~9)
- **§V Implementation**: API, NVMe, SSD 수정(FTL/flash command), standard read, GC, 신뢰성(ESP). (p.9~10)
- **§VI Evaluation**: 면적·덧셈·누산·곱셈·에너지·압축·배포 시나리오. (p.10~14)
- **§VII Related Works / §VIII Limitation / §IX Conclusion**: ICE [43] 비교, floating-point 미지원(전처리로 일부 가능), 암호화는 FHE 필요. (p.14~15)

## 핵심 용어
- **IFP (In-Flash Processing)**: flash 매체 내부에서 데이터 전송 전에 연산을 수행하는 NDP 기법. PIM/ISP보다 데이터 이동을 더 줄임.
- **Page buffer / latch**: BL마다 sensing 결과를 저장하는 회로. 현대 TLC/QLC는 BL당 5~6 latch. Ares의 연산 플랫폼.
- **BL-level parallelism**: 한 WL의 모든 bitline(BL)이 동시에 같은 연산 수행 → 대규모 병렬.
- **Intra-plane transmission peripheral**: internal bus 없이 lower→higher digit으로 carry/shift를 전달하는 추가 회로(Fig.11).
- **Ares-H / Ares-V**: 데이터를 수평(WL 방향) vs 수직(BL 공유) 배치. Ares-V는 BL 병렬 극대화·pipelining.
- **ESP (Enhanced-SLC-mode-Programming)**: Flash-Cosmos가 제안한 신뢰성 향상 프로그래밍(전압 상태 구분 강화).
- **OSP / ISP**: Outside Storage Processing(host CPU 등) / In-Storage Processing(SSD controller·FPGA).

## 강점 · 한계 · 열린 질문
- **강점**: 추가 연산 하드웨어 없이 0.68~0.84% 면적만으로 정수 add/accumulate/mul 지원(범용성). ADC 기반 [66](137% 면적, 253× 전력) 대비 압도적 효율. internal bus 없는 carry/shift 전달이 핵심 차별점. 실데이터 기반 평가.
- **한계(저자 명시, §VIII)**: **Floating-point 미지원** — 모든 BL이 동시 연산되나 FP는 mantissa/exponent 별도 처리 필요(전처리로 일부 우회). **암호화 비호환** — FHE 필요. Ares는 대량 연산이 있을 때만 이득(application requirement, §V-A1). Ares-V는 곱셈 시 공간 희생 + 특정 응용/전용 데이터셋 필요.
- **열린 질문**: 실제 silicon이 아닌 SPICE+MQSim 시뮬레이션 — 양산 flash에서의 신뢰성/내구성(P/E cycle 영향) 영향은? group_id 기반 FTL 배치가 GC/wear-leveling과 장기적으로 충돌하지 않는가? FP 전처리 오버헤드의 실제 비용은?

## ❓ Q&A (자가 점검)
> [!question]- Q1. Ares가 기존 IFP(ParaBit, Flash-Cosmos) 대비 결정적으로 다른 점은?
> 답: 기존 IFP는 sensing 단계에서 bitwise 연산만 수행하고 중간 결과를 cell에 재기록(여러 read/program 필요)하지만, Ares는 page buffer latch를 수정해 **모든 연산을 page buffer 내부에서** 수행하고 N-bit full adder·곱셈 등 산술을 구성한다. 또한 intra-plane transmission peripheral로 internal bus 없이 carry/shift를 전달한다. (p.2)

> [!question]- Q2. ADC 기반 IFP를 쓰지 않는 이유는?
> 답: ADC 등 추가 연산 하드웨어는 면적·전력 오버헤드가 크고(예: [66]은 137% 추가 면적·253× 전력) 연산 기능이 고정되어 특정 응용(예: 4-bit MAC)에만 쓸 수 있어 범용성이 떨어진다. Ares는 0.68~0.84% 면적만으로 범용 정수 연산을 지원한다. (p.4, p.10)

> [!question]- Q3. Transfer-free bit-shift는 어떻게 구현하나?
> 답: lower digit의 carry-out을 latch L4에 저장한 뒤, 인접 BL의 L1 반전단에 2개 트랜지스터로 연결한 intra-plane transmission 회로(Fig.11)로 internal channel 없이 higher digit에 전달한다. 4 BL마다 회로를 두고 shf/EN_8/EN_16 enable로 4/8/16/32-bit를 지원한다. (p.6~7)

> [!question]- Q4. N-bit 덧셈의 carry 순차 의존성을 어떻게 해결했나?
> 답: 단계를 재배열해 **모든 디지트의 carry를 먼저 계산**한 뒤 sum을 계산한다. 1-bit full adder의 중간값 t₁(AND), t₂(XOR)는 carry-in 계산과 독립이므로 L3, L2에 보관했다가 해당 디지트 계산 시 재사용한다. (p.6, Fig.10)

> [!question]- Q5. 곱셈은 어떤 메커니즘인가?
> 답: shift + accumulate. operand A는 flash, B는 controller에 두고 B의 각 비트를 LSD→MSD로 검사해 0이면 `shift`(w/o ADD), 1이면 `shift_and_add`를 flash에 전송한다. 부분곱을 free latch L5에 누산한다. (p.7~8, Fig.13)

> [!question]- Q6. Ares-H와 Ares-V의 차이와 트레이드오프는?
> 답: Ares-H는 operand를 수평(WL 방향) 배치, Ares-V는 수직(같은 BL 공유) 배치. Ares-V는 BL-level 병렬을 극대화하고 carry 전파 중 idle을 없애 pipelining하지만, die-level 병렬로 plane-level 병렬을 일부 희생하고 곱셈 시 공간을 더 쓰며 특정 응용·전용 데이터셋이 필요하다. Ares-H가 기본 권장(일관된 성능·친화적 데이터 배치). (p.8~9, p.14)

> [!question]- Q7. 누산에서의 대표 성능·에너지 수치는?
> 답: K-Means에서 Ares-H가 OSP **18.58×**·ISP **9.89×**, Page Rank에서 OSP/ISP **12.47×/6.82×**. 에너지는 Page Rank 누산에서 OSP/ISP 대비 **97.9×/14×** 절감. (p.12~13)

> [!question]- Q8. Ares의 명시된 한계는?
> 답: Floating-point 미지원(전처리로 일부 우회 가능), 암호화는 FHE 필요, 대량 연산이 있을 때만 이득. 또한 실제 silicon이 아닌 SPICE+MQSim 시뮬레이션 기반 평가다. (§VIII, p.15)

## 🔗 Connections
[[In-Storage Computing]] · [[MICRO]] · [[2024]]

## References worth following
- **ParaBit** [33] — C. Gao et al., "Parabit: processing parallel bitwise operations in nand flash memory based ssds," MICRO 2021. (Ares의 주요 비교 baseline)
- **Flash-Cosmos** [72] — J. Park et al., "Flash-cosmos: In-flash bulk bitwise operations using inherent computation capability of nand flash memory," MICRO 2022. (ESP 기법 출처, baseline)
- **ICE** [43] — H.-W. Hu et al., "Ice: An intelligent cognition engine with 3d nand-based in-memory computing for vector similarity acceleration," MICRO 2022. (ADC 없는 곱셈/VVM 선행, 1.6% 면적)
- **Ambit** [79] — V. Seshadri et al., MICRO 2017. (commodity DRAM bulk bitwise, PIM 비교 기준)
- **MQSim** [82] — A. Tavakkol et al., FAST 2018. (평가에 쓴 SSD 시뮬레이터)

## Personal annotations
<본인 메모 영역>
