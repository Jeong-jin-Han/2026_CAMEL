# Re-architecting End-host Networking with CXL: Coherence, Memory, and Offloading

> **Source PDF**: [CXL-NIC.pdf](CXL-NIC.pdf)
> 🕸️ NodeGraph: [CXL-NIC.html (새 탭에서 렌더링)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/CXL-NIC%20-%20Re-architecting%20End-host%20Networking%20with%20CXL%20Coherence%2C%20Memory%2C%20and%20Offloading/CXL-NIC.html)
> **Authors**: Houxiang Ji, Yifan Yuan (Meta), Yang Zhou, Ipoom Jeong (Yonsei Univ.), Ren Wang (Intel), Saksham Agarwal, Nam Sung Kim (Univ. of Illinois Urbana-Champaign, corresponding author)
> **Venue / Year**: MICRO '25 (58th IEEE/ACM International Symposium on Microarchitecture), Seoul, Oct 18–22, 2025
> **arXiv / DOI**: 10.1145/3725843.3756102
> **Length**: 15 pages
> **Read status**: ☑ Full read (2026-07-13)
> **My reading purpose**: 내 CXL/coherence 연구 방향과의 직접적 접점 확인 — CXL.cache 기반 device coherence를 메모리 확장이 아닌 NIC I/O에 적용한 사례

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

PCIe 기반 NIC은 DMA(대량 전송에만 유리, 소형 패킷엔 setup/completion 오버헤드가 큼)와 MMIO(높은 지연·낮은 대역폭)로만 호스트 CPU와 통신할 수 있고, 하드웨어 캐시 일관성이 없어 소프트웨어가 명시적으로 flush/pinning/barrier를 관리해야 한다. 이 논문은 CXL의 `CXL.cache` 프로토콜(주로 메모리 확장용 Type-3 `CXL.mem`에 가려져 있던 기능)을 이용해 **CXL Type-1/Type-2 NIC("CXL-NIC")**를 FPGA(Agilex-7)로 직접 구현한다. Type-1(온디바이스 메모리 없음)은 `NC-read/write`, `CS-read`, `CO-read` 네 종류의 요청을 Rx/Tx 데이터패스 각 단계에 세밀하게 배분해 PCIe의 DMA/MMIO를 대체하고, Type-2(coherent on-device memory 보유)는 여기에 더해 host/device-bias 모드 전환과 `NC-P`(push-write) 최적화로 패킷 버퍼·디스크립터 링을 NIC 메모리에 둘지 호스트 메모리에 둘지 자유롭게 배치할 수 있게 한다. 마지막으로 Key-Value Store(KVS)를 NIC에 직접 offload하는 application co-acceleration 사례로 이 설계의 실전 가치를 보인다. 상용 PCIe NIC(BF-3) 대비 패킷 처리 지연 49%, KVS 요청 처리 지연 39% 감소를 달성했다.

---

## Core thesis

> "We contend that Network Interface Controllers (NICs) represent a particularly compelling, indeed a 'killer application' for harnessing the power of CXL's cache coherence." (§1, p.1810)

추가 설명: CXL 연구·산업의 초점은 대부분 `CXL.mem` 기반 메모리 확장(Type-3)에 쏠려 있었는데, 이 논문은 그로 인해 가려진 `CXL.cache`(Type-1/2의 핵심 기능)를 정면으로 다룬다. NIC은 호스트와의 통신 빈도가 매우 높고 데이터 단위가 작아(64B 디스크립터 등) PCIe의 구조적 약점(느린 DMA setup, MMIO의 높은 지연, coherence 부재로 인한 소프트웨어 오버헤드)이 가장 극명하게 드러나는 워크로드이므로, CXL.cache의 fine-grained load/store 기반 coherent 통신이 가장 잘 들어맞는 "killer application"이라는 주장이다.

---

## Why this matters to me

CXL 연구 대부분이 "메모리 용량·대역폭 확장"(Type-3, CXL.mem)에 집중된 반면, 이 논문은 정확히 내가 관심 갖는 "device coherence 프로토콜 자체"(CXL.cache, Type-1/2)를 다룬다는 점에서 결이 매우 가깝다. 특히 저자 그룹(Nam Sung Kim UIUC 랩)이 같은 Agilex-7 FPGA로 CXL Type-2 디바이스를 반복적으로 구현해온 계보(Demystifying CXL Type-2 Device MICRO'24, Demystifying CXL Memory MICRO'23 등)의 연장선이라, "FPGA로 실제 CXL coherence 하드웨어를 만들어서 측정한다"는 feasibility-by-building 방법론 자체를 참고할 수 있다. 다만 이 논문은 host-device 1:1 coherence에 국한되고 multi-node/rack-scale coherence는 다루지 않는다 — 내 연구 방향(multi-node coherence)이 이 논문의 자연스러운 확장 지점이 될 수 있다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1809-1810 | PCIe의 두 가지 근본 한계(DMA-only, no coherence) 제시, CXL.cache가 가려져 있던 지점이라 주장 |
| 2 | Background | p.1811-1812 | CXL 프로토콜 3종(io/cache/mem), DCOH·host/device-bias, 기존 PCIe NIC Rx/Tx 데이터패스 |
| 3 | PCIe vs CXL in Networking | p.1812-1813 | 실측: PCIe가 e2e 지연의 45-52% 차지, CXL primitive가 DMA/MMIO 대비 68-85% 낮은 지연 |
| 4 | Facilitating Communication with Coherence | p.1813-1815 | Type-1 CXL-NIC 아키텍처(DMU+PFU), CS/CO/NC-read/write 배분 전략 |
| 5 | Acceleration with Coherent NIC Memory | p.1815-1817 | Type-2 CXL-NIC, 4가지 버퍼 배치 레이아웃, NC-P push-write + KVS co-acceleration |
| 6 | Implementation | p.1817-1818 | Agilex-7 FPGA(400MHz) + BF-3 SmartNIC 비교 대상, 커스텀 패킷 생성기 |
| 7 | Evaluation | p.1818-1820 | Loopback latency/throughput, CC-NIC 대비 37% 우위, KVS 18-39% 개선 |
| 8 | Discussion | p.1820-1821 | CAS 부재로 인한 MPMC 한계, 보안(HPA 기반 access), 가상화(SR-IOV) 이슈 |
| 9 | Related Work | p.1821-1822 | CC-NIC/Dagger/Enzian 등 coherent NIC 계보, DPDK/mTCP/Enso 등 소프트웨어 스택 계보 |
| 10 | Conclusion | p.1822 | coherence·memory·offloading 세 축의 co-design으로 정리 |

---

## Section notes

### §1 Introduction (p.1809-1810)

PCIe NIC의 두 근본 한계를 제시한다: (1) load/store가 아닌 DMA만 지원해 소형 전송(예: 64B 디스크립터)에 setup/completion 오버헤드가 상대적으로 크고, (2) 호스트-디바이스 간 공유 메모리·캐시 일관성이 없어 MMIO(>8μs @512B, <0.3GB/s)에 의존하며 소프트웨어가 explicit flush/pinning/barrier로 일관성을 직접 관리해야 한다. CXL은 PCIe 물리/전기 계층 위에서 하드웨어 관리 unified memory와 cache coherence를 제공해 이 한계를 근본적으로 우회할 수 있다고 주장하며, 특히 지금까지 CXL 연구·산업의 초점이 Type-3(`CXL.mem` 기반 메모리 확장)에 쏠려 `CXL.cache`(Type-1/2의 핵심)가 저평가되어 왔다는 문제의식에서 출발한다.

### §2 Background (p.1811-1812)

CXL 세 프로토콜: `CXL.io`(필수, 초기화·기본 I/O), `CXL.cache`(Type-1/2, device→host coherent 접근, D2H), `CXL.mem`(Type-2/3, host↔device 메모리 접근, H2D+D2D). Device Coherence Engine(DCOH)이 device cache(Host Memory Cache/HMC, Device Memory Cache/DMC)의 상태를 CXL.cache 프로토콜에 따라 관리한다. 디바이스는 요청 시 4가지 힌트 중 하나를 지정한다: `NC`(non-cacheable read/write, 캐시하지 않고 직접 읽기/쓰기), `NC-P`(non-cacheable push, device 쪽에서 host LLC로 밀어넣기), `CS`(cacheable shared, 읽기 전용 캐싱), `CO`(cacheable owned, 배타적 소유권 획득 후 캐싱). Host-bias 모드는 매 접근마다 host cache를 체크해 coherence를 보장하지만 느리고, device-bias 모드는 그 체크를 생략해 device-local 처리를 빠르게 하는 대신 coherence 관리를 소프트웨어가 명시적으로 전환해야 한다. 기존 PCIe NIC의 Rx/Tx 데이터패스(RxD/TxD 링 버퍼, MMIO로 tail index 갱신, DMA로 패킷 전송)를 기준선으로 상세히 서술한다.

### §3 PCIe vs CXL in Networking (p.1812-1813)

DPDK 기반 ping-pong 마이크로벤치마크로 PCIe가 e2e 패킷 지연에 기여하는 비중을 측정한다: 64B 패킷에서 52%, 1500B(MTU)에서 45%. 처리량이 1→64Gbps로 증가하면 이 비중은 57%→52%로 감소(DPDK 배칭이 MMIO 도어벨 빈도를 낮추기 때문). Agilex-7(CXL 디바이스)와 BF-3(DOCA-DMA 기준선) 비교: CXL D2H `NC-write`/`NC-read`가 각각 DOCA-DMA 대비 69%/81% 낮은 지연(64B), `NC-read`는 Agilex 자체 DMA 대비 68% 낮은 지연. 동기화 연산(descriptor fetch, completion signaling)에서는 CXL `NC-read` polling이 PCIe MMIO 대비 각각 85%/82% 지연 감소.

### §4 Facilitating Communication with Coherence (p.1813-1815)

Type-1 CXL-NIC(온디바이스 메모리 없음) 아키텍처: Descriptor Management Unit(DMU, RxD/TxD 처리)과 Packet Forwarding Unit(PFU, 패킷 이동, 1MB SRAM 버스트 버퍼)이 DCOH를 통해 별도 포트로 host memory에 접근한다. Rx 경로에서 `CS-read`로 RxD를 HMC에 프리페치(shared 상태, 읽기 전용이라 ownership 오버헤드 없음), `CO-read`로 패킷 버퍼 주소를 owned 상태로 확보, `NC-write`로 실제 패킷을 device cache를 우회해 직접 host memory에 씀(device cache pollution 방지). Tx 경로는 CXL-NIC이 자체적으로 tail index를 `CO-read` polling으로 이벤트 기반으로 추적해(host의 store가 자동으로 캐시 무효화를 트리거) 불필요한 폴링 트래픽을 없애는 "event-driven Tx datapath"를 구현한다.

### §5 Acceleration with Coherent NIC Memory (p.1815-1817)

Type-2 CXL-NIC은 `CXL.mem`으로 coherent on-device memory("NIC memory")를 host에 remote NUMA node처럼 노출한다. 패킷 버퍼·디스크립터 링을 host memory에 둘지 NIC memory에 둘지 4가지 레이아웃(L1: 둘 다 host / L2: Rx만 NIC / L3: Tx만 NIC / L4: 둘 다 NIC)으로 조합 가능하며, 실측 결과 NIC memory에 버퍼를 두면(L2-L4) L1(모두 host memory) 대비 23-71% 더 높은 지연을 보인다 — CXL 인터커넥트 홉 추가와 host-bias 모드의 coherence 체크 오버헤드 때문. `NC-P`(push-write)는 device가 데이터를 host LLC로 미리 밀어넣어 이후 host의 `ld`가 로컬 캐시 hit이 되게 하는 최적화인데, 무분별하게 쓰면 LLC를 오염시킬 수 있어 (1) adaptive push-write gating(런타임 임계치 기반 on/off)과 (2) post-push write-back(N cycle 뒤 지연된 `NC-write`로 device memory에 다시 씀)의 두 메커니즘으로 완화한다. KVS를 NIC에 직접 offload하는 co-acceleration 사례를 제시 — hot key-value 쌍은 NIC memory에, cold 데이터는 host memory에 두는 tiered 배치를 CXL의 하드웨어 coherence로 투명하게 구현.

### §6 Implementation (p.1817-1818)

Agilex-7 FPGA(CXL 1.1, PCIe 5.0 x16, 2x DDR4-2400, 400MHz 클럭 — FPGA synthesis·CXL IP 제약으로 상한)를 CXL 디바이스로, BF-3 SmartNIC(8x ARM A72 @2.5GHz, DDR4-1600 16GB)을 PCIe 기준선으로 사용. 호스트는 5세대 Xeon 6538Y+(32코어, 60MB LLC, 8x DDR5-4800). 커스텀 패킷 생성기를 FPGA에 직접 구현해 인바운드 트래픽을 재현(오프더셸프 NIC과의 직접 연동이 하드웨어 제약으로 불가능했기 때문).

### §7 Evaluation (p.1818-1820)

Loopback 테스트에서 요청 조합(Rx/TxD/Tx 각각 어떤 CXL request type을 쓸지) 8가지를 비교 — 모두 non-coherent `NC`만 쓰는 조합(Comb.0)이 BF-3 대비 median 46%, p99 49%(64B)/38%(1500B) 지연 감소로 최적. Throughput: Rx `NC-write`/`NC-P`가 이론적 상한(204Gbps @400MHz, 64B/cycle)의 90%, Tx `NC-read`는 62%(D2H 접근이 인터커넥트를 2회 왕복해야 하는 구조적 비대칭 때문). 배칭이 이 격차를 크게 줄임(Rx batch=8에서 95%, Tx batch=32에서 88%). Type-2 평가에서는 latency-optimal 설계(Rx 구조는 NIC memory, Tx 구조는 host memory에 배치)가 CC-NIC 대비 median 37% 낮은 지연을 보임. KVS 애플리케이션 평가에서 Zipfian read-only 워크로드 기준 SNIC 대비 median 18%, p99 39% 지연 감소(uniform 분포에서는 median 20% 증가 — locality 이점이 사라지기 때문).

### §8 Discussion (p.1820-1821)

CXL 1.1은 Compare-and-Swap(CAS) 같은 원자적 연산을 지원하지 않아 Tx 경로에서 여러 CXL-NIC 엔진이 동시에 shared tail index를 갱신하는 MPMC(Multiple-Producer-Multiple-Consumer) 큐를 lock-free하게 구현할 수 없다 — CXL 2.0+ 대응 과제로 명시. 보안 측면에서 CXL 디바이스는 IOMMU 없이도 Host Physical Address(HPA)로 임의 host memory 영역에 D2H 접근이 가능해 out-of-bound access 위험이 있고, CXL 2.0의 HPA 범위 사전 지정 보호는 세밀하지 않다고 지적. 가상화(SR-IOV)에서도 CXL 디바이스 자체가 MMU급 보호 메커니즘을 갖추지 않으면 hypervisor가 이를 직접 에뮬레이션해야 하는 한계를 짚는다.

### §9-10 Related Work & Conclusion (p.1821-1822)

CC-NIC[48]·Dagger[26]·Enzian[7] 등 기존 coherent NIC 연구는 대부분 특정 벤더의 proprietary coherence 프로토콜(예: Intel UPI)에 의존해 범용성이 떨어진다고 지적하며, CXL은 open standard라 이런 한계 없이 fine-grained cache-line 제어(cache line state manipulation) 같은 저수준 최적화 여지가 더 넓다고 차별화한다. 결론적으로 coherence·memory·offloading 세 기여를 "CXL-NIC" 하나의 co-design으로 통합했다고 정리.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "Network Interface Controllers (NICs) represent a particularly compelling, indeed a 'killer application' for harnessing the power of CXL's cache coherence."
- "This focus on memory expansion inadvertently overshadows the potential residing within the CXL.cache protocol."

**Technical concepts:**
- "coherence-driven datapath" / "event-driven Tx datapath"
- "host-bias mode" / "device-bias mode"
- "networking-application co-acceleration"
- "adaptive push-write gating" / "post-push write-back"

**Value language:**
- "hardware-managed unified memory and cache coherence"
- "load/store semantics" (DMA/MMIO 대비 대비어)

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 안 됨):
> - "CXL-NIC" (이 논문이 만든 고유 시스템명 그 자체)
> - "killer application" (이 논문의 핵심 슬로건, 그대로 인용하면 모방으로 보임)
> - "Rambda methodology" (평가 방법론 고유명사, 출처 명시 없이 쓰면 안 됨)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §1, p.1810 | 상용 PCIe NIC 대비 패킷 처리 지연 **49%**, KVS 요청 처리 지연 **39%** 감소 | 논문 전체 핵심 성과 수치, motivation 인용에 |
| §3.1, p.1812 | 64B 패킷 기준 e2e 지연의 **52%**가 PCIe 연산에 기인(1500B에서는 45%) | "PCIe가 소형·지연 민감 워크로드의 진짜 병목"이라는 주장 근거 |
| §3.2, p.1813 | CXL D2H `NC-write`/`NC-read`가 DOCA-DMA 대비 **69%/81%** 낮은 지연(64B) | CXL.cache 대 DMA의 정량적 우위 |
| §3.2, p.1813 | CXL `NC-read` polling이 PCIe MMIO 대비 descriptor fetch 지연 **85%**, completion signaling 지연 **82%** 감소 | 동기화 연산에서의 CXL 이점 |
| §4.3, p.1815 | `CS-read`로 HMC에 캐싱된 RxD 접근이 host memory 접근 대비 **3.3배** 빠름 | fine-grained cache-line 제어의 가치 |
| §7.1, p.1818 | 전체 non-coherent(`NC`) 조합이 BF-3 대비 median **46%**, p99 **49%(64B)/38%(1500B)** 지연 감소 | 최적 request-type 조합 결론 |
| §7.1, p.1819 | Rx `NC-write`/`NC-P`가 이론적 처리량 상한의 **90%**, Tx `NC-read`는 **62%** | Rx/Tx 구조적 비대칭(왕복 횟수 차이) 근거 |
| §7.2, p.1819-1820 | NIC memory에 버퍼 배치 시 host memory 대비 **23-71%** 높은 지연(레이아웃별) | "NIC memory가 무조건 빠르지 않다"는 반직관적 결과 |
| §7.2, p.1820 | latency-optimal Type-2 설계가 CC-NIC 대비 median **37%** 낮은 지연 | 최신 coherent NIC 경쟁작 대비 우위 |
| §7.2, p.1820 | KVS Zipfian read-only에서 SNIC 대비 median **18%**, p99 **39%** 지연 감소 | application-level 성과 |

---

## 🎯 Strategic anchor

> "While CXL offers a versatile suite of protocols (CXL.io, CXL.cache, CXL.mem), much of the initial research and industry momentum has gravitated towards CXL Type-3 devices... This focus on memory expansion inadvertently overshadows the potential residing within the CXL.cache protocol, a key feature of CXL Type-1 and Type-2 devices... Therefore, this work shifts the research lens towards CXL Type-1 and Type-2 devices." (§1, p.1810)

→ **본인 활용**: 면담·자소서에서 "CXL 연구가 CXL.mem(메모리 확장)에 편중되어 CXL.cache(device coherence)가 상대적으로 덜 탐구됐다"는 이 논문의 문제의식을 인용하며, 내가 보려는 지점(multi-node coherence, device-to-device 일관성 프로토콜 자체)이 바로 이 "가려진 영역"의 자연스러운 다음 단계임을 설명하는 데 쓸 수 있다.

---

## Connection to my research direction

| 차원 | 이 paper | 본인 방향 |
|---|---|---|
| Scope | 단일 host ↔ 단일 CXL 디바이스(NIC) 간 coherence | multi-node/rack-scale coherence — 여러 host·디바이스 간 상호작용 |
| Mechanism | CXL.cache의 4가지 request type(NC/NC-P/CS/CO)을 데이터패스 단계별로 배분하는 설계 공간 | 유사한 fine-grained coherence-state 제어를 노드 간 프로토콜로 확장하는 문제 |
| Workload | 네트워킹(패킷 처리) + KVS 하나의 co-acceleration 사례 | 워크로드 특정보다 coherence protocol/hardware 자체의 원리 |
| Open space | single-device MPMC(CAS 부재)까지만 다룸, 노드 간 확장은 미다룸 | 바로 이 지점 — 이 논문의 단일 디바이스 coherence를 다중 노드로 일반화하는 문제가 내 연구의 출발점이 될 수 있음 |

이 논문은 "CXL.cache가 메모리 확장(Type-3)에 가려진 저평가 영역"이라는 프레이밍을 NIC이라는 구체적 I/O 디바이스로 증명한 사례다. 내 연구 방향은 이걸 한 단계 더 일반화해서, 단일 host-device coherence가 아니라 **여러 노드·여러 디바이스가 함께 참여하는 coherence 도메인**에서 이런 fine-grained request-type 설계 공간이 어떻게 바뀌는지(예: CAS 같은 원자 연산이 다중 host 환경에서 어떻게 구현되어야 하는지)를 다룬다는 점에서 명확히 차별화된다.

---

## Open questions / gaps

- [ ] CXL 1.1의 CAS 부재로 인한 MPMC 큐 문제를 저자들도 "CXL 2.0+ future work"로 명시적으로 남겨둠 — 다중 producer/consumer 간 원자성 문제가 multi-node 환경으로 가면 더 근본적으로 재검토될 필요
- [ ] host-device 1:1 관계만 평가 — 여러 CXL 디바이스가 같은 host coherence 도메인을 공유하거나, 여러 host가 하나의 CXL 스위치/디바이스를 공유하는 시나리오는 다루지 않음
- [ ] 보안(HPA 기반 out-of-bound access, IOMMU급 보호 부재)을 "future work/open challenge"로만 짚고 실제 구현·평가는 하지 않음
- [ ] FPGA 400MHz 클럭 제약으로 절대 처리량 수치는 향후 ASIC 구현에서 달라질 가능성을 저자 스스로 인정 — 정성적 트렌드(NC-write/NC-P 우위 등)는 유효하나 절대 latency/throughput 수치는 하드웨어 세대 종속적

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [48] | Schuh et al., "CC-NIC: A Cache-Coherent Interface to the NIC", ASPLOS'24 | 이 논문의 직접 비교 baseline(UPI 기반), 설계 철학 차이(symmetric NUMA coherence 가정 vs 이 논문의 hardware-asymmetry-aware 설계) 이해 필수 |
| ☐ | [22] | Ji et al., "Demystifying a CXL Type-2 Device: A Heterogeneous Cooperative Computing Perspective", MICRO'24 | 같은 저자 그룹의 선행 Type-2 CXL 디바이스 분석 — 이 논문의 방법론적 뿌리 |
| ☐ | [54] | Sun et al., "Demystifying CXL Memory with Genuine CXL-Ready Systems and Devices", MICRO'23 | 같은 랩의 실제 CXL 하드웨어 measurement 계보, 내 feasibility-by-building 접근과 방법론적으로 가장 가까움 |
| ☐ | [53] | Sun et al., "M5: Mastering Page Migration and Memory Management for CXL-based Tiered Memory Systems", ASPLOS'25 | tiered CXL memory 관리 — device-bias/host-bias 전환 문제의 소프트웨어 계층 대응판 |
| ☐ | [63] | Zhong et al., "Managing Memory Tiers with CXL in Virtualized Environments", OSDI'24 | §8 가상화 논의에서 언급된 CXL 메모리 티어 관리, VM 환경에서의 coherence 확장 문제 |
| ☐ | [21] | Ji et al., "STYX: Exploiting SmartNIC Capability to Reduce Datacenter Memory Tax", USENIX ATC'23 | 같은 저자의 SmartNIC-메모리 상호작용 선행 연구 |
| ☐ | [7] | Enzian (관련 상세 미기재, 본문 §9 인용) | Proprietary coherence 기반 host-FPGA coherent 시스템의 원조 격 — CXL-NIC과의 설계 철학 비교 |

---

## Personal annotations

<본인 메모 영역>
