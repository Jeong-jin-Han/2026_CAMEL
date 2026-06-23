---
title: "CXL 1.0 / 1.1 — Coherent Attach (출발점)"
aliases: [CXL 1.0, CXL 1.1, CXL 1.0-1.1]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL 1.0 / 1.1 — Coherent Attach (출발점)

> **2019 · PCIe 5.0 기반** · 한 줄: "PCIe 선 위에서 **캐시 일관성**을 처음 얹어, 가속기·메모리를 CPU에 직접 붙인다."

## 왜 나왔나
PCIe는 빠르지만 **비일관(non-coherent)** — 장치와 CPU가 서로의 캐시를 모른다. 가속기가 CPU 메모리를 쓰려면 비싼 복사·동기화가 필요했다. CXL 1.1은 PCIe 물리계층을 그대로 쓰되 **일관성 있는 메모리 접근**을 추가했다.

## 새로 생긴 것 (모든 CXL의 기반)
- **3개 프로토콜**: `CXL.io`(PCIe 그대로) + `CXL.cache`(장치→호스트 메모리 캐시) + `CXL.mem`(호스트→장치 메모리 접근). → [[CXL Overview#cxl의-3가지-프로토콜-항상-등장]]
- **3개 장치 타입**: Type 1/2/3.
- **HDM (Host-managed Device Memory)**: 장치 메모리를 호스트 물리주소 공간에 매핑해 CPU가 일반 메모리처럼 load/store.
- **Bias 기반 coherency**: Type 2(가속기+메모리)에서 `Host Bias`/`Device Bias`로 일관성 비용 최소화.

## 한계 (다음 버전이 푸는 것)
- **single host, 직접 연결만** — switch 없음, 여러 호스트가 메모리 공유 불가.
- **hot-plug 불가** — 부팅 후 CXL 자원 추가 불가. → [[CXL 2.0]]에서 해결.

## 새 용어
- **HDM** — Host-managed Device Memory.
- **Host Bias / Device Bias** — Type 2 메모리의 일관성 접근 모드.
- **FLIT** — 링크 전송 단위(이 시기 68-byte).

→ 자세한 정의: [[CXL Glossary]]

## Personal annotations
<본인 메모 영역>
