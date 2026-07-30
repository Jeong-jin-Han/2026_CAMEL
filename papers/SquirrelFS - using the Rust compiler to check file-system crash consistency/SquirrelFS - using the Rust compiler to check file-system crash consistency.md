---
title: "SquirrelFS: Using the Rust Compiler to Check File-System Crash Consistency"
description: "Rust의 typestate 패턴으로 crash-consistency ordering 불변식을 컴파일 타임에 검사하고, PM 전용 Synchronous Soft Updates 메커니즘으로 crash-safe file system을 구현"
venue: "ACM Transactions on Storage (TOS)"
year: 2025
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/fs
  - venue/tos
  - year/2025
  - list/26s-v2
  - topic/persistent-memory
  - topic/crash-consistency
  - topic/rust-typestate
  - topic/formal-methods
---

# SquirrelFS: Using the Rust Compiler to Check File-System Crash Consistency

> **ACM Trans. Storage (TOS) 2025** · cluster/fs · Source: [SquirrelFS - using the Rust compiler to check file-system crash consistency.pdf](<SquirrelFS - using the Rust compiler to check file-system crash consistency.pdf>)

저자: Hayley LeBlanc (The University of Texas at Austin), Nathan Taylor (Semgrep, Seattle), James Bornholt (The University of Texas at Austin and Amazon Web Services Inc, Seattle), Vijay Chidambaram (The University of Texas at Austin)

## TL;DR
Rust의 typestate 패턴을 이용해 파일시스템 객체의 persistence/operational 상태를 타입에 인코딩함으로써, crash-consistency의 핵심인 "업데이트 순서(ordering)" 불변식을 별도 proof 없이 컴파일러가 정적으로 검사하게 만든다. 이를 위해 전통적 soft updates의 복잡한 비동기 의존성 추적과 순환 의존성 문제를 없앤 **Synchronous Soft Updates (SSU)**를 persistent memory(PM) 전용으로 새로 설계했고, rename pointer라는 장치로 soft updates가 원래 지원 못했던 atomic rename도 구현했다. 이렇게 만든 새 PM 파일시스템 SquirrelFS는 NOVA·WineFS 등 기존 PM 파일시스템과 비슷하거나 더 나은 성능을 내면서도, 컴파일(및 crash-consistency 검사)에 단 10초, 코드량 7.5K LOC만 필요하다. 다만 typestate 기반 정적 검사는 ordering 불변식만 잡을 뿐 함수 구현 자체의 정확성이나 임의 크기 집합에 대한 성질은 검증하지 못하며, 이 프로토타입은 mount 시간과 메모리 사용량 면에서 다른 시스템보다 현저히 무겁다.

## 문제 & 동기
Crash consistency를 보장하는 두 축은 (1) 테스팅 — 빠르고 저렴하지만 불완전(버그 존재만 증명, 부재는 증명 불가)하고, (2) 정형 검증 — 완전하지만 구현 코드 1줄당 7–13줄의 proof가 필요할 정도로 개발 비용이 크다 (Table 1, p.2). 예컨대 BilbyFS는 1K줄 구현에 13K줄 proof, VeriBetrKV는 6K줄 구현에 45K줄 proof, FSCQ는 unverified 유사 시스템의 10배 크기 코드가 필요했다(p.4). 이 논문은 "테스팅보다 완전하고 검증보다 저렴한" 중간 지점을 Rust 컴파일러의 typestate 패턴으로 만들고자 한다.

> [!quote]- 📄 원문 표현 (paper)
> - "For each line of code in the implementation, we may need to write 7–13 lines of proof." (p.2)
> - "Another verified file system, FSCQ [11], has interleaved proof and implementation code that is 10× the size of the most similar unverified system." (p.4)
> - "In this work, we seek to find a middle ground between these two approaches. We would like to verify some aspects of file systems, but without the burden of having to write and maintain proofs." (p.2)

## 핵심 통찰 (Key Insight)

**1. Crash consistency는 결국 ordering 문제이고, ordering은 Rust의 typestate로 컴파일 타임에 강제할 수 있다.** Soft updates 계열 메커니즘은 in-place update를 올바른 순서로만 storage에 반영하면 crash-safe함을 보장한다(soft updates 3원칙: never point to an uninitialized structure, never re-use a resource before nullifying previous pointers, never reset an old pointer to a live resource before setting the new one; p.7). Rust는 함수 시그니처의 제네릭 타입 파라미터로 "이 객체가 어떤 연산까지 거쳤는지"를 표현할 수 있어(typestate pattern), 잘못된 순서로 호출하면 아예 컴파일이 안 되게 만들 수 있다. 이게 효과적인 이유는, 실제 컴파일러 수정 없이(unmodified Rust compiler) 기존 타입 시스템만으로 강제된다는 점 — 오버런타임 비용이 0(zero-sized type)이라는 점이다.

> [!quote]- 📄 원문 표현 (paper)
> - "We ensure that the ordering invariants of Synchronous Soft Updates hold using the Rust compiler. We take advantage of Rust's support for the typestate pattern..." (p.3)
> - "It is important to note that we are not modifying the Rust compiler in any way. To the Rust compiler, it is no different from type-checking any other code base..." (p.6)

**2. PM의 낮은 지연시간이 soft updates의 비동기성(→순환 의존성)을 근본적으로 제거한다.** 기존 soft updates는 DRAM 페이지 캐시에 업데이트를 모았다가 나중에 비동기로 storage에 쓰기 때문에, 한 메타데이터 객체가 여러 번 갱신되며 의존성이 얽히는 cyclic dependency 문제가 생겨 구현이 매우 복잡했다(p.6). PM은 byte-addressable하고 synchronous라 시스템 콜이 끝날 때 이미 durable하다는 특성이 있으므로, 이를 이용한 **Synchronous Soft Updates(SSU)**는 pending update가 아예 존재하지 않아 cyclic dependency 문제 자체가 사라진다.

> [!quote]- 📄 원문 표현 (paper)
> - "We observe that the root of complexity in soft updates (such as cyclic dependencies and structures for tracking dependencies) is asynchrony. A synchronous implementation of soft updates neatly avoids these complexities." (p.6)

**3. Rename pointer로 soft updates가 원래 못 하던 atomic rename을 지원.** 전통적 soft updates는 rename의 atomicity를 보장하지 않아 crash 시 src·dst 둘 다 존재할 수 있었다. SquirrelFS는 destination directory entry에 "rename pointer" 필드를 추가해 crash 후에도 rename이 어디까지 진행됐는지(source 유지/롤백 vs 완료/롤포워드) 판단 가능하게 만든다(Fig 2, p.7–8).

> [!quote]- 📄 원문 표현 (paper)
> - "SSU fixes this flaw; renames are atomic, and a crash during rename will result in either src or dst after recovery." (p.6)
> - "To resolve this, SSU adds an extra field, called the rename pointer, to directory entries in order to persistently save enough information to complete the rename operation after a crash." (p.7)

## 설계 / 메커니즘 (Design)

**Typestate 두 축(p.8, Table 2 p.17).** SquirrelFS는 모든 persistent object(inode, dentry, page descriptor 등)에 두 종류의 typestate를 부여한다: (1) **persistence typestate** — `Dirty`(캐시에만 있음) → `InFlight`(flush됐지만 fence 전) → `Clean`(flush+fence 완료, durable). (2) **operational typestate** — 객체별로 다르며 다음에 어떤 연산이 가능한지 표현(예: inode의 `Free/Init/IncLink/DecLink/...`, dentry의 `Free/Alloc/Committed/Renaming/...`). `flush()`/`fence()` typestate transition 함수만이 persistence typestate를 바꿀 수 있어(Listing 3, p.9; Fig 6, p.18), 캐시라인 flush와 store fence를 빠뜨리면 컴파일이 실패한다.

**mkdir/unlink 예시(Fig 3 p.10, Fig 4 p.12).** mkdir은 (1) 새 inode를 초기화, (2) 새 directory entry에 이름 설정, (3) 부모 inode의 link count 증가라는 독립적 갱신을 먼저 하고, 이 셋이 모두 durable해진 뒤에야 directory entry의 inode 번호를 커밋(commit_dentry, Listing 3)해서 tree에 연결한다 — dangling link를 막기 위한 순서다. unlink는 반대 순서: directory entry를 무효화 → inode link count 감소(durable) → link count가 0이면 backpointer 기반으로 페이지들을 해제 후 inode 해제(§3.3, p.11).

**Persistent layout(Fig 5, p.13).** 디바이스는 superblock, inode table, page descriptor table, data pages 네 영역으로 나뉜다. Page descriptor는 (NoFS 스타일로) 자신을 소유한 inode를 가리키는 backpointer와 자체 typestate를 가지며, 이 backpointer 기반 설계 덕분에 페이지 할당/해제 의존성이 상수 개(constant number)의 durable update로 끝난다(p.13, p.23). Allocator·인덱스는 전부 volatile(DRAM)이며 mount 시 전체 디바이스를 스캔해 재구축한다(p.13–14).

**Rename recovery(Listing 2, p.9; Fig 2, p.7–8).** rename은 6단계 state machine으로 모델링된다: dst의 rename pointer를 src로 설정 → dst를 valid로 만들어 이 시점부터 rename 완료가 보장되는 atomic point 통과 → src를 물리적으로 invalid 처리 → rename pointer 해제 → src dentry 완전 할당 해제. Recovery는 mount 시 전체 directory entry를 스캔하며 rename pointer가 남아있는 항목을 찾아 src/dst의 inode 포인터 비교만으로 crash가 step 2, 3, 4 중 어디서 났는지 판별해 롤백 또는 롤포워드한다(rename_recover, Listing 2 p.9).

**동시성.** VFS-level locking에 의존하며, Rust의 소유권 시스템이 리소스에 항상 정확히 하나의 owner·타입만 존재함을 보장하므로 typestate 기반 정적 검사가 동시성 하에서도 유효하다(p.14).

**Alloy 모델(§3.4, p.14–16).** 구현과 별개로 Alloy 모델링 언어로 SquirrelFS의 typestate transition을 1:1에 가깝게 미러링한 모델을 만들어, 2개 concurrent operation·10개 persistent object·최대 30 step 범위에서 crash-consistency invariant(legal link count, 초기화 안 된 객체를 가리키는 포인터 없음, 해제된 객체는 다른 구조를 안 가리킴, rename pointer 사이클 없음 등)를 SAT 기반으로 bounded model checking했다(§5.7, p.31).

> [!quote]- 📄 원문 표현 (paper)
> - "Persistence and operational typestate are separate to capture the fact that most storage devices do not synchronously flush updates." (p.8)
> - "SquirrelFS uses a backpointer-based page management approach ... this approach simplifies dependency rules for updates involving page allocation and deallocation." (p.13)
> - "We check that a correctness invariant always holds in all traces of our Alloy model. We bound traces to include two operations (which may be concurrent), 10 persistent objects, and up to 30 steps." (p.31)

## 평가 (Evaluation)
2-socket 32-core, 128GB DRAM + 128GB Intel Optane DC PM, Debian Bookworm/Linux 6.3에서 ext4-DAX, NOVA, WineFS와 비교(metadata consistency만 맞추고 data consistency는 미보장으로 통일; §5.1, p.27).

- **Microbenchmarks(Fig 11a, p.28)**: 1K/16K append·read, create, mkdir, rename, unlink 지연시간 측정. WineFS 또는 SquirrelFS가 대부분 항목에서 최저 지연시간. NOVA는 mkdir·rename에서 여러 inode를 journal에 기록해야 해서 상대적으로 느림(p.27).
- **Filebench(Fig 11b, p.28)**: fileserver +8%, varmail +13% (다음으로 빠른 시스템 대비), webserver·webproxy는 최고 대비 10% 이내(§5.3, p.27).
- **YCSB on RocksDB(Fig 11c, p.28)**: 25GB DB, 25M records/ops, 8 threads. 100% insert인 Load A·E에서 SquirrelFS가 최고 throughput(저널링 없음의 이점). append 시 journaling/logging이 NOVA·WineFS에 2–3us, Ext4-DAX에 3–4us 추가 오버헤드 발생(p.28). Run B/C/D(모두 95%+ 소량 4KB read)에서는 Ext4-DAX 대비 10% 이내. Run A·F(50/50 read-update, read-modify-write)에서도 SquirrelFS 최고.
- **LMDB(Fig 11d, p.29)**: fillseqbatch/fillrandbatch/fillrandom, 100M keys, 모든 시스템이 서로 12% 이내.
- **git checkout(Table 3, p.29)**: Linux v3.0→v6.0 순차 checkout, 모든 시스템이 서로 8% 이내.
- **Mount time(Table 4, p.29–30)**: SquirrelFS가 Ext4-DAX보다 훨씬 느림 — mkfs 5.80s vs 0.33s, empty mount 5.51s vs 0.01s, full(128GB, 128 dirs) mount 30.50s vs 0.01s; recovery 포함 시 empty 5.76s vs 0.01s, full 55.50s vs 0.01s. 전체 디바이스를 zero-out/scan해야 하기 때문(§5.5).
- **컴파일 시간(Table 5, p.30)**: SquirrelFS 7.5K LOC, 10초. Ext4 45K LOC/38초, NOVA 16K/20초, WineFS 9K/13초 — typestate checking이 컴파일 시간에 유의미한 영향을 주지 않음. FSCQ는 verify에 약 11시간, VeriBetrKV는 1.8시간(parallelize 시 10분) 소요(p.30).
- **메모리 사용량(Table 6, p.30–31)**: Empty 상태 SquirrelFS 1,104 MiB vs Ext4-DAX/NOVA 1 MiB, WineFS 3 MiB. Full 상태 3,220 MiB vs Ext4-DAX 336, NOVA 64, WineFS 57 MiB — red-black tree 기반 free list와 dentry/data page index가 각각 약 1GB씩 차지(p.31).
- **정확성(§5.7, p.31)**: xfstests generic suite 지원되는 67개 테스트 전부 통과. Chipmunk로 24시간 systematic+fuzz crash-consistency 테스트한 결과 ordering 관련 버그 0개(typestate-checked SSU의 효과 방증), 다만 typestate로 검사되지 않는 코드에서 버그 4개 발견(volatile 구조 rebuild 3개, typestate transition 내부의 잘못된 주소로의 cache line flush 1개)(p.31).

> [!quote]- 📄 원문 표현 (paper)
> - "SquirrelFS achieves slightly better throughput than the next fastest system on fileserver and varmail (8% and 13% better, respectively) and within 10% of the fastest system on both webserver and webproxy." (p.27)
> - "SquirrelFS takes approximately 10 seconds to compile on our test machine, including typestate checking. This compares well to fully-verified systems; FSCQ [11] takes about 11 hours to verify, and VeriBetrKV [28] takes 1.8 hours (10 minutes when parallelized)." (p.30)
> - "Chipmunk did not find any ordering-related crash-consistency bugs in this code, providing evidence that typestate-checked SSU is an effective mechanism for preventing such bugs." (p.31)

## 섹션 노트
- §1 Introduction: 테스팅 vs 검증의 완전성/개발비용 트레이드오프(Table 1)를 배경으로, typestate 기반 statically-checked crash consistency와 SSU를 기여로 제시.
- §2 Background: crash consistency 정의(journaling/copy-on-write/soft updates), soft updates의 비동기성 유래 복잡도, Corundum[31]에서 영감을 받은 typestate pattern 소개.
- §3 SquirrelFS: SSU 메커니즘(3.1), Rust typestate로 ordering 강제(3.2), mkdir/unlink 예시(3.3), 구현·아키텍처·Alloy 모델(3.4), 22개 typestate 목록(3.5, Table 2), 한계(3.6: undecidable한 집합 성질은 검사 불가), CXL로의 확장 가능성(3.7).
- §4 Experience: 개발 과정에서 typestate 세분화 정도 트레이드오프, Alloy와 Rust 구현을 병행 개발하며 상호 피드백, typestate가 잡은 버그(missing flush/fence, 잘못된 unlink 순서) vs Alloy가 잡은 설계 버그(rename 후 재등장하는 dangling dentry, `.`/`..` durable 저장 이슈) vs 테스팅만 잡은 버그(volatile index 갱신 누락) 구분.
- §5 Evaluation: 위 수치 참고.
- §6 Related Work: Corundum(저수준 PM 안전성만), SoupFS/ArckFS(비동기 또는 userspace soft updates, C로 구현, Rust 타입시스템 미사용), Bento/ShardStore(Rust 기반이지만 typestate로 crash consistency 검사는 안 함), Vault/Plaid와의 typestate 표현력 비교(다중 mutable alias, 임의 개수 컬렉션 지원 등에서 Rust typestate는 더 제한적).
- §7 Conclusion: typestate 기반 저비용 crash-consistency 방법론 제안, SSU가 이를 가능케 한 핵심 메커니즘.

## 핵심 용어 (Key terms)
- **Typestate pattern**: 객체의 런타임 상태(어떤 연산까지 수행됐는지)를 타입 파라미터로 인코딩해, 잘못된 순서의 연산 호출을 컴파일 타임에 에러로 잡는 API 설계 패턴.
- **Synchronous Soft Updates (SSU)**: PM의 synchronous 특성을 활용해 비동기 의존성 추적과 순환 의존성 문제를 제거한, SquirrelFS의 crash-consistency 메커니즘.
- **Persistence typestate**: `Dirty`(캐시에만 존재) → `InFlight`(flush 완료, fence 전) → `Clean`(durable) 3단계로 객체의 durability 상태를 표현하는 타입.
- **Operational typestate**: 객체별로 다음에 어떤 연산이 허용되는지를 나타내는 타입(예: inode의 `Init`, dentry의 `Committed`).
- **Rename pointer**: rename 중인 destination directory entry에 저장되는 필드로, crash 후 rename을 롤백할지 완료할지 판단하는 데 쓰임.
- **Backpointer-based page management**: 페이지가 자신을 소유한 inode를 가리키는 포인터를 저장해, 페이지 할당/해제 의존성을 단순화하는 설계(NoFS에서 영감).
- **PhantomData**: 런타임 자원을 쓰지 않고 컴파일러에 타입 파라미터 소유를 증명하기 위한 Rust 표준 라이브러리의 zero-sized 타입.
- **Alloy**: relational logic 기반 model checking 언어/도구로, SquirrelFS의 typestate transition을 모델링해 crash-consistency 설계를 bounded 범위에서 검증.
- **DAX (Direct Access)**: 커널이 PM 디바이스를 page cache 우회하고 애플리케이션 주소 공간에 직접 매핑하는 기능.
- **Soft updates**: in-place update의 순서를 강제해 crash consistency를 얻는 고전 메커니즘(BSD FFS). 원 3원칙: uninitialized 구조를 가리키지 않기, 이전 포인터 제거 전 자원 재사용 안 하기, 새 포인터 설정 전 살아있는 자원으로 옛 포인터 되돌리지 않기.

## 강점 · 한계 · 열린 질문
- **강점**: 별도 proof-writing 없이 unmodified Rust 컴파일러만으로 crash-consistency ordering을 정적 검사 — 컴파일 10초, 7.5K LOC로 verified 시스템 대비 압도적으로 저비용(Table 5). 성능도 기존 PM 파일시스템과 comparable하거나 더 나음(Filebench, YCSB에서 우위). 24시간 fuzz 테스트에서 ordering 버그 0건이라는 실증적 증거(§5.7).
- **한계**: typestate는 ordering 불변식만 검사하며, 함수 구현 자체의 정확성(예: 올바른 오프셋 계산)이나 임의 크기 집합에 대한 성질(모든 페이지가 할당됐는가 등, undecidable)은 커버하지 못함(§3.6, §4.3). 실제로 unchecked 코드(volatile 구조 rebuild, cache line flush 주소)에서 버그 4건이 Chipmunk로 발견됨(§5.7). Mount time과 메모리 사용량이 다른 PM 파일시스템 대비 현저히 큼(각각 최대 5500배, 수십 배 이상; Table 4, 6) — 저자들도 §5.8에서 parallelizing mount, durable index/allocator, 더 space-efficient한 volatile 자료구조 등 개선 방향을 명시.
- **열린 질문**: typestate 기반 접근을 journaling·copy-on-write처럼 ordering만으로 완전히 설명되지 않는 다른 crash-consistency 메커니즘(예: atomicity가 필요한 key-value store)에 어떻게 확장할지(§4.4)는 미해결. §3.7에서 언급되듯 CXL Type 3(CXL.mem) 부착 PM 디바이스로의 적용 가능성이 있으나, 디바이스 용량이 커질수록 mount 성능·메모리 footprint 문제가 더 악화될 수 있음.

## ❓ Q&A (자가 점검)

> [!question]- SquirrelFS는 왜 전통적(비동기) soft updates 대신 synchronous 버전을 새로 설계했나?
> 전통적 soft updates는 DRAM 페이지 캐시에 업데이트를 모았다가 비동기로 storage에 쓰기 때문에 여러 연산 간 의존성 추적과 순환 의존성(cyclic dependency) 문제가 생겨 구현이 매우 복잡했다. PM은 byte-addressable하고 지연시간이 낮아 시스템 콜 종료 시점에 이미 durable한 synchronous 업데이트가 가능하므로, 이 특성을 활용해 pending update 자체를 없애 비동기성에서 오는 복잡도를 원천 제거했다(§3.1, p.6).

> [!question]- rename의 atomicity는 구체적으로 어떻게 보장되나?
> destination directory entry에 rename pointer 필드를 추가한다. 6단계 state machine(Fig 2, p.7–8)으로 진행되며, dst가 valid가 되는 시점(step 3)이 atomic point다. 이 시점 이전에 crash가 나면 rename_recover(Listing 2)가 src/dst의 inode 포인터를 비교해 롤백(step 2/4로 판단되면), 혹은 롤포워드(step 3로 판단되면) 처리를 수행한다.

> [!question]- typestate 정적 검사가 절대 못 잡는 버그 유형은 무엇인가?
> 임의 크기(런타임에 결정되는) 집합에 대한 전칭 성질(universally-quantified property), 예를 들어 "파일에 속한 모든 페이지가 할당되어 있다"는 것은 undecidable이라 typestate로 인코딩할 수 없다(§4.3, p.25). 이 문제를 우회하기 위해 SquirrelFS는 페이지 range 전체에 단일 typestate를 부여하는 방식을 택했는데, 이는 정적 검사력을 일부 희생한다(Fig 10, p.21).

> [!question]- Alloy 모델과 Rust 구현(typestate)은 각각 어떤 종류의 버그를 잡았나?
> Typestate checking은 missing flush/fence 같은 낮은 수준의 persistence 누락과, 잘못된 unlink 순서(link count를 directory entry 삭제 전에 감소) 같은 높은 수준의 ordering 버그를 컴파일 에러로 즉시 잡았다. Alloy 모델은 구현 전 설계 단계에서 crash 후 재등장하는 dangling directory entry, `.`/`..` durable 저장 관련 의존성 오류 등 더 고차원적인 설계 결함을 발견했다(§4.2, p.24).

> [!question]- SquirrelFS의 성능이 다른 PM 파일시스템보다 나은 워크로드와 나쁜 지표는 각각 무엇인가?
> 저널링/로깅이 없다는 이점 덕분에 write-dominant·small-append 워크로드(Filebench fileserver +8%, varmail +13%; YCSB Load A/E, Run A/F)에서 우위를 보인다. 반대로 mount time(최대 55.50초 vs Ext4-DAX 0.01초, Table 4)과 메모리 사용량(full 상태 3,220 MiB vs 최소 57 MiB, Table 6)에서는 크게 뒤처지는데, 이는 전체 디바이스를 스캔해 volatile index·allocator를 재구축해야 하기 때문이다.

> [!question]- FSCQ 같은 완전 검증(fully-verified) 시스템과 비교했을 때 SquirrelFS의 근본적 차이는?
> SquirrelFS는 ordering-based invariant만 정적으로 보장하며 함수 구현의 완전한 정확성은 검증하지 않는다(§3.6). 그 대가로 개발 비용이 극적으로 낮다 — 컴파일(및 crash-consistency 검사)에 10초, FSCQ는 verification에 약 11시간이 걸린다(Table 5, p.30). 즉 SquirrelFS는 "테스팅보다 강하지만 완전 검증보다 약한" 중간 지점을 목표로 한다.

> [!question]- Chipmunk를 이용한 크래시 컨시스턴시 퍼징에서 실제로 발견된 버그는?
> 24시간 동안 systematic+fuzzed 테스트를 돌린 결과 ordering 관련 crash-consistency 버그는 0개였다. 대신 typestate로 검사되지 않는 코드 영역에서 버그 4개가 발견됐다 — 3개는 volatile 데이터 구조를 rebuild하는 로직에서, 1개는 typestate transition 내부에서 잘못된 주소에 cache line flush를 수행한 것이었다(§5.7, p.31).

> [!question]- SquirrelFS 설계가 CXL-attached memory로도 확장될 수 있다고 저자들이 언급한 근거와 한계는?
> §3.7에서 CXL Type 3(CXL.mem)이 기존 NVDIMM과 동일한 인터페이스·persistence semantics를 가진 byte-addressable, low-latency 스토리지를 제공할 것이므로 SSU 기반 SquirrelFS 설계가 원칙적으로 적용 가능하다고 본다. 다만 mount 성능과 메모리 footprint가 디바이스 용량에 비례해 나빠지는 현재 설계 특성상, CXL이 흔히 지향하는 대용량 디바이스에서는 이 문제가 더 심각해질 수 있어 추가 최적화가 필요하다고 명시한다.

## 🔗 Connections
[[File System]] · [[TOS]] · [[2025]]
관련: 이 리스트에서 PM/CXL-attached memory, crash consistency를 다루는 다른 File System cluster 논문들과 비교해 볼 만하다(예: NOVA/WineFS 계열 PM 파일시스템, Smart-Infinity의 SSD 이중역할 논의와 대비되는 "transparent co-design vs typestate 정적 검사"의 축).

## References worth following
- Morteza Hoseinzadeh & Steven Swanson, "Corundum: Statically-enforced persistent memory safety," ASPLOS 2021 — SquirrelFS가 영감을 받은, Rust 타입 시스템으로 저수준 PM 안전성(트랜잭션, no dangling volatile pointer)을 강제한 선행 연구.
- Marshall Kirk McKusick & Gregory R. Ganger, "Soft updates: A technique for eliminating most synchronous writes," USENIX ATC 1999 — SquirrelFS SSU의 원류가 되는 고전 soft updates 메커니즘.
- Rohan Kadekodi et al., "WineFS: A hugepage-aware file system for persistent memory that ages gracefully," SOSP 2021 — 본 논문의 주요 성능 baseline.
- Jian Xu & Steven Swanson, "NOVA: A log-structured file system for hybrid volatile/non-volatile main memories," FAST 2016 — 본 논문의 또 다른 핵심 baseline (journaling 오버헤드 비교 대상).
- Hayley LeBlanc et al., "Chipmunk: Investigating crash consistency in persistent-memory file systems," EuroSys 2023 — 본 논문의 crash-consistency fuzzing 평가에 쓰인 도구, 저자 겹침.
- Mingkai Dong & Haibo Chen, "SoupFS: soft updates made simple and fast on non-volatile memory," USENIX ATC 2017 — 비동기 soft updates를 PM에 적용한 선행 연구로, SSU와의 비동기/동기 비교 대상.

## Personal annotations
<!-- 본인 메모 영역 -->
