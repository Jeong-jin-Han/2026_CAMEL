---
title: "LightPC: Hardware and Software Co-Design for Energy-Efficient Full System Persistence"
aliases: [LightPC, OC-PMEM, PecOS]
description: "정명수 교수님 랩(KAIST CAMEL)의 ISCA 2022 논문 — open-channel PMEM(OC-PMEM) 하드웨어와 persistence-centric OS(PecOS)를 co-design해, DRAM 관련 HW/SW를 memory hierarchy에서 제거하고 전원 장애에 강한 lightweight full-system persistence를 구현. DRAM-only 수준 성능에 73% 적은 전력, 69% 적은 에너지."
venue: ISCA
year: 2022
doi: "10.1145/3470496.3527397"
type: insight
tags: [insight, insight/professor, cluster/cxl, topic-pmem, topic-persistence, topic-reliability, topic-nvm]
---
# LightPC: Hardware and Software Co-Design for Energy-Efficient Full System Persistence

> **Source PDF**: [LightPC.pdf](<LightPC.pdf>)
> **Authors**: Sangwon Lee, Miryeong Kwon, Gyuyoung Park, **Myoungsoo Jung** (KAIST, Computer Architecture and Memory Systems Lab — CAMEL)
> **Venue / Year**: ISCA 2022 (49th Int. Symp. on Computer Architecture)
> **DOI**: 10.1145/3470496.3527397
> **Length**: 17 pages
> **Read status**: ☑ Full read (2026-06-28)
> **My reading purpose**: 교수님 그룹의 "PMEM/persistence를 OS·HW co-design으로 근본적으로 다시 짠다"는 사고방식의 대표작. TrainingCXL·CXL-SSD 라인의 토대가 되는 "persistence를 first-class로, DRAM 없이도 빠르게"의 원형을 이해하기 위해.

---

## 📋 목차

- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary](#key-vocabulary)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR

기존 PMEM(Optane)은 내부 DRAM buffer + 복잡한 firmware로 DRAM 성능을 흉내 내지만, 그 hybrid 설계가 **non-deterministic latency**와 100% 넘는 공간 overhead, 그리고 crash consistency를 위한 잦은 flush/sync 부담을 낳는다. **LightPC**는 두 축을 co-design한다: ① **OC-PMEM(open-channel PMEM)** — 내부 DRAM·buffer·firmware를 access path에서 걷어내고 bare-metal PRAM channel을 host에 노출, reliability(ECC)·resource conflict만 **PSM(persistent support module)**이 관리. ② **PecOS(persistence-centric OS)** — 전원 장애 시 **single execution persistence cut(EP-cut)**을 만들어 multi-core의 비휘발 실행 상태까지 빠르게 영속화(**Stop-and-Go**). 결과: DRAM-only 대비 성능은 동등하면서 **전력 73%↓, 에너지 69%↓**, persistent 시스템(checkpoint-restart) 대비 실행시간 **평균 4.3× 단축**(p.289).

---

## Core thesis

> "if one can make data structures and execution flows persistent by reconsidering a vertical design of the current memory subsystems, it addresses the crash control overhead (on a power failure) and eliminates frequent memory flushes/synchronizations imposed by the PMEM's hybrid design." (p.289)

PMEM이 느린 이유는 PRAM 매체 자체가 아니라 **DRAM을 흉내 내려는 hybrid 구조**(내부 DRAM cache + firmware)다. 그 layer를 vertical하게 걷어내고(OC-PMEM), 대신 OS가 "전원이 꺼지는 순간 실행 상태 전체를 한 번의 cut으로 영속화"(PecOS)하면, persistence를 first-class로 두면서도 DRAM 수준 성능을 유지할 수 있다. 즉 **persistence control overhead를 매체가 아니라 시스템 설계에서 없앤다.**

---

## Why this matters to me

LightPC는 교수님 그룹이 **"메모리 계층과 OS를 같이 다시 설계한다"**는 방식을 가장 순수하게 보여주는 논문이다. 내 발표 라인이 다루는 CXL-SSD/near-data는 결국 "비휘발 매체를 compute 가까이에 두고 빠르게 쓰기"인데, 그 토대 질문이 바로 LightPC의 **"비휘발 매체를 working memory로 쓰면서 어떻게 DRAM 성능과 persistence를 동시에 얻나"**이다.
- TrainingCXL이 "응용(추천모델)"이라면 LightPC는 "기반(full-system persistence)".
- PecOS의 **EP-cut / Stop-and-Go**는 checkpoint를 매체 수준이 아니라 OS 수준에서 거의 0에 가깝게 만드는 발상 → Sparse Checkpointing 발표의 "checkpoint를 어떻게 싸게"와 같은 계열.
- OC-PMEM이 SSD의 open-channel 철학(firmware를 host로 끌어올림)을 PMEM에 적용한 점은 ZNS/FDP 등 "host가 매체를 직접 제어"하는 SSD internals 흐름과도 통한다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| I | Introduction | p.289–290 | PMEM hybrid 설계가 non-determinism·flush 부담 → OC-PMEM+PecOS 제안 |
| II | Background | p.290–292 | PMEM HW 구조(LSQ·NMEM cache·firmware), persistence 관리(DAX·libpmemobj) overhead |
| III | LightPC Platform | p.292–293 | OC-PMEM(PSM+Bare-NVDIMM), PecOS(EP-cut, Stop-and-Go) 개요 |
| IV | Persistence-Centric OS | p.293–295 | Stop(Drive-to-Idle, Auto-Stop) / Go의 상세 |
| V | OC-PMEM Implementation | p.295–296 | PSM(ECC·wear-leveling·conflict), bare-metal PRAM channel |
| VI | Evaluation | p.296–300 | DRAM 동등 성능 + 73% 전력↓ / 69% 에너지↓ / 4.3× vs checkpoint |
| VII–X | Related/Disc/Concl | p.300–301 | NVP·WSP·eADR 대비, write endurance·ECC 논의 |

---

## Section notes

### Introduction (p.289–290)

장기 실행 server 응용은 power failure에 대비해 journaling/checkpoint-restart를 쓰지만 data replication·serialized I/O·lock으로 성능 손해가 크다(p.289). PMEM을 쓰면 비휘발성을 얻지만, modern PMEM은 **non-deterministic latency**(프로세스가 DRAM 대비 3× 느린 경우 보고)가 문제다. 그 원인은 PMEM이 자체 DRAM buffer + 외부 DRAM cache로 DRAM 성능을 흉내 내는 **hybrid 설계**라서, fault-tolerant SW library가 buffer를 주기적으로 flush하고 DRAM↔PMEM 상태를 동기화해야 하기 때문(100%+ 공간 overhead, 성능 저하)(p.289). 측정상 PMEM이 user-level 성능을 **8.7×** 떨어뜨리고 전력을 **1.6×** 더 쓴다(p.289).

LightPC = **Light**weight **P**ersistence-**C**entric platform. 두 컴포넌트: ① **OC-PMEM**(open-channel PMEM, non-persistent path 제거 + PRAM access penalty 감소), ② **PecOS**(running process의 실행 상태를 빠르게 persistent로 전환)(p.289–290). 기여: open-channel architecture for PMEM, transparent full-system persistence(SnG), 실제 prototype(p.290).

### Background — PMEM HW & persistence 관리 (p.290–292)

PMEM은 단순 DIMM이 아니라 SSD 같은 복잡한 내부 시스템(p.289–290). PMEM hardware complex는 local-node DRAM DIMM + PMEM DIMM을 함께 두고 세 controller(DRAM ctrl, PMEM ctrl, **NMEM=near memory cache** controller)로 관리한다. memory mode는 DRAM-like 성능을 내지만 비휘발성 이점을 잃고, app-direct/sector mode는 `/dev`로 노출(p.290). PMEM DIMM 내부는 **LSQ(load-store queue)**가 요청을 reorder·combine해 256B 단위로 만들고, SRAM+DRAM 2-level inclusive cache로 64B cacheline을 처리 — firmware가 mapping table·wear-leveling을 관리(p.291). 이 buffering이 **DIMM-level read를 bare-metal PRAM 대비 2.9× 느리게** 만드는 등 non-determinism의 원천(p.291, Fig.2b).

persistence 관리(p.291–292): app-direct PMEM은 DAX로 device file을 가상주소에 매핑(주소변환 overhead는 작음). 그러나 multiple address object를 트랜잭션으로 durable하게 관리하려면 PMDK **libpmemobj**(object ID 기반 pointer)를 쓰는데, VA를 매 접근마다 계산해야 해 SW 개입·overhead가 크다(p.291). 측정상 `trans-mode`가 DRAM-only 대비 latency **8.7×**, `object-mode`가 latency·전력을 1.8×·1.6× 증가(p.292).

### LightPC Platform 개요 (p.292–293)

**OC-PMEM**: 모든 bare-metal PRAM device를 computing complex에 노출하고, controller 측에는 parallelism·tail-latency 관리만 남긴다(p.292). DRAM 관련 HW를 memory path에서 제거 → OS가 휘발 자원을 최소로 유지해 빠르게 persistent로 전환 가능. 두 핵심 모듈: **PSM(persistent support module)**(resource conflict·reliability 관리)과 **Bare-NVDIMM**(parallelism 위해 cache flush를 빨리 처리). 응용·OS가 PMEM 위에서 직접 실행되어 stack/heap/code까지 거의 영속(p.292).

**기본 철학**: DRAM 대체 NVM은 write가 아니라 **read 성능**으로 DRAM을 따라잡아야 한다(p.292). PRAM write는 4~8× 느리지만, PecOS의 Stop이 cache flush/memory fence 시에만 완료를 기다리므로 write latency는 대체로 감내 가능. 문제는 write가 진행 중인 위치를 read할 때 → PSM이 다른 매체 + ECC로 **target을 재생성(regenerate)**해 non-blocking read 제공(p.292).

### Persistence-Centric OS — PecOS (p.293–295)

OC-PMEM이 persistence를 주지만 cache·register 등 multi-core의 non-persistent 상태가 남는다(p.293). **PecOS**는 **single execution persistence cut(EP-cut)**을 만들어, 내용 변경 없이 안전하게 재실행할 수 있는 지점을 보장. EP-cut은 **Stop-and-Go(SnG)**로 구현되며 power event signal로 트리거되어 power hold-up time(전압이 규격의 95%로 떨어지기 전까지) 안에 비휘발 상태를 영속화(p.293).

- **Stop = Drive-to-Idle + Auto-Stop**(p.294–295).
  - **Drive-to-Idle**(p.294): power event를 처음 잡은 core가 master가 되어 system-wide persistent flag를 atomic하게 세팅, 살아있는 PCB(task_struct)를 순회. user process엔 TIF_SIGPENDING mask, 다른 core엔 IPI를 보내 깨어난 task를 idle로. 각 process를 TASK_UNINTERRUPTIBLE로 만들어 더 못 바뀌게 하고 idle task로 교체(register는 PCB에 저장). cache flush·memory fence가 없어 전체 Stop latency의 **12%**만 차지(p.294).
  - **Auto-Stop**(p.294–295): device/peripheral을 끄고 정보를 DCB(device control block)에 저장(표준 dpm 메커니즘 활용, 의존성 순서대로 suspend). 각 core가 cache를 dump하고 pending memory 요청을 memory fence로 완료, master는 bootloader로 점프해 모든 register를 OC-PMEM의 BCB에 저장하고 재시작 주소(MEPC)를 기록. device 처리까지 포함해 전체 SnG latency의 **38%**(all cores busy 시)(p.294–295).
- **Go**(p.295): 전원 복구 시 bootloader로 load되어 Stop commit을 확인, BCB로 master register 복원, dpm을 역순으로 device 부활, MEPC가 가리키는 EP-cut에서 모든 process를 재실행. PCB에 page table directory까지 있어 process별 가상주소 공간 복원 가능.

### OC-PMEM Implementation (p.295–296)

**PSM**(p.295–296): write/read/flush/reset port를 노출(표준 memory bus, AXI4). OC-PMEM이 PMEM을 가볍게 만드는 대신 PRAM의 긴 write·reliability 단점을 PSM이 떠안는다.
- **Reliability**: 요청은 캐시↔Bare-NVDIMM 직접 전송, write는 PSM의 ECC로 인코딩해 원본과 함께 저장. **XOR 기반 ECC(XCC)** — device당 XOR gate로 conflict half를 재생성, 32B/cacheline 복구. wear-leveling은 **StartGap** 기반(100 write마다 64B block shift), metadata는 4TB당 64B 미만(p.295–296, p.301).
- **Conflict management**: PRAM write가 read보다 4~8× 느리므로, PSM은 각 PRAM device에 **row buffer(BRAM)**를 둬 같은 page write를 모아 처리하고, 충돌 시 다른 매체+ECC로 **reconstruct**해 non-blocking read 제공(p.296).

**Bare-Metal PMEM Channel**(p.296): DRAM-like rank(단일 chip enable)는 PRAM의 큰 입력 granularity(32B) 때문에 256B 접근이 되어 64B cacheline 요청에 PRAM 자원을 낭비. LightPC의 **Bare-NVDIMM은 dual-channel**(PRAM 2개씩 묶어 CE 공유)이라 64B cacheline을 dual-channeled PRAM이 바로 처리하면서 나머지는 다른 요청에 — last-line cache 연산과 잘 맞물림(intra/inter-DIMM parallelism)(p.296).

### Evaluation (p.296–300)

**환경**(p.297): 28nm FPGA, 8 RV64 core(7-stage O3), 16KB I$.D$, Bare-NVDIMM 6 DIMM(DRAM 대비 read latency 2× / write latency 4.1×). 17개 HPC·SPEC·In-memory DB 워크로드(Redis/KeyDB/Memcached/SQLite 등). 4 비교군: LegacyPC(DRAM+OC-PMEM hybrid), LightPC-B(persistence 없이 OC-PMEM에 다 올림), LightPC.
- **In-memory 실행 성능**(p.297): PRAM read/write가 DRAM보다 1.1×/4.1× 느려도 완료시간 차이는 작아 **LightPC가 LegacyPC 대비 평균 12%만 느림** — 워크로드는 load가 store보다 27× 많고(load-store 특성), memory-level conflict가 사용자 경험에 더 중요하기 때문(p.297–298).
- **Head-of-line blocking 제거**(p.298): PSM의 non-blocking read 덕에 low-level read latency가 LightPC-B 대비 **7×~14.8× 개선**(wrf 평균 14.8×). 평균 read latency 9× 감소.
- **전력·에너지**(p.298): LightPC/LightPC-B가 LegacyPC 전체 전력의 **28%**만 소비(LegacyPC 18.9W vs LightPC 5.3W) — PRAM 자체가 저전력 + DRAM refresh/system 전력 부담 없음. 에너지는 LegacyPC 대비 **69% 우수**.
- **Persistence 비교**(p.299): persistence를 first-class로 두면 LightPC 실행시간이 SysPC/A-CheckPC/S-CheckPC 대비 각 **1.6×/8.8×/2.4×** 짧음. SnG가 전체 실행시간의 **0.3%**만 차지. flush latency도 checkpoint-restart보다 ATX/Server hold-up time 대비 짧음(p.299).
- **SnG 분석**(p.299–300): Stop이 power-down에서 19M cycle(4.5W·53mJ), Go가 복구에서 12.8M cycle(4.4W·52mJ). power inactivation delay보다 20% 짧아 fully consistent. SysPC는 system image 저장에 358× 느림(p.299).
- **Scalability**(p.300): 16KB cache로 worst-case에 최대 **32 core**까지 16ms ATX hold-up 안에 Stop 가능(server PSU 55ms면 64 core).

종합: **DRAM-only 수준 성능 + 73% 적은 전력 + 69% 적은 에너지, persistent 시스템 대비 4.3× 빠름**(Abstract, p.289).

### Related work & Discussion (p.300–301)

- **NVP**(energy-harvesting용 non-volatile processor): register/latch만 영속화, heap/stack은 못 다룸 → full-system persistence 아님(p.300).
- **WSP(whole-system persistence)**: ultracapacitor로 flush-on-fail. 연속 전원 장애·scaling·BIOS 보정 한계(p.300).
- **eADR**(Intel): power signal 시 cached data를 PMEM에 flush. 단 일관된 system state·device offline 순서(EP-cut) 보장은 부족(p.300).
- **Write endurance/ECC**(p.301): PRAM endurance 10^6~10^9(DRAM보다 낮음) → LightPC는 무거운 write-only 응용엔 부적합할 수 있음. 단 read가 write보다 민감하고 PRAM은 destructive read·refresh가 없어 working memory로는 여전히 유효. UBER 10^-19 위해 8-bit ECC 필요하나 XCC 보정력이 그보다 큼.

---

## Key vocabulary

**Thesis / framing:**
- "lightweight persistence-centric platform"
- "full system persistence"
- "reconsidering a vertical design of the current memory subsystems"

**Technical concepts:**
- "open-channel PMEM (OC-PMEM)" / "Bare-NVDIMM"
- "persistent support module (PSM)"
- "single execution persistence cut (EP-cut)"
- "Stop-and-Go (SnG)" / "Drive-to-Idle" / "Auto-Stop"
- "XOR-based ECC (XCC)" / "StartGap wear-leveling"

**Value language:**
- "make the system robust against power loss"
- "DRAM-only level performance ... while consuming 73% lower power"
- "non-blocking read services"

> ⚠ **피해야 할 어휘** (LightPC-signature, echo 시 모방으로 보임):
> - "LightPC" / "OC-PMEM" / "PecOS" / "Stop-and-Go" — 모두 이 논문 고유 명명, 인용 시 출처 명시

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.289 | "73% lower power and 69% less energy" (vs DRAM-only) | OC-PMEM+PecOS의 에너지 효율 한 줄 |
| Abstract, p.289 | persistent 시스템 대비 실행시간 "4.3×" 단축 | persistence를 first-class로 둔 효과 |
| Intro, p.289 | hybrid PMEM이 user-level 성능 "8.7×" 저하·전력 "1.6×" 증가 | PMEM hybrid 설계의 문제 정량화 |
| p.298 | LightPC가 LegacyPC 대비 "only 12%" 느림 | 비휘발 working memory의 실용성 근거 |
| p.298 | head-of-line read latency "7×~14.8×" 개선 | PSM non-blocking read 효과 |
| p.299 | SnG가 전체 실행시간의 "0.3%" | persistence cut의 거의-0 overhead |

---

## 🎯 Strategic anchor

> "if one can make data structures and execution flows persistent by reconsidering a vertical design of the current memory subsystems, it addresses the crash control overhead (on a power failure) and eliminates frequent memory flushes/synchronizations imposed by the PMEM's hybrid design." (§I Introduction, p.289)

→ **본인 활용**: 교수님 그룹의 일관된 방법론 — "매체가 아니라 **시스템 vertical 설계**(HW+OS co-design)를 다시 짜서 overhead를 없앤다" — 를 가장 압축적으로 보여주는 문장. 면담에서 내가 관심 있는 near-data/CXL-SSD 방향이 이 co-design 철학의 연장선임을 연결하는 데 사용.

---

## Connection to my research direction

| 차원 | 이 paper (LightPC) | 발표/관심 방향 |
|---|---|---|
| Scope | full-system persistence (OS+HW) | CXL-SSD, near-storage compute, checkpoint |
| Mechanism | OC-PMEM(firmware 제거) + PecOS(EP-cut) | host가 매체 직접 제어(open-channel/ZNS/FDP), checkpoint 경량화 |
| Persistence | 전원 장애 시 1-cut으로 실행상태 영속 | Sparse Checkpointing의 critical-path-off checkpoint |
| Open space | PRAM working memory의 endurance 한계 | CXL 매체·SSD로 확장 시 endurance/reliability 설계 |

LightPC의 "firmware/DRAM buffer를 host로 끌어올려 매체를 직접 노출"은 SSD internals의 **open-channel/ZNS/FDP** 흐름과 같은 철학이고, "persistence를 OS 수준 cut으로"는 checkpoint 발표의 상위 추상이다. 내 라인은 같은 co-design 철학을 CXL/최신 SSD 매체로 옮긴 것.

---

## Open questions / gaps

- [ ] PRAM(Optane) 단종 이후 OC-PMEM 철학을 어떤 nonvolatile 매체(CXL-attached, MRAM 등)로 잇나
- [ ] EP-cut/Stop-and-Go가 8 core FPGA를 넘어 수십~수백 core 상용 서버에서 hold-up time 안에 닫히는가 (p.300의 32/64 core 추정의 실측 검증)
- [ ] write-heavy 응용(burst buffer 등)에서 PRAM endurance 한계를 어떻게 우회 — 논문도 specialized 용도엔 부적합 가능성 인정(p.301)
- [ ] eADR 같은 상용 메커니즘이 발전한 지금, EP-cut의 device-offline 순서 보장 이점이 얼마나 차별적인가

---

## References worth following up

| 상태 | Ref | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [79] | Narayanan & Hodson, "Whole-system persistence", ASPLOS 2012 | WSP — LightPC의 직접 비교 대상(ultracapacitor flush-on-fail) |
| ☐ | [53] | Qureshi et al., "StartGap wear leveling", MICRO 2009 | PSM wear-leveling 근거 |
| ☐ | [81] | Intel eADR (Optane 200 brief), 2021 | 상용 persistence 메커니즘과의 차이 |
| ☐ | [13] | Intel Optane DC Persistent Memory | PMEM HW 구조의 근거 (→ concepts/CXL 인접 NVM 배경) |
| ☐ | [45] | Zhao et al., "Sonicboom: 3rd gen Berkeley out-of-order", 2020 | prototype의 RISC-V O3 core 기반 |

---

## Personal annotations

<자유 메모 영역 — 직접 추가>

- (메모) 같은 저자군의 [TrainingCXL](<../Failure Tolerant Training With Persistent Memory Disaggregation Over CXL/Failure Tolerant Training With Persistent Memory Disaggregation Over CXL.md>)은 LightPC의 "persistence-first" 철학을 CXL type-2 + 추천모델 학습이라는 응용으로 확장한 후속 격. LightPC(기반) → TrainingCXL(응용) → CXL-SSD 발표들(SkyByte/XHarvest)로 이어지는 계보로 읽으면 교수님 연구의 큰 그림이 잡힌다.
