---
title: "CXL Glossary — 용어 사전"
aliases: [CXL Glossary, CXL 용어, CXL 용어 사전]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL Glossary — 용어 사전

> 처음 보는 CXL 용어를 **친절하게 한 줄**로. (어느 버전에서 등장하는지 표기) 개념 흐름은 [[CXL Overview]].

## 프로토콜 · 기본
- **CXL.io** *(1.1~)* — PCIe 그대로의 비일관 채널. 장치 발견·설정·DMA·인터럽트. 모든 장치 필수.
- **CXL.cache** *(1.1~)* — 장치가 호스트 메모리를 **일관성 있게 캐시**.
- **CXL.mem** *(1.1~)* — 호스트가 장치 메모리를 **load/store**로 직접 접근.
- **Type 1 / 2 / 3 device** *(1.1~)* — 캐시만 / 캐시+자체메모리 / 메모리확장기. → [[CXL Overview#장치-3가지-타입-어떤-프로토콜을-쓰냐로-구분]]
- **HDM (Host-managed Device Memory)** *(1.1~)* — 장치 메모리를 호스트 물리주소에 매핑해 일반 메모리처럼 사용.
- **FLIT** *(1.1~)* — 링크 전송 단위. 68-byte(초기) / **256-byte**(3.0, PCIe 6.0).
- **Bias coherency (Host/Device Bias)** *(1.1~)* — Type 2 메모리를 누가 주로 접근하느냐로 모드 전환해 일관성 비용 절감.

## 풀링 · 패브릭
- **Memory Pooling** *(2.0~)* — 메모리 풀을 조각내 **한 호스트당 한 조각** 할당(재할당 가능). <a id="memory-pooling-vs-sharing"></a>
- **Memory Sharing** *(3.0~)* — **같은 영역을 여러 호스트가 동시에** 일관성 있게 공유.
  > 🔑 **pooling = 돌려쓰기, sharing = 동시에 같이 쓰기.** 이게 2.0↔3.0의 핵심 차이.
- **MLD / SLD** *(2.0~)* — Multi-/Single-Logical Device. Type 3 하나를 최대 16 logical device로 분할(MLD).
- **LD-ID** *(2.0~)* — logical device 식별자.
- **Fabric Manager (FM)** *(2.0~)* — 풀 할당·바인딩·패브릭 구성을 관리.
- **Single-/Multi-level switching** *(2.0 / 3.0)* — switch 1단 / 계단식 다단.
- **HBR (Hierarchy-Based Routing)** *(~2.0)* — PCIe 전통 방식. 호스트를 뿌리로 한 **트리의 계층 위치**로 라우팅. single-host 트리라 노드 수·토폴로지가 제한됨.
- **PBR (Port-Based Routing)** *(3.0~)* — 각 endpoint에 고유 **포트 ID**를 줘서 그 ID로 라우팅. 트리에서 분리되어 mesh·multi-host 토폴로지 + **12-bit ID → 최대 4,096노드** 가능. 개념 설명: [[CXL 3.0#🔎 PBR이란? (Port-Based Routing)]]
- **Back-Invalidation (BI)** *(3.0~)* — 하드웨어가 캐시 사본을 무효화하는 snoop. **sharing 일관성의 핵심.**
- **HDM-DB** *(3.0~)* — BI 기반 device-managed coherence HDM.
- **GFAM (Global Fabric-Attached Memory)** *(3.0~)* — 패브릭에 붙어 다수 호스트가 접근하는 대규모 공유 메모리.
- **MHD (Multi-Headed Device)** *(3.0~)* — 한 장치가 여러 호스트 포트(head)에 연결.
- **P2P (Peer-to-Peer)** *(3.0~)* — 장치 간 직접 통신/메모리 접근(호스트 우회).

## 관리 · tiering (3.2)
- **CHMU (CXL Hot-Page Monitoring Unit)** *(3.2~)* — 메모리 장치에 내장된 hot-page 카운터. **memory tiering**용(뜨거운 페이지 파악).
- **HDM-H** *(3.2~)* — Host-only Coherent Host-managed Device Memory (meta-bits 저장 지원).

## 대역폭 · 거리 (4.0)
- **Bundled ports** *(4.0~)* — 여러 물리 포트를 한 논리 연결로 묶어 대역폭 합산 (x16 = 1.536 TB/s).
- **Native x2 width** *(4.0~)* — x2 링크 폭 기본 지원.
- **Retimer** — 신호를 재생성해 링크 거리를 늘리는 부품 (4.0은 최대 4개 → multi-rack).

## 보안 · 확장 (3.1)
- **IDE (Integrity and Data Encryption)** *(2.0~)* — 링크 레벨 암호화·무결성.
- **TSP (TEE Security Protocol)** *(3.1~)* — CXL 메모리를 TEE 신뢰 경계에 포함, confidential computing 지원.
- **TVM (Trusted VM)** *(3.1~)* — TEE의 신뢰 실행 단위.
- **GIM (Global Integrated Memory)** *(3.1~)* — CXL 패브릭 통한 host-to-host 통신.
- **RAS** *(3.1 강화)* — Reliability, Availability, Serviceability.
- **GPF (Global Persistent Flush)** *(2.0~)* — 전원 손실 시 영속 메모리 일관성 보장.

## 관련
[[CXL Overview]] · [[CXL 1.0-1.1]] · [[CXL 2.0]] · [[CXL 3.0]] · [[CXL 3.1]] · [[CXL 3.2]] · [[CXL 4.0]] · [[CXL SOTA & Roadmap]] · 토픽 허브 [[CXL]]

## Personal annotations
<본인 메모 영역>
