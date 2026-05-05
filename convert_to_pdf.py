#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HTML to PDF 변환 (고급 버전)
weasyprint 또는 reportlab 설치 후 변환
"""

import subprocess
import sys
import os

def try_weasyprint():
    """WeasyPrint로 HTML to PDF 변환"""
    try:
        print("📦 WeasyPrint 설치 시도...")
        os.system(f"{sys.executable} -m pip install weasyprint -q 2>/dev/null")
        from weasyprint import HTML
        
        html_file = '/workspaces/PINTOS-_WEEK10/pintos_guide.html'
        pdf_file = '/workspaces/PINTOS-_WEEK10/pintos_guide.pdf'
        
        HTML(html_file).write_pdf(pdf_file)
        print(f"✅ WeasyPrint로 PDF 생성 완료!")
        return pdf_file
    except Exception as e:
        print(f"⚠️ WeasyPrint 실패: {e}")
        return None

def try_reportlab():
    """ReportLab으로 마크다운 to PDF 변환"""
    try:
        print("📦 ReportLab 설치 시도...")
        os.system(f"{sys.executable} -m pip install reportlab -q 2>/dev/null")
        from reportlab.lib.pagesizes import letter, A4
        from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle
        from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
        from reportlab.lib.units import inch
        from reportlab.lib import colors
        
        # 마크다운 읽기
        with open('/workspaces/PINTOS-_WEEK10/pintos_guide.md', 'r', encoding='utf-8') as f:
            content = f.read()
        
        # PDF 준비
        pdf_file = '/workspaces/PINTOS-_WEEK10/pintos_guide.pdf'
        doc = SimpleDocTemplate(pdf_file, pagesize=A4, topMargin=0.5*inch, bottomMargin=0.5*inch)
        
        # 스타일 정의
        styles = getSampleStyleSheet()
        title_style = ParagraphStyle(
            'CustomTitle',
            parent=styles['Heading1'],
            fontSize=28,
            textColor=colors.HexColor('#2c3e50'),
            spaceAfter=30,
            alignment=1  # center
        )
        
        heading_style = ParagraphStyle(
            'CustomHeading',
            parent=styles['Heading2'],
            fontSize=16,
            textColor=colors.HexColor('#3498db'),
            spaceAfter=12,
            spaceBefore=12
        )
        
        # 요소 생성
        story = []
        
        # 제목
        story.append(Paragraph("📚 PintOS Week10", title_style))
        story.append(Paragraph("Complete Guide", title_style))
        story.append(Spacer(1, 0.3*inch))
        story.append(Paragraph("✅ 95/95 Tests PASS | 🎯 Production Level", styles['Normal']))
        story.append(PageBreak())
        
        # 내용 추가
        lines = content.split('\n')
        for line in lines:
            if line.startswith('# '):
                story.append(PageBreak())
                story.append(Paragraph(line[2:], title_style))
            elif line.startswith('## '):
                story.append(Paragraph(line[3:], heading_style))
            elif line.startswith('### '):
                story.append(Paragraph(line[4:], styles['Heading3']))
            elif line.startswith('- '):
                story.append(Paragraph("• " + line[2:], styles['BodyText']))
            elif line.startswith('```'):
                # 코드 블록은 스킵
                pass
            elif line.strip() and not line.startswith('|'):
                story.append(Paragraph(line.strip(), styles['BodyText']))
            elif line.strip():
                story.append(Spacer(1, 0.1*inch))
        
        # PDF 생성
        doc.build(story)
        print(f"✅ ReportLab으로 PDF 생성 완료!")
        return pdf_file
    except Exception as e:
        print(f"⚠️ ReportLab 실패: {e}")
        return None

def try_pypdf():
    """PyPDF로 HTML + CSS 처리"""
    try:
        print("📦 xhtml2pdf 설치 시도...")
        os.system(f"{sys.executable} -m pip install xhtml2pdf -q 2>/dev/null")
        from xhtml2pdf import pisa
        
        html_file = '/workspaces/PINTOS-_WEEK10/pintos_guide.html'
        pdf_file = '/workspaces/PINTOS-_WEEK10/pintos_guide.pdf'
        
        with open(html_file, 'rb') as f:
            html = f.read()
        
        with open(pdf_file, 'wb') as f:
            pisa.CreatePDF(html, f)
        
        print(f"✅ xhtml2pdf로 PDF 생성 완료!")
        return pdf_file
    except Exception as e:
        print(f"⚠️ xhtml2pdf 실패: {e}")
        return None

def main():
    print("=" * 70)
    print("📖 PDF 파일 생성 시작")
    print("=" * 70)
    print()
    
    pdf_file = None
    
    # 순서대로 시도
    print("1️⃣ WeasyPrint 시도...")
    pdf_file = try_weasyprint()
    
    if not pdf_file:
        print()
        print("2️⃣ xhtml2pdf 시도...")
        pdf_file = try_pypdf()
    
    if not pdf_file:
        print()
        print("3️⃣ ReportLab 시도...")
        pdf_file = try_reportlab()
    
    # 결과
    print()
    print("=" * 70)
    
    if pdf_file and os.path.exists(pdf_file):
        size = os.path.getsize(pdf_file) / 1024
        print(f"✅ PDF 파일 생성 완료!")
        print(f"📁 파일: {pdf_file}")
        print(f"📊 크기: {size:.1f} KB")
        print()
        print("🎉 다음 단계:")
        print(f"   → 파일 다운로드 및 열기")
        print(f"   → 브라우저나 PDF 리더로 확인")
        print(f"   → 인쇄 또는 공유")
    else:
        print("⚠️  라이브러리 설치 실패")
        print()
        print("💡 대안:")
        print("1. HTML 파일을 브라우저에서 열기:")
        print("   file:///workspaces/PINTOS-_WEEK10/pintos_guide.html")
        print()
        print("2. 브라우저 인쇄 기능으로 PDF 저장:")
        print("   Ctrl + P → 'PDF로 저장'")
        print()
        print("3. 또는 온라인 변환 도구 사용:")
        print("   https://cloudconvert.com/html-to-pdf")

if __name__ == "__main__":
    main()
