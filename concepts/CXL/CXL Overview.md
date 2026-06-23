---
title: "CXL Overview — 한 눈에 보는 CXL 기초"
aliases: [CXL 기초, CXL Basics, CXL 101, CXL Overview]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL Overview — 한 눈에 보는 CXL 기초

> [!abstract] 이 폴더는 뭐지?
> CXL을 처음 보는 사람을 위한 **개념 정리**. 논문(`papers/`)이 아니라 **배경지식**(`concepts/`)이다. spec(버전)마다 새 개념이 *누적*되므로, **버전별로 "새로 생긴 것 + 새 용어"** 를 따라가면 한눈에 들어온다.
> 읽는 순서: 이 Overview → [[CXL 2.0]] → [[CXL 3.0]] → [[CXL 3.1]] → [[CXL 3.2]] → [[CXL 4.0]] (필요 시 [[CXL 1.0-1.1]]). 모르는 단어는 [[CXL Glossary]]. 최신 동향은 [[CXL SOTA & Roadmap]].

## 한 문장
**CXL(Compute Express Link)** = PCIe 물리계층 위에서 도는, **CPU·가속기·메모리가 캐시 일관성(cache coherence)을 유지하며 메모리를 직접 load/store로 주고받게 해주는** 개방형 인터커넥트.

> 왜 필요? GPU/CPU는 메모리가 각자 고정돼 있어 낭비·부족이 동시에 생긴다. CXL은 메모리를 연산에서 **떼어내(disaggregate)** 풀로 만들고, 여러 장치가 **일관성 있게 공유**하게 한다. → [[Communication Tax]](교수님 framing)의 하드웨어 토대.

## CXL의 3가지 프로토콜 (항상 등장)
| 프로토콜 | 하는 일 | 일관성 | 비유 |
|---|---|---|---|
| **CXL.io** | 장치 발견·설정·DMA·인터럽트 (PCIe 그대로) | 비일관 | "기본 배선" — 모든 장치 필수 |
| **CXL.cache** | **장치가 호스트 메모리를 캐시**(coherent) | 일관 | "장치가 CPU 메모리를 자기 것처럼" |
| **CXL.mem** | **호스트가 장치 메모리를 load/store**로 접근 | 일관 | "CPU가 장치 메모리를 자기 것처럼" |

## 장치 3가지 타입 (어떤 프로토콜을 쓰냐로 구분)
| Type | 사용 프로토콜 | 메모리 노출? | 예시 |
|---|---|---|---|
| **Type 1** | .io + .cache | ✕ | 캐시 가진 가속기·SmartNIC |
| **Type 2** | .io + .cache + .mem | ✓ | GPU·FPGA (자체 메모리 보유) |
| **Type 3** | .io + .mem | ✓ | **메모리 확장기 / 메모리 풀** (가장 흔한 CXL 용도) |

> SSD를 CXL 메모리처럼 붙이는 [[SkyByte]]·[[XHarvest]]는 **Type 3 (memory-semantic)** 계열로 이해하면 된다.

## ⭐ 버전 진화 — 한 눈에 (핵심 표)
| 버전 | 발표 | PCIe 기반 | 새로 생긴 핵심 | 새 용어 |
|---|---|---|---|---|
| **1.0 / 1.1** | 2019 | PCIe 5.0 (32 GT/s) | 3개 프로토콜, Type 1/2/3, **single host 직접 연결** | `CXL.io/.cache/.mem`, `HDM`, `bias coherency` |
| **2.0** | 2020 | PCIe 5.0 | **switch 1단**, **memory pooling**, hot-plug, persistent memory, 링크 암호화 | `MLD/SLD`, `Fabric Manager`, `IDE` |
| **3.0** | 2022 | **PCIe 6.0 (64 GT/s)** | **다단 switch**, **memory sharing**, **PBR(4096노드)**, 장치 간 **P2P** | `PBR`, `back-invalidation`, `GFAM`, `MHD`, `256B FLIT` |
| **3.1** | 2023 | PCIe 6.0 | **fabric 확장(scale-out)**, **보안 TEE(TSP)**, **host간 통신(GIM)** | `TSP`, `GIM`, `PBR switch FM API` |
| **3.2** | 2024-12 | PCIe 6.0 | device 관리·**memory tiering(CHMU)**·보안 보강 (속도 동일) | `CHMU`, `HDM-H` |
| **4.0** | **2025-11** | **PCIe 7.0 (128 GT/s)** | **대역폭 2배**, **bundled ports(1.5 TB/s)**, multi-rack | `bundled ports`, `native x2`, `retimer` |

> 핵심 흐름 = **연결(1.1) → 풀링(2.0) → 공유·패브릭(3.0) → 보안·확장(3.1) → 관리·tiering(3.2) → 대역폭·multi-rack(4.0)**.
> 최신 동향·배포 시점·경쟁기술(UALink/NVLink)은 [[CXL SOTA & Roadmap]].
> **pooling vs sharing** 차이가 2.0↔3.0의 분수령 → 헷갈리면 [[CXL Glossary#memory-pooling-vs-sharing]].

## 일관성 모델의 진화 (왜 3.0이 중요한가)
- **1.1 / 2.0 — bias 기반**: Type 2 장치 메모리를 **Host Bias**(호스트가 주로 접근) / **Device Bias**(장치가 주로 접근) 모드로 전환해 일관성 오버헤드를 줄임.
- **3.0 — Back-Invalidation(BI)**: 하드웨어가 캐시 사본을 무효화하는 snoop을 추가 → 여러 호스트가 **같은 메모리를 동시에 일관성 있게 공유**(true sharing) 가능. (`HDM-DB`)

## 우리 위키와의 연결
- 토픽 허브: [[CXL]] (CXL 관련 발표 논문 모음)
- 발표 논문: [[SkyByte]] (memory-semantic CXL-SSD), [[XHarvest]] (CXL 자원 harvesting)
- 상위 framing: [[Communication Tax]] (CXL composable → CXL-over-XLink)

## Personal annotations
<본인 메모 영역>
