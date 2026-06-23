---
title: "CXL 3.2 — Device Management & Tiering"
aliases: [CXL 3.2]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL 3.2 — Device Management & Tiering

> **2024년 12월 3일 발표 · PCIe 6.0 유지(속도 향상 없음)** · 한 줄: "링크 속도가 아니라 **메모리 장치의 모니터링·관리·보안**을 다듬은 버전."

## 왜 나왔나
3.0/3.1이 패브릭·공유·보안의 큰 골격을 세웠다면, 3.2는 **실사용에서 필요한 device-level 기능**(메모리 tiering용 모니터링, OS/앱 연동, 보안 검증)을 채운 보강판. **완전 후방호환**, 링크 속도는 그대로(PCIe 6.0).

## 새로 생긴 것
- **CHMU (CXL Hot-Page Monitoring Unit)** ⭐: 메모리 장치 안에 **hot-page 카운터를 하드웨어로 내장** → OS가 자주 쓰는 페이지를 싸게 파악해 **memory tiering**(뜨거운 데이터는 가까운 계층으로) 수행.
- **Meta-bits Storage Feature (HDM-H용)**: Host-only Coherent HDM(`HDM-H`) 영역에 메타데이터 비트 저장 지원.
- **HDM-DB 보안 강화**: Back-Invalidation 기반 device-managed 메모리(`HDM-DB`)의 보안 향상.
- **TSP 확장 + device 모니터링/관리 개선**: telemetry, self-repair, 세분화된 error reporting, 보안 프로토콜 검증.

## 새 용어
- **CHMU** — CXL Hot-Page Monitoring Unit (tiering용 하드웨어 hot-page 카운터).
- **HDM-H** — Host-only Coherent Host-managed Device Memory.
- **HDM-DB** — Device-managed(Back-Invalidation) HDM. (3.0에서 등장, 3.2에서 보안 강화)

→ 자세한 정의: [[CXL Glossary]]

## 우리 위키 연결
**memory tiering** = 뜨거운/차가운 데이터를 계층 메모리에 배치 → [[CXL Overview]]의 Tier-1/Tier-2 사고, 그리고 tiered memory 논문(예: ArtMem)과 직접 연결. CHMU는 "어디가 뜨거운지"를 하드웨어로 알려주는 장치.

## Personal annotations
<본인 메모 영역>
