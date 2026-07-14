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
> ⚠️ **Type은 host/device 개수를 정하지 않는다** — 개수·멀티호스트·일관성 관계는 아래 [[CXL Overview#⭐ Type × host/device 개수 × 일관성 — "언제 multi-host가 되나"|Type × 개수 × 일관성]] 참고.

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

## ⭐ Type × host/device 개수 × 일관성 — "언제 multi-host가 되나"
> [[SkyByte]] 읽다 판 질문 정리. **핵심: Type(1/2/3)은 host/device 개수를 정하지 않는다.** 개수를 정하는 건 ① device가 **CXL.cache를 쓰냐**(→ 한 host의 coherency domain에 묶임) ② **버전 + logical 구성**(`SLD`/`MLD`/`MHD`)이다.

**① 타입이 태생적으로 거는 제약 (cache vs mem)**
| Type | 프로토콜 | coherency domain 소속 | 태생적 host 수 |
|---|---|---|---|
| **Type 1** | .io + **.cache** | 한 host 도메인의 caching agent | **1** |
| **Type 2** | .io + .cache + .mem | 위 + 자체 메모리(bias) | **1** |
| **Type 3** | .io + **.mem** | 없음 (순수 메모리, 캐싱 안 함) | **1 → 다수** |

→ **.cache를 쓰는 Type 1/2는 태생적 single-host**(한 coherency domain에 묶임). **Type 3만** 풀링/공유로 멀티호스트 확장. (device 개수는 어느 타입이든 `1 host : 여러 device` 가능 — root port + switch fan-out.)

**② 버전이 얹는 topology (개수·공유가 실제로 갈리는 곳)**
| 버전 | host : device | Type 3 확장 | 여러 host면 coherence? |
|---|---|---|---|
| **1.1** | 1 : 1 (직결, `SLD`) | — | (single host) `HDM-H` |
| **2.0** | N : M (switch) | **`MLD`**: 1 device를 최대 **16 host에 분할** | ❌ 불필요 — **분할(pooling)이라 공유 아님** |
| **3.0+** | 다 : 다 (fabric) | **`MHD`/`GFAM`**: 같은 영역을 다수 host가 **공유** | ✅ 필요 — **`HDM-DB` Back-Invalidate** |

**③ 일관성 모델 3형 (`HDM-*`)** — 관리 주체가 host냐 device냐, 그리고 host 수
| 모델 | 관리 주체 | host 수 | 등장 |
|---|---|---|---|
| **HDM-H** (Host-only) | host가 전담, device 무개입 | 1 | 1.1~ (bias), 용어 정식화 3.2 |
| **HDM-D** (Device, bias) | device (Type 2 Host/Device Bias) | 1 | 1.1~ |
| **HDM-DB** (Device + Back-Invalidate) | device `DCOH`가 snoop filter로 **다수 host 캐시 무효화** | 다수 | **3.0~** |

> [!warning] "왜 기존 coherence로 multi-host가 안 되나" — domain 경계 때문
> coherence domain은 **host 경계에서 끊긴다.** host A의 home agent는 A의 캐시들만 알고 B는 존재조차 모른다 — A·B의 캐시 계층을 잇는 선도 프로토콜도 원래 없다. 그래서 A가 공유 라인을 고쳐도 **B에게 알릴 주체가 없어** B는 stale copy를 본다. 이 다리를 device가 놓는 게 **`HDM-DB` back-invalidate**(3.0). → single-host Type 3가 coherence를 "고려 안 하는" 건 기존 coherence가 **커버해서가 아니라 domain이 하나뿐이라 문제 자체가 안 생겨서**다. (경계가 깨지는 4지점: [[CXL Multi-node Coherence]])

> [!tip] "Type 2 쓰면 멀티호스트 되지 않나?" — 안 됨
> Type 2의 bias coherence는 **host ↔ device(가속기) 사이 single-host** 문제를 푼다(방향도 device→host 캐싱으로 오히려 반대). 두 host를 놓아도 A↔B 경계는 그대로. 멀티호스트 공유는 축이 다른 **`HDM-DB`(back-invalidate)**가 담당한다. (프로토콜 방향은 [[CXL Coherence]])

**우리 좌표**
- [[SkyByte]] = **Type 3 + `SLD` + `1 host : 1 device` + `HDM-H`** → 위 표 맨 왼쪽(1.1/2.0 single-host).
- 내 연구방향(multi-node coherence) = **Type 3 + `MHD`/`GFAM` + `HDM-DB`** → 맨 오른쪽(3.0). 개수로는 **"여러 host : 공유된 하나의 memory 영역"**이 무대이고 device가 coherence agent가 되는 지점. → [[CXL Multi-node Coherence]]
- **표준은 이미 있다**(3.0 Back-Invalidate) — 안 여문 건 **실리콘·directory 구현·정확성 증명** → 그게 연구 자리. (방어 포인트: "표준 없는 걸 한다"가 아니라 "표준 메커니즘 위 미해결을 판다")

### ④ "여러 host가 붙는다" ≠ "공유한다" — 3 레벨 분리
[[DirectCXL]] 영상에서 **스위치에 여러 host + 여러 device**가 붙은 걸 보고 "single host라며?" 헷갈리기 쉽다. **물리 연결**과 **논리 공유**를 섞어서 그렇다. 세 레벨로 쪼개면 명확하다.

| 레벨 | 내용 | CXL 2.0(DirectCXL) |
|---|---|---|
| ① **물리 연결** | switch에 multi-host, multi-device | ✅ (영상에서 본 것) |
| ② **메모리 소유** | HDM 1조각 = host 1개 (`MLD` 분할) | ✅ pooling |
| ③ **coherent 공유** | 여러 host가 **같은 영역**을 동시 캐싱 | ❌ (3.0에서만) |

> 🔑 **"여러 host가 같은 device에 붙는다"(①) ≠ "여러 host가 같은 HDM을 공유한다"(③).** CXL 2.0은 device를 `MLD`로 쪼개 각 조각을 다른 host에 주므로 ①·②는 되지만 ③은 안 된다. DirectCXL 원문: *"different hosts can be connected to a CXL switch and a CXL device ... **no host is sharing an HDM**"*. → 즉 "single host"는 **host 수가 아니라 'HDM 1조각당 host 1개(공유 없음)'** 라는 뜻.
> **주차장 비유**: 차 여러 대(host)가 한 주차장(device/switch)을 쓰지만(①) 한 칸(HDM)엔 한 대(②). 두 대가 같은 칸에 겹쳐 조율(③)하는 게 진짜 '공유' — 그건 3.0 back-invalidate가 있어야 한다.

### ⑤ coherence는 두 축 — 직교 + 합성
"Type-1이 device를 host cache처럼 만드는데, 거기에 sharing을 더하면 별개 coherence가 또 생기지 않나?" → **맞다. 축이 둘이고 직교한다.**

| 축 | 언제 | 범위 | 누가 관리 | 등장 |
|---|---|---|---|---|
| **축 A — device 캐싱** | device가 host mem을 캐시 | **intra-host**(한 도메인) | **Type-1/2** (CXL.cache, bias) | 1.1~ |
| **축 B — multi-host sharing** | 여러 host가 같은 영역 캐시 | **cross-host** | **device** directory + back-invalidate | **3.0** |

- **축 A는 sharing을 못 푼다** — "이 device ↔ 이 host" 일관성이지 "host A ↔ host B"가 아니다. 그래서 sharing을 더하면 **Type과 무관하게 새 coherence(축 B)**가 생긴다.
- **공유되는 건 memory(HDM)** → 축 B의 *원천*은 **HDM을 가진 Type-2/3**. Type-1은 HDM이 없어(host mem을 캐시만) 풀로 공유할 자기 메모리가 없다. 단 Type-1의 캐시 사본도 무효화 대상 — 그건 **자기 host가 back-invalidate 받을 때** 그 host 도메인 안(축 A)에서 함께 처리된다(device가 Type-1 agent를 따로 추적 안 함).
- **두 축은 합성**된다: **host 내부 = 축 A**(native + CXL.cache), **host 사이 = 축 B**(back-invalidate). B가 A 위에 얹힌 2층 구조.

> 🔑 **coherence = 사본이 있으면 필요.** 사본 주체가 (a) 한 host 안의 CPU/Type-1·2 캐시 → 축 A, (b) 다른 host들 → 축 B. **sharing은 축 B를 켜는 스위치**이고, 그 스위치는 **공유되는 HDM을 가진 device(Type-2/3)**에 달려 있다. → 방향·본질은 [[CXL Coherence]], 축 B의 난관(directory 폭발 등)은 [[CXL Multi-node Coherence]].

## 우리 위키와의 연결
- 토픽 허브: [[CXL]] (CXL 관련 발표 논문 모음)
- 발표 논문: [[SkyByte]] (memory-semantic CXL-SSD), [[XHarvest]] (CXL 자원 harvesting)
- 상위 framing: [[Communication Tax]] (CXL composable → CXL-over-XLink)

## Personal annotations
<본인 메모 영역>
