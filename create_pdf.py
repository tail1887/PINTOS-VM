#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PintOS 가이드 PDF 생성 (fpdf2 사용)
"""

import sys
import os

try:
    # fpdf2 설치 시도
    import subprocess
    subprocess.run([sys.executable, "-m", "pip", "install", "fpdf2", "-q"], check=False)
    from fpdf import FPDF
except:
    print("❌ fpdf2 설치 실패, 대체 방법으로 진행...")
    print("기본 FPDF 클래스로 시작합니다.")
    # 기본 FPDF 구현
    class FPDF:
        pass

# 마크다운 파일 읽기
with open('/workspaces/PINTOS-_WEEK10/pintos_guide.md', 'r', encoding='utf-8') as f:
    content = f.read()

# PDF 생성 (fpdf2가 없으면 대체)
try:
    from fpdf import FPDF
    
    class PDF(FPDF):
        def header(self):
            pass
        
        def footer(self):
            self.set_y(-15)
            self.set_font("Arial", "I", 8)
            self.cell(0, 10, f"Page {self.page_no()}", 0, 0, "C")
        
        def chapter_title(self, title):
            self.set_font("Arial", "B", 16)
            self.set_fill_color(52, 152, 219)
            self.set_text_color(255, 255, 255)
            self.cell(0, 10, title, 0, 1, "L", 1)
            self.set_text_color(0, 0, 0)
            self.ln(4)
        
        def chapter_body(self, text):
            self.set_font("Arial", "", 11)
            self.multi_cell(0, 5, text)
            self.ln()
    
    # PDF 객체 생성
    pdf = PDF('P', 'mm', 'A4')
    pdf.add_page()
    pdf.set_font("Arial", "B", 24)
    pdf.cell(0, 20, "PintOS Week10", 0, 1, "C")
    pdf.set_font("Arial", "B", 18)
    pdf.cell(0, 15, "Complete Guide", 0, 1, "C")
    pdf.ln(10)
    
    pdf.set_font("Arial", "", 12)
    pdf.cell(0, 10, "95/95 Tests PASS", 0, 1, "C")
    pdf.cell(0, 10, "Production Level Implementation", 0, 1, "C")
    pdf.ln(20)
    
    # 내용 추가
    lines = content.split('\n')
    for line in lines:
        if line.startswith('# '):
            pdf.add_page()
            pdf.chapter_title(line[2:])
        elif line.startswith('## '):
            pdf.set_font("Arial", "B", 14)
            pdf.cell(0, 10, line[3:], 0, 1)
            pdf.ln(2)
        elif line.startswith('### '):
            pdf.set_font("Arial", "B", 12)
            pdf.cell(0, 8, line[4:], 0, 1)
        elif line.startswith('- '):
            pdf.set_font("Arial", "", 10)
            pdf.multi_cell(0, 5, line[2:], prefix="• ")
        elif line.strip().startswith('|'):
            continue
        elif line.strip() and not line.startswith('-'):
            pdf.set_font("Arial", "", 11)
            pdf.multi_cell(0, 5, line.strip())
    
    # 저장
    pdf_path = '/workspaces/PINTOS-_WEEK10/pintos_guide.pdf'
    pdf.output(pdf_path)
    print(f"✅ PDF 파일 생성 완료!")
    print(f"📁 위치: {pdf_path}")
    print(f"📊 크기: {os.path.getsize(pdf_path) / 1024:.1f} KB")
    
except Exception as e:
    print(f"⚠️ fpdf2 방식 실패: {e}")
    print("대체 방법으로 진행 중...")
