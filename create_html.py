#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os

# 마크다운 읽기
with open('/workspaces/PINTOS-_WEEK10/pintos_guide.md', 'r', encoding='utf-8') as f:
    content = f.read()

# 기본 HTML 구조 생성
html = """<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PintOS Week10 완성 가이드</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            line-height: 1.8;
            color: #2c3e50;
            background: #ecf0f1;
            padding: 20px;
        }
        
        .container {
            max-width: 900px;
            margin: 0 auto;
            background: white;
            padding: 60px 50px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.1);
            border-radius: 10px;
        }
        
        h1 {
            font-size: 2.5em;
            color: #2c3e50;
            margin: 40px 0 20px 0;
            border-bottom: 4px solid #3498db;
            padding-bottom: 15px;
            text-align: center;
        }
        
        h2 {
            font-size: 2em;
            color: #34495e;
            margin: 35px 0 15px 0;
            border-left: 5px solid #3498db;
            padding-left: 15px;
        }
        
        h3 {
            font-size: 1.5em;
            color: #7f8c8d;
            margin: 20px 0 10px 0;
        }
        
        h4 {
            font-size: 1.2em;
            color: #34495e;
            margin: 15px 0 8px 0;
        }
        
        p, li {
            font-size: 1.05em;
            margin: 12px 0;
            line-height: 1.9;
        }
        
        ul, ol {
            margin-left: 30px;
            margin-bottom: 15px;
        }
        
        li {
            margin: 8px 0;
        }
        
        code {
            background: #f5f5f5;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
            color: #d63384;
            font-size: 0.95em;
        }
        
        pre {
            background: #f8f9fa;
            padding: 15px 20px;
            border-radius: 5px;
            overflow-x: auto;
            margin: 15px 0;
            border-left: 4px solid #3498db;
            font-family: 'Courier New', monospace;
            font-size: 0.95em;
            line-height: 1.5;
        }
        
        pre code {
            background: none;
            color: #2c3e50;
            padding: 0;
        }
        
        table {
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
            background: white;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        
        th {
            background: #3498db;
            color: white;
            padding: 15px;
            text-align: left;
            font-weight: bold;
        }
        
        td {
            padding: 12px 15px;
            border-bottom: 1px solid #ecf0f1;
        }
        
        tr:hover {
            background: #f8f9fa;
        }
        
        tr:last-child td {
            border-bottom: none;
        }
        
        .emoji {
            font-size: 1.2em;
            margin-right: 8px;
        }
        
        .highlight {
            background: #fff3cd;
            padding: 15px 20px;
            border-left: 4px solid #ffc107;
            margin: 15px 0;
            border-radius: 3px;
        }
        
        .success {
            background: #d4edda;
            padding: 15px 20px;
            border-left: 4px solid #28a745;
            margin: 15px 0;
            border-radius: 3px;
        }
        
        .info {
            background: #d1ecf1;
            padding: 15px 20px;
            border-left: 4px solid #17a2b8;
            margin: 15px 0;
            border-radius: 3px;
        }
        
        .summary {
            background: #e7f3ff;
            padding: 20px 25px;
            border: 2px solid #3498db;
            border-radius: 5px;
            margin: 25px 0;
        }
        
        .checklist {
            background: #f9f9f9;
            padding: 20px 25px;
            border-radius: 5px;
            margin: 20px 0;
        }
        
        .checklist li {
            margin: 8px 0;
        }
        
        strong {
            color: #2c3e50;
            font-weight: 600;
        }
        
        em {
            color: #7f8c8d;
        }
        
        .page-break {
            page-break-after: always;
            margin: 40px 0;
            border-bottom: 2px dashed #bdc3c7;
            padding-bottom: 20px;
        }
        
        .title-page {
            text-align: center;
            padding: 100px 0;
            border-bottom: 3px solid #3498db;
            margin-bottom: 50px;
        }
        
        .title-page h1 {
            font-size: 3.5em;
            border: none;
            margin: 0;
            padding: 0;
            color: #2c3e50;
        }
        
        .title-page p {
            font-size: 1.5em;
            color: #7f8c8d;
            margin: 20px 0;
        }
        
        footer {
            text-align: center;
            margin-top: 50px;
            padding-top: 20px;
            border-top: 2px solid #bdc3c7;
            color: #7f8c8d;
            font-size: 0.95em;
        }
        
        @media print {
            body {
                background: white;
                padding: 0;
            }
            
            .container {
                max-width: 100%;
                box-shadow: none;
                padding: 50px 40px;
            }
            
            h1, h2, h3 {
                page-break-after: avoid;
                page-break-inside: avoid;
            }
            
            table {
                page-break-inside: avoid;
            }
            
            pre {
                page-break-inside: avoid;
            }
        }
        
        @media (max-width: 768px) {
            .container {
                padding: 30px 20px;
            }
            
            h1 {
                font-size: 2em;
            }
            
            h2 {
                font-size: 1.5em;
            }
            
            table {
                font-size: 0.95em;
            }
            
            td, th {
                padding: 10px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="title-page">
            <h1>📚 PintOS Week10</h1>
            <h1 style="font-size: 2.5em; color: #3498db;">완성 가이드</h1>
            <p>🎯 95/95 테스트 PASS</p>
            <p>✅ 프로덕션 수준의 OS 구현</p>
            <p style="margin-top: 50px; color: #7f8c8d; font-size: 1em;">
                작성일: 2026년 5월 5일
            </p>
        </div>
        
        <div class="info" style="text-align: center; margin-bottom: 40px;">
            <strong>이 문서는 PintOS Week10 프로젝트의 완성 내용을 누구나 이해할 수 있도록 정리했습니다.</strong><br>
            5분이면 전체 구조를 이해할 수 있고, 누군가에게 쉽게 설명할 수 있습니다.
        </div>
"""

# 마크다운 간단한 변환 (정규표현식 사용)
import re

# 코드 블록 처리
content = re.sub(
    r'```(.*?)\n(.*?)```',
    lambda m: f'<pre><code>{re.escape(m.group(2))}</code></pre>',
    content,
    flags=re.DOTALL
)

# 이모지와 제목 처리
lines = content.split('\n')
output_lines = []

for line in lines:
    # h1
    if line.startswith('# '):
        output_lines.append(f'<h1>{line[2:]}</h1>')
    # h2
    elif line.startswith('## '):
        output_lines.append(f'<h2>{line[3:]}</h2>')
    # h3
    elif line.startswith('### '):
        output_lines.append(f'<h3>{line[4:]}</h3>')
    # h4
    elif line.startswith('#### '):
        output_lines.append(f'<h4>{line[5:]}</h4>')
    # 링크 변환
    else:
        line = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2" style="color: #3498db; text-decoration: none;"><strong>\1</strong></a>', line)
        # 강조 변환 (**)
        line = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', line)
        # 기울임 변환 (*)
        line = re.sub(r'\*([^*]+)\*', r'<em>\1</em>', line)
        # 백틱 변환
        line = re.sub(r'`([^`]+)`', r'<code>\1</code>', line)
        output_lines.append(line)

# 다시 합치기
content = '\n'.join(output_lines)

# 테이블 처리 (간단한 마크다운 테이블)
lines = content.split('\n')
final_lines = []
i = 0
while i < len(lines):
    line = lines[i]
    if '|' in line and i + 1 < len(lines) and '|' in lines[i+1] and '-' in lines[i+1]:
        # 테이블 시작
        table_start = i
        table_end = i + 2
        while table_end < len(lines) and '|' in lines[table_end]:
            table_end += 1
        
        # 테이블 생성
        table_html = '<table>\n<thead>\n<tr>'
        header = lines[table_start].split('|')[1:-1]
        for cell in header:
            table_html += f'<th>{cell.strip()}</th>'
        table_html += '</tr>\n</thead>\n<tbody>\n'
        
        for row_idx in range(table_start + 2, table_end):
            table_html += '<tr>'
            cells = lines[row_idx].split('|')[1:-1]
            for cell in cells:
                table_html += f'<td>{cell.strip()}</td>'
            table_html += '</tr>\n'
        
        table_html += '</tbody>\n</table>'
        final_lines.append(table_html)
        i = table_end
    else:
        final_lines.append(line)
        i += 1

content = '\n'.join(final_lines)

# 리스트 처리
lines = content.split('\n')
final_html = []
in_list = False
list_type = None

for line in lines:
    if line.strip().startswith('- '):
        if not in_list:
            final_html.append('<ul>')
            in_list = True
            list_type = 'ul'
        final_html.append(f'<li>{line.strip()[2:]}</li>')
    elif line.strip().startswith('├─ ') or line.strip().startswith('└─ ') or line.strip().startswith('  '):
        if not in_list:
            final_html.append('<ul style="margin-left: 50px;">')
            in_list = True
        final_html.append(f'<li>{line.strip()}</li>')
    elif line.strip() and line[0].isdigit() and '. ' in line:
        if not in_list:
            final_html.append('<ol>')
            in_list = True
            list_type = 'ol'
        final_html.append(f'<li>{line.strip()[line.find(". ")+2:]}</li>')
    else:
        if in_list:
            final_html.append('</ul>' if list_type == 'ul' else '</ol>')
            in_list = False
        if line.strip():
            final_html.append(f'<p>{line}</p>')
        else:
            final_html.append('')

if in_list:
    final_html.append('</ul>' if list_type == 'ul' else '</ol>')

content = '\n'.join(final_html)

# 최종 HTML 조합
html += content
html += """
        <footer>
            <p>© 2026 PintOS Week10 Project | 모든 테스트 통과 ✅ | 프로덕션 수준의 완성도</p>
        </footer>
    </div>
</body>
</html>
"""

# 파일 저장
output_path = '/workspaces/PINTOS-_WEEK10/pintos_guide.html'
with open(output_path, 'w', encoding='utf-8') as f:
    f.write(html)

print(f"✅ HTML 파일 생성 완료: {output_path}")
print(f"📄 파일 크기: {os.path.getsize(output_path) / 1024:.1f} KB")
