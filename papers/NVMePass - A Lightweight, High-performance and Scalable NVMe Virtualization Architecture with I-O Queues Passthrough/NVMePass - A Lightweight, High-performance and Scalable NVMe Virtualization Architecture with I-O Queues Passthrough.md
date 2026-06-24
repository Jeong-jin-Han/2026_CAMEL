---
title: "NVMePass: A Lightweight, High-performance and Scalable NVMe Virtualization Architecture with I/O Queues Passthrough"
aliases: [NVMePass]
description: "VM에 NVMe I/O queue를 직접 passthrough하고 NVMe 컨트롤러 펌웨어의 NRD로 보안을 보장해, CPU overhead 없이 VFIO에 준하는 near-native 성능과 높은 확장성을 동시에 달성한 software-hardware co-design NVMe 가상화 구조."
venue: HPCA
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/infra
  - topic/virtualization
  - topic/nvme
  - topic/passthrough
  - venue/hpca
  - year/2025
---

# NVMePass: A Lightweight, High-performance and Scalable NVMe Virtualization Architecture with I/O Queues Passthrough

> **HPCA 2025** · `cluster/infra` · Source: [NVMePass - A Lightweight, High-performance and Scalable NVMe Virtualization Architecture with I-O Queues Passthrough.pdf](<NVMePass - A Lightweight, High-performance and Scalable NVMe Virtualization Architecture with I-O Queues Passthrough.pdf>)

저자: Yiquan Chen, Zhen Jin (공동 1저자), Yijing Wang, Yi Chen, Jiexiong Xu, Hao Yu, Jinlong Chen, Wenhai Lin, Kanghua Fang, Keyao Zhang, Chengkun Wei, Qiang Liu, Yuan Xie, Wenzhi Chen — Zhejiang University, Alibaba Group, University of Michigan, HKUST

## TL;DR
NVMePass는 NVMe 가상화의 control resource(느린 경로)는 hypervisor에서 software emulation으로 처리하고, data resource(빠른 경로, I/O queue·doorbell·LBA·DMA)는 VM에 직접 passthrough하는 software-hardware co-design 구조다. CMB와 custom page fault handler로 I/O queue를 EPT에 직접 매핑해 hypervisor 개입 없이 zero-copy I/O를 가능하게 하고, NVMe 컨트롤러 펌웨어에 NVMe Resource Domain(NRD) 기반 NP guarder를 두어 illegal I/O를 차단해 보안을 확보한다. 결과적으로 VFIO 대비 IOPS 100.1~100.5%, SPDK-Vhost 대비 150 VM에서 latency 40.0% 감소·CPU core 0개 소모, 실제 RocksDB 100 VM에서 OPS 68.0% 향상을 달성한다.

## 문제 & 동기
기존 NVMe 가상화는 (1) software-based(virtio, polling)와 (2) hardware-assisted(SR-IOV, SIOV)로 나뉘는데 각각 치명적 한계가 있다. virtio는 VM_Exit/context switch/cache pollution으로 native 대비 50% 이상 성능 저하를 겪고, SPDK-Vhost 같은 polling 방식은 near-native 성능을 내지만 dedicated CPU core를 소모한다(16 SSD 서버에서 16 core = CPU의 12.5%). hardware-assisted 방식은 CPU overhead는 없으나 dedicated hardware 설계가 필요해 비용·복잡도·확장성·호환성 문제를 안는다(SR-IOV는 spec상 optional이라 vendor lock-in, SIOV는 PASID·ADI 요구). 특히 VM density가 급증(Firecracker는 서버당 8000 MicroVM 지원)하면서 확장성이 핵심 과제가 되었다.

> [!quote]- 📄 원문 표현 (paper)
> "Virtio suffers from severe performance degradation, and polling-based solutions consume too many valuable CPU resources. Hardware-assisted solutions provide high performance and no CPU usage but have the challenges of developing dedicated hardware." (Abstract, p.1)
>
> "virtualizing NVMe storage on a server with 16 SSDs consumes 16 CPU cores of servers configured with 128 CPU cores, NVMe virtualization overhead can amount to 12.5% of CPU resources." (§III-B, p.3)
>
> "Compared to existing hardware-assisted schemes, such as SR-IOV, and SIOV, the NVMePass has the advantage of being lightweight, simple, and flexible. For the NVMePass, the only requirement for the hardware is to support NRD in NVMe SSD to ensure security. It is done in the NVMe controller firmware and only needs a few hundred lines of C code." (§I, p.2)

## 핵심 통찰

> [!note]- 통찰 1 — Control/Data resource 분리: 느린 경로만 emulate, 빠른 경로는 passthrough
> NVMe 자원을 control resource(PCIe config space, BAR, admin queue — device 관리용, I/O request에 미관여)와 data resource(I/O queue, doorbell, interrupt, LBA — I/O request 관련)로 나눈다. control은 software emulation(trap-and-emulate)로 처리해도 I/O 성능에 영향이 없고, 성능에 critical한 data 경로만 passthrough하면 된다. 이 분리가 "no CPU overhead + high performance"를 동시에 가능하게 한 핵심 아이디어다.

> [!note]- 통찰 2 — Virtual CMB + page fault handler로 I/O queue를 EPT에 직접 매핑
> 실제 NVMe hardware의 CMB 지원 없이도, 가상화된 NVMe device에 virtual Controller Memory Buffer(CMB)를 만들고 custom page fault handler를 두어 VM의 I/O queue GPA와 physical I/O queue를 직접 주소 매핑한다. GPA-HPA 매핑은 page fault 시 1회만 설정되므로 이후 I/O는 page fault 없이 EPT translation으로 처리 → hypervisor 개입 0, zero-copy.

> [!note]- 통찰 3 — 보안을 hardware가 아닌 NVMe 컨트롤러 펌웨어로 해결
> Passthrough의 근본 위험(malicious VM이 위조한 NVMe command로 남의 데이터 접근)을 dedicated hardware가 아닌 NVMe controller firmware의 NP guarder로 막는다. LBA와 PRP를 I/O queue에 묶어 NVMe Resource Domain(NRD)을 만들고, SSD 컨트롤러가 command의 LBA/PRP가 해당 NRD에 속하는지 검증. 펌웨어 수백 줄(NP guarder 300 LOC)이면 충분해 SR-IOV/SIOV보다 가볍고 유연하다.

> [!note]- 통찰 4 — I/O queue 수가 곧 scalability 한계 → NVMe spec이 본질적으로 확장성 보장
> 가상 NVMe device 수는 SSD가 구현한 I/O queue 수에 의해 제한되는데, NVMe spec은 device당 최대 65,535 I/O queue를 허용한다. 서버는 보통 다중 SSD로 구성되므로 VM 수가 늘어도 자연스럽게 scale-out 가능 — 별도 hardware 추가 없이 high density VM을 수용.

## 설계 / 메커니즘

> [!abstract]- 4-component 아키텍처 (NP frontend/backend driver, device manager, guarder)
> NVMePass는 4개 구성요소로 이뤄진다 (Fig.2, p.4):
> - **NP frontend driver** (guest kernel): VM에 표준 virtualized NVMe device 제공. CMB 안의 I/O queue를 직접 사용하고, virtual LBA→physical LBA 변환 및 destination address(LBA·PRP) legitimacy 검증.
> - **NP backend driver** (host kernel): I/O queue·doorbell register 할당, DMA remapping, posted interrupt 처리. I/O queue buffer를 NP device manager의 CMB를 통해 VM에 매핑. non-overlapping LBA range 할당.
> - **NP device manager** (hypervisor): control resource를 software emulation하고, backend가 할당한 data resource와 결합해 full-featured virtual NVMe device 제공.
> - **NP guarder** (NVMe controller firmware): 각 NVMe I/O command의 LBA·DMA address 유효성 검증으로 malicious tenant 차단.

> [!abstract]- I/O Queues Passthrough 메커니즘 (3단계, Fig.3, p.5)
> ❶ NP frontend driver가 guest kernel에서 CMB의 I/O SQ에 I/O request 제출. MMU가 GVA→GPA 변환, EPT가 GPA를 받아 I/O queue GPA에 대한 HPA를 EPT에서 lookup. ❷ 매핑 안 돼 있으면 page fault → NVMePass 자체 page fault handler가 해당 I/O SQ의 HPA를 얻어 EPT에 GPA-HPA 매핑 설정(❷a), 이후 address translation으로 복귀(❷b). CQ·doorbell register 매핑도 동일하게 설정. ❸ EPT가 guest I/O queue GPA를 HPA로 변환 → VM이 host memory의 I/O SQ에 직접 제출, NVMe device가 fetch·실행. 매핑은 첫 request에서만 1회 발생.

> [!abstract]- Isolation & Security: NRD + NP guarder (Algorithm 1, p.6; Fig.4-5, p.7)
> **Isolation 3중**: (1) NP backend가 device CMB에 non-overlapping LBA range 저장, frontend가 I/O command의 slba 조정으로 disk space 격리. (2) I/O queue·doorbell register는 EPT page table로 격리(각 VM이 독립 I/O queue·EPT). (3) DMA address는 hypervisor가 VM별 non-overlapping GPA 할당(VM0=0~4G, VM1=4~8G) + IOMMU의 GPA-HPA 매핑으로 격리.
> **Register 격리 세부**: NVMe SQ command 64B → 4K page에 64개, doorbell pair는 QP당 8B → 4K page에 512 I/O queue pair register가 들어가 같은 page에서 타 VM register 접근 위험. `CAP.DSTRD` 값을 키워 register 간격을 늘려 page 단위로 분산.
> **Security(NP guarder)**: NRD = LBA + PRP를 I/O queue에 bind. SSD 컨트롤러가 host memory에서 command를 fetch한 뒤 target LBA·PRP가 pre-allocated NRD에 속하는지 검증, 아니면 IO_ERROR 반환. command validation 모듈에 간단한 NRD validity compare statement만 추가 → 몇 cycle만 소모, I/O 성능 무영향.

> [!abstract]- DMA Remapping & Posted Interrupt, 구현 규모
> DMA: I/O request의 PRP/SGL address(GPA)를 IOMMU가 GPA-HPA 자동 변환. Posted Interrupt: IOMMU interrupt remapping + posted-interrupt로 VM_Exit 없이 VM이 직접 interrupt 수신(IRQ bypass manager 사용). **구현**: Linux kernel 4.19 + Firecracker 0.14.0. 총 6,600 LOC(guest kernel 300, host kernel 4,200, Firecracker 2,100) + NP guarder 300 LOC(Yingren Dongting-N2 NVMe SSD, PCIe 4.0 QLC NAND 펌웨어). 원래 Intel P4510 대상 설계였으나 firmware source 접근 불가로 Dongting-N2 선택.

## 평가

> [!example]- 실험 환경
> 2×48-core Intel Xeon Platinum 8163, NUMA 단일 노드 사용, Yingren Dongting-N2(SoC NVMe), 일부 scalability는 Intel P4510(128 I/O queue 지원)×6. CentOS 64-bit, kernel 4.19, Firecracker 0.14.0. 비교: VFIO(hardware-assisted 대표), SPDK-Vhost(software polling 최고), virtio. fio(libaio) + RocksDB(db_bench).

> [!example]- Single-VM 처리량/지연 (Fig.6-7, p.8-9)
> - IOPS: randread-4k-4-128에서 VFIO의 **100.1%**, randwrite에서 **100.5%**. virtio 대비 randread **203.2%** IOPS. SPDK-Vhost 대비 randwrite 2.6% 높음.
> - Bandwidth(128k seq): VFIO의 99.1%, SPDK의 99.2%, virtio의 112.8%.
> - Latency(4k-1-1): NVMePass와 VFIO 차이 1% 미만(예: randread 101.2 vs 101.3 us). seqread-4k-1-1에서 SPDK-Vhost는 NVMePass보다 70.2% 높은 latency. virtio는 34.2~347.6% 높음. Tail latency도 NVMePass 최고.

> [!example]- Scalability 다중 VM (Fig.8-9, p.9-10)
> - 2 SSD, 4k-randread-1-1, 150 VM: NVMePass가 SPDK-Vhost 대비 latency **40.0%** 낮고(25→150 VM 구간 12.0~40.0% 우위), virtio 대비 25 VM에서 52.4% 낮음. Total IOPS는 150 VM에서 SPDK 대비 **68.0%**, virtio 대비 **117.2%** 높음. SPDK-Vhost는 NVMePass보다 CPU core 4개 더 소모.
> - 6×P4510, 100~700 VM(100당 10 core 공유, SPDK는 6 core 전용): NVMePass가 모든 구간에서 최적 latency·IOPS 유지.

> [!example]- CPU 소모 / Fairness / RocksDB (Table III, Fig.10-12, p.10-12)
> - **CPU(Table III, p.10)**: I/O pressure 10~400K IOPS 전 구간에서 VFIO와 NVMePass 모두 **0 core**. SPDK-Vhost는 항상 1 core, virtio는 최대 1.94 core(400K에서 측정 불가 'x').
> - **Fairness(Fig.10)**: 2 SSD에 1·2·4·8 VM 실행 시 overall IOPS가 SSD limit(2 SSD≈1028.5K)에 수렴, 각 VM에 IOPS·tail latency 균등 분배.
> - **RocksDB Single-VM(128 thread)**: NVMePass latency가 VFIO 대비 1.7%만 높고 SPDK·virtio 대비 11.9%·15.6% 낮음. OPS는 VFIO의 98.3%, SPDK 대비 readrandom 13.6%·readwhilewriting 28.5% 높음, virtio 대비 18.6%·143.7% 높음.
> - **RocksDB 100 VM**: readrandom에서 SPDK 대비 latency 7.4% 낮음, virtio 대비 readrandom/readwhilewriting 34.6%·40.3% 낮음. (Abstract의 OPS 68.0% 향상은 100 VM real-world 기준)

## 섹션 노트
- **§I Introduction**: software/hardware 두 분류의 trade-off, VM density 폭증(Firecracker 8000 MicroVM)에 따른 scalability 강조, 4대 기여 제시(novel architecture / I/O queues passthrough / NRD security / 종합 평가).
- **§II Background**: NVMe(최대 65,535 I/O QP, SQ/CQ doorbell register, CMB 정의). virtio·SPDK-Vhost·Mdev-NVMe(software), VFIO·SR-IOV·FVM·BM-Store·LeapIO·SIOV(hardware) 정리.
- **§III Motivation**: virtio 성능 저하, polling CPU overhead(Fig.1 — SPDK-Vhost가 150 VM에서 2 core가 1 core보다 46.3% 낮은 latency), hardware-assisted 복잡도·확장성 한계. Table I로 6개 지표(high perf/no CPU/scalability/compatibility/lightweight) 비교 — NVMePass만 전 항목 충족.
- **§IV Design**: 4 설계 목표(고성능/no CPU/확장성/보안), 4 component, I/O queue passthrough 3단계, isolation·security(NRD/NP guarder/CAP.DSTRD), DMA remapping·posted interrupt, NP device manager(trap-and-emulate), 구현(6,600 LOC).
- **§V Evaluation**: I/O perf, scalability, CPU 소모, fairness, RocksDB.
- **§VI Related Work / §VII Conclusion**: software(virtio/SPDK/MDev/FinNVMe/Direct-Virtio)·hardware(SR-IOV/FVM/BM-Store/LeapIO/vDPA) 정리.

## 핵심 용어
- **I/O Queues Passthrough**: VM이 hypervisor를 거치지 않고 물리 NVMe device의 I/O queue에 직접 I/O command를 제출하게 하는 NVMePass의 핵심 기법. zero-copy·no CPU overhead 달성.
- **Control resource / Data resource**: NVMe 자원의 이분법. control(PCIe config space·BAR·admin queue)은 device 관리용으로 software emulation, data(I/O queue·doorbell·interrupt·LBA)는 I/O 빠른 경로로 passthrough.
- **CMB (Controller Memory Buffer)**: NVMe controller의 범용 read/write memory 영역. NVMePass는 가상 NVMe device에 virtual CMB를 만들어 I/O queue를 host/guest 간 매핑(실제 hardware CMB 불필요).
- **NRD (NVMe Resource Domain)**: LBA·PRP를 I/O queue에 bind한 격리 단위. 각 VM은 자기 NRD 내 자원만 접근 가능, NP guarder가 이를 검증.
- **NP guarder**: NVMe controller firmware에 구현된 보안 모듈. I/O command의 LBA·PRP가 pre-allocated NRD에 속하는지 검증해 illegal I/O 차단(300 LOC).
- **EPT (Extended Page Table)**: hypervisor가 GPA→HPA 변환을 위해 만드는 page table. NVMePass는 page fault handler로 I/O queue의 GPA-HPA 매핑을 EPT에 설정.
- **Posted Interrupt**: VM_Exit 없이 VM이 interrupt를 직접 수신하게 하는 hardware 메커니즘. IRQ bypass manager로 posted-interrupt I/O queue 구성.
- **CAP.DSTRD (Doorbell Stride)**: NVMe spec의 doorbell register 간격 설정값. 키워서 각 I/O queue pair register를 독립 page에 분산 → register 격리.
- **VFIO**: IOMMU 기반 hardware-assisted passthrough framework. device를 단일 VM에 전용 할당(공유 불가). NVMePass의 성능 비교 상한선.
- **SR-IOV / SIOV**: PCIe 기반 hardware 가상화. SR-IOV는 PF+VF(spec optional, vendor lock-in), SIOV는 PASID·ADI 요구(특정 CPU·NVMe 제약).

## 강점 · 한계 · 열린 질문
**강점**
- VFIO에 준하는 near-native 성능(IOPS 100.1~100.5%, latency 차 1% 미만)을 CPU core 0개로 달성 — software와 hardware 방식의 장점을 모두 취함.
- 보안을 펌웨어 수백 줄로 해결해 SR-IOV/SIOV 대비 deployment 비용·복잡도가 낮고 기존 NVMe SSD에 적용 용이.
- I/O queue 수(최대 65,535/device) 기반으로 high VM density에서 우수한 scalability(150~700 VM 검증).
- 실제 prototype + RocksDB real-world 평가까지 포함한 종합 검증.

**한계**
- NP guarder가 NVMe controller firmware 수정을 요구 — firmware source 접근이 가능한 SSD(Yingren Dongting-N2)에서만 검증. 폐쇄 펌웨어(Intel P4510)에는 적용 불가.
- 가상 NVMe device 수가 SSD의 I/O queue 수에 묶임 — 극단적 density에서 여전히 hardware 자원 제약.
- 평가가 단일 NUMA 노드·특정 SSD/kernel(4.19)/Firecracker 0.14.0에 한정.

**열린 질문**
- live migration, hot-plug, over-subscription 시 I/O queue GPA-HPA 매핑·NRD 재구성은 어떻게 처리되는가?
- NP guarder의 NRD 메타데이터가 SSD 컨트롤러 메모리를 점유 — 수만 개 I/O queue 규모에서 메모리·검증 비용은?
- firmware 미공개 commodity SSD vendor의 채택 incentive와 표준화 가능성은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. NVMePass가 기존 software/hardware 방식의 한계를 어떻게 동시에 극복하는가?
> 답: NVMe 자원을 control(느린 경로)과 data(빠른 경로)로 분리해, control은 hypervisor에서 software emulation(성능 무영향)하고 data(I/O queue·doorbell·LBA·DMA)만 VM에 passthrough한다. 빠른 경로에서 hypervisor가 빠지므로 polling용 CPU core가 불필요(software 한계 극복)하고, 보안은 dedicated hardware 대신 NVMe firmware의 NP guarder로 처리해 hardware 설계 복잡도를 제거(hardware 한계 극복)한다.

> [!question]- Q2. 실제 hardware CMB 없이 어떻게 I/O queue를 직접 매핑하는가?
> 답: 가상화된 NVMe device에 virtual CMB를 software로 만들고, custom page fault handler를 둔다. VM이 I/O SQ에 처음 접근하면 page fault가 발생하고, handler가 해당 I/O queue의 HPA를 얻어 EPT에 GPA-HPA 매핑을 설정한다. 매핑은 첫 request에서만 1회 발생하므로 이후 I/O는 page fault 없이 EPT translation으로 직접 접근 — 성능 영향 negligible.

> [!question]- Q3. NP guarder와 NRD는 어떻게 malicious VM을 막는가?
> 답: 각 VM의 LBA와 PRP를 I/O queue에 bind해 NRD(NVMe Resource Domain)를 형성한다. SSD 컨트롤러가 host memory에서 fetch한 NVMe command의 target LBA·PRP가 해당 I/O queue의 pre-allocated NRD에 속하는지 검증하고, 불일치 시 IO_ERROR_LBA/IO_ERROR_PRP를 반환해 실행을 거부한다(Algorithm 1). VM이 frontend driver를 위조해도 firmware 단에서 차단된다.

> [!question]- Q4. NVMePass는 왜 CPU core를 0개 소모하는가? SPDK-Vhost와의 근본 차이는?
> 답: SPDK-Vhost는 virtual NVMe I/O queue를 polling하기 위해 dedicated CPU core(thread)를 상시 점유한다. NVMePass는 I/O command가 hypervisor를 완전히 우회해 VM에서 물리 NVMe device로 직접 전달되고, interrupt도 posted-interrupt로 VM이 직접 수신하므로 polling thread가 필요 없다. Table III에서 10~400K IOPS 전 구간 0 core로 확인된다.

> [!question]- Q5. 같은 4K page에 여러 VM의 register가 들어가는 격리 문제는 어떻게 푸는가?
> 답: SQ doorbell·CQ doorbell pair는 QP당 8B여서 4K page에 512개가 들어가 같은 page에 여러 VM register가 공존할 위험이 있다. NVMePass는 NVMe spec의 `CAP.DSTRD`(doorbell stride)를 키워 register 간격을 늘리고, 각 I/O queue pair register를 독립 page에 분산시킨 뒤 EPT address translation으로 격리한다.

> [!question]- Q6. NVMePass의 scalability 근거는 무엇인가?
> 답: 제공 가능한 virtual NVMe device 수는 SSD가 구현한 I/O queue 수에 의해 결정되는데, NVMe spec은 device당 최대 65,535 I/O queue를 허용한다. 서버가 다중 SSD로 구성되므로 VM 수가 늘어도 자연스럽게 확장된다. 실험에서 150 VM(2 SSD)·700 VM(6 SSD)까지 SPDK·virtio 대비 우수한 latency·IOPS를 유지했다.

> [!question]- Q7. VFIO와 비교한 NVMePass의 핵심 우위는?
> 답: VFIO는 device를 단일 VM에 전용 할당하므로 shareability를 희생한다(다중 VM 공유 불가). NVMePass는 I/O queue 단위 passthrough로 하나의 SSD를 여러 VM이 공유하면서도 VFIO와 거의 동일한 near-native 성능(IOPS 100.1~100.5%, latency 차 1% 미만)을 낸다. 즉 VFIO의 성능 + 공유 가능성을 동시에 제공한다.

> [!question]- Q8. NVMePass 채택의 가장 큰 실용적 제약은?
> 답: NP guarder가 NVMe controller firmware 수정을 요구한다는 점이다. 저자들도 Intel P4510 firmware source에 접근 불가해 source가 공개된 Yingren Dongting-N2로 prototype을 구현했다. 따라서 commodity SSD 대부분의 폐쇄 펌웨어 환경에서는 vendor 협력 없이 즉시 적용하기 어렵다.

## 🔗 Connections
[[Infra]] · [[HPCA]] · [[2025]]

## References worth following
- **[55] Z. Yang et al., "SPDK vhost-NVMe", SC 2018** — software polling 방식의 대표·성능 baseline. NVMePass의 주요 비교 대상.
- **[43] B. Peng et al., "MDev-NVMe", USENIX ATC 2018** — mediated pass-through 방식. data resource passthrough 아이디어의 선행 연구.
- **[26] J. Kwak et al., "NVMeVirt", FAST 2023** — versatile software-defined virtual NVMe device. NP guarder의 NRD validity check가 NVMeVirt의 `__nvmev_proc_io`에 구현 가능하다고 언급.
- **[30] D. Kwon et al., "FVM", OSDI 2020** — FPGA-assisted virtual device emulation. hardware-assisted offloading 대안.
- **[33] H. Li et al., "LeapIO", ASPLOS 2020** — storage stack을 ARM SoC로 offload. NVMePass가 ARM 성능 저하(68% throughput) 한계를 지적하며 대비.
- **[51] A. Tian, "Scalable IOV (SIOV)"** — hyperscale 대상 hardware-assisted I/O 가상화. NVMePass가 PASID·ADI 요구의 제약을 비판하며 비교.

## Personal annotations
<본인 메모 영역>
