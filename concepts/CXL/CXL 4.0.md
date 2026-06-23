---
title: "CXL 4.0 — Bandwidth-First & Multi-Rack"
aliases: [CXL 4.0]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL 4.0 — Bandwidth-First & Multi-Rack

> **2025년 11월 18일 발표 · PCIe 7.0 기반 (128 GT/s)** · 한 줄: "**대역폭 2배 + bundled ports + 장거리(multi-rack)** 로 패브릭을 rack을 넘어 확장." (현재 최신 released spec, 2026-06 기준)

## 왜 나왔나
3.x는 PCIe 6.0(64 GT/s)에 묶여 있었다. AI 클러스터의 폭증하는 통신 수요([[Communication Tax]])에 맞춰 4.0은 **속도를 다시 2배로** 올리고, 여러 rack에 걸친 메모리 풀(scale-out)을 겨냥한다.

## 새로 생긴 것
- **PCIe 7.0 기반, 128 GT/s** ⭐: 3.x(64 GT/s) 대비 **대역폭 2배, 지연은 동일**.
- **Bundled ports**: 여러 물리 포트를 **하나의 논리 연결로 묶음** → x16에서 방향당 768 GB/s, **합계 1.536 TB/s**.
- **Native x2 width**: x2 폭을 기본 지원(유연한 구성).
- **최대 4개 retimer 지원**: 링크 거리를 늘려 **multi-rack** 구성 가능.
- 완전 후방호환.

## 배포 현실 (중요)
- spec ≠ 제품. **CXL 4.0 multi-rack 시스템은 2026년 말~2027년경** 구현 예상.
- PCIe 7.0 자체도 컴플라이언스 테스트가 2027~, 실제 장치는 **2028~2029년** 전망.
- → 지금(2026-06)은 **2.0/3.x 기반 장치가 등장하는 단계**, 4.0은 미래 로드맵. 자세한 시점은 [[CXL SOTA & Roadmap]].

## 새 용어
- **Bundled ports** — 여러 물리 포트를 한 논리 연결로 묶어 대역폭 합산.
- **Native x2 width** — x2 링크 폭 기본 지원.
- **Retimer** — 신호를 재생성해 링크 거리를 늘리는 부품(4.0은 최대 4개).

→ 자세한 정의: [[CXL Glossary]]

## 우리 위키 연결
[[Communication Tax]]의 "bandwidth-first + supercluster(multi-rack)" 방향과 정확히 일치. CXL-over-XLink에서 inter-cluster fabric의 대역폭 천장을 올리는 게 4.0.

## Personal annotations
<본인 메모 영역>
