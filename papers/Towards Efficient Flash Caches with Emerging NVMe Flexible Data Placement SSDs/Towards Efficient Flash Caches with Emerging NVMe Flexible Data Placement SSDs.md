---
title: Towards Efficient Flash Caches with Emerging NVMe Flexible Data Placement SSDs
aliases: [Flash Caches FDP]
description: FDP SSD의 데이터 배치 기능으로 CacheLib의 SOC/LOC 데이터를 분리해 호스트 오버프로비저닝 없이 DLWA ~1을 달성하는 연구.
venue: Eurosys
year: 2025
arxiv: "2503.11665"
tier: deep
status: done
tags: [paper, cluster/fs, topic/flash-cache, topic/fdp, topic/caching, venue/eurosys, year/2025]
---

# Towards Efficient Flash Caches with Emerging NVMe Flexible Data Placement SSDs

> **EuroSys 2025** · cluster/fs · Source: [Towards Efficient Flash Caches with Emerging NVMe Flexible Data Placement SSDs.pdf](<Towards Efficient Flash Caches with Emerging NVMe Flexible Data Placement SSDs.pdf>)

**저자:** Michael Allison, Arun George, Javier Gonzalez, Dan Helmick, Vikash Kumar, Roshan R Nair, Vivek Shah (Samsung Electronics) — 저자 이름은 알파벳순으로 나열됨

---

## TL;DR

대규모 웹 서비스의 Flash cache(예: Meta의 CacheLib)는 작은 객체용 set-associative **SOC**(Small Object Cache)와 큰 객체용 log-structured **LOC**(Large Object Cache)를 한 SSD에 함께 저장한다. SOC는 random/hot/small 쓰기, LOC는 sequential/cold/large 쓰기를 만드는데, 이 둘이 같은 SSD erase block에 **섞이면(intermixing)** garbage collection(GC) 비용이 커져 **Device-Level Write Amplification(DLWA)**가 높아진다. 현실에서는 이를 acceptable한 ~1.3으로 낮추려고 SSD 용량의 **거의 50%를 host overprovisioning**으로 비워둔다.

이 논문은 새로 ratified된 **NVMe Flexible Data Placement(FDP)** 인터페이스를 CacheLib에 통합하여, SOC와 LOC 데이터를 서로 다른 **Reclaim Unit Handle(RUH)**에 태깅해 물리적으로 분리한다. 그 결과 host overprovisioning 없이도 **DLWA ~1**을 달성하며, ALWA·throughput·hit ratio에는 거의 영향이 없다. 이는 **SSD device 비용 2x 절감, embodied carbon 4x 절감**, 그리고 DRAM 절감·multi-tenant 배치 같은 기존에 불가능했던 기회를 연다. 변경 사항은 CacheLib 상류(upstream)에 머지되어 배포되었다.

---

## 문제 & 동기

- **Flash cache의 핵심 과제:** 제한된 Flash endurance 하에서 높은 hit ratio와 낮은 indexing overhead를 유지하며, 다양한 크기·접근 패턴의 객체를 다뤄야 한다. 대규모 서비스는 수십억 개의 자주 접근되는 small item과 수백만 개의 드물게 접근되는 large item을 처리한다.
- **DLWA가 그동안 등한시됨:** Flash cache 연구는 admission policy, ALWA(application-level WA), caching algorithm에 집중했지만 **device-level WA(DLWA)**는 거의 다뤄지지 않았다. DLWA = Total NAND Writes / Total SSD Writes.
- **DLWA의 중요성:** SSD 수명은 DLWA에 반비례한다. DLWA=2면 수명이 절반으로 줄고 두 배 빨리 교체해야 한다. SSD 제조는 연간 수백만 톤의 CO2(embodied carbon)를 배출하며, SSD의 embodied carbon은 HDD보다 최소 한 자릿수 크다. DLWA 감소는 capital cost와 embodied/operational carbon 모두를 줄인다.
- **근본 원인(저자 분석):** SOC(random/hot)와 LOC(sequential/cold) 두 caching engine의 데이터가 SSD 블록에서 **intermixing**되는 것이 높은 DLWA의 원인. 이를 분리(targeted data placement)하면 DLWA와 embodied carbon을 줄일 수 있다.
- **기존 대처의 비효율:** Meta의 CacheLib 운영은 acceptable DLWA(~1.3)를 위해 Flash 용량의 **~50%를 host overprovisioning**으로 비워둔다 → 비용·carbon 양면에서 비효율.

> [!quote]- 📄 원문 표현 (paper)
> - "However, the problem of device-level write amplification (DLWA) in Flash caches has not received much attention." (p.1)
> - "Our analysis shows that the cause of high DLWA in these cache designs is the intermixing of data from the two different caching engines." (p.2)
> - "In production CacheLib deployments, 50% of the Flash capacity is overprovisioned to keep DLWA within acceptable levels of ~1.3" (p.4)

---

## 핵심 통찰

논문 §4의 6가지 Insight:

1. **SOC/LOC 데이터 intermixing이 높은 DLWA를 유발** — LOC의 sequential·cold 쓰기와 SOC의 random·hot 쓰기가 한 SSD 블록에 섞이면 GC 시 valid data 이동이 커진다 (Fig 3a).
2. **Host overprovisioning을 DLWA 제어 수단으로 쓰는 것은 비효율적** — LOC는 sequential·cold라 OP가 필요 없는데, SOC와 섞이면 device OP를 둘이 공유하게 되어 낭비된다 (Fig 3a).
3. **SOC의 높은 invalidation과 작은 크기를 DLWA 제어에 활용 가능** — 작은 SOC 크기 → bucket이 적고 key collision rate가 높아 invalidation rate가 높다. 4KB bucket 전체가 매번 통째로 쓰여서, invalidation이 SSD page 단위로 일어나면 erase block 전체가 free되어 valid data 이동이 불필요해진다 (단, SOC가 격리되어 있을 때만).
4. **FDP 데이터 배치가 CacheLib의 DLWA 제어를 도움** — SOC/LOC를 서로 다른 RUH로 분리하면 상호 배타적 reclaim unit에 저장된다. (a) LOC는 sequential 덮어쓰기로 최소 이동·DLWA, (b) 분리된 SOC는 자기 자신만 invalidate, (c) device OP를 SOC 전용으로 cushion에 사용 (Fig 3b).
5. **Initially Isolated FDP 장치로도 CacheLib의 DLWA 제어 가능** — SOC/LOC 분리 시 live data 이동은 SOC 때문에만 발생하므로, Initially Isolated든 Persistently Isolated든 SOC/LOC 격리는 보존된다 (저렴한 Initially Isolated로 충분).
6. **FDP 데이터 배치는 carbon emission을 줄임** — DLWA 개선 → device 수명 증가 → 교체 감소 → embodied carbon 감소. GC 감소 → active state 시간 감소 → operational carbon 감소.

> [!quote]- 📄 원문 표현 (paper)
> - "Insight 1: Intermixing of SOC and LOC data leads to high DLWA." (p.6)
> - "Insight 4: Data placement using FDP can help CacheLib control DLWA." (p.7)
> - "Insight 6: Data placement using FDP can help reduce carbon emissions in CacheLib." (p.8)

---

## 설계 / 메커니즘

- **CacheLib 구조(Fig 1):** RAM Cache(인기 항목) → SSD Cache Engine(Navy). SSD 엔진은 **SOC**(set-associative, 4KB bucket 단위, uniform hashing, 거의 추적 overhead 없음, random write)와 **LOC**(log-structured, 16MB/256MB region 단위, erase block과 정렬, FIFO/LRU eviction, sequential write)로 구성. SOC/LOC threshold와 크기는 배포 시 설정 가능.
- **FDP 개념(§3.2):**
  - **Reclaim Unit(RU):** NAND이 묶인 단위(이 논문에선 superblock 크기 = die의 plane들에 걸친 erase block 집합). RU는 직접 주소지정 불가.
  - **Reclaim Group(RG):** RU의 집합.
  - **Reclaim Unit Handle(RUH):** RU를 가리키는 host측 포인터 추상화. host는 RUH로 데이터를 논리적으로 격리. device가 RUH→RU 매핑을 관리.
  - **RUH 타입 2종:** (1) **Initially Isolated** — GC 시 같은 RG 내 RU들이 이동 후보가 되어 intermix될 수 있음. 구현 저렴, 제약 적음. (2) **Persistently Isolated** — 해당 RUH로 쓴 RU만 이동 후보. 격리 보장 강하나 추적 비용 큼.
  - FDP는 **새 command set 없이** write command에 RUH를 지정하는 data placement directive만 추가 → **backward compatible**, 애플리케이션 변경 불필요.
- **본 논문 SSD 설정:** Samsung PM9D3a, 단일 FDP 설정 = **8 Initially Isolated RUHs, 1 RG, RU 크기 ~6GB**.
- **CacheLib 통합 설계(§5, Fig 4):**
  - **Placement Handle** 추상 개념 도입 → SOC/LOC가 각자 다른 placement handle을 받아 데이터를 분리. FDP 미지원 장치면 default handle(배치 선호 없음).
  - **Placement Handle Allocator(1a)** → `<RUH, RG>` 쌍 = Placement Identifier(PID)를 module에 할당. minor consumer(예: metadata)는 default RUH.
  - **FDP-aware I/O Management** → SOC/LOC가 자기 I/O를 placement handle로 태깅 → device layer가 PID를 NVMe spec의 DSPEC/DTYPE 필드로 변환 → Linux **I/O Passthru** + `io_uring`(worker thread당 queue pair)로 동기화 없이 제출.
  - **설계 원칙:** Keep it simple / FDP는 또 하나의 storage tech일 뿐(backward compat) / SW·HW 확장성.
  - **Lessons:** FDP 특화 LOC eviction policy는 이득 적었음. **dynamic/adaptive 배치는 simple static solution보다 못함**(static predefined placement handle이 더 좋음).
- **이론 모델(§4.2, Appendix A):** LOC DLWA≈1 가정, SOC만 device WA에 기여. SOC bucket을 uniform hash로 모델링. **Theorem 1:** `DLWA = 1/(1−δ)`, 여기서 δ는 GC로 인한 평균 live SOC bucket migration이며 Lambert W 함수로 표현. **Theorem 2:** embodied carbon = `DLWA × Device_cap × (T/L_dev) × C_SSD`. **Theorem 3:** operational energy ∝ GC event 수. 모델은 실측과 최대 ~16% 오차(높은 SOC 크기에서, skew 때문).

> [!quote]- 📄 원문 표현 (paper)
> - "We introduce the abstract concept of Placement Handle to CacheLib's SSD I/O path to achieve FDP-based data placement while preserving backward compatibility." (p.8)
> - "The SOC and LOC instances tag their I/Os with unique placement handles." (p.9)
> - "Dynamic and adaptive data placement is outperformed by simple static solutions." (p.9)

---

## 평가

- **셋업:** 24-core Intel Xeon Gold 6432 ×2, ~528GB DRAM, **Samsung PM9D3a 1.88TB** FDP SSD(2 namespace, 1 RG, 8 Initially Isolated RU handles, RU ~6GB). Ubuntu 22.04(6.1.64) / CentOS 9(6.1.53). `nvme-cli` 2.7.1. CacheBench로 trace replay. **FDP**(분리 켬) vs **Non-FDP**(분리 끔) 비교. DLWA는 `nvme get-log`로 10분 간격 측정, 실험마다 TRIM으로 초기화.
- **Workload:** Meta **KV Cache**(read-intensive, GET:SET=4:1), Twitter **cluster12**(write-intensive, SET:GET=4:1), **WO KV Cache**(write-only, KV Cache에서 GET 제거).
- **§6.2 DLWA ~1:** KV Cache, 50% utilization, SOC=4%. FDP가 DLWA를 Non-FDP의 **1.3 → 1.03**으로 낮춤(1.3x 감소) (Fig 5).
- **§6.3 SSD utilization vs 성능:** utilization을 50→100%로 올리면 Non-FDP DLWA는 **1.3 → 3.5**로 급증하지만, FDP는 **~1.03 유지**(Fig 6). throughput·DRAM/NVM hit ratio·ALWA는 불변. 100% util에서 p99 read **1.75x**, p99 write **10x** 개선(GC 간섭 감소).
- **§6.4 write-intensive 워크로드:** Twitter, WO KV Cache 모두 FDP로 **DLWA ~1** 달성(Fig 7, 8).
- **§6.5 SOC 크기 효과(Fig 9):** SOC를 4→64%로 키우면 FDP DLWA가 **1.03 → 2.5**로 상승(Non-FDP는 항상 >3). SOC가 device OP(보통 7–20%) 크기를 넘으면 spare block이 부족해 cushion 실패. SOC 90%·96%에선 FDP 이득 사라짐. 즉 **FDP 이득은 SOC가 작을 때(small-object dominant)** 최대.
- **§6.6 carbon(Fig 10, 5년 lifecycle, 0.16 CO2 Kg/GB):** embodied CO2e 대폭 감소, **GC event ~3.6x 감소**(operational). Table 2: 100% util, SOC 4%에서 FDP 4GB RAM은 CO2e **347.2 vs 1081.1 Kg**(Non-FDP) → 낮은 DRAM + 100% util 배치가 FDP로만 viable, throughput 1.5x 손해로 carbon 4x 이득.
- **§6.7 multi-tenant(Fig 11):** overprovisioning 없는 1.88TB SSD를 두 tenant가 절반씩(~930GB) 공유, 각 WO KV Cache. FDP는 각 tenant가 SOC/LOC를 분리해 **DLWA ~1 유지**, Non-FDP는 ~3.5. **3.5x DLWA 감소.** FDP로 host OP 50%를 해제해 SSD를 100%까지 활용 가능.
- **Appendix B(Fig 13):** WO KV Cache 100% util에서 FDP가 DLWA **3.5x**, p99 read **2.2x**, p99 write **9.5x** 이득.

> [!quote]- 📄 원문 표현 (paper)
> - "We can see that the SOC and LOC data segregation into two different reclaim unit handles helps to lower DLWA from 1.3 observed without data segregation to 1.03." (p.10)
> - "We can see that the throughput, DRAM and NVM cache hit ratio metrics remain unchanged with FDP-based segregation compared to the baseline." (p.10)
> - "Figure 11 shows that the DLWA remains ~1 because each tenant segregates its SOC and LOC data. In contrast, without FDP the DLWA increases to ~3.5." (p.13)

---

## 섹션 노트

- **§1 Introduction** — caching이 대규모 웹 서비스의 성능·자원 활용의 핵심. SOC(random write)/LOC(sequential write)의 distinct write pattern. CacheLib는 Meta 수백 개 서비스의 building block이며 Flash 용량의 **50%만 활용**. 기여: FDP 인터페이스 리뷰, FDP 데이터 분리 이점 분석, DLWA 이론 모델, CacheLib 구현(아키텍처/API 불변), 평가.
- **§2 Background** — SSD basics(die/plane/block/page, FTL, GC), DLWA/ALWA 정의(식 1, 2), DLWA와 carbon emission 관계, CacheLib 아키텍처(RAM/SSD hybrid, SOC/LOC).
- **§3 NVMe FDP** — FDP는 Google SmartFTL + Meta Direct Placement Mode의 병합이며 ZNS/Open-Channel의 SW 엔지니어링 비용 없이 데이터 배치. backward compatible. Table 1: Streams/Open-Channel/ZNS/FDP 비교(FDP만 random+sequential 지원 & 애플리케이션 unchanged). 한계: (1) 신기술·진화 중, (2) GC에 대한 host 제어 없음, (3) device에 OP·매핑 테이블 필요.
- **§4 Why FDP Matters** — 6 Insight + 이론 모델(Theorem 1–3).
- **§5 Design/Implementation** — Placement Handle, Allocator, FDP-aware I/O(io_uring passthru), lessons.
- **§6 Evaluation** — DLWA ~1, utilization·SOC 크기 효과, carbon, multi-tenant.
- **§7 Related Work** — SSD 데이터 배치(Streams, Open-Channel, ZNS, SDF), KV store/hybrid caching(Flashield, Kangaroo, FairyWREN). 본 연구는 CacheLib 아키텍처·디자인을 바꾸지 않고 I/O 경로의 데이터 배치만으로 DLWA를 줄이는 점에서 보완적.
- **§8 Conclusion** — host OP 없이 ideal DLWA ~1 → 2x cost / 4x carbon 절감. 상류 머지 완료.

---

## 핵심 용어

- **DLWA (Device-Level Write Amplification):** SSD 내부 NAND 쓰기 / host가 SSD에 보낸 쓰기. 본 논문의 주 metric. = `Total NAND Writes / Total SSD Writes`.
- **ALWA (Application-Level Write Amplification):** SSD 쓰기 / 애플리케이션이 받은 쓰기. = `Total SSD Writes / Total Application Writes`.
- **SOC (Small Object Cache):** set-associative, 4KB bucket, uniform hashing, random/hot write, 추적 overhead 거의 없음.
- **LOC (Large Object Cache):** log-structured, 16MB+ region, FIFO/LRU, sequential/cold write, DRAM 추적 overhead 있음.
- **FDP (Flexible Data Placement):** NVMe ratified 인터페이스(2022 말). write에 RUH directive 추가, backward compatible, GC는 SSD가 관리.
- **RU / RG / RUH:** Reclaim Unit / Reclaim Group / Reclaim Unit Handle. host는 RUH로 데이터를 논리적으로 격리, device가 RUH→RU 매핑 관리.
- **Initially Isolated vs Persistently Isolated:** RUH 타입. 전자는 GC 시 intermix 가능(저렴), 후자는 강한 격리 보장(비쌈).
- **Placement Handle / PID:** CacheLib 추상화. module별 데이터 배치 선호. PID = `<RUH, RG>` → NVMe DSPEC/DTYPE 필드로 변환.
- **Embodied vs Operational carbon:** 제조 시 배출(embodied, SSD 교체 빈도) vs 운영 시 배출(operational, GC/active state energy).
- **Host overprovisioning:** host가 SSD 용량 일부를 비워두는 것. CacheLib는 DLWA ~1.3 위해 ~50% 사용 → FDP로 제거.

---

## 강점 · 한계 · 열린 질문

**강점**
- 실제 production trace(Meta KV Cache, Twitter cluster12)와 실제 FDP 하드웨어(Samsung PM9D3a)로 평가 → 현실성 높음.
- CacheLib **아키텍처·API·ALWA 불변**, 변경이 minimal·non-invasive, 이미 **상류 머지·배포**됨 → 실용성 입증.
- DLWA의 이론 모델(Lambert W 기반)을 제시하고 실측과 ~16% 이내로 검증.
- carbon(embodied + operational)을 정량화 → sustainability 관점의 contribution.

**한계**
- **FDP 이득은 SOC가 작을 때만** 유효. SOC가 device OP 크기(7–20%)를 넘으면 이득이 사라짐(§6.5, Fig 9) → small-object dominant 워크로드에 국한.
- FDP는 **GC에 대한 host 제어가 없음**(데이터 배치 전용). host가 GC를 더 잘 관리할 수 있는 시나리오엔 부적합(저자 인정).
- dynamic/adaptive 배치는 이득 없었고 static만 사용 → 더 정교한 정책의 잠재력 미탐구.
- 단일 vendor의 단일 FDP 설정(8 RUH, 1 RG, ~6GB RU)에 의존 → 일반화 범위 제한.
- 저자 전원 Samsung 소속(FDP SSD 제조사) → 잠재적 이해상충.

**열린 질문**
- SOC가 큰 워크로드나 다른 caching 아키텍처(예: 순수 log-structured)에도 FDP 이득이 있을까?
- Persistently Isolated RUH가 더 큰 이득을 줄 수 있는 RU가 작은 시나리오는?
- multi-tenant를 2개 이상으로 확장하면 RUH 8개 제한이 병목이 될까?

---

## ❓ Q&A (자가 점검)

> [!question]- Q1. 높은 DLWA의 근본 원인은 무엇이며 기존엔 어떻게 대응했나?
> SOC(random/hot/small)와 LOC(sequential/cold/large) 두 caching engine의 데이터가 같은 SSD erase block에 **intermixing**되어 GC 시 valid data 이동이 커지는 것. CacheLib 운영은 DLWA를 acceptable한 ~1.3으로 낮추려고 SSD 용량의 **~50%를 host overprovisioning**으로 비워두었다(비용·carbon 비효율).

> [!question]- Q2. FDP는 어떻게 데이터를 물리적으로 분리하나?
> write command에 **Reclaim Unit Handle(RUH)** 데이터 배치 directive를 추가하면, device가 해당 RUH가 가리키는 별도 Reclaim Unit(RU)에 데이터를 쓴다. CacheLib는 SOC와 LOC에 서로 다른 RUH(PID)를 할당해 상호 배타적 RU에 저장 → intermixing 제거. 새 command set이 없어 backward compatible.

> [!question]- Q3. FDP는 ZNS·Open-Channel·Streams와 무엇이 다른가?
> Table 1 기준: ZNS는 sequential write만·host-based GC, Open-Channel은 host의 NAND media 관리 필요, Multi-Streamed는 host feedback 없음. **FDP만** random+sequential 둘 다 지원하고 GC를 SSD가 관리(log feedback 제공)하며 **애플리케이션을 변경 없이 실행** 가능(backward compatible). FDP는 Google SmartFTL + Meta Direct Placement Mode의 병합.

> [!question]- Q4. Initially Isolated와 Persistently Isolated RUH의 차이는? 이 논문은 무엇을 썼나?
> Initially Isolated는 GC 시 같은 RG 내 RU들이 이동 후보가 되어 intermix될 수 있으나 구현이 저렴. Persistently Isolated는 해당 RUH로 쓴 RU만 이동 후보라 격리가 강하지만 추적 비용이 크다. Insight 5에 따라 SOC/LOC 격리는 두 타입 모두에서 보존되므로, 이 논문은 **저렴한 Initially Isolated RUH 8개**를 사용했다.

> [!question]- Q5. 핵심 평가 수치는?
> KV Cache 50% util에서 DLWA **1.3 → 1.03**(1.3x). utilization 100%에서 Non-FDP는 **3.5**로 치솟지만 FDP는 ~1.03 유지. SSD device 비용 **2x**, embodied carbon **4x** 절감, GC event **~3.6x** 감소. 100% util p99 write latency **10x**(KV Cache) 개선. multi-tenant에서 DLWA **3.5x** 감소.

> [!question]- Q6. FDP 이득이 사라지는 조건은?
> SOC(random write) 크기를 키워 device OP 크기(보통 7–20%)를 넘기면, SOC 데이터를 cushion할 spare block이 부족해진다(§6.5). SOC 4→64%면 FDP DLWA가 1.03→2.5로 오르고, 90%·96%에선 이득이 거의 없다. 즉 **small-object dominant 워크로드**에서만 큰 이득.

> [!question]- Q7. CacheLib 통합이 "minimally invasive"한 이유는?
> SOC/LOC 분리는 **Placement Handle 추상화**와 FDP-aware device layer만 추가하고 CacheLib의 아키텍처·user-facing API·ALWA를 바꾸지 않는다. FDP 미지원 장치엔 default handle을 써서 backward compatible하다. 그래서 변경이 **상류(upstream)에 머지**되어 배포될 수 있었다.

> [!question]- Q8. DLWA 이론 모델의 핵심 식은?
> Theorem 1: `DLWA = 1/(1−δ)`. δ는 GC로 인한 평균 live SOC bucket migration이며 Lambert W 함수로 표현된다. LOC DLWA≈1로 가정하고 SOC만 device WA에 기여한다고 본다. 모델은 실측과 최대 ~16% 오차(높은 SOC 크기에서 key skew 때문).

---

## 🔗 Connections

[[File System]] · [[Eurosys]] · [[2025]]

---

## References worth following

- **CacheLib** — Berg et al., *The CacheLib Caching Engine: Design and Experiences at Scale*, OSDI 2020 [23]. 본 논문의 기반 시스템.
- **Kangaroo** — McAllister et al., *Kangaroo: Caching Billions of Tiny Objects on Flash*, SOSP 2021 [48]. small object Flash caching, log-structured + set-associative 동기.
- **FairyWREN** — McAllister et al., *A Sustainable Cache for Emerging Write-Read-Erase Flash Interfaces*, OSDI 2024 [47]. Kangaroo + ZNS, 가장 가까운 비교 대상.
- **ZNS** — Bjørling et al., *ZNS: Avoiding the Block Interface Tax for Flash-based SSDs*, USENIX ATC 2021 [24]. FDP의 직전 데이터 배치 제안.
- **FDP TP4146** — NVMe Flexible Data Placement Technical Proposal [19]. 인터페이스 명세.
- **Twitter trace** — Yang et al., *A Large-Scale Analysis of Hundreds of In-Memory Cache Clusters at Twitter*, OSDI 2020 [59]. 평가 워크로드 출처.
- **DLWA modeling** — Dayan et al., *Modelling and Managing SSD Write-Amplification*, 2015 [30]. 이론 모델의 방법론 기반.
- **Embodied Carbon / ACT** — Tannu & Nair [57], Gupta et al. ISCA 2022 [34, 35]. carbon 모델링 근거.

---

## Personal annotations

<본인 메모 영역>
