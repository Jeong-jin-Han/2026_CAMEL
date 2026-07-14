# FpgaNIC: An FPGA-based Versatile 100Gb SmartNIC for GPUs

> **Source PDF**: [FpgaNIC.pdf](FpgaNIC.pdf)
> 🕸️ NodeGraph: [FpgaNIC.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/FpgaNIC%20-%20An%20FPGA-based%20Versatile%20100Gb%20SmartNIC%20for%20GPUs/FpgaNIC.html)
> **Authors**: Zeke Wang, Hongjing Huang, Jie Zhang, Fei Wu (Zhejiang University, China), Gustavo Alonso (ETH Zurich)
> **Venue / Year**: USENIX ATC 2022 (2022 USENIX Annual Technical Conference), pp. 967–990
> **arXiv / DOI**: https://www.usenix.org/conference/atc22/presentation/wang-zeke
> **Length**: 21 pages (본문 + appendix + artifact 포함)
> **Read status**: ☑ Full read (2026-07-13)
> **My reading purpose**: H3 가설(GPU 옆 FPGA 제어 평면)의 feasibility 선례 조사 — FPGA가 CPU 없이 GPU 메모리에 P2P DMA로 접근하는 실제 시스템 사례 확보

---

## 📋 목차

- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary (for own writing)](#key-vocabulary-for-own-writing)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR

FpgaNIC은 GPU 클러스터를 위한 최초의 FPGA 기반 GPU-중심(GPU-centric) 100Gbps SmartNIC이다. 기존 멀티코어 SmartNIC(BlueField-2)이나 FPGA-증강(augmented) SmartNIC(Mellanox Innova-2)은 GPU 트래픽을 line-rate로 처리하지 못하거나 GPU 가상주소를 직접 다루지 못하는데, FpgaNIC은 (1) GPU 통신 스택(control/data plane offload, PCIe P2P + GPU virtual-to-physical address translation via GTLB), (2) 하드웨어로 구현된 100Gbps 신뢰성 네트워크 트랜스포트(TCP), (3) HLS(C++)로 프로그래밍 가능한 On-NIC Computing(ONC) 데이터패스 가속기, 이 세 요소를 하나의 FPGA(Xilinx Alveo U50/U280) 위에 계층적으로 결합했다. 이를 통해 direct(GPU-centric networking), off-path(AllReduce), on-path(HyperLogLog) 세 가지 SmartNIC 모델을 모두 지원함을 실증하고, 8-GPU 분산 AllReduce에서 NCCL 대비 최대 2.5배 가속을 달성했다.

---

## Core thesis

> "In this paper, we present the design of a 100Gb GPU-centric SmartNIC to serve distributed applications running on GPUs. From a GPU's perspective, such a SmartNIC should 1) enable the GPU directly triggering doorbell registers and polling on status registers on the SmartNIC without CPU intervention (**G1**); 2) use the GPU virtual address space to directly access GPU memory via Peer-to-Peer (P2P) communication without CPU intervention (**G2**); 3) implement in hardware the full network stack to ensure low latency and high throughput (**G3**); 4) implement application logic offloading to a software-defined and hardware-accelerated data-path accelerator ... (**G4**); and 5) The data-path accelerator should be easily programmed by system programmers (**G5**)." (§1, p.967)

추가 설명: 이 다섯 가지 목표(G1–G5)가 곧 FpgaNIC의 전체 설계를 규정한다. 핵심은 "GPU가 SmartNIC을 CPU 없이 직접 제어하고(G1, G2), NIC은 그 트래픽을 line-rate로 처리하며(G3, G4), 이를 HDL 대신 HLS로 프로그램 가능하게 만든다(G5)"는 것이다.

---

## Why this matters to me

이 논문은 내 H3 가설("GPU 패키지 옆에 FPGA 제어 평면을 두고 host CPU가 하던 결정을 대신하게 하면 어떨까")이 요구하는 **저수준 메커니즘**의 실증 선례다. 정확히 말하면, 내가 가설의 실현 경로에서 필요조건으로 꼽았던 "① PCIe endpoint + bus mastering(능동 write) ② GPU와 같은 switch 아래 P2P(ACS/IOMMU 설정) ③ 성숙한 스택(XRT 등)"이 FpgaNIC에서 거의 그대로 구현되어 있다: FPGA가 PCIe endpoint의 master interface로 GPU 가상주소를 이용해 GPU 메모리에 직접 DMA read/write하고(NVIDIA GPUDirect P2P), 반대로 GPU가 FPGA의 doorbell 레지스터를 CUDA 커널 안에서 직접 트리거해 FPGA를 기동한다. 다만 이 논문의 "제어"는 네트워크 트래픽 오프로드를 위한 것이지, 내 가설이 그리는 "FPGA가 GPU 커널 스케줄링/launch를 대신 결정"하는 방향은 아니다 — 이 차이는 아래 [Connection to my research direction](#connection-to-my-research-direction)에서 정직하게 정리한다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.967–969 | 5개 목표(G1–G5), 기존 SmartNIC(멀티코어/FPGA-증강)의 한계, FpgaNIC 개요 |
| 2 | Design and Implementation of FpgaNIC | p.969–973 | 4개 설계 챌린지(C1–C4): GTLB, 하드웨어 TCP, HLS 프로그래밍, direct/on-path/off-path 모델 |
| 2.1 | Design Challenges | p.969 | C1(가상주소 접근) ~ C4(SmartNIC 모델 다양화) |
| 2.2–2.3 | GPU Communication Stack | p.969–971 | 제어 평면 오프로드(doorbell/status reg), GTLB로 VA→PA 변환, decoupled application interface |
| 2.4 | 100Gbps Hardware Network Transport | p.971–972 | 오픈소스 FPGA TCP/IP 스택 기반, decoupled sending/receiving interface |
| 2.5 | On-NIC Computing (ONC) | p.972–973 | HLS 기반 고수준 인터페이스, direct/on-path/off-path 세 모델 지원, multi-tenant(Coyote/vFPGA) 논의 |
| 3 | Experimental Evaluation | p.973–978 | 공유 인프라(GPU stack, TCP) 벤치마크 + 세 모델(direct/off-path/on-path) 평가 |
| 4 | Related Work | p.977–978 | FPGA-증강 SmartNIC, GPU-FPGA 통신, FPGA 기반 가속, 멀티코어 SmartNIC, 산업 SmartNIC 비교 |
| 5 | Insights and Implications | p.978 | On-NIC 컴퓨팅의 고성능 이유, 성능 격리, 중간 수준 프로그래밍 가능성 |
| 6 | Conclusion | p.978 | 오픈소스 공개(GitHub: RC4ML/FpgaNIC), future work |
| A.1 | Appendix: GCN (direct model) | p.982–984 | GPU-centric Networking 상세 설계 (send/recv 버퍼, handshake, flow control) |
| A.2 | Appendix: AllReduce (off-path model) | p.984–985 | 링 기반 AllReduce 8-step state machine, Innova 대비 우위 |
| A.3 | Appendix: HyperLogLog (on-path model) | p.985 | on-path 모듈이 "bump in the wire"로 동작함을 실증 |
| 7 | Artifact | p.985–986 | 재현 실험 절차, 하드웨어/소프트웨어 체크리스트 |

---

## Section notes

### §1 Introduction (p.967–969)

FpgaNIC의 동기는 명확하다: 네트워크 대역폭은 CPU 처리 능력보다 빠르게 성장하고 있고, AI/HPC 클러스터의 대부분 트래픽은 (CPU가 아니라) GPU가 만들어낸다. 그런데 기존 SmartNIC은 두 갈래로 나뉜다 — 멀티코어 SmartNIC(BlueField-2 등)은 임베디드 ARM 코어가 100Gbps 트래픽을 line-rate로 처리하지 못하고(실측 achievable memory bandwidth 27.3GB/s로 100Gbps 스트림 처리에 못 미침, §1 p.968), FPGA-증강 SmartNIC(Mellanox Innova-2)은 FPGA가 NIC ASIC(ConnectX-5)에 PCIe Gen4 x8로 종속되어 있어 데이터패스 가속기(G4)로 완전히 기능하지 못한다. Table 1(p.968)이 이 비교를 요약한다: 오직 FpgaNIC만 G1~G5를 모두(또는 부분) 만족.

> "To our knowledge, the multicore SmartNIC is controlled from the host CPU, so G1 is not supported. The network transport is implemented with the packet processing engine with necessary control on the host (or ARM) CPU, so G3 is partially supported." (§1, p.968)

### §2.1–2.2 Design Challenges & GPU Communication Stack (p.969–971)

네 가지 핵심 챌린지(C1–C4) 중 내 연구에 가장 직결되는 것은 **C1(GPU 가상주소 접근)**이다. NVIDIA GPUDirect가 제공하는 DMA 엔진은 물리주소 기반으로 동작하는데, GPU 프로그램은 가상주소를 다룬다. 이를 해결하기 위해 GTLB(GPU Translation Lookaside Buffer)를 설계했다: main TLB(2MB 단위 대형 페이지) + complementary TLB(64KB 단위, 물리적으로 비연속인 경우)로 구성되며, 32GB GPU 메모리에 대해 겨우 18K 엔트리(16K + 2K)만으로 온칩 메모리에서 완전 파이프라인된 주소 변환을 수행한다(Algorithm 1, p.971). 제어 평면 오프로드는 GPU 드라이버 + FPGA 드라이버 + CUDA 유저 코드의 co-design으로 구현되며, GPU 커널 내부에서 `misc_register`/`mmap`으로 매핑된 PCIe BAR를 통해 512개의 doorbell/status 레지스터를 직접 트리거·폴링한다.

> "FpgaNIC enables offloading of control plane onto GPUs (via a slave interface) and offloading of the data plane onto the FPGA (via a master interface), such that the host CPU is bypassed." (§2.3, p.970)

이 문장이 내 H3 가설의 "FPGA endpoint + bus mastering + GPU-triggered doorbell"과 정확히 대응한다.

### §2.4 100Gbps Hardware Network Transport (p.971–972)

오픈소스 FPGA TCP/IP 스택([57,60])을 기반으로, 원래의 control-handshake 방식(패킷당 10~30 사이클의 handshake, 최대 1460B 페이로드당 10~23 사이클 — 즉 handshake 오버헤드가 payload 전송 시간과 맞먹어 대역폭 낭비)을 "decoupled application interface"로 개선했다. 이 인터페이스는 handshake와 data transfer를 오버랩시켜 최대 4GB 스트림을 프로그래머 개입 없이 자동 chunking한다.

### §2.5 On-NIC Computing (p.972–973)

ONC은 GPU 통신 스택과 네트워크 트랜스포트 "사이"에 위치해 두 모듈을 직접 조작할 수 있다. HLS(C++) 기반 고수준 인터페이스(axilite_control 슬레이브 인터페이스, dma_read/write 커맨드+데이터 스트림, tcp_tx/rx meta+data 스트림)를 노출해 시스템 프로그래머가 하드웨어 세부사항 없이 데이터패스 가속기를 작성할 수 있게 한다. Direct/on-path/off-path 세 모델의 코드량(Table 5, p.973)은 하드웨어 1K~15.3K HLS 라인, 소프트웨어 0.3K~1.5K C++/CUDA 라인 수준으로, 완전한 HDL 대비 훨씬 접근성이 높다.

### §3 Experimental Evaluation (p.973–978)

실험은 8대의 4U AMAX 서버(Intel Xeon Silver 4214, 128GB RAM), 각 서버당 FpgaNIC(Xilinx Alveo U50/U280) + Nvidia RTX 8000 GPU(2대 서버는 A100 추가), Mellanox 100Gbps Ethernet SN2700 스위치로 구성된 클러스터에서 수행됐다(Figure 2, p.973). 세 가지 핵심 결과:

1. **GPU 통신 스택 벤치마크** (§3.2): 제어 평면 지연은 GPU→FPGA가 1μs 미만이며 CPU→FPGA와 비슷하지만 변동폭(fluctuation)이 훨씬 작다(Figure 3, p.974). 데이터 평면(PCIe P2P DMA) 처리량은 A100 GPU 대상 read에서 12.6GB/s(PCIe 이론 대역폭에 근접), burst size에 따라 read는 512B, write는 8KB에서 최대 처리량 도달(Figure 4, p.974).
2. **하드웨어 네트워크 트랜스포트** (§3.2.2): TCP RTT는 마이크로초 단위(64B에서 3.1μs, 32KB에서 7μs, Figure 5a), 큰 패킷에서 100Gbps 채널 용량에 근접하는 처리량 달성(94.4Gbps @ 1408B, 1000 connections, Figure 5b).
3. **Direct 모델(GCN)** (§3.3): 제어 평면 오프로드가 있을 때 처리량이 유의미하게 향상(특히 작은 chunk/transfer size에서), CUDA 커널 invocation 오버헤드를 제거하기 때문.
4. **Off-path 모델(AllReduce)** (§3.4): 8-GPU 분산 풀에서 FpgaNIC-enhanced AllReduce가 NCCL 대비 최대 2.5배 speedup(작은 데이터 사이즈에서), 8MB 이상에서 이론적 bus bandwidth에 도달(Figure 7–8, p.976). GPU/CPU 컴퓨팅 사이클을 전혀 소모하지 않는다는 점이 핵심.
5. **On-path 모델(HyperLogLog)** (§3.5): FPGA 내 HLL 모듈이 데이터 스트림을 "bump in the wire"처럼 처리해도 전체 처리량에 영향이 없고, 오프로딩하지 않을 경우 필요했을 최소 8개의 A100 GPU SM을 절약(Table 6, Figure 9, p.976).

### §4 Related Work (p.977–978)

FpgaNIC을 GPU-FPGA 통신 관련 선행연구와 명확히 구분한다:

> "Previous work [6,64] has implemented GPUDirect RDMA on an FPGA to directly access GPU memory, but not allowing the GPU to trigger doorbell registers within an FPGA. In contrast, FpgaNIC allows GPUDirect RDMA and the GPU to trigger registers within an FPGA, and is an FPGA-based SmartNIC that allows large design space exploration of SmartNIC architecture." (§4, p.977)

즉, "FPGA→GPU 방향의 GPUDirect RDMA"는 2012년([6] Bittner & Ruf) 및 2013년([64] Thoma) 선행연구에서 이미 존재했지만, "GPU→FPGA 방향으로 doorbell을 직접 트리거"하는 양방향 제어는 FpgaNIC이 처음이라는 것이 저자들의 명시적 주장이다. Table 7(p.977)은 산업계 SmartNIC(Broadcom Stingray, Pensando DSC-25, Netronome NFP4000, Intel IPU)과 비교하며, 이들 모두 CPU-centric이고 GPU-centric은 FpgaNIC뿐임을 보인다.

### §5 Insights and Implications (p.978)

ONC이 FPGA 전체 자원의 약 20%만 사용(GPU 통신 스택 + 100G HW 트랜스포트 합산, Table 2)하므로 나머지 80%를 온-라인-레이트(on-line-rate) 컴퓨팅에 자유롭게 쓸 수 있다는 점, 그리고 U50의 2채널 독립 HBM(채널당 13.6GB/s)이 오프로드된 태스크 간 메모리 대역폭 격리를 자연스럽게 보장한다는 점을 강조한다.

### Appendix A.1–A.3 (p.982–985)

GCN(direct model)의 상세 구현은 GPU user layer(응용) → GPU kernel layer(circular send/receive buffer, GPU 메모리 220MB 사용) → FpgaNIC(control/DMA read-forward/DMA write) 3계층 co-design이다(Figure 11, p.983). 신뢰성은 credit-based flow control로 보장되며, 핸드셰이크는 CUDA 커널이 소켓처럼 동작하도록 만든 API(`create_socket_context`, `socket`, `listen`, `accept`, `connect`)로 추상화된다. AllReduce(off-path)는 링 토폴로지에서 8-step state machine(Table 8, p.984)으로 PCIe DMA + 네트워크 스택 + 온보드 메모리를 동시에 오버랩시킨다. HyperLogLog(on-path)는 `op_in`/`op_out`/`op_return` 세 포트로 온-패스 모듈이 데이터 흐름에 개입한다(Figure 13, p.985).

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "GPU-centric SmartNIC" (기존 CPU-centric SmartNIC과 대비되는 개념)
- "large SmartNIC design space exploration" (direct/on-path/off-path 세 모델을 하나의 아키텍처로 포괄)
- "FPGA-GPU co-processing" (GPU는 expressiveness/compute flexibility, FPGA는 네트워크 인프라 + ONC 제공)

**Technical concepts:**
- "control plane offloading onto GPUs" / "data plane offloading onto the FPGA"
- "GPU Translation Lookaside Buffer (GTLB)" — main TLB + complementary TLB
- "doorbell registers" / "status registers" (GPU가 CPU 없이 직접 트리거·폴링)
- "decoupled application interface" (handshake와 payload transfer 분리)
- "on-NIC computing (ONC)" / "data-path accelerator"
- "direct / on-path / off-path SmartNIC models"

**Value language:**
- "line-rate processing" (100Gbps 트래픽을 지연 없이 처리)
- "without CPU intervention" (CPU 완전 우회)
- "performance guarantee and isolation" (전용 하드웨어 자원 + 독립 HBM 채널)

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "FpgaNIC" 자체 (제품명)
> - "versatile 100Gb SmartNIC" (이 논문의 타이틀 프레이징 그대로)
> - "GCN(GPU-centric Networking)" 이라는 약어 (이 논문 고유의 네이밍)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §1, p.968 | BlueField-2 achievable memory bandwidth 27.3GB/s, "matching the findings in [40]" | 멀티코어 SmartNIC 임베디드 CPU가 100Gbps 스트림을 감당 못함을 뒷받침하는 정량 근거 |
| §2.3.2, p.970 | 32GB GPU 메모리에 필요한 GTLB 엔트리 = 16K + 2K = 18K (naive 512K 대비 대폭 감소) | 온칩 메모리로 GPU 가상주소 변환을 구현 가능함을 보이는 수치 |
| §3.2.1, p.974 | 제어 평면 지연(GPU→FPGA) < 1μs; DMA read to A100 GPU = 12.6GB/s (PCIe 이론 대역폭에 근접) | "FPGA가 CPU 없이 GPU에 마이크로초 이하 지연으로 접근 가능" 주장의 핵심 근거 |
| Appendix D4, p.982 | PCIe P2P bandwidth: Quadro GPU 10.6GB/s vs Tesla-class GPU 12.6GB/s | GPU 클래스별 P2P 대역폭 차이 |
| §3.2.2, p.974 | TCP RTT 3.1μs(소형 메시지)~7μs(32KB); 최대 처리량 94.4Gbps(1408B payload, 1000 connections) | "SmartNIC 오프로드로 μs 단위 RTT 달성, ms 단위인 CPU 소켓 스택과 대비" |
| §3.4, p.975–976 | 8-GPU AllReduce: NCCL 대비 최대 2.5× speedup(소데이터), 8MB 이상에서 이론적 bus bandwidth 도달 | GPU/CPU 사이클 소모 없이 collective communication 가속 |
| §3.5, p.976 | HLL on-path 오프로딩으로 A100 GPU SM 최소 8개 절약, 처리량 영향 없음(Table 6, Figure 9) | on-path 모델이 "bump in the wire"로 작동함을 보이는 수치 |
| §5, p.978 | GPU 통신 스택 + 100G HW 트랜스포트가 U50 FPGA 자원의 약 20%만 사용 | ONC(온-NIC 컴퓨팅)에 80% 이상 자원 여유가 있음을 보이는 수치 |

---

## 🎯 Strategic anchor

> "Previous work [6,64] has implemented GPUDirect RDMA on an FPGA to directly access GPU memory, but not allowing the GPU to trigger doorbell registers within an FPGA. In contrast, FpgaNIC allows GPUDirect RDMA and the GPU to trigger registers within an FPGA..." (§4 Related Work, p.977)

→ **본인 활용**: 이 문장은 "FPGA가 GPU 메모리에 CPU 없이 P2P로 접근하는 것"과 "GPU가 FPGA를 CPU 없이 직접 트리거하는 것"이 **2012년([6]) 이래 독립적으로 알려진 각각의 메커니즘**이고, 이를 **양방향으로 결합**한 것이 FpgaNIC(2022)의 기여라는 사실을 명시적으로 보여준다. 면담에서 "제 가설(H3)의 최소 구성요소 — PCIe endpoint로서의 FPGA, bus mastering, GPU 가상주소 기반 P2P DMA, CPU 우회 트리거 — 가 개별적으로는 이미 2012년부터, 결합된 형태로는 2022년 ATC에 실제 하드웨어로 존재합니다"라고 인용 가능. 단, 이 논문의 "트리거"는 GPU가 FPGA에 네트워크 작업을 요청하는 것이지 FPGA가 GPU에 커널 스케줄링을 지시하는 것은 아니므로, 방향이 반대라는 점은 반드시 같이 언급해야 함(과장 방지).

---

## Connection to my research direction

| 차원 | 이 paper (FpgaNIC) | 내 방향 (H3) |
|---|---|---|
| Scope | **원격** GPU-to-GPU 네트워킹 (분산 클러스터, Ethernet 너머) | **로컬** GPU 옆 제어 평면 (단일 노드, PCIe 내부) |
| Mechanism | FPGA가 네트워크 트래픽을 오프로드; GPU가 doorbell로 FPGA를 트리거(GPU→FPGA 제어), FPGA가 P2P DMA로 GPU 메모리 접근(FPGA→GPU 데이터) | FPGA가 host CPU 대신 GPU 커널 스케줄링/launch를 결정(FPGA→GPU 제어라는 반대 방향) |
| Workload | AI/HPC 분산 통신 원시함수(AllReduce, GCN 소켓 통신, 스트리밍 cardinality 추정) | 단일 GPU 상에서의 커널 launch 타이밍/우선순위 결정 |
| Open space | GPU가 FPGA "스마트 기능"의 클라이언트일 뿐, FPGA가 GPU 실행을 능동적으로 지시하는 사례는 없음 | 정확히 이 지점 — FPGA가 GPU의 "제어자(controller)"가 되는 것이 미탐색 영역 |

이 논문의 스코프는 내 가설과 **겹치지 않는 부분이 더 크다**는 점을 정직하게 인정해야 한다. FpgaNIC은 "네트워킹"(distributed GPU-to-GPU 통신)이 핵심이고, 온-패스/오프-패스 컴퓨팅도 네트워크 데이터 흐름 위에서 동작하는 부가 기능(AllReduce 리덕션, HLL cardinality)이지, GPU 커널 자체의 launch/scheduling을 FPGA가 결정하는 사례는 전혀 없다. 그럼에도 이 논문이 가치 있는 이유는, 내 H3가 요구하는 **저수준 배관(plumbing)** — ① FPGA가 PCIe endpoint로서 bus mastering DMA를 수행하고 ② 같은 PCIe 스위치 아래에서 GPU 가상주소 기반 P2P를 실현하며(GTLB가 그 구체적 해법) ③ HLS라는 성숙한 스택으로 이를 프로그램 가능하게 만든다는 것 — 을 **실측 수치와 오픈소스(GitHub: RC4ML/FpgaNIC)로 증명**했다는 점이다. 즉 "FPGA가 GPU에 CPU 없이 접근하는 것 자체는 실현 가능하고 이미 상용 FPGA 보드(Alveo U50/U280)로 구현되어 있다"는 명제의 직접적 증거이며, 내 가설의 "실현 경로" 섹션에서 [[BaM]]과 나란히 놓을 수 있는 두 번째 선례(precedent)다. BaM이 "GPU가 스토리지를 직접 제어"하는 사례라면, FpgaNIC은 그 대칭점인 "FPGA가 GPU 메모리를 직접 제어(그리고 GPU가 FPGA를 직접 제어)"하는 사례로 위치시킬 수 있다. 다만 "FPGA가 GPU의 실행 그 자체(커널 스케줄링)를 지시"하는 것은 이 논문의 범위 밖이며, 그 지점이 바로 내가 채워야 할 gap이다.

---

## Open questions / gaps

- [ ] FpgaNIC의 doorbell/GTLB 메커니즘을 "네트워크 요청" 대신 "GPU 커널 launch 요청"에 재사용할 수 있는가? (동일한 P2P DMA + doorbell 인프라를 스케줄링 결정 전달에 쓸 수 있을지는 이 논문에서 다루지 않음)
- [ ] FPGA가 GPU 실행 상태(SM occupancy, queue depth 등)를 관찰할 수 있는 경로가 있는가? 이 논문은 FPGA→GPU 데이터 DMA와 GPU→FPGA doorbell 트리거만 다루고, GPU의 "실행 상태"를 FPGA가 능동적으로 폴링/관찰하는 메커니즘은 제시하지 않음.
- [ ] Multi-tenant 지원(Coyote/vFPGA, §2.5.2)이 "future work"로 남겨짐 — 여러 GPU 프로세스가 동시에 하나의 FpgaNIC을 공유할 때의 격리·스케줄링은 미해결.
- [ ] 논문은 인접 GPU-FPGA 통신을 다루지만, GPU 패키지 "내부" 혹은 "매우 근접한" 위치에 FPGA를 두는 패키징/레이턴시 임팩트는 다루지 않음(모두 별도 PCIe 카드로 구현).
- [ ] Intel FPGA 포팅은 "미래 작업"으로 남겨짐(현재 Xilinx Alveo U50/U280만 검증, §1 각주3) — 벤더 종속성 문제.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [6] | Bittner & Ruf, "Direct GPU/FPGA Communication via PCI Express", ICPP Workshops 2012 | FPGA→GPU P2P DMA의 최초 선행연구 — H3의 "실현 경로" 역사적 뿌리 확인 |
| ☐ | [64] | Thoma, Dassatti, Molla, "FPGA2: An Open Source Framework for FPGA-GPU PCIe Communication", ReConFig 2013 | 위와 같은 계열의 오픈소스 프레임워크 — 실제 재현 가능한 코드 존재 여부 확인 |
| ☐ | [32] | Korolija, Roscoe, Alonso, "Do OS abstractions make sense on FPGAs?", OSDI 2020 (Coyote) | vFPGA 개념 — FPGA 위에서 OS-level 격리/가상화를 어떻게 구현하는지, H3의 "제어 평면"을 여러 GPU/프로세스에 어떻게 격리시킬지에 참고 |
| ☐ | [30] | Kim, Huh, Zhang, Hu, Wated, Witchel, Silberstein, "GPUnet: Networking Abstractions for GPU Programs", OSDI 2014 | GCN(direct model)의 소켓 API 설계가 GPUnet에서 영감을 받음 — GPU 커널 내부에서 시스템 호출류 API를 제공하는 선례 |
| ☐ | [39] | Lin, Patel, Stephens, Sivaraman, Akella, "PANIC: A High-Performance Programmable NIC for Multi-tenant Networks", OSDI 2020 | Multi-tenant SmartNIC 아키텍처 — FpgaNIC이 미래작업으로 남긴 부분과 비교할 참고 사례 |
| ☐ | [52] | NVIDIA, "Developing a Linux Kernel Module using GPUDirect RDMA" | GPUDirect의 실제 커널 모듈 구현 방식 — H3의 "성숙한 스택" 요건과 직결 |
| ☐ | [17] | He, Korolija, Alonso, "EasyNet: 100 Gbps Network for HLS", FPL 2021 | FpgaNIC의 HLS 기반 네트워크 스택 설계 철학과 같은 계열 — HLS로 100Gbps를 다루는 다른 사례 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
