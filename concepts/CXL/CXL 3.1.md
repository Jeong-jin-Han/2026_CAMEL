---
title: "CXL 3.1 — Security & Scale-out"
aliases: [CXL 3.1]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL 3.1 — Security & Scale-out

> **2023년 11월 · PCIe 6.0 기반** · 한 줄: "3.0 패브릭을 **더 크게(scale-out) + 안전하게(TEE) + 호스트끼리 통신(GIM)**."

## 왜 나왔나
3.0이 sharing·PBR로 큰 패브릭의 길을 열었고, 3.1은 이를 **실제 데이터센터 규모로 확장**하고 **보안(confidential computing)** 과 **호스트 간 통신**을 채워 넣은 보강판.

## 새로 생긴 것
- **Fabric 확장 + PBR switch용 Fabric Manager API**: 더 유연한 토폴로지·대규모 scale-out.
- **TSP (TEE Security Protocol)** ⭐: CXL 직결 메모리를 **TEE(신뢰실행환경)의 신뢰 경계 안**에 포함 → confidential computing(암호화된 워크로드)에서도 CXL 메모리 사용. → [[XHarvest]]가 firmware를 host enclave에서 돌리며 CXL traffic을 암호화하는 근거가 이 계열.
- **GIM (Global Integrated Memory)** ⭐: CXL 패브릭으로 **host-to-host 통신** 지원(호스트끼리 공유 메모리로 메시지 교환).
- **Memory Expander 개선**: 최대 34-bit 메타데이터, RAS(신뢰성·가용성·서비스성) 강화.
- **Direct P2P CXL.mem** 등 일관성 P2P 보강.

## 새 용어
- **TSP** — TEE Security Protocol (confidential computing용).
- **GIM** — Global Integrated Memory (host간 통신).
- **TVM** — Trusted VM (TEE 신뢰 경계).
- **RAS** — Reliability, Availability, Serviceability.

→ 자세한 정의: [[CXL Glossary]]

## 우리 위키 연결
[[XHarvest]]의 **CXL + TEE(enclave) 보안 harvesting**은 3.1 TSP 사고와 직접 맞닿는다. [[Communication Tax]]의 supercluster scale-out도 3.1 fabric 위에서 그려진다.

## Personal annotations
<본인 메모 영역>
