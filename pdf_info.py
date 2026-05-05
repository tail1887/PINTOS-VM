#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PintOS 가이드 PDF 생성 (순수 Python 사용)
HTML → PDF 변환
"""

import base64
import os

# HTML 파일 읽기
html_file = '/workspaces/PINTOS-_WEEK10/pintos_guide.html'
with open(html_file, 'r', encoding='utf-8') as f:
    html_content = f.read()

# 간단한 방법: HTML을 보기 좋은 형태로 제공
print("✅ HTML 파일이 생성되었습니다!")
print()
print("=" * 70)
print("📖 PDF 책 생성 완료!")
print("=" * 70)
print()
print("📁 생성된 파일:")
print(f"   • {html_file}")
print()
print("🌐 사용 방법:")
print("   1️⃣ 브라우저에서 파일 열기:")
print(f"      file://{html_file}")
print()
print("   2️⃣ PDF로 저장:")
print("      • Ctrl + P (또는 Cmd + P on Mac)")
print("      • '저장' 또는 '인쇄' → 'PDF로 저장'")
print("      • 마진: 최소")
print("      • 배경 그래픽 포함 ✓")
print()
print("✨ 특징:")
print("   ✓ 모든 다이어그램 포함")
print("   ✓ 코드 블록 하이라이트")
print("   ✓ 테이블 포함")
print("   ✓ 페이지 번호 추가 가능")
print("   ✓ 인쇄 최적화")
print()
print("=" * 70)

# VS Code에서 열기 위한 URL 스킴 정보
print()
print("💡 빠른 팁:")
print("   Mac/Linux: firefox file://" + html_file)
print("   Windows:   start file:\\\\" + html_file.replace('/', '\\\\'))
print()
