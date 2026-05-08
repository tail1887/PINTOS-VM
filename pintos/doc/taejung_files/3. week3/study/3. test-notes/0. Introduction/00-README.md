# Project3 Introduction 노트

이 폴더는 `0. Introduction.md`를 기준으로 Project 3 전체 범위와 개발 순서를 잡기 위한 진입 문서입니다.

## 원본 문서 위치

- `../../../../0. references/kaist_docs/project3/0. Introduction.md`

## 이 문서에서 확인할 것

- Project 3의 목표: Virtual Memory
- 새로 다루는 소스 파일 범위
- VM에서 쓰는 주요 용어
- SPT, frame table, swap table, mmap의 역할 구분

## 개발 순서 연결

1. `../1. Memory Management`
2. `../2. Anonymous Page`
3. `../3. Stack Growth`
4. `../4. Memory Mapped Files`
5. `../5. Swap In&Out`
6. `../6. Copy-on-write (Extra)`는 기본 구현 완료 후 진행

## 빠른 체크 질문

- page와 frame을 구분해서 설명할 수 있는가?
- page fault가 항상 프로세스 종료가 아니라는 점을 이해했는가?
- anonymous/file-backed page의 backing store 차이를 말할 수 있는가?
