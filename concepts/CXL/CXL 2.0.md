---
title: "CXL 2.0 — Switching & Memory Pooling"
aliases: [CXL 2.0]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL 2.0 — Switching & Memory Pooling

> **2020 · PCIe 5.0 기반** · 한 줄: "**switch**가 생겨 메모리를 **풀(pool)** 로 만들고, 호스트들이 나눠 쓴다."

## 왜 나왔나
1.1은 장치를 호스트에 1:1 직접 붙이는 것만 가능 → 메모리가 특정 호스트에 묶여 낭비. 2.0은 **switch**를 도입해 메모리를 모아두고 **필요한 호스트에 동적으로 할당**한다(stranded memory 문제 해결).

## 새로 생긴 것
- **Single-level switching**: CXL switch 1단으로 여러 호스트 포트(최대 16) ↔ 여러 장치를 연결.
- **Memory Pooling** ⭐: 메모리 풀을 조각내 **각 조각을 한 호스트에 할당**(나중에 재할당 가능). *동시 공유는 아직 아님* — 그건 [[CXL 3.0]]의 sharing.
- **MLD (Multi-Logical Device)**: Type 3 장치 하나를 최대 16개 **logical device**로 쪼개 여러 호스트에 분배. (단일은 `SLD`.)
- **Fabric Manager (FM)**: 풀의 할당·바인딩을 관리하는 주체(소프트웨어/하드웨어).
- **Hot-plug**: 부팅 후 CXL 자원 추가/제거 가능.
- **Persistent Memory 지원 + Global Persistent Flush(GPF)**.
- **IDE (Integrity and Data Encryption)**: 링크 레벨 암호화·무결성.

> [!tip] pooling = "돌려쓰기", sharing ≠ 아직
> 2.0의 pooling은 *한 번에 한 호스트*가 한 조각을 소유한다. **같은 영역을 여러 호스트가 동시에** 일관성 있게 쓰는 것(sharing)은 [[CXL 3.0]]부터. → [[CXL Glossary#memory-pooling-vs-sharing]]

## 새 용어
- **MLD / SLD** — Multi-/Single-Logical Device.
- **LD-ID** — logical device 식별자.
- **Fabric Manager (FM)** — 풀 할당 관리자.
- **IDE** — Integrity and Data Encryption.
- **Memory Pooling** — 메모리 풀을 호스트들에 분배.

→ 자세한 정의: [[CXL Glossary]]

## 우리 위키 연결
[[Communication Tax]] §4에서 말하는 "CXL 2.0 = memory pooling"이 바로 이 버전. [[XHarvest]]의 자원 harvesting도 pooling 사고의 연장.

## Personal annotations
<본인 메모 영역>
