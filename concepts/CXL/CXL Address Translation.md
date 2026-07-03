---
title: "CXL Address Translation — logical·bus·physical·block과 MMU/IOMMU"
aliases: [Address Translation, 주소 변환, MMU, IOMMU, logical bus physical block, HPA DPA]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL Address Translation — 주소 4종과 변환기

> [!abstract] 이 노트
> **어떤 요청자(CPU/디바이스)가 어떤 주소를 쓰고, 무엇이 그걸 physical로 바꾸며, CXL이 그 경계를 어떻게 흐리는가**에 대한 배경지식(`concepts/`, 논문과 별개). 큰 그림은 [[CXL Overview]], 모르는 단어는 [[CXL Glossary]].

## 한 문장
프로그램·디바이스는 *자기만의 주소*로 메모리를 가리키고, 변환기(**MMU**/**IOMMU**)가 그걸 **physical**로 바꾼다. CXL은 디바이스를 메모리 시스템의 *정식 멤버*로 만들어 "디바이스 주소 ↔ CPU 주소"의 경계를 흐린다 — 여기서 새로운 변환·일관성 문제가 생긴다.

## 주소 4종 (누가 보느냐로 갈림)
| 주소 | 누가 보나 | 변환 후 |
|---|---|---|
| **logical (= virtual)** | 프로그램 (CPU 요청) | → physical |
| **bus address** | 디바이스 (GPU/CXL/NVMe 요청) | → physical |
| **physical** | 실제 DRAM 위치 | (메모리 최종) |
| **block** | 스토리지(SSD) 블록 단위 | (스토리지 최종) |

## 변환기 — requestor마다 다름
```
CPU     → virtual addr → [MMU]            → physical addr
GPU     → bus addr     → [IOMMU]          → physical addr
CXL dev → bus addr     → [IOMMU/CXL 변환]  → physical addr
NVMe    → bus addr     → [IOMMU]          → physical addr
```
- **MMU** = CPU 경로 변환기 (page table 기반).
- **IOMMU** = 디바이스 경로 변환기. MMU가 프로세스에 해주는 일을 디바이스에 해줌.

## 디바이스마다 주소 공간이 있다 (isolation)
- 프로세스마다 virtual space가 따로이듯, **디바이스마다 bus address space가 따로** 있다.
- 이유: ① 격리(isolation·보안) ② 추상화(편의) ③ 주소 크기 차이 보정.
- **여러 사적 공간 → 변환 → 하나의 physical 공간으로 수렴.**
- 따라서 **같은 bus address여도 어느 디바이스에서 출발했냐에 따라 physical이 다름** (각 디바이스가 자기 변환표를 가짐 = 격리의 핵심).

## ⚠️ "block" 두 가지 — 같은 단어, 다른 세계
| | **cache block (= cache line)** | **storage block** |
|---|---|---|
| 어디 | CPU 캐시 ↔ 메모리 | 메모리 ↔ SSD/디스크 |
| 크기 | ~64 byte | ~4 KB |
| 주소 | physical addr에서 offset 제외한 부분 (= block number = tag+index) | **LBA** (Logical Block Address) |
| 세계 | **메모리 세계** | **스토리지 세계** |

- **cache block**: physical address를 `[block number | offset]`으로 해석. offset = block(line) 안 byte 위치, 나머지 = block number(= 메모리를 64B로 쪼갠 몇 번째 block). block number를 다시 `tag + index`로 나눠 캐시가 찾음. → 캐시는 *새 주소 공간을 쓰는 게 아니라* physical addr를 block 단위로 해석할 뿐.
- **storage block**: "physical → block" 할 때의 그 block. SSD를 4KB 단위로.
- 둘 다 "큰 걸 일정 크기로 쪼갠 단위"라 이름만 같음 — **크기도 세계도 다름**. 계층 위치: cache block(위, CPU 가까이) / storage block(아래, SSD 가까이).

## MMU vs IOMMU
**IOMMU = 디바이스를 위한 MMU** (구조 거의 동일: page table로 변환, 격리 목적).

| | MMU | IOMMU |
|---|---|---|
| 누구 위해 | 프로세스 (CPU) | 디바이스 (GPU/NVMe/CXL) |
| 입력 | virtual | bus (= I/O virtual) |
| 출력 | physical | physical |
| 변환표 | page table | I/O page table (page table 유사) |

> 학부 OS 범위는 보통 **CPU 관점(virtual→physical, MMU)**까지. **IOMMU/bus address는 학부 범위 밖**(대학원 OS·가상화·아키텍처 심화). MMU를 발판 삼아 IOMMU로 한 걸음 넘어가면 됨.

## CXL이 바꾸는 것 — 경계가 흐려진다
전통 디바이스(GPU via PCIe, NVMe) = 자기 bus 공간에 격리된 **"손님"**(IOMMU 거쳐 접근).
**CXL은 cache-coherent** → 디바이스가 메모리 시스템의 **"정식 멤버"**가 됨:
- **CXL Type-3 (메모리 확장기)**: CPU의 physical 주소 공간 *안으로 직접* 들어옴.
- **CXL Type-2 (가속기+메모리, 예: GPU)**: CPU와 *같은 coherent 주소 공간* 공유.

→ "디바이스 주소(bus) ↔ CPU 주소(physical)"의 경계가 흐려짐 = **주소 공간이 합쳐지는 것.**
→ 공간이 합쳐지면 **coherence 유지**가 필요하고, multi-node면 **노드 간 주소 매핑·일관성**이 새 문제가 됨. (Type 1/2/3 정의는 [[CXL Overview]], 노드 간 난관은 [[CXL Multi-node Coherence]])

## ★ CXL에선 physical이 하나가 아니다 — HPA vs DPA
학부에선 physical = 최종 종착지 "하나". **CXL은 physical 공간이 여러 개**다.
- **host → device 메모리 (Type-3)**: `VA → [MMU] → HPA(host physical, device에 매핑된 구역) → CXL 링크 → [디바이스 컨트롤러 HPA→DPA] → DPA(device physical) → device DRAM`. → **bus address를 안 거침.** bus address는 *device→host* 방향 전용.
- **device → host 메모리 (Type-2)**: device가 `bus address → [IOMMU 등] → host physical`.
- 즉 위 "변환기" 표의 `device → bus → physical`은 *device→host* 방향이고, *host→device*는 `VA→HPA→DPA`로 다르다.

**왜 HPA→DPA(physical 간 변환)가 필요한가**: HPA·DPA 둘 다 physical이지만 **서로 다른 시스템의 공간(다른 주인)**. device는 자기가 host 어디에 매핑됐는지 모르고, pooling이면 host마다 다른 HPA로 매핑될 수 있어 경계에서 "번역"이 필요. (변환 *주체*는 [[CXL Distributed Translation]], HPA↔DPA가 *복사본 아님*은 [[CXL Coherence]]의 translation≠caching)

## CXL SSD — memory ↔ storage (byte→block) 융합
전통적으로 메모리(DRAM)는 **byte 단위 load/store**, 스토리지(SSD)는 **block 단위 read/write**로 갈라져 있었다. CXL은 이 경계를 무너뜨린다:
```
CPU → byte 주소로 load/store (메모리인 줄 앎)
       ↓ 《CXL SSD 내부 변환》
       ↓ byte(memory) 주소 → block(storage) 주소
SSD의 실제 block
```
- 새 변환 차원: 전통 AT = virtual→physical(둘 다 메모리), **CXL SSD = memory→storage block**.
- 비효율 지점: SSD는 block(4KB) 단위인데 byte로 찌르면 "4KB 읽어 8byte만 쓰기" 식 낭비 발생.
- 풀 문제: byte→block 매핑, hot block DRAM 캐싱, 일관성.

> 위키 연결: CMM-H(CXL memory device) 위에서 filesystem 정합성을 다루는 [[DJFS]], memory-semantic CXL-SSD인 [[SkyByte]]가 바로 이 "byte↔block / memory↔storage" 경계 위의 사례다.

## 한 줄 요약
> 주소 체계(logical/bus/physical/block) + 변환기(MMU/IOMMU)가 Address Translation의 기초. CXL은 cache-coherent라 "디바이스 공간 ↔ CPU 공간" 경계를 흐리고, CXL SSD는 memory↔storage(byte↔block)를 잇는다 — 이 경계가 흐려지는 길목에서 변환·일관성 문제가 생긴다.

---
**관련**: [[CXL Overview]] · [[CXL Glossary]] · [[CXL 3.0]] · [[DJFS]] · [[SkyByte]]
