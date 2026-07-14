---
title: "SkyByte refs"
aliases: [SkyByte refs, SkyByte references]
type: refs-index
ref-of: "SkyByte"
tags:
  - meta/refs-index
---

# SkyByte refs — 참고문헌 정독 index

> [[SkyByte]] 정독에 필요해 함께 읽은 reference들. 규약: [paper_wiki.md §6-C](../../../workflow/paper_wiki.md).

## 정독한 refs (8편)

- [[FlatFlash - Exploiting the Byte-Accessibility of SSDs within a Unified Memory-Storage Hierarchy|FlatFlash]] — ASPLOS'19 · SSD의 byte-accessibility를 unified memory-storage 계층으로 노출 · **memory-semantic SSD 계보의 뿌리** — SkyByte가 물려받은 출발점
- [[DirectCXL - Direct Access, High-Performance Memory Disaggregation|DirectCXL]] — ATC'22 · CXL 메모리 disaggregation 실물 원조 (정명수 랩) · SkyByte가 올라탄 **CXL 무대의 기점**
- [[Hello Bytes, Bye Blocks - PCIe Storage Meets Compute Express Link for Memory Expansion (CXL-SSD)|Hello Bytes, Bye Blocks]] — HotStorage'22 · block→byte, **CXL-SSD 개념 제안** · SkyByte가 실현하려는 비전의 선언문
- [[Overcoming the Memory Wall with CXL-Enabled SSDs|Overcoming the Memory Wall]] — ATC'23 · CXL-SSD로 메모리 확장의 실측·설계 고려사항 · SkyByte 직전의 feasibility 근거
- [[TPP - Transparent Page Placement for CXL-Enabled Tiered-Memory|TPP]] — ASPLOS'23 · OS transparent page placement (CXL tiered memory) · SkyByte **OS-측 정책의 비교 배경**
- [[ByteFS - System Support for (CXL-based) Memory-Semantic Solid-State Drives|ByteFS]] — ASPLOS'25 · memory-semantic SSD를 위한 FS/시스템 지원 · SkyByte와 같은 장치 가정의 **SW 스택 이웃**
- [[AstriFlash - A Flash-Based System for Online Services|AstriFlash]] — HPCA'23 · flash를 online service의 메모리처럼 쓰는 HW 시스템 · flash-as-memory의 **성능 요구 기준점**
- [[Informing Memory Operations - Providing Memory Performance Feedback in Modern Processors|Informing Memory Operations]] — ISCA'96 · 메모리 이벤트를 SW에 피드백하는 HW 인터페이스 · SkyByte **context-switch 힌트 메커니즘의 고전 계보**

## ⚠️ 중복 주의 (규약 §6-C-④)
- **DirectCXL**·**Hello Bytes, Bye Blocks**는 `concepts/CXL/papers/`에도 별도 노트가 존재 (내용 상이 — concepts본은 CXL 계보용, refs본은 SkyByte 정독용). `[[DirectCXL]]` 같은 짧은 링크는 모호할 수 있으니 **전체 제목 wikilink** 사용. 통합 여부는 미결.

## 🔗 Connections
부모: [[SkyByte]] · 허브: [[CXL]]
