---
title: "DNA data storage: A generative tool for Motif-based DNA storage"
aliases: [DNA data storage]
description: "Motif 기반 DNA 저장에서 생물학적 제약을 항상 만족하는 motif(key+payload) 집합을 MDP로 생성하는 도구"
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/infra
  - topic/dna-storage
  - topic/novel-media
  - venue/fast
  - year/2025
---

# DNA data storage: A generative tool for Motif-based DNA storage

> **FAST 2025** · `cluster/infra` · Source: [DNA data storage - A generative tool for Motif-based DNA storage.pdf](<DNA data storage - A generative tool for Motif-based DNA storage.pdf>)

저자: Samira Brunmayr, Omer S. Sella, Thomas Heinis (Imperial College London)

## TL;DR
DNA 저장에서 nucleotide 단위 합성(synthesis)은 비싸고 느리므로, 미리 대량 합성해 둔 짧은 서열인 **motif**(key + payload)를 PCR/ligation으로 조립해 데이터를 쓰는 방식이 더 경제적이다. 이 논문은 motif 집합 설계를 **Markov Decision Process(MDP)** 로 정식화하고, homopolymer·GC-content·hairpin·key-in-payload 등 생물학적/기술적 제약을 보상(reward) 함수로 인코딩한 **생성 도구(Motif Generation Tool)** 와 별도의 **validation 도구**를 제시한다. 조립 순서에 관계없이 제약을 항상 만족하는 motif를 만들며, 기존 도구(DNA Fountain, Euclid, Preuss 등)와 random 생성보다 빠르게 적합 집합을 생성한다.

## 문제 & 동기
DNA는 정보 밀도(최대 10^18 bytes/mm^3)와 보존성(반감기 ~500년)이 매우 높아 장기 archival 매체로 유망하다. 그러나 nucleotide 단위로 데이터를 쓰는 **synthesis**는 1TB 쓰는 데 4억 USD가 넘어(coding density 2 bits/base 가정) 상용화가 불가능하다. 대안은 미리 대량으로 PCR 생산해 둔 짧은 서열 **motif**를 알파벳 글자로 쓰고, 이들을 bridged oligonucleotide assembly로 연결해 데이터를 쓰는 것이다. 이때 motif는 어떤 순서로 조립되든 (1) payload가 고유하게 식별되고 (2) key는 지정된 reverse-complemented bridge에만 결합하며, homopolymer·GC-content·hairpin·reserved word 등 생물학적/기술적 제약을 모두 만족해야 한다. random 또는 기존 도구로는 이런 조합적 제약을 만족하는 집합을 효율적으로 만들기 어렵다.

> [!quote]- 📄 원문 표현 (paper)
> "The cost of writing one TB of data using state-of-the-art DNA synthesis is higher than 400 million USD, assuming a coding density of 2 bits per base. Conventional synthesis of DNA strands is thus not a viable option for commercialisation of DNA as a data-carrying medium." (p.1)
>
> "Assembly of DNA strands from *motifs*, i.e. short DNA sequences, is an economical and faster way of representing data using DNA. ... Trading the quaternary alphabet {A,C,T,G} for longer fragments, namely motifs, allows an increase in write bandwidth and reduces cost." (Abstract, p.1)

## 핵심 통찰

> [!note]- ① Motif 설계 = 조합 제약 최적화 문제 (p.2)
> 데이터를 nucleotide가 아닌 motif로 인코딩하면, "데이터를 DNA로 쓰는 문제"가 "제약을 만족하는 key와 payload를 설계하는 조합 문제"로 바뀐다. motif는 payload P를 한두 개의 key(S0, S1)로 감싼 구조이며(Figure 1), 여러 motif가 하나의 반응에서 병렬로 조립되어 write bandwidth를 높인다.

> [!note]- ② 임의 조립 순서에 robust해야 한다 (p.2, p.5)
> motif는 어떤 순서로도 결합될 수 있으므로, 제약은 단일 motif만이 아니라 **이미 생성된 다른 모든 motif와의 임의 조합**에 대해서도 성립해야 한다. 따라서 hairpin 등 제약 계산은 현재 payload를 기존 key/payload 집합과 합쳐 worst-case로 평가한다.

> [!note]- ③ 제약을 보상으로 인코딩한 MDP/Markov Chain (p.2-3)
> 부분 payload를 한 base씩 확장하는 과정을 MDP로 본다. 각 후보 base에 대해 제약 위반 정도를 non-positive **log score** ls(i,x)로 정의하고, 가중합 ls(i)=Σ w_x·ls(i,x) 후 softmax로 확률 p_i=e^{ls(i)}/Σe^{ls(j)}를 만든다(Eq.1-2). 위반이 클수록 확률이 낮아져, 확률적으로 base를 뽑되 제약 위반을 회피한다.

> [!note]- ④ 생성 도구와 검증 도구의 분리 (p.2)
> 어떤 방식으로 생성했든 독립적으로 동작하는 **pass/fail validation 도구**를 함께 제공한다. 즉, 생성 과정과 무관하게 motif 집합이 제약을 만족하는지(자신 및 다른 motif와의 조합 포함) 사후 검증할 수 있다.

## 설계 / 메커니즘

> [!abstract]- Motif 구조와 조립 (Figure 1, p.2)
> motif = key S0 + payload P + key S1 (top-left). bridge는 S1', S2'(reverse complement)의 연결이며 key에만 결합한다. Motif1(끝 key S1)과 Motif2(시작 key S2)가 bridge에 annealing(중간)된 뒤, PCR로 빠진 base를 채워 double-stranded DNA를 완성한다(하단). 여러 distinct key를 쓰면 한 반응에서 여러 motif가 병렬 조립되어 write가 가속된다.

> [!abstract]- MDP 정식화와 reward (§2.1-2.2, p.2-3)
> key 집합 K는 주어졌다고 가정하고 payload 집합 P를 빈 집합에서 귀납적으로 구성한다(모든 key 길이 L_K, payload 길이 L_P 동일 가정). 한 step마다 base 하나를 추가하며, full length L_P에 도달하면 payload를 commit. 더 이상 제약을 만족하는 payload를 못 만들 때까지 반복. reward는 여러 제약 log score의 가중합(Eq.1), softmax로 확률화(Eq.2). ls(i,x)≤0이고 "제약 x 위반"에 대해 단조 증가.

> [!abstract]- 4가지 제약 log score (§2.3-2.6, p.3-5)
> - **Homopolymer**(§2.3): 같은 base 연속 길이 homLen이 maxHom에 가까울수록 score↓. ls(homLen,i) = -(h_hom)^{homLen(i)/maxHom} + 1 (Eq.3). h_hom이 클수록 경계 근처 기울기가 가파름(Figure 3).
> - **GC-content**(§2.4): 임의 motif 조합의 GC 비율이 [minGC, maxGC]에 들어가야 함. curMaxGC/curMinGC 편차를 반영, ls(GC,i)=-max{0, W_GC·(curMaxGc-maxGC), W_GC·(minGC-curMinGc)} (Eq.5). 가중치 W_GC=(h_gc)^{|p0|/L_P}-1로 payload가 길어질수록(보정 여지 감소) 더 critical (Eq.6).
> - **Hairpin**(§2.5): 서열 S와 그 reverse complement S'가 loop L을 사이에 두고 self-annealing하면 hairpin(stem+loop, Figure 4) 형성→sequencing 방해. loop size [loopSizeMin, loopSizeMax]인 hairpin의 stem을 maxHairpin 미만으로 제한. 현재 motif 생성 시 **다른 모든 motif의 stem과의 결합**까지 피해야 하므로(Figure 5), motif 간 유사도 log score(ls_similarity)를 추가해 결합(Eq.7-9).
> - **No key in payload**(§2.6): key가 payload에 나타나면 의도치 않은 annealing/데이터 오염 및 용량 감소. seqLen(payload 안에 나타나는 가장 긴 key 부분 길이)에 패널티: ls_noKeyInPayload(i) = -(h_key)^{seqLen/L_k}+1 (Eq.10).

> [!abstract]- 생성 방법: key/payload 생성과 상태/전이 수 (§3, p.6-7)
> 입력: 제약 집합 C, 양의 shape hyperparameter 집합, 양의 weight hyperparameter 집합.
> - **Key 생성**(§3.1): 상태=(제약, 이미 생성된 key 목록, 현재 생성 중 key). 각 상태에서 base A/T/C/G 추가로 4개 상태로 전이(Figure 6). 최대 상태/전이 수 S_K = T_K = 4^{keySize×keyNum} (Eq.11).
> - **Payload 생성**(§3.2): 상태에 "이미 생성된 payload 집합"이 추가됨. S_P = T_P = 4^{payloadSize×payloadNum} (Eq.12).

> [!abstract]- Validation 도구 (§5.1, p.7)
> 입력: 최대 동일 연속 base 길이, hairpin stem 최대 길이, hairpin loop 최대/최소, GC 상/하한, 고정 길이 distinct key 목록, 고정 길이 payload 집합. motif M이 (1) 단독으로 제약 위반하거나 (2) 다른 motif(자신 포함)와의 조합이 위반하면 fail. 생성 방식과 독립적으로 동작.

## 평가

> [!success]- 단일 제약별 생성 시간 비교 (§5.2, Figure 7, p.8)
> quad-core Intel 머신에서 motif 길이 3~100 base, motif 1개(payload·key 1개씩)를 각 제약별로 생성하는 시간 측정. 250ms 초과는 표시 안 함.
> - homopolymer/hairpin 제약: motif 길이가 커질수록 Motif Generation Tool이 DNA Fountain[11], Preuss et al.[17], random보다 항상 우수.
> - GC-content 제약: random은 기대 GC가 50%라 유리해 예외적으로 random이 선형 성능.
> - Preuss et al. 도구는 전 제약 최소 377ms, Euclid는 8분 초과로 Motif Tool(최대 25ms)보다 훨씬 느림. DNA Fountain도 더 느림. 설계상 motif 최소 60 base 제한이 있어 일부 크기는 미생성.

> [!success]- 전체 제약 동시 만족 비교 (§5.3, Table 1·2, p.8-9)
> IDT(상용 DNA 제공사) 제약 기반 파라미터(Table 1): keySize=20, keyNum=8, payloadSize=60, payloadNum=15, maxHom=5, maxHairpin=1, loopSize∈[6,7], minGC=25%, maxGC=65%. motif 최대 크기 2×20+60=100 bp. grid search 2회로 하이퍼파라미터 튜닝, 5분 budget.
> - **제약 없이** 생성 시간: Motif Tool 2.3×10^-2 s, DNA Fountain 1.22×10^-1 s, Euclid >5min, Preuss 5.30×10^-1 s, random 2.1×10^-3 s (Table 2).
> - **Table 1 전체 제약 만족** 시간: Motif Tool 2.54 s, 그 외(DNA Fountain·Euclid·Preuss·random) 모두 >5min(=5분 내 미생성) (Table 2).
> - 즉, 전 제약을 동시에 만족하는 motif 집합을 5분 내 생성한 도구는 Motif Generation Tool이 유일.

## 섹션 노트
- **Abstract/§1 (p.1-2)**: synthesis 비용 문제, motif(=key+payload) 조립 기반 write, 두 가지 기여(MDP 정식화 + validation 도구), Zenodo 코드/데이터 링크 공개.
- **§2 (p.2-5)**: MDP 정식화, 통합 reward(log score 가중합+softmax), 4개 제약(homopolymer·GC·hairpin·no-key-in-payload)별 log score 유도.
- **§3 (p.6-7)**: Markov Chain 기반 key·payload 생성, 상태/전이 수 상한.
- **§4 Related Work (p.7)**: DNA Fountain[11], Euclid[21], Preuss[17], 합성 회피/composite letter[3] 등. MDP 사용은 Constraint Programming·text completion과 유사하나 4 base만 score하는 점이 다름. 전체 survey는 Heins et al.[14].
- **§5 (p.7-9)**: validation 도구, 단일/전체 제약 평가.
- **§6 Conclusion (p.9)**: Yan et al.[26] motif 표현을 따르며, 진화하는 기술 제약에 적응 가능한 신뢰성 있는 DNA 서열 생성 자동화의 첫 단계.

## 핵심 용어
- **Motif**: 데이터의 한 글자(알파벳 심볼)를 운반하는 짧은 DNA 서열. payload를 한두 개의 key로 감싼 구조.
- **Payload (P)**: motif에서 실제 데이터 비트를 운반하는 부분 서열(길이 L_P).
- **Key (S0, S1)**: payload 양옆에 붙는 서열로, 지정된 reverse-complemented bridge에만 annealing되어 motif 조립을 제어.
- **Bridge**: 두 key의 reverse complement(S1', S2')를 이은 단일가닥 서열. 인접 motif의 key들이 여기에 결합.
- **Bridged oligonucleotide assembly**: 미리 합성한 motif들을 bridge로 연결(annealing+ligation+PCR)해 긴 가닥을 만드는 조립 방식.
- **Homopolymer**: 같은 nucleotide가 연속 반복된 구간. PCR 시 insertion/deletion/substitution 오류 유발 → maxHom으로 제한.
- **GC-content**: 서열에서 G/C base 비율. 너무 높거나 낮으면 denaturation/PCR 등 화학 공정에 악영향 → [minGC, maxGC].
- **Hairpin**: 서열 S와 그 reverse complement S'가 loop를 사이에 두고 self-annealing해 만드는 stem+loop 2차 구조. sequencing 방해.
- **Log score ls(i,x)**: base i가 제약 x를 위반하는 정도를 나타내는 non-positive 값. 작을수록 선택 확률 낮음.
- **Shape / weight hyperparameter**: 각각 log score 곡선의 민감도(h_*)와 제약 간 상대 가중치(w_x)를 조절.
- **MDP (Markov Decision Process)**: 부분 payload를 base 단위로 확장하는 의사결정 과정의 정식화 [13].

## 강점 · 한계 · 열린 질문
- **강점**: 제약을 reward로 통일적으로 인코딩 → 새 기술 제약 추가/조정이 쉬움. 조립 순서에 무관하게(다른 motif 조합 포함) 제약을 항상 만족. 전 제약 동시 생성에서 기존 도구·random 대비 압도적으로 빠름(2.54s vs >5min). 생성과 독립적인 validation 도구 제공, 코드/데이터 공개(Zenodo).
- **한계**: 상태/전이 수가 4^{size×num}로 지수적이라 큰 motif/많은 key·payload에는 확장성 우려. reward 계산이 4 base에 한해 효율적이며 base가 많아지면 모델 학습이 더 나을 수 있다고 저자도 인정(§4). key 집합 K는 주어졌다고 가정. hairpin stem 크기 데이터 부재로 maxHairpin=1로 보수적 설정. 모든 key/payload 길이가 동일하다는 단순화 가정. 실제 wet-lab 합성/sequencing 실증이 아닌 in-silico 생성/검증 위주.
- **열린 질문**: key 자체를 제약 하에 공동 최적화하면? 가변 길이 motif와 더 큰 알파벳에서의 성능/용량 trade-off는? 학습 기반(RL) 생성기가 제약 수·base 수 증가 시 더 유리한 지점은? 실제 IDT 합성 산물의 오류율과의 상관은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 왜 nucleotide 단위 synthesis 대신 motif 조립을 쓰는가?
> > nucleotide 단위 합성은 1TB에 4억 USD 초과로 너무 비싸고 느리다(p.1). motif는 미리 PCR로 대량 생산해 두고 bridge로 조립만 하면 되어 더 싸고 write bandwidth가 높다.

> [!question]- Q2. motif는 어떤 구조이며 어떻게 조립되는가?
> > payload P를 한두 개의 key(S0, S1)로 감싼 구조(Figure 1). 인접 motif의 key들이 bridge(S1', S2'의 reverse complement)에 annealing된 뒤 PCR로 빠진 base를 채워 double-stranded DNA가 된다(p.2).

> [!question]- Q3. 제약을 어떻게 생성 과정에 반영하는가?
> > 각 후보 base에 대해 제약 위반 정도를 non-positive log score로 정의하고, 가중합 후 softmax로 확률화한다(Eq.1-2). 위반이 클수록 선택 확률이 낮아진다(p.2-3).

> [!question]- Q4. 다루는 4가지 제약은 무엇인가?
> > homopolymer 길이 제한, GC-content 범위, hairpin(2차 구조) 회피, payload 내 key 출현 회피이다(§2.3-2.6). 각각 별도 log score로 정의된다.

> [!question]- Q5. 왜 단일 motif뿐 아니라 motif 조합까지 제약을 확인해야 하는가?
> > motif는 임의 순서로 조립되므로, 한 motif가 다른 motif(자신 포함)와 결합했을 때도 hairpin/GC 등 제약을 위반하면 안 되기 때문이다. 그래서 현재 payload를 기존 집합과 합쳐 worst-case로 평가한다(p.5, §5.1).

> [!question]- Q6. 핵심 평가 수치는?
> > Table 1 전 제약 동시 만족: Motif Generation Tool 2.54초, 나머지 모든 도구(DNA Fountain·Euclid·Preuss·random)는 5분 내 미생성(>5min) (Table 2, p.9). 제약 없을 때도 2.3×10^-2초로 빠른 편.

> [!question]- Q7. 상태 공간의 확장성 문제는?
> > key/payload Markov Chain의 최대 상태·전이 수는 4^{keySize×keyNum} 및 4^{payloadSize×payloadNum}로 지수적(Eq.11-12, p.6-7)이라 큰 motif·다수 key/payload에는 확장성이 제한된다.

> [!question]- Q8. validation 도구는 왜 따로 두는가?
> > 생성 방식과 무관하게 임의의 motif 집합이 제약(단독 및 조합)을 만족하는지 독립적으로 pass/fail 검증하기 위해서다(§5.1, p.7).

## 🔗 Connections
[[Infra]] · [[FAST]] · [[2025]]

## References worth following
- [26] Yiqing Yan et al., "Scaling logical density of DNA storage with enzymatically-ligated composite motifs," *Scientific Reports* 2023 — 본 논문이 따르는 motif 데이터 표현의 토대.
- [11] Yaniv Erlich, Dina Zielinski, "DNA fountain enables a robust and efficient storage architecture," *Science* 2017 — 주요 비교 대상 인코딩 도구.
- [21] Omer S. Sella et al., "DNA archival storage, a bottom up approach," HotStorage 2021 — 비교 대상(Euclid), 저자 그룹의 선행 연구.
- [17] Inbal Preuss et al., "Efficient DNA-based data storage using shortmer combinatorial encoding," *Scientific Reports* 2024 — 비교 대상 도구.
- [14] Thomas Heinis et al., "Survey of information encoding techniques for DNA," *ACM Computing Surveys* 2023 — DNA 인코딩 전반 survey.
- [25] Yixun Wei et al., "An encoding scheme to enlarge practical DNA storage capacity by reducing primer-payload collisions," ASPLOS 2024 — key-in-payload 충돌 문제 관련.

## Personal annotations
<본인 메모 영역>
