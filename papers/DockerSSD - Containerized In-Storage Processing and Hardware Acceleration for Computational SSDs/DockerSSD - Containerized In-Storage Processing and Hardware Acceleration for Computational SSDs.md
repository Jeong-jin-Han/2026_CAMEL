---
title: "DockerSSD: Containerized In-Storage Processing and Hardware Acceleration for Computational SSDs"
aliases: [DockerSSD]
description: "컨테이너화 ISP를 SSD 내부에서 실행하는 모델 (arXiv 제목은 'Containerized In-Storage Processing and Computing-Enabled SSD Disaggregation'으로 IEEE Micro 확장본)"
venue: HPCA
year: 2024
arxiv: "2506.06769"
tier: deep
status: done
tags: [paper, cluster/isc, topic/in-storage-computing, topic/virtualization, topic/disaggregation, venue/hpca, year/2024]
---

# DockerSSD: Containerized In-Storage Processing and Hardware Acceleration for Computational SSDs

> **HPCA 2024** · cluster/isc · Source: [DockerSSD - Containerized In-Storage Processing and Hardware Acceleration for Computational SSDs.pdf](<DockerSSD - Containerized In-Storage Processing and Hardware Acceleration for Computational SSDs.pdf>)

**저자**: Miryeong Kwon, Donghyun Gouk, Eunjee Na, Jiseon Kim, Junhee Kim, Hyein Woo, Eojin Ryu, Hyunkyu Choi, Jinwoo Baek, Hanyeoreum Bae (Panmnesia, Inc.), Mahmut Kandemir (Penn State), Myoungsoo Jung (Panmnesia / KAIST)

> [!note] arXiv 제목 vs 노트 제목
> arXiv 2506.06769의 실제 제목은 **"Containerized In-Storage Processing and Computing-Enabled SSD Disaggregation"** 이며, 본문 footnote에 "extended version of the original paper accepted by IEEE Micro"라고 명시되어 있다. References [14]에서 원본 HPCA'24 "DockerSSD: Containerized in-storage processing and hardware acceleration for computational SSDs"를 직접 인용하므로, 본 노트는 그 DockerSSD 계보의 확장본임이 확인된다.

---

## TL;DR

DockerSSD는 **OS-level virtualization(Docker)을 SSD 펌웨어 내부로 가져와** ISP(In-Storage Processing)를 컨테이너 형태로 실행하는 모델이다. 두 가지 핵심 기술 — **Ethernet over NVMe (Ether-oN)** 로 네트워크 기반 ISP 관리/통신을 제공하고, **Virtual Firmware (Virtual-FW)** 로 최소 OS 기능 + 컨테이너 환경을 펌웨어에 내장 — 을 통해, 사용자는 소스 수정 없이 기존 Docker 이미지를 SSD 위에서 `docker pull`/`docker run`으로 실행할 수 있다. 각 DockerSSD에 IP를 부여해 호스트로부터 **disaggregation된 computing-enabled storage pool**을 형성하며, I/O-intensive 워크로드에서 최대 **2.0×**, 분산 LLM inference에서 평균 **7.9×** 성능 향상을 보인다.

## 문제 & 동기

- **ISP의 미채택 원인**: 기존 ISP 모델은 (1) 정적 kernel 또는 vendor-specific API에 의존해 application을 offload하려면 상당한 source-level 수정이 필요하고, (2) host-side daemon을 새로 개발해야 하며, (3) SSD가 file layout/block 정보를 모른 채 데이터를 처리해 unpredictable result 및 resource protection 문제(host user가 처리 중 데이터를 수정 가능)를 낳는다.
- 산업계는 ISP 자원을 storage array/node로 **disaggregation**하여 server-side 연산 부담을 줄이려 하지만, SSD를 다양한 application 요구에 맞추기 어렵다.
- 저자들이 정의한 ISP의 **5대 challenge**: manual ISP implementation, file layout 무시, kernel context switching, device reliance, data vulnerability.
- **Performance Impact 관찰 (Fig.3)**: P.ISP(Programmable-ISP)는 host 대비 Storage latency를 50% 줄이지만(Storage가 전체의 38% 차지), **Communicate**(host↔ISP 통신/동기화)가 P.ISP 지연의 43%를 차지하여 end-to-end는 오히려 1.4× 증가. 즉 통신 오버헤드 제거가 핵심.

## 핵심 통찰

1. **ISP를 host-independent하게 만들면 통신 오버헤드를 제거할 수 있다** → 컨테이너화 + 자율 실행. ISP를 가상화하면 secure, portable sandbox로 동작.
2. **Container는 emulation SW 없이 isolated 환경을 cgroups/namespaces로 구성** → SSD의 제한된 자원에도 lightweight하게 올릴 수 있다. 단, container의 self-contained 특성상 (a) flash 데이터 처리용 specialized interface와 (b) 동적 container 구성을 위한 firmware redesign이 필요.
3. **NVMe 위에 Ethernet을 overlay**하면 익숙한 Docker CLI/HTTP REST API를 그대로 쓰면서 SSD를 독립 네트워크 노드로 취급 가능 (IP 부여).
4. **Full OS 대신 system call emulation(function wrapper)** 으로 ISP system call 비용을 function management 수준으로 낮춰, context switch 없이 컨테이너를 펌웨어에서 실행.

## 설계 / 메커니즘

DockerSSD = **Ether-oN (system driver)** + **Virtual-FW (firmware)** (Fig.4).

### Ether-oN (Ethernet over NVMe)

- NVMe를 가상화해 socket-based Ethernet 통신을 PCIe 위에 구현하는 **kernel driver**. 각 endpoint에 IP를 할당, host는 `docker-cli`로 Virtual-FW에 ISP 요청을 issue.
- Host에 virtual network adapter 생성 → Ethernet packet을 NVMe command로 번역해 DockerSSD로 전송 (Fig.6).
- 두 가지 **NVMe vendor-specific command** (opcode **0xE0~0xE1**): `transmit`, `receive` frame. Ether-oN은 frame에서 `struct sk_buff` (4KB-aligned kernel page)를 추출, PRP에 주소 채워 NVMe command로 submit.
- **Inbound 지원 (asynchronous upcall)**: PCIe device는 host로 NVMe command를 발행할 수 없으므로, 커널 초기화 시 receive frame용 NVMe command를 미리 SQ에 pre-submit. ISP-container가 Ethernet frame을 보내면 DockerSSD가 outstanding command를 완료시켜 upcall 실현. 예비 연구 결과 **SQ당 4개 pre-allocated command**가 효율/자원 균형점.

### Virtual-FW (Docker-enabled firmware)

- 기존 I/O service path **HIL ⇒ ICL ⇒ FTL** 사이에 3개 handler 삽입 (Fig.7): **thread handler(65 syscalls)**, **I/O handler(43)**, **network handler(25)**.
- **2개 page-granular partition**: privileged 모드의 **FW-pool** (handler용, MPU로 보호) + **ISP-pool** (call args/데이터). Privileged 모드가 ISP-pool에 직접 접근 → data copy/mode-switch 오버헤드 제거.
- **System call emulation**: full OS 대신 lightweight function wrapper로 syscall override. 예: glibc `open`은 내부적으로 `openat`을 호출 — Virtual-FW는 `openat`만 구현. userland 복귀 시 context switch가 없어 mode boundary를 회피, latency 절감.
- **mini-docker**: Docker command 106개 중 핵심 **11개**(`docker pull/rmi/create/run/start/stop/restart/kill/rm/logs/ps`)를 지원하는 streamlined 구현. `dockerd`/`containerd`/`runc` 핵심 기능 통합, host `docker-cli`와 HTTP로 통신.

### λFS (Lambda filesystem) — Backend Media Management

- EXT4 기반, host file 구조와 통합되어 file/directory 공유 (Fig.4b).
- 미디어를 **2개 NVMe namespace**로 분할: **private-NS**(host로부터 격리; `/images/`, `/rootfs/` 등 보호 대상) + **sharable-NS**(host와 ISP-container 공유; in/out 데이터).
- **inode lock (in-memory)**: ISP-container가 file binding 시 Ether-oN으로 host inode cache와 동기화. VFS에 virtual file system의 inode reference counter 추가 — open 시 증가/close 시 감소, counter==0일 때만 file 접근 허용해 concurrent access 충돌 해결. 동기화 전용이므로 persistence 없음(power failure 시 host가 filesystem 복원 후 ISP-container를 초기 상태에서 재시작).

### Resource Disaggregation & Computing-enabled Storage Pool

- Ether-oN+Virtual-FW로 DockerSSD가 IP를 얻어 독립 노드로 동작 → 여러 DockerSSD를 PCIe switch로 연결해 **array pool / computing-enabled storage pool** 구성 (Fig.8a). `docker-compose`/Kubernetes로 오케스트레이션.
- 두 offloading 방식: (1) 독립 application을 각 DockerSSD에 분산(autonomous), (2) **선호 방식** — 여러 DockerSSD를 distributed computing system으로 묶음.
- **분산 LLM inference use-case**: pipeline/data/tensor parallelism 지원. LLM의 **KV cache**(K/V vector 재사용으로 $O(n^2)\to O(n)$)를 host DRAM 대신 **DockerSSD의 flash에 직접 저장** → cache pollution, mode switch, 불필요한 data copy 회피. TorchServe로 모델 serving.

> [!quote]- 📄 원문 표현 (paper)
> - "We propose DockerSSD, an ISP model leveraging OS-level virtualization and lightweight firmware to enable containerized data processing directly on SSDs." (p.1, Abstract)
> - "It achieves up to 2.0× better performance for I/O-intensive workloads, and 7.9× improvement in distributed LLM inference." (p.1, Abstract)
> - "We propose Ethernet over NVMe (Ether-oN), a kernel driver enabling network-based ISP management and direct communication between host users and SSDs." (p.2)
> - "Virtual-FW maintains ISP system call execution costs comparable to function management costs. It creates ISP-containers from existing Docker images and executes them without the overhead of a full OS." (p.2)
> - "Our findings reveal that Storage accounts for 38% of the total execution time ... this improvement is offset by a 1.4× increase in overall end-to-end latency, primarily due to Communicate, which constitutes 43% of P.ISP latency." (p.3)

## 평가

- **Prototype**: 16nm FinFET FPGA에 multi-core RISC-V (in-order) + NVMe HW IP. 4개 RISC-V core가 Virtual-FW 실행, AXI/TileLink bus. Ether-oN/λFS 3.4K LOC, firmware/storage stack 7.5K LOC 수정. Frontend 2.2GHz/2GB DRAM, host 3.8GHz CPU/64GB DDR4, NVMe SSD 48 MLC flash/12 channel.
- **gem5 full-system simulator** + cycle-accurate SSD simulator(prototype backend과 cross-validate)로 다양한 구성 평가.
- **Virtual-FW의 binary size를 Linux 대비 83.4× 축소** (Fig.10) → embedded processor에 적합.
- **워크로드 (Table 2)**: 6개 벤치마크 13개 워크로드 — embed(DLRM embedding), mariadb(TPC-H), rocksdb(KV), pattern(text mining find/line/word), nginx(web), vsftpd(file server). 각 10회 실행.
- **비교 모델**: Host(non-ISP baseline), P.ISP-R(이전 연구 [3] interface), P.ISP-V(NVMe vendor-specific [4]), D-Naive(별도 processor/controller complex), D-FullOS(같은 complex에 full Linux), **D-VirtFW(=DockerSSD, Virtual-FW)**.

### 주요 수치

- **Overall (Fig.11)**: DockerSSD(D-VirtFW)는 P.ISP-R/V를 **1.6×**, D-Naive를 **1.8×**, D-FullOS를 **1.6×** 능가. 워크로드별 최대 약 **2.1×~2.2×** (pattern-find/word). λFS의 backend 접근으로 **LBA-set 핸드셰이크 제거 → 8.4% latency 감소**, rootfs에 실행 파라미터 pre-package로 **Kernel-ctx 제거 → P.ISP-R/V 대비 30.9% 향상**.
- P.ISP-V는 RPC 제거·network response 불필요로 P.ISP-R보다 13.7% 낮은 latency. D-Naive는 D-FullOS 대비 12.8% slowdown (processor↔controller complex 간 빈번한 data transfer).
- **분산 LLM inference**: 8개 LLM (lamda-137B, gpt3-175B, jurassic-178B, pangu-200B, gopher-280B, turing-530B, palm-540B, megatron-1T), pool 16~128 DockerSSD.
  - D-Cache는 H-Cache 대비 **7.9×**, H-NoCache 대비 **3.2K×** 향상 (Fig.12). DockerSSD는 host와 달리 flash를 local memory처럼 접근(SW swap 불필요).
  - KV cache 효과: H-Cache는 H-NoCache 대비 **421×**, D-Cache는 D-NoCache 대비 **4.6×**.
  - DockerSSD 분산 inference는 host 대비 단 **1.7× degradation** (2.2GHz vs 3.8GHz 연산 능력 차이 기인).
  - **Sequence length sensitivity (Fig.13)**: 짧은 sequence는 compute-bound라 host의 빠른 clock(roughly 60% of host perf)이 유리하나, lamda는 length **256**, megatron은 **1024**에서 crossover → 이후 DockerSSD가 host를 앞서고 speedup이 약 **9.5×**로 수렴. Optimal parallelism: NoCache는 per-layer 연산이 무거워 pipeline parallelism, Cache는 tensor parallelism이 최적.
  - **Batch size sensitivity**: batch 증가 시 KV cache memory가 linear 증가해 bottleneck → 긴 sequence엔 작은 batch 선호. D-Cache는 H-Cache 대비 최대 **1.3×** (modest).

> [!quote]- 📄 원문 표현 (paper)
> - "DockerSSD outperforms host system and leading ISP models [3, 4, 30] by 1.3× and 1.8×, respectively. In addition, the compute-enabled storage pool enhances LLM performance by an average of 7.9×." (p.2)
> - "FW-level containerization (DockerSSD). D-VirtFW combines the advantages of full-fledged application execution while avoiding the software stack overhead of D-FullOS and the hardware inefficiencies of D-Naive. It delivers significant performance improvements, outperforming P.ISP-R/V, D-Naive, and D-FullOS by 1.6×, 1.8×, and 1.6×, respectively." (p.9)
> - "Expanding memory capacity to enable KV caching greatly boosts performance. Specifically, H-Cache achieves a 421× performance gain over H-NoCache, while D-Cache achieves a remarkable 4.6× improvement over D-NoCache." (p.10)
> - "As a result, D-Cache outperforms H-Cache by 7.9× and H-NoCache by 3.2K× in distributed inference." (p.10)
> - "Virtual-FW reduced the Linux binary size by 83.4×, making it suitable for embedded processors." (p.8)

---

## 섹션 노트

- **High Performance SSDs (Preliminary, p.2)**: 현대 SSD = frontend(embedded multi-core + DRAM, PCIe endpoint) + backend(channel별 flash via FMC). embedded processor가 2GHz/8core 또는 500MHz/32core 수준으로 frontend 연산 능력이 커짐. NVMe는 SQ/CQ, PRP(physical region page), MSI로 block I/O 처리. Firmware stack = HIL(NVMe control)/ICL(DRAM cache)/FTL(LBA→physical).
- **OS-Level Virtualization (p.3)**: container는 cgroups+namespaces로 isolated 환경. Docker stack = `dockerd`(daemon, HTTP REST API)/`containerd`(image/container engine)/`runc`(namespace·cgroup·container 생성). 이미지는 blob을 retrieve→unpack/merge(lower dir, overlay)→rootfs(+upper dir) 생성. cgroups/namespaces는 OS kernel feature이지 container 고유가 아님.
- **Challenges in ISP (p.3)**: Summarizer[8], KAML[7](KV DB), Willow[3], Biscuit[4] 등은 정적 kernel/vendor API 의존. → 5대 challenge 제시.
- **ISP Containerization (p.4)**: 3대 기술 challenge — (1) storage+network stack 호환 통신 interface, (2) storage pool 전반의 consistency, (3) firmware의 container 구조 인식·저오버헤드 실행.
- **Ether-oN (p.5)**: TCP/IP 위에 intranet. 두 NVMe vendor command + asynchronous upcall.
- **Docker-enabled firmware (p.6)**: I/O handler는 ISP-container의 I/O만 처리(무거운 block layer 불필요), λFS의 path walking(LBA→filename), I/O node caching 제공.
- **Resource disaggregation (p.7)**: storage pool, 분산 LLM inference 아키텍처(encoder/attention K·V/FFN).

## 핵심 용어

- **ISP (In-Storage Processing)**: 데이터를 flash 근처에서 처리해 host↔storage data movement 최소화하는 모델.
- **Ether-oN (Ethernet over NVMe)**: NVMe 위에 socket-based Ethernet을 overlay한 kernel driver. transmit/receive vendor command(0xE0/0xE1) + async upcall로 양방향 통신, 각 SSD에 IP 부여.
- **Virtual-FW (Virtual Firmware)**: minimal OS feature + container 환경을 내장한 lightweight firmware stack. HIL⇒ICL⇒FTL path에 thread/I/O/network handler 삽입, system call emulation으로 full OS 회피.
- **ISP-container**: 기존 Docker image로부터 Virtual-FW가 생성·실행하는 컨테이너. NVMe namespace로 memory isolation.
- **mini-docker**: Docker command 11/106개를 지원하는 firmware 내 streamlined Docker 구현.
- **λFS (Lambda filesystem)**: EXT4 기반 flash file 관리. private-NS/sharable-NS 분할 + in-memory inode lock으로 host-ISP concurrent access 동기화.
- **KV cache**: LLM의 K/V vector를 재사용해 $O(n^2)\to O(n)$ 으로 연산 절감. DockerSSD는 flash에 직접 저장.
- **Computing-enabled storage pool**: 여러 DockerSSD를 PCIe switch로 묶어 disaggregation한 분산 연산 풀.

## 강점 · 한계 · 열린 질문

**강점**
- **소스 수정 없이 기존 Docker 생태계 재사용**: `docker pull/run`만으로 ISP 실행 → 채택 장벽 대폭 완화.
- Full OS 대신 system call emulation + mini-docker로 **embedded processor에 현실적인 footprint** (Linux 대비 83.4× 작음).
- **KV cache를 flash에 직접 저장**해 host의 SW swap 오버헤드를 제거 — 긴 sequence LLM inference에서 host를 앞서는 명확한 사용 사례.
- private/sharable-NS와 inode lock으로 **data vulnerability/isolation** challenge를 정면 대응.

**한계**
- Prototype은 FPGA RISC-V(2.2GHz)로 host(3.8GHz) 대비 연산 능력이 낮아 **compute-bound 짧은 sequence/작은 모델에선 host보다 느림** (crossover 이전). 이득은 memory-intensive·long-sequence 시나리오에 한정.
- LLM 평가의 큰 수치(7.9×, 3.2K×)는 **analytical KV cache model + simulator** 기반이며 실제 trillion-scale 모델 end-to-end 실측이 아님.
- inode lock이 persistence가 없어 **power failure 시 ISP-container를 초기 상태에서 재시작** — 장기 실행 stateful 작업엔 제약.
- mini-docker가 106개 중 11개 command만 지원 → 복잡한 orchestration/이미지 기능엔 제약 가능.

**열린 질문**
- 다수 DockerSSD가 PCIe switch fabric을 공유할 때 Ether-oN(NVMe로 번역된 Ethernet)의 aggregate bandwidth/혼잡 특성은? SQ당 4 pre-allocated command가 large pool에서도 충분한가?
- λFS의 inode lock이 수십~수백 노드 규모에서 동기화 오버헤드/scalability 한계를 보이지 않는가?
- Virtual-FW의 security sandbox(NVMe namespace isolation)가 실제 multi-tenant 환경의 side-channel/자원 격리를 충분히 보장하는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. DockerSSD가 해결하려는 기존 ISP의 핵심 병목은 무엇인가?
> ISP는 host↔storage data movement를 줄이지만(Storage가 전체 실행시간의 38%, P.ISP가 이를 50% 절감), host↔ISP 간 **Communicate(통신/동기화) 오버헤드가 P.ISP latency의 43%**를 차지해 end-to-end가 오히려 1.4× 증가한다. DockerSSD는 ISP를 host-independent·autonomous하게 만들어 이 통신 오버헤드를 제거하는 것이 목표.

> [!question]- Q2. Ether-oN은 어떻게 PCIe 위에서 Ethernet 통신을 구현하나?
> NVMe를 가상화해 host에 virtual network adapter를 만들고, Ethernet packet(`struct sk_buff`, 4KB-aligned page)을 vendor-specific NVMe command(opcode 0xE0 transmit / 0xE1 receive)로 번역한다. PCIe device가 host로 command를 발행 못하는 문제는 **asynchronous upcall** — receive frame용 NVMe command를 SQ에 미리 pre-submit(SQ당 4개)해 두고 ISP-container가 frame을 보내면 그 outstanding command를 완료시키는 방식 — 으로 해결한다.

> [!question]- Q3. Virtual-FW가 full OS를 쓰지 않고도 컨테이너를 실행할 수 있는 이유는?
> **System call emulation**: 모든 syscall 대신 lightweight function wrapper로 핵심만 구현(예: glibc `open`이 부르는 `openat`만). userland 복귀 시 context switch가 없어 mode boundary를 회피하므로 syscall 비용이 function management 수준으로 떨어진다. 또한 privileged FW-pool / ISP-pool을 page 단위로 분리하되 privileged 모드가 ISP-pool에 직접 접근해 data copy·mode switch를 없앤다. 결과적으로 binary가 Linux 대비 83.4× 작다.

> [!question]- Q4. λFS는 host와 ISP-container의 동시 접근 충돌을 어떻게 막나?
> 미디어를 private-NS(host 격리, /images·/rootfs 보호)와 sharable-NS(공유 데이터)로 나눈다. 공유 데이터 충돌은 **in-memory inode lock**으로 처리: VFS inode에 reference counter를 추가해 open 시 증가/close 시 감소, counter==0일 때만 접근 허용하고 ISP-container가 접근 권한을 얻으면 VFS가 inode cache를 invalidate해 최신 상태를 참조하게 한다. 이 lock은 동기화 전용이라 persistence가 없다.

> [!question]- Q5. 분산 LLM inference에서 DockerSSD가 host를 능가하는 핵심 메커니즘은?
> LLM의 KV cache(K/V vector 재사용으로 연산을 $O(n^2)\to O(n)$)를 **host DRAM의 SW swap 대신 DockerSSD flash에 직접 저장**한다. host는 DRAM 용량 부족 시 Linux swap(외부 SSD)으로 KV cache를 확장하는데 이 오버헤드가 크다. DockerSSD는 flash를 local memory처럼 접근해 이를 제거 → D-Cache가 H-Cache 대비 7.9×, H-NoCache 대비 3.2K× 향상.

> [!question]- Q6. DockerSSD가 항상 host보다 빠른가? 아니라면 언제 느린가?
> 아니다. Prototype의 RISC-V는 2.2GHz로 host 3.8GHz보다 느려, compute-bound인 **짧은 sequence·작은 모델**에선 host의 약 60% 성능에 그친다. crossover는 lamda(137B)는 sequence length 256, megatron(1T)은 1024에서 발생하며, 그 이상 길이의 memory-intensive 시나리오에서야 DockerSSD가 앞서고 speedup이 약 9.5×로 수렴한다.

> [!question]- Q7. 평가에 쓰인 6개 비교 모델의 차이는?
> Host(non-ISP baseline), P.ISP-R(이전 연구[3]의 interface 사용), P.ISP-V(NVMe vendor-specific command[4]로 interface 오버헤드 최소화), D-Naive(processor와 controller complex를 분리), D-FullOS(같은 complex에 full Linux), D-VirtFW(=DockerSSD, Linux 대신 Virtual-FW). DockerSSD는 P.ISP-R/V를 1.6×, D-Naive를 1.8×, D-FullOS를 1.6× 능가한다.

> [!question]- Q8. arXiv 버전과 원본 HPCA'24 논문의 관계는?
> arXiv 2506.06769는 제목이 "Containerized In-Storage Processing and Computing-Enabled SSD Disaggregation"이며 footnote에 "extended version of the original paper accepted by IEEE Micro"로 명시된다. References [14]에서 원본 HPCA'24 DockerSSD 논문을 인용하므로, 이는 DockerSSD 연구의 확장본(특히 disaggregation·분산 LLM inference 부분 확장)이다.

## 🔗 Connections

[[In-Storage Computing]] · [[HPCA]] · [[2024]]

관련:
- [[Conduit]] — disaggregation / near-storage computing 계보
- [[Computational Storage]], [[CXL]] — memory/storage disaggregation 인접 주제
- [[Distributed LLM Inference]], [[KV Cache]] — 핵심 use-case
- [[Docker]] / [[OS-level Virtualization]] — 기반 기술

## References worth following

- [3] Seshadri et al., **Willow: A User-Programmable SSD**, OSDI'14 — P.ISP-R 비교 baseline.
- [4] Gu et al., **Biscuit: A framework for Near-data Processing of Big Data Workloads**, ISCA'16 — P.ISP-V 비교 baseline.
- [7] Jin et al., **KAML: A flexible, high-performance key-value SSD**, HPCA'17.
- [8] Koo et al., **Summarizer: trading communication with computing near storage**, MICRO'17.
- [14] Gouk, Kwon, Bae, Jung, **DockerSSD: Containerized in-storage processing and hardware acceleration for computational SSDs**, HPCA'24 — 본 논문의 원본.
- [18] Kwon et al., **Vigil-KV: HW-SW Co-Design ... Key-Value stores**, USENIX ATC'22.
- [28] Narayanan et al., **Megatron-LM: Efficient large-scale LM training on GPU clusters**, SC'21 — megatron-1T 모델.
- [54] Isaev et al., **Calculon: ... co-design of systems and large language models**, SC'23 — 분산 inference simulator 기반.

## Personal annotations

<본인 메모 영역>
