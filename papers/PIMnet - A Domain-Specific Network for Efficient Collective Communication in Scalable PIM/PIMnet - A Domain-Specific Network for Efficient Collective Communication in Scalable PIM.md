---
title: "PIMnet: A Domain-Specific Network for Efficient Collective Communication in Scalable PIM"
aliases: [PIMnet]
description: "PIM 뱅크 간 직접 연결을 제공하는 PIM-controlled 도메인 특화 네트워크로 host를 거치지 않고 collective communication을 가속한다."
venue: HPCA
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/pim
  - topic/collective-communication
  - venue/hpca
  - year/2025
---

# PIMnet: A Domain-Specific Network for Efficient Collective Communication in Scalable PIM

> **HPCA 2025** · `cluster/isc` · Source: [PIMnet - A Domain-Specific Network for Efficient Collective Communication in Scalable PIM.pdf](<PIMnet - A Domain-Specific Network for Efficient Collective Communication in Scalable PIM.pdf>)

저자: Hyojun Son, Gilbert Jonatan, Xiangyu Wu, Haeyoon Cho, Kaustubh Shivdikar, José L. Abellán, Ajay Joshi, David Kaeli, John Kim (KAIST, Northeastern University, Universidad de Murcia, Boston University)

## TL;DR
현재 PIM은 compute unit이 자기 "local" 메모리만 접근 가능하고 "remote" 메모리 접근은 반드시 host CPU를 거쳐야 하므로, collective communication이 PIM 확장성의 근본적 병목이 된다. PIMnet은 DRAM 패키징 계층(inter-bank/inter-chip/inter-rank)에 맞춘 multi-tier 도메인 특화 네트워크로, host 없이 PIM 뱅크 간 직접 통신을 제공한다. 통신 패턴이 deterministic하다는 점을 활용해 buffer/arbitration/routing이 없는 PIM-controlled(스케줄 기반) 네트워크를 설계, collective communication에서 최대 85x 속도, 실제 애플리케이션에서 평균 11.8x 개선을 달성한다.

## 문제 & 동기
PIM의 모든 compute unit은 자신의 local 뱅크 메모리만 접근할 수 있고, remote 메모리 접근은 host CPU를 통한 indirect PIM-to-PIM 통신으로만 가능하다. 이로 인해 AllReduce, All-to-All 같은 collective communication의 성능이 host-to/from-PIM 대역폭에 의해 근본적으로 제한된다. SimplePIM, PID-Comm 같은 소프트웨어 접근은 host overhead를 줄이지만 여전히 host 대역폭에 묶여 있다. roofline 분석 결과 ideal software collective조차 64 뱅크를 넘어 다중 DIMM/rank가 같은 채널을 쓰면 확장성이 한계에 부딪힌다.

> [!quote]- 📄 원문 표현 (paper)
> "one fundamental limitation of current PIM architectures is that remote memory accesses or accesses to a 'non-local' PIM bank occurs through the host CPU. This indirect PIM-to-PIM communication through the host impacts both the latency and bandwidth of inter-PIM communications." (p.1)
>
> "PIMnet exploits bandwidth parallelism where communication across the different PIM bank/chips can occur in parallel to maximize communication performance." (p.1)
>
> "Effectively, the goal of PIMnet is to enable 'One Gigantic PIM' architecture, instead of an architecture that is a collection of many PIM banks." (p.2)

## 핵심 통찰

> [!note]- 통찰 1: collective communication은 deterministic하다
> AllReduce/All-to-All 같은 collective communication은 source, destination, message size가 실행 전에 모두 알려져 있다(deterministic). 따라서 dynamic flow control, buffering, arbitration 없이 통신을 "스케줄링"할 수 있고, 이것이 PIM-controlled 네트워크의 토대가 된다 (p.5-6).

> [!note]- 통찰 2: DRAM 기술 제약을 우회하는 도메인 특화 네트워크
> PIM 인터커넥트는 DRAM 공정(제한된 metal layer 3장, 적은 logic/wire) 위에 만들어져야 해서 일반 목적 라우터/crossbar는 비현실적으로 비싸다. PIMnet은 keep-radix-low, simplify-arbitration, no-buffers, minimize-pins라는 네 가지 설계 목표로 하드웨어 오버헤드를 최소화한다 (p.5).

> [!note]- 통찰 3: 기존 채널 재사용 + bandwidth parallelism
> PIMnet은 새 핀을 추가하지 않고 기존 DRAM I/O bus, DQ pin, DDR multi-drop bus를 재사용한다. 동시에 여러 뱅크/칩이 병렬 통신할 수 있어(bandwidth parallelism), 뱅크 수가 늘수록 local AllReduce가 병렬로 수행되어 확장성이 좋아진다 (p.4, p.6).

> [!note]- 통찰 4: PIM-controlled 인터커넥트
> 전통 네트워크와 달리 통신을 PIM logic이 직접 "orchestrate"한다. host가 kernel launch 시 traffic pattern/scope/message size를 control unit에 넘기면, READY/START 신호로 동기화 후 미리 스케줄된 경로로 데이터가 이동해 on-chip contention이 제거된다 (p.5-6).

## 설계 / 메커니즘

> [!abstract]- Multi-tier 토폴로지 (inter-bank / inter-chip / inter-rank)
> DRAM 패키징 계층에 맞춘 3계층 네트워크 (Fig.4, Table IV):
> - **Inter-bank**: 한 칩 내 뱅크 간 연결. PIMnet stop("라우터")을 추가. 물리 토폴로지는 ring. 기존 Bank I/O bus 재사용, 채널 4개 x 32bit, 채널당 약 0.7 GB/s. → 각 PIM 노드가 자기 라우터를 가지는 **direct** 네트워크.
> - **Inter-chip**: 한 rank 내 칩 간 연결. 기존 DQ pin(8개를 4 송신/4 수신으로 분할) 사용, 8x8 inter-chip crossbar를 memory buffer chip에 둠. 채널 2개 x 4bit, 약 1.05 GB/s. → buffer chip을 쓰는 **indirect** 네트워크.
> - **Inter-rank**: rank(DIMM) 간 연결. 기존 DDR multi-drop bus를 broadcast bus로 활용. 채널 1개(half-duplex) x 64bit, 약 16.8 GB/s. inter-rank switch가 정적 스케줄대로 데이터 재배열.

> [!abstract]- Bandwidth 계산
> 뱅크당 채널 약 0.7 GB/s → bisection 2.8 GB/s. 칩당 8뱅크 → total inter-bank bisection 약 2.8 x 8 = 22.4 GB/s. 모든 뱅크가 병렬 통신(예: Ring AllReduce) 시 2.8 x 64 = 179.2 GB/s의 aggregate send/receive 대역폭/rank 제공 (p.5).

> [!abstract]- Software/Interface와 실행 흐름
> 기존 PIM kernel은 그대로 두고, collective communication을 PIM에 offload되는 명령어 시퀀스로 변환(Fig.5). `PIMnet_ReduceScatter(size, scope)`처럼 size(통신할 원소 수)와 scope(참여 뱅크 수)를 인자로 받는다. PIM assembly는 POLL(DPU 동기화), RS(WRAM에서 local reduction + PIMnet 이동), WAIT(공유 채널 접근 스케줄) phase로 구성. 실행 흐름: 각 뱅크가 READY 신호를 control interface에 보냄 → 모두 aggregate되면 START 신호로 통신 시작 (p.6).

> [!abstract]- Address Generation / Traffic Scheduling & Routing
> host가 없으므로 각 뱅크가 메모리 주소를 알아야 한다. 통신 파라미터가 사전에 알려져 있어 CPU가 컴파일 시 모든 주소를 생성. Algorithm 1은 AllReduce의 주소/타이밍(offset)을 bank/chip/rank별로 계산. 데이터는 local scratchpad(WRAM)에서 접근. Routing은 알려진 physical topology + 미리 정해진 logical topology(collective 알고리즘 기반)로 결정되어 하드웨어 라우팅 불필요. Reduce-Scatter는 ring(inter-bank)→broadcast(inter-rank), All-to-all은 ring(inter-bank)→permutation(inter-chip)→unicast(inter-rank)로 구현 (Table V, p.7-9).

## 평가

> [!success]- 평가 결과 (수치 + 페이지)
> - **방법론**: UPMEM HW로 검증된 detailed PIM simulator (Booksim 2.0로 NoC 모델). 베이스라인 B(host 통신 PIM), S(ideal software collective), N(NDPBridge), D(DIMM-Link), P(PIMnet) 비교. 워크로드: EMB, NTT, GEMV, MLP, SpMV, BFS, CC, Join (Table VII) (p.9-11).
> - **Collective communication**: 최대 **85x** 속도, 실제 애플리케이션 평균 **11.8x** 개선 (p.1, Fig.10).
> - **그래프 워크로드(BFS, CC)**: 베이스라인에서 AllReduce가 전체 실행시간의 최대 83% 차지. PIMnet은 통신 시간을 **5%로 감소**, CC에서 **5.6x** 속도 (p.10).
> - **SpMV**: Reduce-Scatter로 **2.43x** 속도. **Join**: All-to-all로 64M tuples에서 **36%** 성능 개선 (p.10-11).
> - **확장성(Fig.12)**: DPU 8→256, weak scaling(message 32KB). AllReduce는 DPU 증가에 따라 속도 증가, All-to-all은 ideal software 대비 약 2x (256 DPU, p.4).
> - **하드웨어 오버헤드**: PIMnet stop + address generator를 Verilog로 구현, OpenROAD 45nm 합성. 면적 +0.09%, 전력 +1.6% (vs baseline PIM). 전통 NoC router 대비 약 **60x** 면적 절감. inter-chip/rank switch는 0.013mm², 17mW. 최악 propagation latency 약 15ns(약 6 DPU cycle) (p.13).
> - **PIM-controlled vs credit-based flow control(Fig.13)**: AllReduce는 1% 이내 차이, All-to-all은 PIM-controlled가 **18.7%** 실행시간 단축 (p.13).
> - **Bandwidth scaling(Fig.14)**: inter-bank 대역폭을 0.7→0.1 GB/s로 낮춰도 bandwidth parallelism 덕에 DIMM-Link 대비 3x 우위 (p.13).
> - **더 강력한 PIM 가정(Fig.15)**: GDDR6-AiM(UPMEM 대비 180x 높은 multiply 처리량) 가정 시 MLP 이득이 1.3x→약 40x로 증가 (p.13).

## 섹션 노트
- **I. Introduction**: PIM의 remote 접근 host 경유 문제 제기, PIMnet 4대 기여 요약 (p.1-2).
- **II. Background**: UPMEM PIM 구조(DPU 14-stage pipeline, 32-bit, 24 thread, MRAM 64MB/IRAM 24KB/WRAM 64KB). collective communication 정의, 평가 방법론, Table I/II (p.2-3).
- **III. Motivation**: roofline 모델 두 종류(conventional, communication arithmetic intensity)로 dedicated interconnect 이점 분석. PIMnet은 ideal software 대비 약 8x compute throughput 가능. 64 뱅크 넘어 확장성 한계 (Fig.2-3, p.3-4).
- **IV. PIMnet Architecture**: PIM-constrained interconnect 설계 목표 4개, multi-tier overview, 소프트웨어/인터페이스 (Table III/IV, Fig.4-5, p.4-6).
- **V. PIMnet Microarchitecture**: inter-bank/chip/rank 상세, address generation, routing (Algorithm 1, Fig.6-9, Table V, p.6-9).
- **VI. Evaluation**: 위 평가 결과 (Fig.10-17, p.9-13).
- **VII. Related work**: 다양한 PIM 워크로드, multi-PIM(AIM, DIMM-Link, ABC-DIMM, NDPBridge), NoC/large-scale network, memory-centric network와 비교 (p.13-14).

## 핵심 용어
- **PIM bank (PIM node)**: compute와 "local" 뱅크 메모리의 단위. PIMnet에서 통신의 기본 노드.
- **PIM-controlled interconnect**: host/CPU가 아니라 PIM logic이 통신을 orchestrate하는 네트워크. buffer/arbitration/dynamic routing 불필요.
- **Bandwidth parallelism**: 서로 다른 PIM bank/chip이 동시에 병렬로 통신해 aggregate 대역폭을 극대화하는 특성.
- **Collective communication**: AllReduce(reduction), All-to-All(전 쌍 교환), Reduce-Scatter, AllGather, Broadcast 등 다수 노드 간 협조적 통신.
- **Deterministic communication pattern**: source/destination/message size가 실행 전에 알려져 contention 없이 스케줄 가능한 통신.
- **PIMnet stop**: inter-bank 네트워크에서 뱅크를 연결하는 "라우터". buffer/arbitration/HW routing 없는 단순 구조.
- **WRAM/MRAM**: UPMEM의 software-managed scratchpad cache(64KB)/main DRAM bank(64MB). compute는 WRAM 데이터만 가능.
- **Communication arithmetic intensity**: 네트워크로 전송한 byte당 연산 수. 수정된 roofline 모델의 x축.

## 강점 · 한계 · 열린 질문
- **강점**: 실제 UPMEM HW로 검증된 시뮬레이터; 기존 핀/배선 재사용으로 면적 +0.09%의 극히 낮은 오버헤드; 통신의 deterministic 특성을 활용한 우아한 PIM-controlled 설계; 다양한 실제 워크로드에서 큰 이득.
- **한계**: All-to-all의 inter-rank 통신은 여전히 DDR memory channel 대역폭에 묶임; 다중 memory channel 간 통신은 여전히 host 경유 필요("inter-memory channel로 확장 가능한지는 미해결"); GEMV/MLP 등은 data parallelism으로 통신 없이도 구현 가능하나 그 경우 PIMnet 이득 제한적.
- **열린 질문**: PIMnet을 inter-memory-channel로 확장 가능한가? 차세대 high-FLOPS DPU에서 이득이 어떻게 변하는가(저자는 더 커질 것으로 예측)? multi-tenancy에서 bandwidth isolation을 얼마나 보장하나?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 현재 PIM 아키텍처의 근본적 한계는 무엇인가?
> 답: 각 compute unit이 자기 "local" 뱅크 메모리만 접근 가능하고, "remote" 메모리(다른 PIM 뱅크) 접근은 반드시 host CPU를 거쳐야 한다. 이 indirect PIM-to-PIM 통신이 collective communication의 latency와 bandwidth를 모두 제한해 확장성 병목이 된다.

> [!question]- Q2. PIMnet이 일반 목적 인터커넥트 대신 도메인 특화 네트워크를 쓰는 이유는?
> 답: PIM 인터커넥트는 DRAM 공정(metal layer 3장, 제한된 logic/wire) 위에 만들어져야 하므로 buffer/crossbar/arbitration이 있는 일반 라우터는 비현실적으로 비싸다. collective communication이 deterministic하다는 점을 활용해 buffer/arbitration 없는 스케줄 기반 설계로 비용을 최소화한다.

> [!question]- Q3. PIMnet의 multi-tier 구조 3계층은 무엇이며 어떤 자원을 재사용하나?
> 답: inter-bank(칩 내 뱅크, Bank I/O bus 재사용, ring, direct), inter-chip(rank 내 칩, DQ pin 재사용, 8x8 crossbar in buffer chip, indirect), inter-rank(DIMM 간, DDR multi-drop bus를 broadcast bus로 활용, indirect). 새 핀을 추가하지 않고 기존 채널을 재사용한다.

> [!question]- Q4. "PIM-controlled"가 의미하는 바와 그 이점은?
> 답: host가 아니라 PIM logic이 통신을 orchestrate한다. host가 kernel launch 시 traffic pattern/scope/size만 넘기면, READY/START 동기화 후 미리 스케줄된 경로로 데이터가 이동한다. dynamic flow control/arbitration/buffer가 불필요해져 contention이 제거되고 HW 오버헤드가 매우 낮다.

> [!question]- Q5. host가 없는데 각 PIM 뱅크는 통신할 메모리 주소를 어떻게 아나?
> 답: 통신 파라미터(패턴/뱅크 수/토폴로지)가 실행 전에 알려져 있으므로, CPU가 컴파일 시 각 뱅크가 쓸 주소를 모두 생성한다. Algorithm 1이 bank/chip/rank별 offset과 start address를 계산하고, 데이터는 local scratchpad(WRAM)에서 접근한다.

> [!question]- Q6. 주요 성능 수치는?
> 답: collective communication 최대 85x, 실제 애플리케이션 평균 11.8x 개선. 그래프 워크로드는 통신 시간을 83%→5%로 줄여 CC 5.6x, SpMV 2.43x, Join 36% 개선. HW 오버헤드는 면적 +0.09%, 전력 +1.6%로 NoC 라우터 대비 약 60x 면적 절감.

> [!question]- Q7. PIM-controlled 스케줄링이 credit-based flow control보다 항상 좋은가?
> 답: 항상은 아니다. AllReduce는 1% 이내로 거의 동일. 하지만 All-to-all처럼 inter-chip crossbar에서 heavy contention이 발생하는 독립적 point-to-point 통신이 많은 경우 PIM-controlled가 18.7% 더 빠르다. credit-based는 경합 시 통신 시간이 증가한다.

> [!question]- Q8. PIMnet의 주요 한계와 미해결 과제는?
> 답: All-to-all의 inter-rank 통신은 여전히 DDR 채널 대역폭에 묶이고, 다중 memory channel 간 통신은 여전히 host를 거쳐야 한다. inter-memory-channel로의 확장 가능성은 미해결 과제로 남겨두었다.

## 🔗 Connections
[[In-Storage Computing]] · [[HPCA]] · [[2025]]

## References worth following
- PID-Comm (ISCA 2024) [67]: collective communication의 host overhead를 줄이는 소프트웨어 프레임워크. PIMnet의 주요 software 비교 대상.
- DIMM-Link (HPCA 2023) [89]: DRAM rank 간 직접 point-to-point 연결을 제안한 하드웨어 인터커넥트. PIMnet의 주요 HW 비교 대상.
- NDPBridge (ISCA 2024) [85]: DRAM 계층을 가로지르는 하드웨어 "bridge"로 메시지 전송 가속. All-to-all 비교 대상.
- SimplePIM (PACT 2023) [16]: PIM 프로그래밍/통신 primitive software 프레임워크. baseline collective 구현 기준.
- UPMEM PIM [25][68]: 본 연구의 베이스라인 PIM 아키텍처와 차세대 DPU(native FP, 5-8 TFLOPS/chip) 전망.
- "The case for domain-specific networks" (HOTI 2023) [7]: PIMnet의 도메인 특화 네트워크 개념적 기반.

## Personal annotations
<본인 메모 영역>
