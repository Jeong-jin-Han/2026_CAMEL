---
title: "CXL Distributed Translation — IOMMU와 control/data plane"
aliases: [CXL Distributed Translation, IOMMU, control plane, data plane, HPA DPA, 분산 변환, VT-d, SMMU]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL Distributed Translation — 변환이 어디서 일어나나

> [!abstract] 이 노트
> 주소 변환을 **누가·어디서** 하는가. 전통 IOMMU(CPU 안 중앙집중)에서 CXL이 그걸 **CPU 밖(디바이스·스위치)으로 분산**하는 그림. 주소 4종·MMU/IOMMU 기초는 [[CXL Address Translation]], coherence는 [[CXL Coherence]].

## 한 문장
변환에는 **설정(규칙 만들기)**과 **실행(매 접근 변환)**이 있고, 전통적으로 실행은 CPU 안 **IOMMU**가 독점했다. CXL은 이 실행을 **디바이스 컨트롤러·스위치·호스트**로 분산한다.

## 설정 vs 실행 — 다른 주체
| 단계 | 하는 일 | 주체 |
|---|---|---|
| **① 설정 (control)** | 변환 규칙(매핑표) 만들기·등록 — "bus 0x1000 = phys 0x5000" | **SW** (OS 커널 + 디바이스 드라이버) |
| **② 실행 (data)** | 매 접근마다 실제 변환 | **HW** (IOMMU) |

- SW가 매 접근에 끼면 너무 느림(디바이스는 초당 수십억 번 접근) → **실제 변환은 HW가 자동**.
- 예: NVIDIA driver는 "무엇을 매핑할지" 설정·DMA에 관여, IOMMU 셋업은 OS 커널, **실시간 변환은 IOMMU HW**.

## IOMMU는 어디 있나
```
[디바이스] → PCIe → [CPU 안 PCIe 컨트롤러] → [IOMMU] → [메모리 컨트롤러] → [DRAM]
                                              ↑ 디바이스→메모리 길목, 여기
```
- 현대엔 **CPU 패키지 안에 통합**. (과거 노스브리지 시절엔 메모리 컨트롤러와 함께 칩셋에 있었음 → CPU가 흡수)
- 브랜드: Intel **VT-d** / AMD **AMD-Vi** / ARM **SMMU**.

## 왜 디바이스가 CPU(IOMMU)를 거쳤나 — 직접 연결하면 잃는 3가지
| 잃는 것 | 직접 연결하면 | CPU(IOMMU) 거치면 |
|---|---|---|
| **① 보안/격리** | 디바이스가 메모리 아무 데나 접근 (고장·해킹 시 커널·타 프로세스 노출) | "넌 여기만" 강제 |
| **② 일관성(coherence)** | DRAM을 고쳐도 CPU 캐시엔 옛값 → 데이터 깨짐 | 캐시 무효화 조율 |
| **③ 주소 변환** | 디바이스가 physical을 직접 알아야 → ① 또 깨짐 | bus→physical 변환 |

→ "CPU 거치기"는 멍청해서가 아니라 **보안·일관성·변환을 얻으려는 의도적 비용**. AI 시대엔 GPU가 메모리를 끊임없이 써서 이 길목이 진짜 병목이 됨 → 업계가 직접 연결로(폐쇄 NVLink / 개방 **CXL**).

## CXL의 답 — 직접 연결하되 3가지를 CPU 밖으로 분산
> "직접 연결한다. 단 보안·일관성·변환을 버리지 않고, 그 로직을 CPU 밖(디바이스/스위치)으로 옮겨 분산한다."

**변환이 일어나는 곳 (분산):**
| 주체 | 하는 일 |
|---|---|
| **① CXL 디바이스 컨트롤러** (디바이스 안) | 호스트 주소 → 디바이스 내부 위치. **HPA(Host Physical Addr) → DPA(Device Physical Addr)** 매핑 |
| **② CXL 스위치 / fabric** (디바이스 밖) | "이 주소는 어느 디바이스·노드로?" 라우팅 (예: port-based routing) |
| **③ 호스트** (CPU 안, 여전히) | CXL 메모리를 CPU physical 주소 공간에 매핑·관리 |

- **CXL controller = 변환의 한 조각(HPA→DPA)** 담당. IOMMU 전체 대체가 아님.
- 비유: IOMMU = 공항 하나의 **중앙 관제탑**, CXL = 여러 공항이 각자 관제탑 + 항로 라우팅하는 **분산 항공망**, CXL controller = 그중 한 공항 관제탑.

## control plane / data plane 분리 (핵심 원리)
| | 누가 | 빈도 |
|---|---|---|
| **Control plane (관리·설정)** | **CPU** | 가끔 (매핑 규칙·권한 설정) |
| **Data plane (실제 데이터 이동)** | **디바이스 ↔ 메모리 직접** | 매 순간 (실시간 변환·일관성) |

- 용어 정밀화: "CPU 없이"가 아니라 **"CPU를 데이터 경로에서 거치지 않고"**. 데이터가 CPU를 물리적으로 통과하지 않되, **CPU가 깔아둔 규칙(매핑·권한) 위에서** 직접 통신이 일어남. = 병목이던 CPU를 빠른 경로에서 뺀 것.

## single-node vs multi-node — 스위치는 언제 등장하나
```
single-node (직결):   [host] ──CXL──→ [디바이스 컨트롤러] → device 메모리   (스위치 없음)

multi-node / pooling: [host A]┐                ┌[device 1]
                      [host B]┼→ [CXL 스위치] →┼[device 2]
                      [host C]┘  (교통정리)     └[device 3]
```
- **single-node** = 디바이스 컨트롤러(HPA→DPA) + 호스트 = **2개, 스위치 없음, 단순**.
- **multi-node** = + 스위치/fabric 라우팅 = **3개**. 노드 간 주소 매핑이 새 층으로 등장 → 난이도 점프.

## 한 줄 요약
> 변환 = 설정(SW) + 실행(HW IOMMU). 전통은 CPU 안 IOMMU 독점. CXL은 직접 연결의 대가(보안·일관성·변환)를 CPU 밖으로 분산 — 디바이스 컨트롤러(HPA→DPA) + 스위치(라우팅) + 호스트. 결과 = control plane(CPU 설정) / data plane(직접 이동) 분리. 스위치는 multi-node에서만 등장.

---
**관련**: [[CXL Address Translation]] · [[CXL Coherence]] · [[CXL Overview]] · [[CXL 3.0]] · [[DJFS]]
