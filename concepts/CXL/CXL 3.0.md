---
title: "CXL 3.0 — Fabric, Sharing & PBR"
aliases: [CXL 3.0]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL 3.0 — Fabric, Sharing & PBR

> **2022 · PCIe 6.0 (64 GT/s) 기반** · 한 줄: "대역폭 2배 + **다단 switch 패브릭**으로 수천 노드가 메모리를 **진짜 공유**한다."

## 왜 나왔나
2.0은 switch 1단·풀 *분배*까지. 3.0은 (1) **대역폭 2배**(PCIe 6.0, 256B FLIT), (2) **여러 단계 switch**로 큰 패브릭, (3) 여러 호스트가 **같은 메모리를 동시에 일관성 있게 공유**하는 단계로 도약. → [[Communication Tax]]가 말하는 "true composability"의 분기점.

## 새로 생긴 것
- **PCIe 6.0 기반 (64 GT/s, 256-byte FLIT)**: 2.0 대비 대역폭 2배.
- **Multi-level switching**: switch를 **계단식(cascading)** 으로 쌓아 leaf/spine(fat-tree) 패브릭 구성.
- **Memory Sharing** ⭐: **같은 메모리 영역을 여러 호스트가 동시에** 하드웨어 일관성으로 공유. (`HDM-DB`)
- **Back-Invalidation (BI)**: 하드웨어가 캐시 사본을 무효화하는 snoop → sharing의 일관성을 보장하는 핵심 메커니즘.
- **PBR (Port-Based Routing)** ⭐: 포트 기반 라우팅으로 **최대 4,096 노드** 규모 패브릭. (기존 계층기반 HBR의 한계 돌파.)
- **GFAM (Global Fabric-Attached Memory)**: 패브릭에 붙어 다수 호스트/장치가 접근하는 대규모 공유 메모리.
- **P2P (Peer-to-Peer)**: 장치 간 직접 통신/메모리 접근(호스트 우회).
- **MHD (Multi-Headed Device)**: 한 장치가 여러 호스트 포트(head)에 직접 연결.

> [!important] 2.0 → 3.0 핵심 차이
> **pooling(2.0, 돌려쓰기) → sharing(3.0, 동시 공유)**. 이걸 가능케 한 게 **Back-Invalidation**. 규모를 키운 게 **PBR**.

## 새 용어
- **PBR** — Port-Based Routing (vs HBR Hierarchy-Based Routing).
- **Back-Invalidation (BI)** — 공유 일관성용 캐시 무효화 snoop.
- **HDM-DB** — Device-managed coherence(BI 기반) HDM.
- **GFAM / G-FAM device** — Global Fabric-Attached Memory.
- **MHD** — Multi-Headed Device.
- **256B FLIT** — PCIe 6.0의 큰 전송 단위.

→ 자세한 정의: [[CXL Glossary]]

## 우리 위키 연결
[[Communication Tax]] §4.2 Table 1의 "CXL 3.0 = sharing + PBR + 4096 nodes"가 이 버전. CXL-over-XLink supercluster에서 **inter-cluster coherent fabric** 역할을 맡는 게 CXL 3.0+.

## Personal annotations
<본인 메모 영역>
