---
title: "Hitchhike: Efficient Request Submission via Deferred Enforcement of Address Contiguity"
description: "I/O 스택의 주소 연속성(address contiguity) 검증을 드라이버 단계까지 지연시켜, 비연속 요청들을 하나의 제출 단위로 병합함으로써 커널 오버헤드를 줄이는 요청 제출 로직"
venue: ASPLOS
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/fs
  - venue/asplos
  - year/2026
  - list/26s-v2
  - topic/io-submission
  - topic/io-uring
  - topic/kernel-overhead
  - topic/nvme
---

# Hitchhike: Efficient Request Submission via Deferred Enforcement of Address Contiguity

> **ASPLOS 2026** · cluster/fs · Source: [Hitchhike - Efficient Request Submission via Deferred Enforcement of Address Contiguity.pdf](<Hitchhike - Efficient Request Submission via Deferred Enforcement of Address Contiguity.pdf>)

저자: Xuda Zheng (Huazhong University of Science and Technology), Jian Zhou† (corresponding author, HUST), Shuhan Bai (HUST), Runjin Wu (HUST), Xianlin Tang (HUST), Zhiyuan Li (HUST), Hong Jiang (University of Texas at Arlington), Fei Wu (HUST)

## TL;DR
현대 스토리지 스택은 각 I/O 요청이 file offset·sector·LBA 전 구간에서 연속된 주소 범위를 가지도록(strict address contiguity validation) end-to-end로 강제하는데, 이는 NVMe 프로토콜상 디바이스 경계에서만 필요한 제약을 상위 계층(syscall, file system, block layer)까지 불필요하게 확장한 것이다. Hitchhike는 이 검증을 드라이버 단계로 지연시켜, 하나의 제출 단위("Hitchhike I/O")가 오프셋 벡터와 버퍼 벡터 형태로 여러 비연속 주소 범위를 캡슐화하도록 한다. 상위 계층 연산(request check, bio 준비, bio 제출)은 그룹 전체에 대해 한 번만 수행되고, 오프셋 변환·버퍼 피닝만 벡터 원소별로 반복되며, 최종적으로 드라이버에서 개별 NVMe 커맨드로 재구성되어 프로토콜 준수를 유지한다. FIO, 그래프 엔진 Blaze, B-tree KV 스토어 LeanStore에 통합해 커널 우회(SPDK)나 aggressive polling 없이도 CPU 코어 요구량을 최대 75% 줄이고 처리량을 크게 개선한다.

## 문제 & 동기
현대 스토리지 시스템은 그래프 처리·B-tree/LSM 기반 KV 스토어 등에서 단일 CPU 코어가 수십~수백 개의 동시 outstanding I/O(동시 in-flight 요청)를 관리하는 것이 일상화되었다(p.947-948, Fig.2). 그런데 Linux I/O 스택은 물리 주소가 연속(contiguous)한 요청만 병합(merge)해 오버헤드를 줄이도록 최적화되어 있어, 논리적으로는 가까운 데이터를 가리키는 요청이라도 주소가 비연속이면 file system·block layer·driver를 각각 독립적으로 완주해야 한다(p.947). 이 문제는 원래 슬로우 디바이스를 위해 설계된 커널 I/O 스택이, 스토리지 대역폭이 PCIe 5.0에서 200만 IOPS를 넘어서는 상황(및 CPU 성능 향상 정체, read-as-needed 패러다임 확산)을 못 따라가면서 심화된다(p.948).

> [!quote]- 📄 원문 표현 (paper)
> - "We argue that the primary cause of this inefficiency lies in the end-to-end enforcement of the strict address contiguity validation—the constraint that each I/O request must access a contiguous range of addresses (e.g., file offsets, sectors, and logical block addresses)." (p.946)
> - "This inefficiency is evidenced by the significant 2.9× performance gap we analyze in §2.2." (p.947)
> - "Achieving the maximum throughput under 4 KB random I/O requires significantly more CPU resources, often occupying 2–4 cores... the kernel I/O stack alone consumes over 80% of total CPU cycles, primarily due to the overhead of managing a large number of small, independent I/O requests." (p.948)
> - "under high outstanding I/O, delays in the software stack—especially above the block layer—hinder timely request submission, preventing full exploitation of SSD bandwidth." (p.949)

## 핵심 통찰 (Key Insight)
1. **주소 연속성은 디바이스 경계에서만 필요하다.** NVMe 프로토콜은 하나의 커맨드가 연속된 LBA 범위를 대상으로 할 것을 요구하지만, 이는 소프트웨어 스택 전체가 처음부터 끝까지 연속성을 유지해야 함을 뜻하지 않는다. 상위 계층(syscall, file system, block layer)은 그룹화된 비연속 주소를 다뤄도 안전하며, 연속성은 드라이버가 NVMe 커맨드로 변환하는 마지막 순간에만 복원하면 된다(p.950, §3.1). 이 통찰이 강제 조건을 언제(where) 적용할지에 대한 재배치를 가능하게 하여, 기존에는 병합 불가능했던 비연속 outstanding I/O들을 병합 가능하게 만든다.
2. **메타데이터 벡터화(Metadata Vectorization).** readv의 iov_iter가 메모리 버퍼 다중화를 다루는 것처럼, Hitchhike는 주소(오프셋) 자체를 벡터화한다. 여러 요청의 (offset, buffer) 쌍을 하나의 Hitchhike I/O로 묶어 커널이 단일 제출 단위로 처리하게 함으로써, 요청 검사(request check)·bio 준비·bio 제출 같은 요청 단위(per-request) 연산을 그룹당 1회로 amortize한다(p.947, 951, Fig.5).
3. **지연된 메타데이터 바인딩(Deferred Metadata Binding).** 벡터화된 상태로 커널을 통과한 요청은 드라이버에서 각 오프셋 세그먼트마다 독립적인 tag(command_id)를 할당받아 개별 NVMe 커맨드로 분할·제출된다. 완료 시에도 개별 커맨드는 비공유 자원(tag)만 즉시 해제하고, 그룹 전체가 끝나야 통합 콜백을 트리거해 콜백 오버헤드도 줄인다(p.951, §4.1.3).

> [!quote]- 📄 원문 표현 (paper)
> - "this insight reveals a new design opportunity—by deferring the address contiguity validation to the driver level, we can lift a long-standing constraint on request merging." (p.947)
> - "Hitchhike decouples the strict address contiguity validation from the early stages of the I/O stack, allowing a single request, termed a Hitchhike I/O, to encapsulate multiple non-contiguous addresses." (p.947)
> - "When a device interrupt is triggered upon the completion of any hitchhiker request, the kernel does not immediately perform full callbacks for each individual command. Instead, it first releases only the non-shared resources (e.g., tag)... Once all NVMe commands belonging to a single Hitchhike request have completed, the system triggers a unified callback." (p.951)

## 설계 / 메커니즘 (Design)
- **제출 경로 5단계 모델 (Fig.5, p.950)**: request check(A) → offset translation(B) → bio preparation(C) → buffer pinning(D) → bio submission(E). Strict 모델에서는 요청마다 A~E 전부 반복되지만, Deferred 모델(Hitchhike)에서는 B·D(오프셋 관련 연산)만 벡터 원소별로 반복되고 A·C·E는 그룹 전체에 대해 한 번만 실행된다.
- **Vectored I/O와의 차이 (Fig.6, p.951)**: 표준 I/O는 버퍼·주소 모두 연속을 요구, readv 등 Vectored I/O는 메모리 버퍼만 다중화(주소는 여전히 단일 (offset, length)), Hitchhike는 버퍼와 주소(오프셋 벡터) 둘 다 비연속을 허용한다.
- **3단계 파이프라인 (Fig.7, p.951, §4.1)**: ① Metadata Vectorization — 애플리케이션의 request queue에서 Hitchhike I/O(H)를 구성; ② Vectorized Metadata Handling — syscall/file system/block layer가 주소 관련 연산만 벡터 순회로 처리; ③ Deferred Metadata Binding — NVMe driver가 각 세그먼트를 독립 NVMe 커맨드로 재구성.
- **범위(Scope) 제한 — File-Centric Merging**: 같은 file descriptor(fd)를 대상으로 하는 요청만 하나의 Hitchhike I/O로 병합한다. 서로 다른 fd/마운트포인트/네임스페이스를 넘나드는 병합은 커널 구현 복잡도 때문에 배제했다(p.951, §3.3). 워커셋마다 fd 단위로 그룹핑되며, 최악의 경우(요청마다 fd가 전부 다른 경우) 표준 I/O 동작으로 gracefully 저하된다.
- **성능 모델 (Eq.1, Amdahl's Law, p.952)**: $\alpha = \dfrac{1}{(1-K) + (1-\beta)K + \frac{(\beta K)}{n}}$ (단, $n \ge 1$, $\beta, K \in (0,1)$). $K$=전체 제출 시간 중 커널 비중, $\beta$=커널 처리 중 병합으로 절감 가능한 비율, $n$=병합 개수. 4KB random read 사례 분석에서 offset 관련 연산 23.8%, buffer 관련 연산 3.45%가 non-reducible이라 $\beta$=0.7275; $K$=0.8, $n$=64 대입 시 이론적 speedup 2.34×를 예측했고, 실측 2.29×로 근접(p.952, §4.2).
- **구현 (§5, p.953)**: libaio에는 iocb의 64바이트 제한을 우회하기 위해 offset 배열과 병합 개수를 담는 `struct hitchhiker`를 도입하고, 병합 한도 도달 시 iocb와 hitchhiker 포인터를 함께 제출하는 새 syscall `io_submit_hit()`를 추가(Blaze 통합, §5.1). io_uring에는 공유 메모리 등록을 위한 `IORING_SETUP_HIT` 플래그와 개별 SQE를 hitchhiker로 표시하는 `IOSQE_HIT` 플래그, `io_uring_get_hite()` 인터페이스를 추가(LeanStore 통합, §5.2). 커널 내부적으로는 `hitchhike_enable` 플래그를 iocb/sqe/dio/bio/request 구조체 전반에 전파해 Hitchhike 요청만 선택적으로 벡터화 처리한다(§5.3, Fig.8).

> [!quote]- 📄 원문 표현 (paper)
> - "Standard I/O restricts requests to contiguous buffers and addresses, and Vectored I/O (e.g., readv) relaxes only the memory buffer constraint while retaining strict address contiguity, Hitchhike removes both limitations." (p.950)
> - "Hitchhike adopts a conservative design principle: grouping only requests targeting the same file descriptor (fd) into a single Hitchhike I/O." (p.951)
> - "Assuming kernel time constitutes 80% of total submission time (K=0.8), and Hitchhike merges 64 requests (n=64), Equation 1 predicts a theoretical speedup of 2.34×. In practice, Hitchhike achieves a 2.29× improvement on the same workload... closely aligning with the model." (p.952)

## 평가 (Evaluation)
서버: Intel Xeon Gold 6430 ×2, Linux kernel v6.5(source 수정), PCIe 4.0/5.0 NVMe SSD 3종(Samsung PM1743, PM9A3, Dapustor H5300 — 기본값) (p.954, §6). 비교 대상: libaio, io_uring, io_uring_cmd(NVMe passthrough), SPDK.

- **핵심 파라미터**: merge size는 64에서 성능 포화(Fig.9b, p.954); 병합 concurrency 4 이상에서 이득 marginal. Queue depth가 32를 넘으면 Hitchhike 처리량이 급격히 성장해 최종 2.8M IOPS 도달(다른 baseline은 plateau)(p.954, Fig.9c).
- **Raw disk, single-thread (p.955, Fig.10a)**: hitchhike-uring 2.8M IOPS = libaio 대비 3.5×, io_uring-fb 대비 2.6×, io_uring_cmd-fb 대비 2.1×, SPDK 대비 43% 높음.
- **Multi-thread (p.955)**: 근-최대 SSD 대역폭 도달에 필요한 스레드 수가 전통 인터페이스 대비 1/4~1/3; 1 스레드로 2.7M IOPS.
- **Latency (p.955-956, Fig.11)**: 99th percentile latency 최저; 2.8M IOPS에서 160μs로 SPDK와 유사; 중간 IOPS(1.8M)에서는 62μs로 SPDK/전통 방식 모두 능가.
- **File I/O path (p.956, Fig.12a)**: H5300에서 hitchhike-uring 1.6M IOPS = libaio 대비 2.6×, io_uring 대비 2.3×, io_uring-fb 대비 1.9×.
- **Software overhead (p.957, Fig.13)**: raw disk 제출 지연 169ns(libaio 690ns, io_uring 489ns 대비 24~39% 수준); amortized interrupt 처리 시간 1637ns→226ns로 감소(libaio의 29.5%, io_uring의 39.8%).
- **Blaze 그래프 처리 (p.957, §6.2, Fig.14)**: 6개 그래프 데이터셋 × 4개 알고리즘(BFS/WCC/BC/PR)에서 libaio 대비 실행시간 30~66% 감소. Amdahl's law에 따라 앱 자체의 유저공간 로직 비중이 크면(K가 작으면) 이득이 줄어든다.
- **LeanStore KV Store (p.957, §6.3, Fig.15)**: YCSB A/B/C/F 워크로드에서 io_uring 대비 처리량 17~34% 개선. YCSB-D는 latest-item 분포로 인메모리 버퍼에서 대부분 서빙되어 개선 없음.

> [!quote]- 📄 원문 표현 (paper)
> - "hitchhike-uring reaches 2.8 M IOPS, which is 3.5× that of libaio (0.8 M IOPS)... Notably, even compared to SPDK, Hitchhike achieves 43% higher throughput." (p.955)
> - "Hitchhike achieved the highest bandwidth (2.8 M IOPS) with 160 μs latency, comparable to SPDK." (p.956)
> - "compared to the libaio interface, hitchhike-aio reduces the execution time by 30% to 66%." (p.957)
> - "Hitchhike demonstrated performance improvements ranging from 17% to 34% across workloads A, B, C, and F." (p.957)

## 섹션 노트
- **§1 Introduction**: outstanding I/O 증가 추세와 소프트웨어 병목을 제시하고, 근본 원인이 strict address contiguity validation임을 주장하며 Hitchhike 개요와 기여를 요약.
- **§2 Background and Motivation**: 2.1 그래프/B+Tree 접근 패턴이 outstanding I/O를 생성하는 방식(Fig.2); 2.2 Figure 3·4로 순차 vs 랜덤 I/O의 CPU 오버헤드 격차 실증(scatter submission이 chunked 대비 제출 지연 3.27×, 처리량 34.2%만 달성); 2.3 io_uring_prep_rw(Listing 1)로 strict address contiguity validation을 정의.
- **§3 Design Philosophy**: 3.1 검증을 드라이버까지 지연해도 안전한 이유; 3.2 Hitchhike의 벡터화 접근과 Vectored I/O 비교(Fig.6); 3.3 file-centric merging scope와 SPDK를 채택하지 않은 이유(공유 메커니즘 부재·busy polling CPU 비용).
- **§4 Design Overview**: 4.1 3단계 파이프라인(Metadata Vectorization/Vectorized Metadata Handling/Deferred Metadata Binding); 4.2 Amdahl's law 기반 이론 모델과 케이스 스터디; 4.3 비동기 인터페이스와의 정합성, write 요청을 주 타깃으로 삼지 않는 이유, Direct I/O 중심(Buffered I/O는 future work) 논의.
- **§5 Implementation**: libaio(Blaze, struct hitchhiker, io_submit_hit) / io_uring(LeanStore, IORING_SETUP_HIT/IOSQE_HIT) / 커널 내부 hitchhike_enable 플래그 전파 / FIO 통합.
- **§6 Evaluation**: 방법론, 마이크로벤치마크(핵심 파라미터·raw disk·file I/O·소프트웨어 오버헤드), 그래프 처리(Blaze), KV 스토어(LeanStore) 순으로 실험.
- **§7 Related Work**: kernel bypass(SPDK, XRP, BypassD, io_uring passthru), kernel modification(queue scheduling, context switch 최적화, lightweight block layer), computational storage devices와 비교.
- **§8 Conclusion**: strict address contiguity validation이 outstanding I/O 처리의 주요 병목임을 재확인하고, Hitchhike가 NVMe 준수를 유지하며 이를 해소함을 요약.

## 핵심 용어 (Key terms)
- **Strict Address Contiguity Validation**: 각 I/O 요청이 file offset·physical sector·최종 LBA 전 구간에서 연속된 주소 범위를 갖도록 커널 I/O 스택 전체가 end-to-end로 강제하는 기존 설계 원칙.
- **Deferred Address Contiguity Validation**: 주소 연속성 검증을 NVMe 드라이버 단계(디바이스 경계)까지 지연시켜 상위 계층은 비연속 주소 그룹을 하나의 단위로 처리할 수 있게 하는 Hitchhike의 설계 원칙.
- **Hitchhike I/O (hio)**: 여러 비연속 주소 범위(오프셋 벡터 + 버퍼 벡터)를 하나의 제출 단위로 캡슐화한 요청.
- **Hitchhiker**: 하나의 Hitchhike I/O에 병합된 개별 원본 요청.
- **Outstanding I/O**: 시스템 내에 동시에 in-flight 상태로 존재하는 요청 수.
- **Merge size (n)**: 하나의 Hitchhike I/O에 병합되는 개별 요청 개수(실험에서 최적 64).
- **struct hitchhiker**: libaio용으로 도입한, 오프셋 배열과 병합 개수를 담는 커널 확장 구조체(iocb의 64바이트 제한 우회).
- **io_submit_hit()**: libaio 인터페이스에 추가된 Hitchhike 전용 제출 syscall.
- **IORING_SETUP_HIT / IOSQE_HIT**: io_uring에서 Hitchhike용 공유 메모리를 등록하고 개별 SQE를 hitchhiker로 표시하는 플래그.
- **File-Centric Merging Scope**: 같은 file descriptor를 타깃하는 요청끼리만 병합을 허용하는 Hitchhike의 적용 범위 제한.
- **Deferred Metadata Binding**: 드라이버에서 벡터화된 주소를 개별 NVMe 커맨드(LBA + tag)로 재구성해 프로토콜 준수를 복원하는 마지막 단계.

## 강점 · 한계 · 열린 질문
- **강점**: 커널 우회(SPDK류)나 aggressive polling 없이 표준 Linux 비동기 I/O(io_uring, libaio) 스택 내에서 동작하여 커널 시맨틱·하드웨어 인터페이스 호환성을 그대로 유지; 애플리케이션 코드 수정이 "a few dozen lines" 수준으로 최소; 이론 모델(Amdahl's law)이 실측치와 근접(2.34× 예측 vs 2.29× 실측)하여 설계 근거가 명확; 마이크로벤치마크뿐 아니라 실제 애플리케이션(Blaze, LeanStore)에서도 유의미한 개선 확인.
- **한계**: File-Centric Merging Scope로 인해 서로 다른 fd/마운트포인트를 넘나드는 요청은 병합할 수 없음(§3.3); write 요청에는 명시적으로 적용하지 않음(LSM/LFS의 기존 배치 방식이 이미 효율적이라는 가정에 의존, §4.3); 현재 구현은 Direct I/O에 한정되며 Buffered I/O/page cache 통합은 future work로 명시(§4.3); 커스텀 커널(v6.5 수정본) 필요로 재현·배포에 kernel setup·reboot이 필요함(Artifact Appendix A.4); 애플리케이션 자체의 유저공간 오버헤드 비중이 크면(예: LeanStore가 Blaze보다 상대 개선폭이 작음) 이득이 제한적.
- **열린 질문**: 서로 다른 fd/네임스페이스 간 병합(cross-file merging)을 안전하게 지원할 방법은? Buffered I/O·page cache 통합 시 캐시 정책과의 상호작용은 어떻게 설계해야 하는가? 병합된 요청들의 tag/큐 슬롯 공유가 QoS·fairness에 미치는 영향은 검증되었는가? Hitchhike와 kernel-bypass(SPDK 등) 하이브리드 결합의 이득은 존재하는가?

## ❓ Q&A (자가 점검)
> [!question]- Hitchhike는 정확히 어느 계층에서 주소 연속성 검증을 지연시키는가?
> NVMe 드라이버 단계(디바이스 경계)까지 지연시킨다. syscall·file system·block layer는 비연속 주소 벡터인 채로 요청을 처리하고, 드라이버에서 LBA 세그먼트별로 NVMe 커맨드를 재구성하며 이때 비로소 연속성이 요구된다(§4.1.3, p.951).

> [!question]- Vectored I/O(readv)와 Hitchhike의 근본적 차이는 무엇인가?
> readv는 메모리 버퍼만 비연속(다중 세그먼트)을 허용하고 주소(오프셋)는 여전히 단일 연속 범위를 요구한다. Hitchhike는 버퍼와 주소(오프셋) 모두를 벡터로 비연속 허용한다(Fig.6, p.951).

> [!question]- 어떤 요청들이 하나의 Hitchhike I/O로 병합될 수 있는가?
> 같은 file descriptor(fd)를 타깃하는 요청들만 병합 가능하다(File-Centric Merging Scope). 서로 다른 fd/마운트포인트 간 병합은 커널 구현 복잡도로 인해 배제되며, 최악의 경우(모든 요청이 서로 다른 fd) 표준 I/O로 gracefully 저하된다(§3.3, p.951).

> [!question]- Merge size(n)를 늘리면 성능이 계속 좋아지는가?
> 아니다. Amdahl's law 모델에 따르면 diminishing returns가 있으며, 실험적으로 merge size 64에서 성능이 포화된다(Fig.9b, p.954; Eq.1, p.952).

> [!question]- SPDK 대비 Hitchhike의 장점은 무엇인가?
> SPDK는 busy polling으로 CPU를 크게 소모하고 디바이스 공유 메커니즘이 부재하다는 한계가 있다. Hitchhike는 커널 호환성을 유지하면서도 높은 queue depth에서 SPDK를 상회하며(raw disk single-thread에서 43% 높은 처리량), 다중 스레드 환경에서 더 나은 확장성을 보인다(p.955).

> [!question]- Write 요청은 왜 Hitchhike의 주된 타깃이 아닌가?
> LFS나 LSM-tree 같은 현대 저장 엔진이 이미 작은 쓰기를 메모리에 버퍼링해 큰 순차 I/O로 flush하기 때문에, 쓰기 제출 오버헤드는 애초에 병목이 아니라고 논문은 설명한다. 다만 YCSB mixed workload 실험은 쓰기에 명시적으로 적용하지 않아도 성능 개선이 나타남을 보인다(§4.3, p.952).

> [!question]- 실제 애플리케이션(Blaze)에서의 개선폭이 FIO 마이크로벤치마크보다 작은 이유는?
> Amdahl's law에 따라, 실제 앱은 end-to-end 지연시간 중 실제 I/O 연산(커널 처리)이 차지하는 비중(K)이 FIO보다 낮아 Hitchhike의 개선 잠재력이 자연히 제한되기 때문이다(§6.2, p.957).

> [!question]- LeanStore의 YCSB-D 워크로드에서 Hitchhike가 개선을 보이지 않은 이유는?
> YCSB-D의 latest-item 분포 특성상 대부분의 읽기가 인메모리 버퍼에서 서빙되어 실제 스토리지 I/O 자체가 적기 때문이다(§6.3, p.957).

## 🔗 Connections
[[File System]] · [[ASPLOS]] · [[2026]]
관련: [[Rearchitecting Buffered I-O in the Era of High-Bandwidth SSDs]] · [[Rethinking the Request-to-IO Transformation Process of File Systems for Full Utilization of High-Bandwidth SSDs]] · [[Heimdall - Optimizing Storage I-O Admission with Extensive Machine Learning Pipeline]]

## References worth following
- Diego Didona et al., "Understanding modern storage APIs: a systematic study of libaio, SPDK, and io_uring" (SYSTOR '22) — 본 논문이 분석 기반으로 삼는 libaio/SPDK/io_uring 성능 특성 비교 연구.
- Yuhong Zhong et al., "XRP: In-Kernel Storage Functions with eBPF" (OSDI '22) — 파일시스템·block layer를 우회해 드라이버에서 직접 LBA를 얻는 대안적 접근으로 Hitchhike의 baseline 후보.
- Sujay Yadalam et al., "BypassD: Enabling fast userspace access to shared SSDs" (ASPLOS '24) — 파일 시맨틱을 유지하며 커널 일부를 우회하는 또 다른 저지연 접근, Related Work에서 비교됨.
- Kanchan Joshi et al., "I/O Passthru: upstreaming a flexible and efficient I/O path in Linux" (FAST '24) — io_uring_cmd 기반 NVMe passthrough, 본 논문 baseline(io_uring_cmd)의 근거.
- Jaehyun Hwang et al., "Rearchitecting Linux Storage Stack for μs Latency and High Throughput" (OSDI '21) — 커널 I/O 스택 재설계 동기(CPU 성능 정체·고IOPS 디바이스 부조화)를 공유하는 선행 연구.

## Personal annotations
<!-- 본인 메모 영역 -->
