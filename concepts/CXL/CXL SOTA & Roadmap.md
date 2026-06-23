---
title: "CXL — SOTA & Roadmap (최신 동향)"
aliases: [CXL SOTA, CXL SOTA & Roadmap, CXL Roadmap]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
  - meta/sota
---

# CXL — SOTA & Roadmap (최신 동향)

> [!info] 정보 시점: **2026-06-23 기준**
> 이 노트는 *시간이 지나면 바뀐다.* 새 spec/제품 소식이 나오면 표와 날짜를 갱신할 것. 각 항목은 출처 확인된 사실만.

## 현재 최신 released spec = **CXL 4.0** (2025-11-18)
스펙은 매년 나오지만 **실제 제품은 몇 년 뒤**라는 점이 핵심. 지금 시장은 **CXL 2.0 / 3.x 장치가 등장하는 단계**이고, 4.0은 로드맵.

## 전체 spec 타임라인 (한 눈에)
| 버전 | 발표 | PCIe / 속도 | 한 줄 핵심 | 상세 |
|---|---|---|---|---|
| 1.0 / 1.1 | 2019 | PCIe 5.0 / 32 GT/s | coherent attach (3 프로토콜) | [[CXL 1.0-1.1]] |
| 2.0 | 2020 | PCIe 5.0 / 32 GT/s | switch + **pooling** + hot-plug | [[CXL 2.0]] |
| 3.0 | 2022 | **PCIe 6.0 / 64 GT/s** | 다단 switch + **sharing** + **PBR(4096)** | [[CXL 3.0]] |
| 3.1 | 2023 | PCIe 6.0 / 64 GT/s | fabric scale-out + **TSP** + **GIM** | [[CXL 3.1]] |
| 3.2 | 2024-12 | PCIe 6.0 / 64 GT/s | device 관리·**tiering(CHMU)**·보안 | [[CXL 3.2]] |
| **4.0** | **2025-11** | **PCIe 7.0 / 128 GT/s** | **대역폭 2배 + bundled ports + multi-rack** | [[CXL 4.0]] |

> 패턴: **속도 도약은 3.0(PCIe6)·4.0(PCIe7)**, 그 사이(3.1/3.2)는 **기능·보안·관리 보강**.

## 배포 현실 (spec ≠ 제품)
- **CXL 4.0 multi-rack 시스템**: 2026년 말 ~ 2027년경 예상.
- **PCIe 7.0**: 스펙 2025 확정(128 GT/s, x16 양방향 512 GB/s). 컴플라이언스 테스트 2027~, 실제 장치 **2028~2029** 전망.
- **PCIe 8.0**: 이미 "exploration" 단계 시작(목표 ~1 TB/s).

## 인접 기술 / 경쟁 (interconnect wars)
AI 인터커넥트는 CXL 단독이 아니라 여러 표준이 경쟁·공존한다 — [[Communication Tax]]의 **CXL-over-XLink** framing과 직접 연결:
- **UALink**: 가속기 scale-up용(비-coherent, Ethernet 계열). intra-cluster.
- **NVLink / NVLink Fusion**: NVIDIA 가속기 인터커넥트. intra-cluster.
- **역할 분담 관점**: XLink(클러스터 내) + CXL(클러스터 간 coherent fabric) = supercluster.

## 우리 위키 연결
- 상위 framing: [[Communication Tax]] (왜 대역폭·메모리가 병목인가)
- CXL 발표 논문: [[SkyByte]] · [[XHarvest]]
- 개념: [[CXL Overview]] · [[CXL Glossary]]

## 출처 (확인일 2026-06-23)
- CXL 3.2 발표 (CXL Consortium, 2024-12-03): https://computeexpresslink.org/
- CXL 4.0 발표 (CXL Consortium, 2025-11-18): https://computeexpresslink.org/
- PCIe 7.0 스펙 (PCI-SIG, 2025): 128 GT/s, x16 양방향 512 GB/s

## Personal annotations
<본인 메모 영역>
