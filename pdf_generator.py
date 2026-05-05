#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Pure Python PDF Generator
PDF 기본 구조를 이용한 텍스트 기반 PDF 생성
"""

import os
from datetime import datetime

def create_simple_pdf():
    """순수 Python으로 간단한 PDF 생성"""
    
    # 마크다운 읽기
    with open('/workspaces/PINTOS-_WEEK10/pintos_guide.md', 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    # PDF 기본 구조
    pdf_content = []
    
    # PDF 헤더
    pdf_content.append(b'%PDF-1.4\n')
    
    # 객체들 (간단한 버전)
    objects = []
    obj_offsets = []
    
    def add_object(content):
        """PDF 객체 추가"""
        obj_offsets.append(len(b''.join([bytes(x, 'utf-8') if isinstance(x, str) else x for x in pdf_content])))
        objects.append(content)
    
    # 1번 객체: 카탈로그
    add_object("""1 0 obj
<< /Type /Catalog /Pages 2 0 R >>
endobj
""")
    
    # 2번 객체: 페이지 트리
    add_object("""2 0 obj
<< /Type /Pages /Kids [3 0 R] /Count 1 >>
endobj
""")
    
    # 3번 객체: 페이지
    add_object("""3 0 obj
<< /Type /Page /Parent 2 0 R /Resources 4 0 R /MediaBox [0 0 612 792] /Contents 5 0 R >>
endobj
""")
    
    # 4번 객체: 자원
    add_object("""4 0 obj
<< /Font << /F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> >> >>
endobj
""")
    
    # 5번 객체: 스트림 (콘텐츠)
    stream_content = "BT\n"
    stream_content += "/F1 24 Tf\n"
    stream_content += "50 700 Td\n"
    stream_content += "(PintOS Week10) Tj\n"
    stream_content += "/F1 18 Tf\n"
    stream_content += "0 -50 Td\n"
    stream_content += "(Complete Guide) Tj\n"
    stream_content += "/F1 12 Tf\n"
    stream_content += "0 -50 Td\n"
    stream_content += "(95/95 Tests PASS) Tj\n"
    stream_content += "ET\n"
    
    add_object(f"""5 0 obj
<< /Length {len(stream_content)} >>
stream
{stream_content}
endstream
endobj
""")
    
    # 크로스 레퍼런스 테이블 작성
    xref_offset = len(b''.join([bytes(x, 'utf-8') if isinstance(x, str) else x for x in pdf_content + objects]))
    
    # PDF 생성
    for obj in objects:
        pdf_content.append(obj)
    
    xref = f"""xref
0 {len(objects) + 1}
0000000000 65535 f 
"""
    
    for i, obj in enumerate(objects):
        offset = 0
        for j in range(i):
            offset += len(objects[j]) + len(str(j+1)) + 20
        xref += f"{offset:010d} 00000 n \n"
    
    trailer = f"""trailer
<< /Size {len(objects) + 1} /Root 1 0 R >>
startxref
{xref_offset}
%%EOF
"""
    
    # PDF 저장
    pdf_bytes = b'%PDF-1.4\n'
    for obj in objects:
        pdf_bytes += obj.encode('utf-8') if isinstance(obj, str) else obj
    
    pdf_file = '/workspaces/PINTOS-_WEEK10/pintos_guide.pdf'
    with open(pdf_file, 'wb') as f:
        f.write(pdf_bytes)
    
    return pdf_file

# 더 간단한 방법: HTML을 직접 복사해서 "인쇄용" 버전 만들기
def create_html_printable():
    """인쇄용 HTML 생성"""
    html_file = '/workspaces/PINTOS-_WEEK10/pintos_guide.html'
    print_file = '/workspaces/PINTOS-_WEEK10/pintos_guide_PRINT.html'
    
    with open(html_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 페이지 번호 추가
    content = content.replace(
        '<footer>',
        '''<script>
            // PDF 저장 명령 안내
            document.addEventListener('DOMContentLoaded', function() {
                console.log('📖 PDF 저장 방법:');
                console.log('1. Ctrl + P (또는 Cmd + P)');
                console.log('2. "PDF로 저장" 선택');
                console.log('3. 저장!');
            });
        </script>
        <footer>'''
    )
    
    with open(print_file, 'w', encoding='utf-8') as f:
        f.write(content)
    
    return print_file

if __name__ == "__main__":
    print("=" * 70)
    print("📖 PDF 변환 시도")
    print("=" * 70)
    print()
    
    # 인쇄용 HTML 생성
    print("1️⃣ 인쇄용 HTML 생성...")
    print_html = create_html_printable()
    print(f"✅ {print_html}")
    print()
    
    # 간단한 PDF 생성 시도
    try:
        print("2️⃣ 간단한 PDF 생성 시도...")
        pdf_file = create_simple_pdf()
        if os.path.exists(pdf_file) and os.path.getsize(pdf_file) > 100:
            print(f"✅ PDF 생성 완료: {pdf_file}")
        else:
            raise Exception("PDF 파일이 너무 작음")
    except Exception as e:
        print(f"⚠️ 직접 PDF 생성 실패: {e}")
        print()
        print("=" * 70)
        print("💡 대신 이 방법을 사용하세요:")
        print("=" * 70)
        print()
        print("✅ 방법 1: HTML에서 직접 PDF로 저장 (추천)")
        print("  1. HTML 파일 다운로드:")
        print(f"     /workspaces/PINTOS-_WEEK10/pintos_guide.html")
        print("  2. 브라우저에서 열기")
        print("  3. Ctrl + P (또는 Cmd + P on Mac)")
        print("  4. '대상' → '저장'")
        print("  5. PDF로 저장 완료!")
        print()
        print("✅ 방법 2: 온라인 변환")
        print("  1. https://html2pdf.com/")
        print("  2. HTML 파일 업로드")
        print("  3. PDF 다운로드")
        print()
        print("✅ 방법 3: VS Code 내장 기능")
        print("  1. Markdown Preview Enhanced 확장 설치")
        print("  2. pintos_guide.md 우클릭")
        print("  3. 'Export as PDF' 선택")
