---
title: "BABOL: A Software-Defined NAND Flash Controller"
aliases: [BABOL]
description: "NAND Flash 동작을 소프트웨어로 기술하고 프로그래머블 μFSM 하드웨어가 ONFI 파형을 생성·실행하는 비동기 software-defined storage controller 구조."
venue: MICRO
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/reliability
  - topic/flash-controller
  - topic/ftl
  - venue/micro
  - year/2024
---

# BABOL: A Software-Defined NAND Flash Controller

> **MICRO 2024** · `cluster/reliability` · Source: [BABOL - A Software-Defined NAND Flash Controller.pdf](BABOL%20-%20A%20Software-Defined%20NAND%20Flash%20Controller.pdf)

**저자**: Kibin Park, Alberto Lerner, Sangjin Lee, Philippe Bonnet, Yong Ho Song, Philippe Cudré-Mauroux, Jungwook Choi (Hanyang University, University of Fribourg, University of Copenhagen, Samsung Electronics)

## TL;DR
SSD의 Storage(Channel) Controller는 ONFI 표준 동작뿐 아니라 벤더별 비표준 최적화(pseudo-SLC read, read retry, partial read, erase suspend 등)를 지원해야 해서 하드웨어로 만들면 매우 복잡하고 이식성이 없다. BABOL은 NAND의 큰 동작 지연(수십 μs)을 활용해 **스케줄링과 파형 기술을 소프트웨어로**, **파형 실행만 프로그래머블 하드웨어(μFSM)로** 분리하는 *software-defined* 컨트롤러를 제안한다. C++ coroutine/lambda 또는 FreeRTOS로 동작을 기술하며, 하드웨어 대비 코드 라인 수를 대폭 줄이면서(READ 420→58줄) 성능 손실은 sequential 2~3% 이내로 유지한다.

## 문제 & 동기
ONFI 표준은 Flash 패키지를 전기적으로 교체 가능하게 만들지만, **동작(operation) 차원의 최적화는 추상화해 버린다.** 표준은 READ/PROGRAM/ERASE의 기본 파형만 정의하고, 실제 벤더는 PSEUDO SLC READ, PARTIAL READ, bounded-latency READ, READ RETRY, PROGRAM/ERASE suspend 등 패키지별 비표준 동작을 제공한다. 이를 활용하려면 고도로 정교하고 오류가 잦은 *일회용(one-off)* 하드웨어 컨트롤러를 직접 개발해야 하며, 매년 새 Flash 세대가 나오면 다시 작성해야 한다. 반면 HIC(NVMe), FTL, ECC는 이미 유연한 개발 도구가 있으나 **Storage Controller만 유연한 아키텍처가 없다.**

> [!quote]- 📄 원문 표현 (paper)
> "Writing a controller that exploits these optimizations is the only way to obtain competitive performance, but it makes for a highly intricate, error-prone, and non-portable controller development process." (p.1)
>
> "We argue that the only component missing a flexible architecture is the Storage Controller." (p.2)

## 핵심 통찰

> [!note]- 통찰 1 — NAND의 지연이 곧 소프트웨어 여유 시간
> NAND Flash는 한 동작에 수십 μs가 걸린다(DRAM의 수십 ns 대비). 한 LUN이 Array↔Page Register 내부 데이터 이동으로 바쁜 동안, 다른 영역은 다음 동작을 **소프트웨어로** 준비·스케줄할 시간이 충분하다. BABOL은 이 시간 분리를 활용한 최초의 컨트롤러로, 성능 저하 없이 software-defined를 달성한다 (p.2).

> [!note]- 통찰 2 — 파형 "기술"과 "실행"의 분리 (asynchronous architecture)
> 기존 동기식 컨트롤러는 채널이 비는 순간 즉시 파형을 발행해야 해서 하드웨어가 시간 제약을 받는다(Fig.4). BABOL은 *Operation Scheduling*(다음 segment를 무엇으로 할지 기술)과 *Operation Execution*(실제 발행)을 분리한다. 둘 사이를 **waveform instruction set**(queue 가능한 추상화)로 연결해 비동기로 만든다 (p.4).

> [!note]- 통찰 3 — BTC를 파라미터화한 μFSM = 파형 생성용 명령어 집합
> ONFI의 Basic Timing Cycle(BTC)은 고정 파형 어휘라 표현력이 부족하다. BABOL은 BTC를 **파라미터화 가능한 μFSM**으로 대체하여, 파형을 상수가 아니라 "패턴"으로 기술한다. 이 표현력이 표준·비표준·고급 동작을 모두 인코딩하는 힘의 원천이다 (p.5).

## 설계 / 메커니즘

> [!abstract]- 3-component 아키텍처 (Fig.5, p.4)
> BABOL 컨트롤러는 세 부분으로 구성된다.
> 1. **Stream Processors**: DMA, ECC, Scrambler (데이터 경로, 기존과 유사).
> 2. **Operation Scheduling** (소프트웨어): Task Scheduler + Transaction Scheduler + Support Libraries. 어떤 동작을 admit하고 어떤 transaction이 채널을 쓸지 결정.
> 3. **Operation Execution** (하드웨어, 엄격한 타이밍): Packetizer(데이터) + μFSM(제어). 하드코딩 파형이 아니라 프로그래머블하게 파형을 조립.
>
> 핵심 단위: **transaction** = 원자적으로 실행되며 채널을 독점하는 waveform segment. 한 동작(operation)은 여러 transaction의 연결로 구성된다.

> [!abstract]- 5종 μFSM (Fig.6, p.5–6)
> ONFI 표준에서 유래한 3개 + 2개:
> - **C/A Writer**: command/address latch 파형 생성. (latch 개수, 각 latch 타입 벡터, latch 값 벡터)로 파라미터화.
> - **Data Writer**: Page Register로 데이터 전송(쓰기). Packetizer DMA와 연동, DQS strobe 구동.
> - **Data Reader**: Page Register→DRAM 읽기. Data Writer의 역(inverse).
> - **Chip Control**: 채널 내 LUN당 1비트 bitmap으로 어떤 chip에 파형을 적용할지 변경 → **gang scheduling**(예: RAIL의 데이터 복제 read) 가능.
> - **Timer**: duration(ns)만큼 pause 생성. SET FEATURE 후 t_ADL 대기 등 Array가 작업할 시간 부여.
>
> 각 μFSM은 SDR/NV-DDR 등 데이터 모드별로 재구현되지만 인터페이스는 동일. μFSM은 "ONFI-like 파형을 생성하는 명령어 집합"이다.

> [!abstract]- 타이밍 책임 분담 + 소프트웨어 환경 (p.6–7)
> 타이밍은 3범주로 나뉜다: ① μFSM **내부** timed wait(t_CS, t_CH 등) → μFSM 책임, ② μFSM **직후** mandatory wait(t_WB 등) → μFSM 책임, ③ **연속 μFSM 사이** wait → SSD Architect(소프트웨어) 책임. 이로써 Architect는 ONFI pin 수준이 아닌 높은 추상화에서 추론.
>
> 소프트웨어 환경: μFSM을 연결(concatenate)해 동작을 기술. **C++20 lambda**(나중 실행용 익명 함수)로 transaction을 감싸고 `add_transaction()`로 큐에 넣으며, **coroutine**(`co_await`)으로 제어를 양보해 동작을 polling/nesting. 단, BABOL은 특정 언어 메커니즘을 강제하지 않음(FreeRTOS도 가능).

> [!abstract]- 동작 예시: READ STATUS / Change Column / pseudo-SLC (Algorithm 1–3, Fig.8, p.7–8)
> - **READ STATUS** (Alg.1): chip_control 활성화 → write_ca(0x70) → data_read(4) → 비활성화. polling으로 LUN 완료 확인.
> - **READ with Change Column** (Alg.2, multi-transaction): cmd/addr latch → READ STATUS로 t_R 대기 polling(고정 대기 대신) → CHANGE READ COLUMN으로 임의 offset부터 데이터 전송(16KB 페이지를 4KB 단위로).
> - **pseudo-SLC READ** (Alg.3): Alg.2에서 명령 ID만 PSLC로 바꾸면 비표준 동작으로 변환 → 소프트웨어 재사용성 입증.

## 평가

> [!success]- 실험 환경 & 주요 수치 (p.8–11)
> **플랫폼**: Cosmos+ OpenSSD (Xilinx Zynq 7000 FPGA/SoC, dual ARM Cortex-A9), Vivado/Vitis 2022.1, μFSM=Verilog, 나머지=C++20. Flash: Hynix/Toshiba/Micron SO-DIMM, ONFI NV-DDR2(최대 200 MT/s). Hynix/Toshiba는 채널당 8 LUN, Micron은 2 LUN (Table I).
> - **Flash 파라미터** (Table I, p.8): page read time Hynix 100μs / Toshiba 78μs / Micron 53μs, page size 16384 B, page transfer 185μs(100 MT/s) / 100μs(200 MT/s).
> - **소프트웨어 오버헤드** (Fig.10, p.9): HW baseline 대비 RTOS·Coroutine 비교. processor 150 MHz softcore~1 GHz ARM, 채널 100/200 MT/s. RTOS는 200 MT/s에서 ARM 코어면 baseline과 거의 동등. Coroutine은 100 MT/s·8 LUN·1 GHz ARM에서 가장 빠름(느린 채널일수록 사전 스케줄 여유 큼).
> - **End-to-End (실디바이스)** (Fig.12, p.10): Cosmos+ OpenSSD의 storage controller를 BABOL로 교체, fio로 sequential/random READ. 채널을 8 LUN으로 꽉 채우면 baseline 대비 대역폭 차이 = **sequential RTOS 2% / Coro 8% 이내**, **random RTOS 3% / Coro 9% 이내**. (Coro polling은 worst-case ~30μs 주기.)
> - **프로그래밍 노력** (Table II, p.11, 코드 라인 수): READ 420(동기 HW)/454(비동기 HW)→**58(BABOL)**; PROGRAM 420/260→**44**; ERASE 327/203→**27**.
> - **FPGA 자원** (Table III, p.11): LUT 9343/3909→**3539**, FF 13021/3745→**3635**, BRAM 11.5/8→**6**. (프로세서 면적은 SoC 내장이라 미포함.)

## 섹션 노트
- **§I Introduction** (p.1): ONFI는 전기적 상호운용성은 성공했으나 동작 최적화는 추상화해 버려 컨트롤러가 일회용·비이식적. HIC/FTL/ECC는 유연한 도구가 있으나 Storage Controller만 없음.
- **§II Background & Motivation** (p.2–4): SSD 4 컴포넌트, channel/LUN/interleaving 구조, ONFI 동작의 multi-step(command latch, address latch, data in/out, BTC), 동기식 컨트롤러(Fig.4)의 시간 제약·비유연성 한계.
- **§III BABOL Architecture** (p.4): 두 원리 — (1) 스케줄링과 실행 분리(asynchronous), (2) 동작 스케줄링을 전적으로 소프트웨어로.
- **§IV Programmable Hardware** (p.4–6): μFSM 5종, BTC→μFSM 파라미터화, 타이밍 3범주 책임 분담.
- **§V Software Environment** (p.6–8): single/multi-transaction 동작, lambda+coroutine, Task/Transaction Scheduler(목표 강제 안 함), FreeRTOS 대안.
- **§VI Experiments** (p.8–11): 소프트웨어 오버헤드, coroutine 오버헤드 분석(Fig.11 Logical Analyzer), end-to-end, 프로그래밍 노력, 면적.
- **§VII Related Work** (p.11): OCOWFC/Cosmos+, Open Channel(SDF/OCOWFC), Flash-Cosmos(bit-wise compute), Decoupled/Networked SSD, Autoblox(파라미터 자동화), software-defined radio·DRAM controller와의 유사성.
- **§VIII Conclusion** (p.11): software-defined Flash channel controller 프레임워크, 두 소프트웨어 환경(C++ 쉬움 / RTOS 엄격), 오픈소스.

## 핵심 용어
- **Storage / Channel Controller**: SSD에서 Flash 패키지를 채널 단위로 묶어 FTL에 연속 주소 공간을 노출하고 LUN 동작을 스케줄·실행하는 컴포넌트.
- **ONFI (Open NAND Flash Interface)**: NAND 패키지의 핀·전압·표준 동작(READ/PROGRAM/ERASE)을 규정하는 표준. 동작 차원 최적화는 미정의.
- **LUN (Logical Unit)**: 독립적으로 동작을 수행할 수 있는 Flash 단위. 여러 LUN이 공유 버스(channel)로 묶임.
- **BTC (Basic Timing Cycle)**: ONFI가 정의한 파형 조각(어휘). 명령·주소 등 한 정보를 컨트롤러↔패키지 간 전달.
- **μFSM**: BABOL의 파라미터화 가능한 finite-state machine. BTC를 일반화해 ONFI-like 파형을 생성하는 명령어 단위.
- **Transaction**: 원자적으로 실행되며 채널을 독점하는 waveform segment. 동작 = transaction들의 연결.
- **Interleaving**: 한 LUN이 내부 작업(t_R 등)으로 채널을 양보한 동안 같은 채널의 다른 LUN 동작을 진행시키는 기법.
- **pseudo-SLC READ**: TLC/MLC/QLC 셀을 SLC처럼 사용해 성능·수명을 얻는 비표준 동작.
- **gang scheduling**: Chip Control μFSM으로 여러 LUN(chip)에 동일 파형을 동시 적용(예: RAIL의 read 복제).

## 강점 · 한계 · 열린 질문
**강점**
- NAND 지연을 활용한 비동기 분리로 **성능 저하 거의 없이** 소프트웨어 유연성 확보(sequential 2~3% 이내).
- 코드 라인 수 대폭 감소(READ 420→58) + FPGA 자원도 더 적게 사용(타이밍 로직을 소프트웨어로 이전).
- 오픈소스, 비표준/고급 동작을 소프트웨어로 손쉽게 인코딩·재사용(Alg.2→Alg.3 변환 trivial).

**한계**
- 소프트웨어 스케줄을 감당할 **충분히 빠른 프로세서** 필요(150 MHz softcore에서는 RTOS가 baseline에 못 미침). Coroutine은 idle channel에서 polling 지연(~30μs)으로 최대 9%까지 성능 손실.
- 실험은 READ 중심(t_R가 짧아 반응 시간 부담이 가장 크다는 이유). PROGRAM/ERASE의 end-to-end 성능 직접 측정은 제한적.
- Zynq 7000(구형) 한계; 저자도 신형 플랫폼이면 baseline 동등 가능성 추정.

**열린 질문**
- Decoupled/Networked SSD 같은 복잡한 인터커넥트 구조와의 통합 시 software-defined 원리가 유지되는가?
- Autoblox류 파라미터 자동 탐색과 결합 시 새 패키지 대응 시간을 얼마나 더 줄일 수 있는가?
- Coroutine polling 지연을 줄이는 적응형 polling이 random workload 격차(9%)를 해소할 수 있는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. BABOL이 software-defined를 성능 저하 없이 달성할 수 있는 근본 이유는?
> 답: NAND Flash 동작이 수십 μs로 느려서, 한 LUN이 내부 데이터 이동으로 바쁜 동안 소프트웨어가 다음 동작을 스케줄·기술할 시간이 충분하기 때문이다(p.2). DRAM처럼 빠른 매체였다면 불가능했을 비동기 분리가 가능해진다.

> [!question]- Q2. Operation Scheduling과 Operation Execution을 분리한 이유와 둘을 잇는 추상화는?
> 답: 동기식 컨트롤러는 채널이 비는 순간 즉시 파형을 발행해야 해서 하드웨어가 시간 제약을 받고 비유연하다(Fig.4). BABOL은 "무엇을 다음에 발행할지 기술"(Scheduling)과 "실제 발행"(Execution)을 분리하고, queue 가능한 **waveform instruction set**(μFSM 명령 + Packetizer 데이터 명령)으로 연결한다(p.4).

> [!question]- Q3. μFSM이 ONFI의 BTC와 다른 점은?
> 답: BTC는 고정된 파형 어휘라 표현력이 제한적이다. μFSM은 **파라미터화**되어 파형을 상수가 아니라 패턴으로 기술하므로, 여러 BTC를 합치거나 표준에 없는 비표준 동작까지 인코딩할 수 있다(p.5).

> [!question]- Q4. C++ lambda와 coroutine은 각각 무슨 역할인가?
> 답: lambda는 transaction(파형 segment 생성 파라미터)을 익명 함수로 감싸 `add_transaction()`으로 나중 실행 큐에 넣는다. coroutine은 `co_await`로 제어를 양보(deschedule)해 동작을 polling하거나 한 동작 안에 다른 동작을 nesting할 수 있게 하는 경량 컨텍스트 스위칭 메커니즘이다(p.7–8).

> [!question]- Q5. RTOS 버전과 Coroutine 버전의 trade-off는?
> 답: Coroutine(C++)은 작성이 쉽지만 빠른 프로세서가 필요하고 idle channel에서 polling 지연(~30μs)이 있다. RTOS(FreeRTOS)는 더 가벼운 프로세서로도 baseline에 가깝게 동작하지만 프로그래밍 전문성이 더 필요하다(p.9–10).

> [!question]- Q6. end-to-end 실험에서 BABOL의 성능 손실은 얼마이며, 왜 채널이 꽉 찰수록 격차가 줄어드나?
> 답: 8 LUN으로 채널을 채우면 sequential RTOS 2%/Coro 8%, random RTOS 3%/Coro 9% 이내(p.10). 채널이 바쁘면 다른 동작이 채널을 두고 경쟁하므로 polling 지연으로 인한 재발행이 줄어들어 격차가 작아진다.

> [!question]- Q7. Table II/III가 보여주는 두 가지 이점은?
> 답: Table II — 동작 인코딩 코드 라인이 크게 감소(READ 420/454→58 등), 즉 프로그래밍 노력 절감. Table III — FPGA 자원(LUT/FF/BRAM)도 더 적게 사용, 타이밍·BTC 로직을 소프트웨어로 옮겼기 때문(p.11).

> [!question]- Q8. Chip Control μFSM이 가능하게 하는 고급 기능은?
> 답: 채널 내 LUN당 1비트 bitmap으로 여러 chip에 동일 파형을 동시 적용하는 **gang scheduling**. 예로 RAIL처럼 동일 데이터를 여러 위치에 복제 read해 latency variance를 줄이는 기법을 표현할 수 있다(p.6).

## 🔗 Connections
[[Reliability]] · [[MICRO]] · [[2024]]

## References worth following
- [25] J. Kwak et al., "Cosmos+ OpenSSD: Rapid prototype for flash storage systems," IEEE Access 2020 — BABOL의 실험 플랫폼이자 비교 baseline(asynchronous HW controller).
- [50] L. Lu et al. (Qiu et al.), OCOWFC — synchronous hardware controller baseline, software-defined Flash controller 선행.
- [45] "Open NAND Flash Interface specification revision 5.1," ONFI 2022 — BTC·표준 동작의 근거 문서.
- [32] H. Litz et al., "RAIL: Predictable, low tail latency for NAND flash," ACM TOS 2022 — Chip Control gang scheduling의 응용 사례.
- [14] HyperStone, "How pseudo-SLC mode can make 3D NAND more reliable" — pseudo-SLC READ 비표준 동작의 배경.
- [47] Flash-Cosmos — Flash command set으로 page bit-wise 연산을 수행하는 controller 내부 혁신, BABOL과 상보적.

## Personal annotations
<본인 메모 영역>
