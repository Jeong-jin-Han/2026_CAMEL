---
title: "FPGA Programmability — 회로가 되는 칩, 명령어는 선택사항"
aliases: [FPGA Programmability, FPGA 프로그래머빌리티, soft-core, soft-core CPU, FPGA vs CPU, FPGA kernel]
type: concept
tags:
  - concept
  - concept/hw
  - topic/fpga
  - topic/accelerator
---

# FPGA Programmability — 회로가 되는 칩, 명령어는 선택사항

> [!abstract] 이 노트는 뭐지?
> [[Smart-Infinity]]의 updater는 명령어를 안 받는데, 어떤 FPGA 구현은 명령어를 받는다 — 이 차이는 FPGA 자체의 성질이 아니라 **그 위에 어떤 회로를 구웠느냐**의 차이다. "FPGA가 할 수 있는 것/없는 것", CPU와의 실행 모델 차이, 그리고 고정 회로부터 soft-core CPU까지의 **프로그래머빌리티 스펙트럼**을 정리한다. [[H3 — DockerGPU, in-GPU control plane|H3]]의 dispatcher 설계 축이기도 하다.

## 한 문장
CPU는 고정된 회로가 **명령어 스트림을 읽어가며** 매번 다른 일을 하는 기계고, FPGA는 bitstream을 구우면 fabric이 **특정 회로 그 자체가 되는** 칩이다 — 따라서 "명령어를 받느냐"는 FPGA의 한계가 아니라 설계자의 **선택**이며, CPU 회로를 구우면(soft-core) 명령어도 받는다.

## 1. FPGA의 본질 — "프로그램"이 곧 회로 배선

FPGA fabric의 구성 요소:
- **LUT (Look-Up Table)** — 임의의 조합 논리를 진리표로 흉내
- **FF (Flip-Flop)** — 상태 저장 (레지스터)
- **BRAM/URAM** — 작은 on-chip 메모리 블록
- **DSP slice** — 곱셈·누산 하드 블록
- 이들을 잇는 **programmable interconnect**

**bitstream**을 로드하면 이 자원들이 특정 배선으로 굳는다 = 칩이 그 회로가 *된다*. CPU처럼 "명령어를 fetch해서 decode"하는 단계가 원래 없다 — 무엇을 할지는 이미 배선에 새겨져 있고, 런타임에 주는 건 데이터와 시작 신호뿐.

> [!example] [[Smart-Infinity]]의 updater가 정확히 이 경우
> HLS(C→회로 합성)로 만든 **고정 dataflow 회로**. 받는 것 = ① 시작 신호(OpenCL kernel launch → doorbell) ② 인자(버퍼 포인터·크기·압축비 c%) ③ 데이터 스트림. "Adam update를 한다"는 사실 자체는 회로에 새겨져 있어 바꿀 수 없다(바꾸려면 재합성 → 새 bitstream). 이때 "FPGA kernel"이란 **실행되는 코드가 아니라 HLS로 합성된 회로**를 OpenCL/XRT가 offload 단위로 부르는 이름이다.

## 2. 그런데 "CPU 회로"를 구우면? — 명령어를 받는다

FPGA 위에 fetch→decode→execute 파이프라인 회로를 구우면 그게 곧 CPU다 = **soft-core CPU** (Xilinx MicroBlaze, Intel Nios, RISC-V 계열 VexRiscv·PicoRV32 등). 이제 이 회로는 메모리에서 **명령어를 읽어 실행**한다 — 명령어는 그 CPU-회로의 입력 *데이터*일 뿐이다.

> 즉 FPGA는 명령어를 "못 받는" 게 아니라, **명령어를 받는 기계를 만들지 말지를 선택**할 수 있다.

## 3. 프로그래머빌리티 스펙트럼

"다른 일을 시키려면 무엇을 바꿔야 하나"를 기준으로 한 줄에 세우면:

| 단계 | 무엇 | 동작을 바꾸려면 | 속도/유연성 | 예 |
|---|---|---|---|---|
| **고정 회로** | 한 연산 전용 datapath | bitstream **재합성** (오프라인, 분~시간) | 최고속 / 최저유연 | Smart-Infinity updater·decompressor |
| **파라미터 회로** | 레지스터·인자로 동작 범위 조절 | 런타임 레지스터 write | 설계된 범위 내에서만 | updater의 압축비 c% 인자 |
| **FSM/시퀀서** | 정해진 스케줄·상태기계 재생 | 스케줄 테이블(BRAM) 교체 | 빠름 / 정적 제어만 | H3 Phase 2 dispatcher 후보 |
| **soft-core CPU** | 명령어 fetch·decode·execute 회로 | **프로그램 교체** (컴파일만) | ~100–400MHz, 진짜 CPU보다 수십 배 느림 / 완전 유연 | MicroBlaze, VexRiscv |
| **hard core** | fabric *옆*에 박힌 진짜 ARM 코어 | 일반 SW | CPU급 / fabric과 한 칩 | Zynq·Versal의 ARM |

- 아래로 갈수록 유연↑ 속도↓ — **유연성은 공짜가 아니라 fabric 면적과 클럭으로 지불**한다.
- 중간 절충(overlay/CGRA: fabric 위에 조립식 연산 어레이를 깔고 그 위를 프로그래밍)도 존재.

## 4. 왜 Smart-Infinity는 CPU를 안 굽고 고정 회로로 했나

Trade-off ([[Design Principles]]의 Parallelism·Trade-off 렌즈):

1. **클럭 열세** — fabric은 ~100–400MHz (CPU 3–5GHz). soft CPU로 순차 실행하면 수십 배 손해
2. **면적 기회비용** — fetch/decode 로직이 차지할 자리에 **연산 유닛을 병렬로** 깔 수 있음
3. **워크로드 성질** — Adam update(AXPBY)는 단순·규칙적·스트리밍 → 명령어 유연성이 필요 없음 → 고정 datapath 16개 병렬이 압승 (updater >7GB/s, SSD보다 빨라 병목 아님 — Fig 14)

> [!tip] 선택 원칙
> **일이 규칙적이면 회로로 굽고(빠름), 일이 조건부·가변적이면 명령어 기계를 굽는다(유연함).** "어디까지 회로로 굽고 어디부터 명령어로 두나"의 경계 긋기가 가속기 설계의 핵심 질문.

## 5. FPGA가 할 수 있는 것 / 없는 것 (실무 감각)

**할 수 있는 것**
- 디지털 회로로 표현되는 모든 것 (CPU 포함 — Turing-complete)
- 임의 비트폭·커스텀 파이프라인·수천 개 병렬 datapath
- 결정적(deterministic) latency — 사이클 단위 보장
- I/O 프로토콜 직접 구현: PCIe endpoint·bus mastering(DMA)·doorbell·Ethernet — 장치가 장치를 제어하는 배관 ([[BaM]]·[[H3 — DockerGPU, in-GPU control plane|H3]]의 전제)

**할 수 없는 것 / 비싼 것**
- 높은 클럭 (CPU·GPU의 1/10 수준)
- GPU급 부동소수점 밀도 (DSP 수 한계 — 그래서 FPGA는 연산량 승부가 아니라 **커스텀 구조·near-data 위치** 승부)
- 런타임 재구성의 민첩성 — bitstream 리로드는 ms~초 단위 (partial reconfiguration으로 완화 가능하나 여전히 무거움)
- SW 스택 공짜 획득 — OS·드라이버·라이브러리 전부 직접 마련 (성숙한 shell/XRT가 있는 Alveo류가 연구에 선호되는 이유)

## 6. 내 연구와의 연결

- **[[H3 — DockerGPU, in-GPU control plane|H3]]의 설계 축이 곧 이 스펙트럼**: dispatcher를 ① FSM(캡처한 스케줄 재생 — 빠르나 정적)으로 굽나 ② soft-core(스케줄링 코드 실행 — 동적 제어 가능하나 느림)로 굽나. GPU 옆이라는 위치가 주는 **왕복 latency 절약이 soft-core의 클럭 열세를 상쇄하는지**가 H3의 핵심 측정 질문
- 절충 설계: 자주 도는 판정(EOS 비교 등)은 회로로, 드문 정책 변경은 soft core로 — "경계 긋기" 질문이 [[H1 — 워크로드 특화로 multi-node coherence 줄이기|H1]]·H3에 걸쳐 반복되는 내 수렴 주제

---
**관련**: [[Smart-Infinity]] · [[DockerSSD]] · [[BaM]] · [[H3 — DockerGPU, in-GPU control plane]] · [[Design Principles]]
