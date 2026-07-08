# Smart-Infinity — Figure별 Speedup 수치

> `%빨라짐 = (speedup − 1) × 100`. 모두 **baseline(ZeRO-Infinity + RAID0) 대비**.
> 수치는 논문 본문(§)에서 인용. 표 내부 값이라 본문에 없는 건 "그래프/표 판독" 표기.

---

## 핵심 헤드라인
- **최대 2.11× (= +111% 빨라짐)** — A100 GPU + 10 CSD (§I, §VII-E, Fig 11b)

---

## Figure별 정리

| Figure | 모델 | 조건 | Speedup | %빨라짐 | 근거 |
|---|---|---|---|---|---|
| **Fig 3(b)** | GPT류 | **baseline RAID0** SSD 개수↑ | 4개 초과서 **포화**(≈2×대, 그래프 판독) | — | §III, line 414 |
| **Fig 9** (SU) | **GPT-2+BERT** | SmartUpdate, **6 SSD** | 1.18×~1.24× | **+18~+24%** | §VII-C, line 1729 |
| **Fig 9** (SU) | **GPT-2+BERT** | SmartUpdate, **10 SSD** | 1.54×~1.60× | **+54~+60%** | line 1730 |
| **Fig 9** (SU+O) | **GPT-2+BERT** | +Overlap handler, **10 SSD** | ~1.60×~1.66× | **+60~+66%** | line 1732 |
| **Fig 9** (SU+O+C) | **GPT-2+BERT** | +SmartComp, **10 SSD** | **1.85×~1.98×** | **+85~+98%** | line 1737 |
| ↳ SmartComp 단독 기여 | **GPT-2+BERT** | SU 대비 **추가** 이득 | +1.22×~1.31× | (SU 대비 +22~+31%p) | line 1736 |
| **Fig 10** | GPT-2 **33.0B** | **6 SSD** | 1.37× | **+37%** | §VII-D, line 1785 |
| **Fig 10** | GPT-2 **33.0B** | **10 SSD** | 1.88× | **+88%** | line 1786 |
| **Fig 10** | GPT-2 16.6~33B | 전반 | **일정 유지(consistent)** | — | line 1784 |
| **Fig 11(a)** | GPT-2 4.0B | CSD 1→10, A5000·A100 | **거의 선형↑** (baseline은 4개서 포화) | — | §VII-E, line 2044 |
| **Fig 11(b)** | GPT-2 4.0B | **10 SSD, A100** | **2.11×** | **+111%** | line 2055 |
| **Fig 12** | GPT-2 4.0B | **다른 optimizer** (SGD·AdaGrad) | SGD·AdaGrad는 상태 3/4라 **Adam보다 약간 낮음** | — | §VII-F, line 2062 |
| **Fig 13** | **BLOOM·ViT** | 다른 모델 | 1.32×~1.85× | **+32~+85%** | §VII-G, line 2081 |
| **Fig 17(b)** | GPT-2 1.16B | **multi-GPU(1~3), 10 CSD** | 1.66×~1.86× | **+66~+86%** | §VIII-A, line 2555 |
| **Table IV** | BERT·GPT-2 | fine-tuning, 압축비 10/5/2/1% | 압축↑ → speedup **점증**(gradual↑) | (표 판독) | §VII-J·K, line 2526 |

---

## Speedup 없는 그림/표 (다른 지표)
- **Fig 1·2·4·5·6·7·8**: 개념도·구조도 (speedup 아님)
- **Fig 14**: 모듈 throughput (updater >7 GB/s) — 병목 아님 논증
- **Fig 15**: GFLOPS/달러 (비용효율) — CSD 4개↑부터 baseline 역전
- **Fig 16**: 압축비별 학습시간 (Table IV와 짝)
- **Table I**: 트래픽 (6+2)M→2M→c%×2M / **Table II**: 실험환경 / **Table III**: FPGA 자원

---

## 발표용 한 줄 요약
> **SmartUpdate만 최대 +60%(10 SSD) → +Overlap +66% → +SmartComp +85~98% → A100선 최대 +111%(2.11×).**
> 대형 모델(33B)서도 +88%(10 SSD) 유지, 다른 모델(BLOOM·ViT) +32~85%, multi-GPU +66~86%.
