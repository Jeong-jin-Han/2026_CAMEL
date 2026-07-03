---
title: "DockerSSD: Containerized In-Storage Processing and Hardware Acceleration for Computational SSDs"
aliases: [DockerSSD]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# DockerSSD: Containerized In-Storage Processing and Hardware Acceleration for Computational SSDs

> **Source PDF**: [DockerSSD.pdf](DockerSSD.pdf)
> **Authors**: Donghyun Gouk, Miryeong Kwon, Hanyeoreum Bae, Myoungsoo Jung (KAIST CAMEL Lab · Panmnesia, Inc.) — Gouk·Kwon 공동 1저자
> **Venue / Year**: IEEE HPCA 2024 (2024 IEEE International Symposium on High-Performance Computer Architecture)
> **arXiv / DOI**: 10.1109/HPCA57654.2024.00036
> **Length**: 16 pages (p.379–394)
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 연구 계보 이해. 'storage에 compute를 넣는' in-storage compute 라인이 CXL-SSD와 합류하는 지점을 파악하고, **firmware/HW를 실제로 빌드해 feasibility를 증명**하는 CAMEL식 접근법(prototype+simulator 병행)을 내 메모리 시스템 아키텍처 방향에 대조.

계보: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md) — Phase 3(2024) · 인접(in-storage) 라인.

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

DockerSSD는 **SSD firmware 안에서 Docker container를 그대로 실행**하는 fully flexible in-storage processing(ISP) 모델이다. 기존 ISP는 vendor가 미리 만든 static kernel/API에 사용자가 application을 수동으로 잘라 offload해야 했고(=manual, file-layout 무지, kernel context switch, device dependence, data vulnerability의 5대 challenge), 그 결과 system call·file access가 많은 workload에서 오히려 느려졌다. DockerSSD는 (i) NVMe 위에 Ethernet을 얹은 **Ether-oN** 통신 드라이버, (ii) minimal OS feature + system call emulation + mini-docker를 담은 **Virtual-FW**(Docker-enabled firmware), (iii) path walking·TCP/IP를 hardware로 가속하는 **IIO-X / NET-X** 두 accelerator로 구성된다. source 수정 없이 기존 Docker image를 `docker pull/run`으로 SSD에 내려 ISP-container로 실행하며, 16nm FinFET FPGA + multi-core RISC-V로 실제 prototype을 만들고 gem5 full-system 시뮬레이션으로 설계공간을 검증해 state-of-the-art ISP 대비 **2.0× faster, power 1.6× / energy 2.3× 절감**을 보였다.

---

## Core thesis

> "We propose DockerSSD, a fully flexible in-storage processing (ISP) model that can run a variety of applications near flash without their source-level modification." (Abstract, p.379)

추가 설명: ISP를 vendor-specific static kernel offload가 아니라 **OS-level virtualization(container)**로 재정의한다. Container가 self-governing execution object로서 storage 안에서 데이터를 "where they are"에서 처리하면, host 개입(kernel context 전달·데이터 이동)이 사라져 system call/file access가 많은 실제 workload에서도 ISP가 이득을 낸다. 핵심은 "storage intelligence를 기존 computing environment와 harmonize"하는 것 — 즉 특별한 toolchain·source 수정 없이 기존 Docker ecosystem 그대로.

---

## Why this matters to me

내 방향은 메모리 시스템 아키텍처(CXL/coherence)이고 DockerSSD는 직접적으로 CXL 논문은 아니지만, **CAMEL Lab이 'compute를 데이터 근처로 옮긴다'는 문제를 firmware+HW 레벨에서 실제로 빌드해 증명하는 방식**의 대표 사례다. (1) NVMe 위에 Ethernet을 얹어 기존 프로토콜을 재활용해 새 통신 채널을 만드는 발상(Ether-oN)은, CXL이 기존 PCIe/coherence 프로토콜 위에 memory semantic을 얹는 발상과 구조적으로 닮아 있어 "기존 interconnect를 확장해 새 semantic을 얻는다"는 패턴 학습에 좋다. (2) prototype(FPGA)과 cycle-accurate simulator(gem5+Amber)를 병행해 feasibility를 증명하는 방법론이 내가 선호하는 feasibility-by-building과 정확히 일치한다. (3) in-storage compute 라인이 CXL-SSD로 합류하는 계보상 landmark라 배경 지식으로 필수.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| I | Introduction | p.379–380 | 3대 통합 challenge(container interface / firmware redesign / non-disruptive I/O) + 3대 기여(Ether-oN, Virtual-FW, HW accel) |
| II | Background | p.380–381 | High-perf SSD 구조(frontend/backend, HIL·ICL·FTL), NVMe protocol, OS-level virtualization(cgroup·namespace·Docker) |
| III | Challenge Classification and Analysis | p.381–382 | 기존 ISP의 5대 challenge, Programmable-ISP의 실측 성능 저하(Communicate가 43% latency 차지) |
| IV | Cross-Layer Optimized ISP Containerization | p.383 | DockerSSD 3-component overview, λFS로 private/sharable NS 분리해 data vulnerability 해결 |
| V | Interface Design and Firmware Virtualization | p.384–385 | Ether-oN(transmit/receive NVMe frame), Virtual-FW(FW-pool/ISP-pool, system call emulation, mini-docker) |
| VI | Container Service Hardware Accelerations | p.385–386 | bottleneck 분석(path walk 76.1%, TCP/IP 73.2%) → IIO-X(inode-AT/dir-parser/str-matcher), NET-X(Rx/Tx/metadata) |
| VII | Evaluation | p.387–390 | prototype+gem5, D-CoDesign이 Host 대비 34% 빠르고 energy 54.3% 절감 |
| VIII | Related Work | p.391 | AppSpecific-ISP vs Programmable-ISP 분류, CSD(SmartSSD) 비교, Table III |
| IX | Conclusion | p.391 | 2.0× faster, power 1.6× / energy 2.3× 절감 재확인 |

---

## Section notes

### §I Introduction (p.379–380)
ISP는 emerging data analytics에서 host↔storage 데이터 이동을 줄이는 model이지만, 다양한 application을 수용하는 **fully flexible ISP model**을 만드는 게 핵심 난제다. Vendor가 ISP runtime/API를 design 단계에 노출해야 하는데, 내부 HW·firmware 개발환경을 다 공개하는 건 사실상 불가능하고, 잘 구조화된 API가 있어도 기존 application을 offload하려면 상당한 source 수정이 필요하며 host-side daemon도 새로 붙여야 한다. DockerSSD는 modern SSD에 **OS-level virtualization**을 넣어 ISP를 containerized 방식으로 수행한다. 통합에는 세 challenge가 있다 — (i) storage+network stack 양쪽과 호환되는 single container 관리 interface, (ii) container의 self-contained 특성(자체 resource·dependency 포함)을 지원하는 firmware redesign, (iii) container 처리가 기존 I/O service를 overtax하지 않을 것. 기여는 **Ether-oN**(Ethernet over NVMe), **Virtual-FW**(virtual firmware), **HW-accelerated container requests** 세 가지.

### §II Background (p.380–381)
**High-perf SSD**: frontend(embedded multi-core + internal DRAM, PCIe endpoint) + backend(channel·FMC·NAND). frontend 연산력이 이미 low/mid-range multi-core에 필적하고("2GHz 8 cores" 또는 "500MHz 32 cores"), PCIe throughput도 host system bus에 근접. **NVMe protocol**: SQ/CQ, doorbell, PRP(physical region page), MSI. 모두 host-side "block" semantic. **Firmware stack**: HIL(host interface layer, NVMe control logic) → ICL(internal cache layer, DRAM caching) → FTL(flash translation layer, LBA→physical). **OS-level virtualization**: container는 cgroup+namespace를 control plane으로 쓰는 lightweight VM. 중요한 관찰 — "cgroups and namespaces are OS kernel features, not the functionalities of containers"(p.381)이고, container 실행 자체는 Docker stack(blob·manifest·overlay·rootfs)이 담당하며 각 container는 그냥 하나의 process다.

### §III Challenge Classification and Analysis (p.381–382)
기존 ISP의 **5대 challenge**를 정식화: (i) **manual ISP implementation**(사용자가 static kernel에 맞춰 코드 분리), (ii) **disregard for file layout**(firmware가 filesystem을 몰라 사용자가 LBA set을 직접 넘겨야 함 — DLRM embedding file 예시), (iii) **kernel context switches**(전체 application을 offload 못 해 host 개입·context serialize/deserialize 반복), (iv) **device reliance**(cross-compile toolchain 의존, portability 저하), (v) **data vulnerability**(host app과 ISP kernel이 같은 flash를 "block"으로 동시 접근, 보호 mechanism 부재).

**Performance Impact Assessment**: Programmable-ISP(P.ISP)를 host-only와 비교. Storage는 평균 execution time의 38%를 차지하고 P.ISP가 이를 50% 줄이지만, 전체 end-to-end latency는 오히려 host 대비 1.4× 증가한다 — 주범은 **Communicate가 P.ISP latency의 43%**를 차지하기 때문(Figure 3, p.382). pattern workload는 per-kernel data exchange가 커서 Communicate가 2.4× 증가. 결론: ISP를 host-independent·autonomous하게 만들면 communication overhead를 없애고 낮은 computing power로도 성능을 낼 수 있다.

### §IV Cross-Layer Optimized ISP Containerization (p.383)
DockerSSD는 3개 SW/HW component로 3 challenge를 해결: (i) **Ether-oN**(ISP-container 관리용 새 통신 interface, socket over PCIe로 NVMe overriding), (ii) **Virtual-FW**(minimal OS feature + mini-docker를 bare-metal SSD에 넣은 firmware), (iii) **HW accelerator** 2종. `docker-cli`로 언제든 IP 기반 ISP request 발행 가능.

**Backend Media Management (λFS, Lambda filesystem)**: EXT4 기반. NVMe subsystem으로 media를 두 namespace로 분리 — **private-NS**(container image layer `/images/`, container data `/rootfs/` 보관, host에서 invisible)와 **sharable-NS**(host↔ISP-container 공유 I/O data). 두 PCIe function(storage-side port처럼)이 각각 Virtual-FW와 host에 매핑. 공유 데이터의 concurrent access는 **inode 기반 lock**으로 해결: λFS가 host의 inode cache와 Ether-oN으로 동기화하고, VFS(virtual file system)의 inode에 reference counter를 붙여 파일 open/close 시 special packet으로 업데이트. counter가 0일 때만 접근 허용해 stale inode·corruption 방지. lock persistence는 불필요(실패 시 host가 filesystem 복원 후 container 재시작).

### §V Interface Design and Firmware Virtualization (p.384–385)
**Ether-oN (Ethernet over NVMe)**: socket 기반 networking을 NVMe에 overlay. host와 DockerSSD 사이에 intranet을 세워 source 수정 없이 TCP/IP 통신. kernel driver가 host에 network adapter를 만들어 Ethernet packet ↔ NVMe command 변환. NVMe에 없는 두 문제 해결 — (a) **inbound(upcall)**: NVMe는 host로 request 못 보내므로 vendor-specific NVMe command **transmit/receive frame**(opcode 0xE0/0xE1)을 도입, kernel init 때 receive command를 미리 SQ에 넣어두는 asynchronous upcall. 실측상 SQ당 4개 pre-allocated command 사용. (b) Ether-oN이 `sk_buff`(4KB kernel page)를 복사해 NVMe frame 생성.

**Virtual-FW (Docker-enabled firmware)**: HIL→ICL→FTL I/O path에 thread/I/O/network 3 handler를 삽입. bare-metal DRAM을 **FW-pool**(system call emulation 구현, MPU privilege로 보호)과 **ISP-pool**(call argument·data)로 분리 — Virtual-FW가 privilege mode로 ISP-pool도 접근 가능해 FW-pool↔ISP-pool 복사가 불필요. **System call emulation**: glibc의 `open`이 부르는 `openat` 등을 lightweight function wrapper로 emulate(Table Ia: thread handler 65 syscall, I/O handler 43, network handler 25). function-level이라 userland 복귀 시 발생하는 context switch가 없어 latency 절감. **mini-docker**: Docker 106개 command 중 필수 10개만 구현, host `docker-cli`와 HTTP 통신, `dockerd/containerd/runc` 역할 수행. image blob·manifest는 λFS(`/images/blobs`, `/images/manifest`)에, container log(stdout/stderr)는 `/containers/<id>/rootfs/log`에 저장.

### §VI Container Service Hardware Accelerations (p.385–386)
**Bottleneck 분석**: (a) **path walking** — data-intensive workload(embed/mariadb/rocksdb)는 큰 파일 하나를 열고 query 반복하므로 path walk가 Virtual-FW 실행시간의 25.8%뿐이지만, service-centric workload(pattern/nginx/vsftpd)는 파일을 자주 열어 **ISP-container 실행시간의 평균 76.1%**를 path walk가 차지(Figure 8a). (b) **TCP/IP**가 Tx/Rx network latency의 각각 **73.2% / 59.9%**(Figure 8b).

**IIO-X (In-storage I/O Accelerator)**: path walk를 critical path에서 제거. upper/lower directory용 path walker 2개, 각각 3 core module — **inode-AT**(inode address translator, λFS metadata로 inode의 LBA 계산, ICL/FTL와 협업하되 caching bypass로 복사 제거, 256B buffer), **dir-parser**(inode의 directory data에서 child 이름 parse), **str-matcher**(VLSI pattern matching HW 기반 문자 비교). 결과 inode를 internal SRAM에 caching. host↔storage coherence는 λFS inode lock이 inode cache를 nullify해 유지.

**NET-X (Network Accelerator)**: TCP/IP packetization을 HW로. **Rx engine**(inbound ACK 관리 — PCIe라 checksum 불필요, ACK number = TCP sequence + packet size로 생성), **Tx engine**(encapsulator로 TCP/IP/Ethernet header 생성, PRP data를 Tx buffer에 두어 Ether-oN이 NVMe로 관리), **metadata table**(dual-port memory, source/dest port·IP/MAC 저장).

### §VII Evaluation (p.387–390)
**Prototype**: 16nm FinFET FPGA에 NVMe HW IP + multi-core RISC-V(6 in-order core, AXI/TileLink) + PCIe controller. backend는 DDR4로 flash emulation(48 MLC flash, 12 channel, 2.2GHz frontend, 2GB DRAM). Ether-oN/λFS에 3.4K LOC, firmware/storage-level module에 7.5K LOC 수정. Virtual-FW image가 Linux 대비 **83.4× 작아** embedded processor 탑재 가능(Figure 11). 설계공간 탐색은 gem5 full-system + cycle-accurate SSD simulator(Amber)로. 비교 model: Host, P.ISP-R(gRPC), P.ISP-V(NVMe vendor cmd), D-Naive(별도 processor+Linux), D-FullOS, D-VirtFW, D-CoDesign(VirtFW + IIO-X + NET-X).

**Overall performance(Figure 12)**: D-VirtFW가 P.ISP-R/V, D-Naive, D-FullOS 대비 각각 **1.6× / 1.9× / 1.6×** 빠름. λFS로 LBA-set 불필요 → P.ISP-R/V 대비 9.5% latency 감소, rootfs pre-package로 Kernel-ctx 제거 → 30.9% 개선. **D-CoDesign**은 IIO-X/NET-X로 path walk·network를 8.7%/3.7% 줄여 **Host 대비 34% 짧은 execution time**(전 workload). **Kernel context**: D-VirtFW의 Kernel-ctx가 P.ISP-R/V 대비 **61.3K배 짧음**(containerization으로 host에 kernel context 안 넘김). Virtual-FW가 OS-level virtualization latency를 D-Naive/FullOS 대비 55.9% 감소.

**HW accel sensitivity(Figure 15)**: path walk iteration이 늘어도 D-CoDesign은 near-constant(iteration 1.6~5.9로 file service 3~4.4× 향상). NET-X는 payload 무관 TCP latency를 **3μs 이하**로 유지(D-VirtFW는 6~8μs). **Power/energy(Figure 16)**: idle 전환으로 Host 대비 44% 적은 power, ISP-container용으로 baseline보다 3.4× power지만 총 power는 Host 대비 **35.7% 낮고**, energy는 **평균 54.3% 낮음**. **Container 관리**: setup 30ms / teardown 13ms(총 실행의 0.3% / 0.1%), setup은 image layer 수에 비례(vsftpd 38 layer, rocksdb 9 layer). **Host resource 해방(Figure 20)**: CPU 97.2%, memory 84.7%가 다른 service용으로 free.

### §VIII Related Work (p.391)
ISP를 **AppSpecific-ISP**(static kernel, device design 때 확정 — S.Sort, Summarizer, KAML, GraF, Cognitive, DeepStore, H.GNN)와 **Programmable-ISP**(RPC/vendor command로 programmable — Willow, Biscuit, Insider)로 분류(Table III). 대부분이 file layout을 모르고, LBA set 교환이 필요하며(단 INSIDER는 custom filesystem으로 conventional file storage를 막음), kernel context switch가 필수. **Programmable HW architecture**: CSD(Computational SSD, SmartSSD)는 FPGA를 SSD 옆에 붙이지만 (i) host 동기화 mechanism 부재, (ii) FPGA가 external이라 D-Naive처럼 데이터 이동 필요, (iii) FPGA logic resource 부족, (iv) reconfiguration synthesis/engineering 비용, (v) soft PCIe switch가 logic 잠식. DockerSSD는 이 축들에서 우위(Table III 마지막 열 모두 ○).

### §IX Conclusion (p.391)
> "We propose DockerSSD, a fully flexible in-storage processing (ISP) model that can run a variety of applications near flash without source-level modification. ... DockerSSD exhibits 2.0× faster than state-of-the-art ISP models while 1.6× and 2.3× lower power and energy, respectively." (§IX, p.391)

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "fully flexible in-storage processing (ISP) model"
- "run a variety of applications near flash without source-level modification"
- "harmonize storage intelligence with the existing computing environment"
- "process data where they are, in real-time"

**Technical concepts:**
- "OS-level virtualization in modern SSDs"
- "ISP-container" / "containerized in-storage processing"
- "Ethernet over NVMe (Ether-oN)" — asynchronous upcall mechanism
- "virtual firmware (Virtual-FW)" / "system call emulation"
- "In-storage I/O Accelerator (IIO-X)" / "Network Accelerator (NET-X)"
- "path walking" / "inode address translator"
- "Lambda filesystem (λFS)" — private-NS / sharable-NS

**Value language:**
- "self-governing execution object"
- "self-contained objects running independently on various systems"
- "secure, portable sandbox"
- "eliminate device reliance and file layout dependence"

> ⚠ **피해야 할 어휘** (paper-signature — 그대로 echo하면 모방으로 보임):
> - "DockerSSD" / "Ether-oN" / "Virtual-FW" / "IIO-X" / "NET-X" / "λFS(Lambda filesystem)" — 전부 이 논문 고유 명칭
> - "containerized in-storage processing" 통째 — 이 논문 title 문구
> - "83.4× smaller image" 같은 특정 수치를 내 것처럼 인용 금지

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.379 | "DockerSSD is 2.0× faster than state-of-the-art ISP models for workloads with a high volume of system calls or file accesses" | ISP가 syscall-heavy workload에서 왜 어려운지 + container 해법의 효과 |
| §I, p.380 | "1.5× and 2.0× faster than the host system and leading ISP models" / energy "1.6× and 2.3× better" | in-storage compute의 성능·에너지 이득 근거 |
| §III-B, p.382 | "Storage comprises 38% of the total execution time on average" | 데이터 이동/storage latency가 전체에서 큰 비중 |
| §III-B, p.382 | "Communicate, accounting for 43% of the P.ISP latency" | 기존 ISP의 host↔storage 통신 overhead 문제 |
| §VI-A, p.386 | path walking "averaging 76.1% of the total latency" (service-centric workloads) | file access가 많은 workload에서 filesystem traversal이 병목 |
| §VI-A, p.386 | "TCP/IP accounts for 73.2% and 59.9% of the Tx and Rx network latency" | network stack HW 가속 동기 |
| §VII, p.387 | image size "reduced by a factor of 83.4×, making Virtual-FW compatible with the embedded processor" | full OS를 SSD에 넣는 것의 비현실성 → minimal firmware 정당화 |
| §VII-A, p.389 | "Kernel-ctx being 61.3K times shorter than P.ISP-R/V" | containerization이 host kernel 개입을 사실상 제거 |
| §VII-A, p.388 | D-CoDesign "34% shorter execution time than Host across all tested workloads" | full-application in-storage 실행의 end-to-end 이득 |
| §VII-C, p.390 | "35.7% lower total power" / "54.3% lower energy on average" | data movement 제거의 에너지 효율 |
| §VII-D, p.390 | "97.2% of the CPU and 84.7% of the memory become free" | host resource 해방 = disaggregation 관점 가치 |

---

## 🎯 Strategic anchor

> "Note that if we can render ISP models host-independent and manage their execution autonomously, we can eliminate communication overhead, enhancing performance with lower computing power in many instances." (§III-B, p.382)

→ **본인 활용**: 이 문장이 in-storage compute와 CXL/memory-disaggregation 방향의 공통 축을 정확히 짚는다 — "데이터가 있는 곳에서 host 개입 없이 autonomous하게 처리하면 communication overhead를 제거한다." 면담·자소서에서 "compute-near-data의 본질적 이득은 통신 제거이며, 이를 위해선 device가 host filesystem/context에 의존하지 않고 자율 실행되어야 한다(§III-B, p.382)"로 인용해, 내 memory-system 아키텍처(CXL coherence로 host-device 간 데이터 semantic을 일치시켜 이동을 없애는 방향)의 motivation과 연결.

---

## Connection to my research direction

| 차원 | 이 paper (DockerSSD) | 본인 방향 (메모리 시스템 아키텍처 / CXL·coherence) |
|---|---|---|
| Scope | SSD firmware 내부 ISP를 container로 virtualize | interconnect 레벨에서 memory semantic·coherence를 재정의 |
| Mechanism | Ethernet over NVMe(기존 프로토콜에 새 채널 overlay), firmware system-call emulation, HW accel | CXL(기존 PCIe에 memory/coherence semantic overlay), HW coherence engine |
| Workload | data analytics(embed/mariadb/rocksdb/pattern/nginx/vsftpd) | multi-node shared memory, disaggregated memory workload |
| Data model | block/file(inode·LBA) + λFS로 host↔device 동기화 | cacheline/address 단위 coherence로 host↔device 일치 |
| Open space | flash 근처 compute, coherence는 inode-lock 수준 | true HW coherence·multi-node coherence로 확장 |

DockerSSD는 **file/block granularity**에서 "device를 host로부터 독립시켜 데이터 이동을 없앤다"는 목표를 firmware+HW로 달성한다. 내 방향은 같은 목표를 **cacheline/memory granularity의 coherence**로 끌어올리는 것 — DockerSSD가 λFS inode-lock으로 host cache와 storage의 stale 문제를 ad-hoc하게 푸는 지점(§IV-B, p.383)이, 정확히 CXL.cache/coherence가 HW protocol로 correct-by-construction하게 풀려는 문제다. 즉 DockerSSD의 inode 동기화 mechanism은 "SW로 흉내 낸 coherence"이고, 내 관심은 이를 interconnect HW로 격상하는 것. 또한 CAMEL이 FPGA prototype + Amber/gem5 simulator로 feasibility를 실제 빌드해 증명하는 방법론은 내 접근법의 template이 된다.

---

## Open questions / gaps

- [ ] λFS의 inode-lock coherence는 host filesystem 참여를 전제로 하고 lock persistence를 포기(§IV-B). **multi-node**로 확장 시 이 SW 동기화가 병목/부정확성이 될 지점 — HW coherence로 대체 가능한가?
- [ ] Ether-oN은 host↔single-SSD intranet 가정. **여러 DockerSSD 간(peer-to-peer)** 통신·데이터 공유는 다루지 않음 → CXL-fabric로 device 간 직접 연결 시 그림이 달라짐.
- [ ] ISP-container는 flash near-data. **CXL memory pool** 위 데이터에 대한 in-fabric compute와는 별개 — storage-near vs memory-near compute의 경계.
- [ ] HW accel(IIO-X/NET-X)은 EXT4/λFS·TCP/IP에 특화. filesystem·protocol이 바뀌면 재설계 필요 → generality 대 성능 trade-off.
- [ ] 후속 IEEE Micro'25 "Containerized In-Storage Processing … SSD Disaggregation"이 이 gap(disaggregation/multi-node)을 어떻게 메우는지 대조 필요.

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [8] | Willow: A User-Programmable SSD (OSDI 2014) | Programmable-ISP의 원류, DockerSSD의 주 비교대상(P.ISP) |
| ☐ | [9] | Biscuit: near-data processing framework (ISCA 2016) | 또 다른 주 비교대상, near-data framework 설계 |
| ☐ | [16] | Jung, OpenExpress: Fully HW Automated Open Research Framework for NVMe (USENIX ATC 2020) | prototype의 NVMe HW IP 출처, CAMEL의 HW 빌드 인프라 |
| ☐ | [18] | Amber: Precise Full-System Simulation of All SSD Resources (MICRO 2018) | DockerSSD 설계공간 탐색에 쓴 SSD simulator — 방법론 핵심 |
| ☐ | [44] | Kwon, Gouk, Lee, Jung, HW/SW Co-Programmable framework for CSD to accelerate DL (FAST 2022) | 같은 저자 그룹, CSD 가속 — 계보상 인접 |
| ☐ | [80][81] | Xilinx/Samsung SmartSSD 1.0 / 2.0 | CSD(FPGA+SSD) 대표, DockerSSD가 §VIII에서 구조적 한계 비판 |
| ☐ | [47] | Active Disks: Programming Model, Algorithms and Evaluation (ASPLOS 1998) | ISP/active storage의 고전적 origin |

---

## Personal annotations

<!-- 자유 형식. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. -->
